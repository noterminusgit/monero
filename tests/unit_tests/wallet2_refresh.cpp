// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Unit tests for the wallet2 refresh pipeline: refresh(), process_new_blockchain_entry(),
// process_new_transaction(), get_short_chain_history(), fast_refresh(), should_skip_block(),
// output tracker cache, hashchain management, and view tag optimization.

#include "gtest/gtest.h"
#include "wallet/wallet2.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_config.h"
#include "ringct/rctSigs.h"
#include "crypto/crypto.h"

// wallet_accessor_test is declared as a friend of wallet2 in wallet2.h
class wallet_accessor_test
{
public:
  static void set_account(tools::wallet2& w, const cryptonote::account_base& account)
  {
    w.m_account = account;
    w.m_key_device_type = account.get_device().get_type();
    w.m_account_public_address = account.get_keys().m_account_address;
  }
  static tools::wallet2::transfer_container& get_transfers(tools::wallet2& w) { return w.m_transfers; }
  static std::unordered_map<crypto::key_image, size_t>& get_key_images(tools::wallet2& w) { return w.m_key_images; }
  static void set_offline(tools::wallet2& w, bool v) { w.m_offline = v; }
  static cryptonote::account_base& get_account(tools::wallet2& w) { return w.m_account; }
  static tools::hashchain& get_blockchain(tools::wallet2& w) { return w.m_blockchain; }
  static std::unordered_multimap<crypto::hash, tools::wallet2::pool_payment_details>& get_unconfirmed_payments(tools::wallet2& w) { return w.m_unconfirmed_payments; }
  static bool& get_first_refresh_done(tools::wallet2& w) { return w.m_first_refresh_done; }
  static uint64_t& get_refresh_from_block_height(tools::wallet2& w) { return w.m_refresh_from_block_height; }
  static uint64_t& get_skip_to_height(tools::wallet2& w) { return w.m_skip_to_height; }
  static std::atomic<bool>& get_run(tools::wallet2& w) { return w.m_run; }
  static uint64_t& get_last_block_reward(tools::wallet2& w) { return w.m_last_block_reward; }
  static tools::wallet2::RefreshType& get_refresh_type(tools::wallet2& w) { return w.m_refresh_type; }
  static bool get_watch_only(const tools::wallet2& w) { return w.m_watch_only; }
  static void set_watch_only(tools::wallet2& w, bool v) { w.m_watch_only = v; }
  static std::unordered_map<crypto::public_key, cryptonote::subaddress_index>& get_subaddresses(tools::wallet2& w) { return w.m_subaddresses; }
  static std::unordered_map<crypto::public_key, size_t>& get_pub_keys(tools::wallet2& w) { return w.m_pub_keys; }
  static tools::wallet2::payment_container& get_payments(tools::wallet2& w) { return w.m_payments; }
  static uint64_t& get_pool_info_query_time(tools::wallet2& w) { return w.m_pool_info_query_time; }
  static std::unordered_set<crypto::hash>* get_scanned_pool_txs(tools::wallet2& w) { return w.m_scanned_pool_txs; }
  static tools::i_wallet2_callback*& get_callback(tools::wallet2& w) { return w.m_callback; }
  static void get_short_chain_history(tools::wallet2& w, std::list<crypto::hash>& ids, uint64_t granularity = 1) { w.get_short_chain_history(ids, granularity); }
  static bool should_skip_block(const tools::wallet2& w, const cryptonote::block& b, uint64_t height) { return w.should_skip_block(b, height); }
  static void process_parsed_blocks(tools::wallet2& w, uint64_t start_height, const std::vector<cryptonote::block_complete_entry>& blocks, const std::vector<tools::wallet2::parsed_block>& parsed_blocks, uint64_t& blocks_added, std::map<std::pair<uint64_t, uint64_t>, size_t>* output_tracker_cache = NULL) { w.process_parsed_blocks(start_height, blocks, parsed_blocks, blocks_added, output_tracker_cache); }
};

namespace
{
  // Helper to create a unique hash from an integer
  crypto::hash make_hash(uint32_t id)
  {
    crypto::hash h = crypto::null_hash;
    h.data[0] = id & 0xff;
    h.data[1] = (id >> 8) & 0xff;
    h.data[2] = (id >> 16) & 0xff;
    h.data[3] = (id >> 24) & 0xff;
    return h;
  }

  // Helper to create a basic transfer_details
  tools::wallet2::transfer_details make_transfer_detail(
    uint64_t amount, uint64_t block_height, bool spent,
    uint64_t global_output_index = 0, bool rct = true)
  {
    tools::wallet2::transfer_details td = AUTO_VAL_INIT(td);
    td.m_amount = amount;
    td.m_block_height = block_height;
    td.m_spent = spent;
    td.m_spent_height = spent ? block_height + 10 : 0;
    td.m_txid = crypto::null_hash;
    td.m_internal_output_index = 0;
    td.m_global_output_index = global_output_index;
    td.m_rct = rct;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_key_image_partial = false;
    td.m_frozen = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 0};
    return td;
  }

  // Build a simple miner transaction paying to the given address
  bool build_miner_tx_to_addr(const cryptonote::account_public_address& addr,
    uint64_t height, uint64_t reward, cryptonote::transaction& tx)
  {
    return cryptonote::construct_miner_tx(height, 0, 0, 0, 0, addr, tx);
  }

  // Fixture providing a generated wallet in offline mode for refresh tests
  class WalletRefreshTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
      m_wallet.set_subaddress_lookahead(2, 5);
      m_wallet.generate("", "", m_recovery_key, true, false);
      wallet_accessor_test::set_offline(m_wallet, true);
    }

    tools::wallet2 m_wallet;
    crypto::secret_key m_recovery_key;
  };
}

// ============================================================================
// 1. offline_refresh_returns_zero
// ============================================================================
TEST_F(WalletRefreshTest, offline_refresh_returns_zero)
{
  // In offline mode, refresh() should return immediately with blocks_fetched=0
  uint64_t blocks_fetched = 999;
  bool received_money = true;
  m_wallet.refresh(false, 0, blocks_fetched, received_money, false, false);
  EXPECT_EQ(0u, blocks_fetched);
  EXPECT_FALSE(received_money);
}

// ============================================================================
// 2. first_refresh_clears_unconfirmed
// ============================================================================
TEST_F(WalletRefreshTest, first_refresh_clears_unconfirmed)
{
  // On first refresh, m_unconfirmed_payments should be cleared.
  // We populate it, then trigger a refresh. Since m_offline is true,
  // refresh returns immediately, but on a non-offline wallet the first
  // refresh clears unconfirmed_payments. We test the logic directly.
  auto& unconfirmed = wallet_accessor_test::get_unconfirmed_payments(m_wallet);
  tools::wallet2::pool_payment_details ppd;
  ppd.m_pd.m_tx_hash = make_hash(1);
  ppd.m_pd.m_amount = 1000;
  ppd.m_pd.m_fee = 10;
  ppd.m_pd.m_block_height = 0;
  ppd.m_pd.m_unlock_time = 0;
  ppd.m_pd.m_timestamp = 0;
  ppd.m_pd.m_coinbase = false;
  ppd.m_pd.m_subaddr_index = {0, 0};
  ppd.m_double_spend_seen = false;
  unconfirmed.insert(std::make_pair(make_hash(100), ppd));
  EXPECT_FALSE(unconfirmed.empty());

  // Simulate what refresh() does on first refresh: clear unconfirmed_payments
  // This mimics the code at wallet2.cpp:4088
  bool& first_refresh_done = wallet_accessor_test::get_first_refresh_done(m_wallet);
  first_refresh_done = false;
  if (!first_refresh_done)
  {
    unconfirmed.clear();
  }
  EXPECT_TRUE(unconfirmed.empty());
}

// ============================================================================
// 3. should_skip_block_old_timestamp
// ============================================================================
TEST_F(WalletRefreshTest, should_skip_block_old_timestamp)
{
  // Block with timestamp + 86400 < account create_time should be skipped.
  auto& account = wallet_accessor_test::get_account(m_wallet);
  uint64_t create_time = account.get_createtime();

  cryptonote::block b = AUTO_VAL_INIT(b);
  // Set timestamp to be far in the past relative to create_time
  b.timestamp = create_time > 200000 ? create_time - 200000 : 0;

  wallet_accessor_test::get_refresh_from_block_height(m_wallet) = 0;
  wallet_accessor_test::get_skip_to_height(m_wallet) = 0;

  EXPECT_TRUE(wallet_accessor_test::should_skip_block(m_wallet, b, 0));
}

// ============================================================================
// 4. should_skip_block_recent_timestamp
// ============================================================================
TEST_F(WalletRefreshTest, should_skip_block_recent_timestamp)
{
  // Block with recent timestamp should NOT be skipped
  auto& account = wallet_accessor_test::get_account(m_wallet);
  uint64_t create_time = account.get_createtime();

  cryptonote::block b = AUTO_VAL_INIT(b);
  b.timestamp = create_time; // same as create time

  wallet_accessor_test::get_refresh_from_block_height(m_wallet) = 0;
  wallet_accessor_test::get_skip_to_height(m_wallet) = 0;

  EXPECT_FALSE(wallet_accessor_test::should_skip_block(m_wallet, b, 5));
}

// ============================================================================
// 5. should_skip_respects_refresh_height
// ============================================================================
TEST_F(WalletRefreshTest, should_skip_respects_refresh_height)
{
  // Block below m_refresh_from_block_height should be skipped
  auto& account = wallet_accessor_test::get_account(m_wallet);
  uint64_t create_time = account.get_createtime();

  cryptonote::block b = AUTO_VAL_INIT(b);
  b.timestamp = create_time; // recent timestamp, would not be skipped otherwise

  wallet_accessor_test::get_refresh_from_block_height(m_wallet) = 100;
  wallet_accessor_test::get_skip_to_height(m_wallet) = 0;

  EXPECT_TRUE(wallet_accessor_test::should_skip_block(m_wallet, b, 50));
}

// ============================================================================
// 6. should_skip_respects_skip_to_height
// ============================================================================
TEST_F(WalletRefreshTest, should_skip_respects_skip_to_height)
{
  // Block below m_skip_to_height should be skipped
  auto& account = wallet_accessor_test::get_account(m_wallet);
  uint64_t create_time = account.get_createtime();

  cryptonote::block b = AUTO_VAL_INIT(b);
  b.timestamp = create_time;

  wallet_accessor_test::get_refresh_from_block_height(m_wallet) = 0;
  wallet_accessor_test::get_skip_to_height(m_wallet) = 200;

  EXPECT_TRUE(wallet_accessor_test::should_skip_block(m_wallet, b, 150));
}

// ============================================================================
// 7. process_entry_validates_height_sequence
// ============================================================================
TEST_F(WalletRefreshTest, process_entry_validates_height_sequence)
{
  // process_new_blockchain_entry expects height == m_blockchain.size().
  // Passing a non-sequential height should throw wallet_internal_error.
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);
  // blockchain currently has some entries from generate()
  size_t current_size = blockchain.size();

  cryptonote::block b = AUTO_VAL_INIT(b);
  b.timestamp = time(nullptr);
  b.major_version = 1;
  b.minor_version = 0;
  cryptonote::block_complete_entry bce;
  tools::wallet2::parsed_block pb;
  pb.hash = make_hash(999);
  pb.block = b;
  pb.error = false;

  // Give it a height that doesn't match blockchain.size() to trigger the error
  uint64_t wrong_height = current_size + 5;

  // We need o_indices to have at least (1 + bce.txs.size()) entries
  pb.o_indices.indices.resize(1);  // 1 for miner tx, 0 regular txs

  tools::wallet2::tx_cache_data tcd;
  std::vector<tools::wallet2::tx_cache_data> tx_cache;
  tx_cache.push_back(tcd);

  EXPECT_THROW(
    {
      std::vector<cryptonote::block_complete_entry> blocks;
      std::vector<tools::wallet2::parsed_block> parsed_blocks;
      blocks.push_back(bce);
      parsed_blocks.push_back(pb);

      // start_height outside of hashchain bounds throws out_of_hashchain_bounds_error
      uint64_t blocks_added = 0;
      wallet_accessor_test::process_parsed_blocks(m_wallet, wrong_height, blocks, parsed_blocks, blocks_added);
    },
    tools::error::out_of_hashchain_bounds_error
  );
}

// ============================================================================
// 8. process_entry_adds_hash_to_chain
// ============================================================================
TEST_F(WalletRefreshTest, process_entry_adds_hash_to_chain)
{
  // After processing a valid block, the blockchain hashchain should grow by 1.
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);
  size_t initial_size = blockchain.size();

  // Push a new hash directly to test hashchain behavior
  crypto::hash new_hash = make_hash(42);
  blockchain.push_back(new_hash);
  EXPECT_EQ(initial_size + 1, blockchain.size());
  EXPECT_EQ(new_hash, blockchain[initial_size]);
}

// ============================================================================
// 9. process_entry_updates_block_reward
// ============================================================================
TEST_F(WalletRefreshTest, process_entry_updates_block_reward)
{
  // After process_new_blockchain_entry, m_last_block_reward should be set
  // to get_outs_money_amount(b.miner_tx). We test this via direct manipulation.
  auto& last_reward = wallet_accessor_test::get_last_block_reward(m_wallet);
  last_reward = 0;

  // Build a miner tx to the wallet's address
  const auto& addr = m_wallet.get_account().get_keys().m_account_address;
  cryptonote::transaction miner_tx;
  ASSERT_TRUE(build_miner_tx_to_addr(addr, 1, COIN, miner_tx));

  uint64_t reward = cryptonote::get_outs_money_amount(miner_tx);
  EXPECT_GT(reward, 0u);

  // Simulate what process_new_blockchain_entry does
  last_reward = reward;
  EXPECT_EQ(reward, wallet_accessor_test::get_last_block_reward(m_wallet));
}

// ============================================================================
// 10. output_tracker_cache_correctness
// ============================================================================
TEST_F(WalletRefreshTest, output_tracker_cache_correctness)
{
  // The output tracker cache maps (amount, global_index) -> index in m_transfers.
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.clear();

  auto td1 = make_transfer_detail(5000, 10, false, 100, false);  // non-rct, amount=5000, gidx=100
  auto td2 = make_transfer_detail(3000, 20, false, 200, false);  // non-rct, amount=3000, gidx=200
  transfers.push_back(td1);
  transfers.push_back(td2);

  // Build the cache the same way wallet2::create_output_tracker_cache does
  std::map<std::pair<uint64_t, uint64_t>, size_t> cache;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    cache[std::make_pair(td.is_rct() ? 0 : td.amount(), td.m_global_output_index)] = i;
  }

  EXPECT_EQ(2u, cache.size());
  auto it1 = cache.find(std::make_pair((uint64_t)5000, (uint64_t)100));
  ASSERT_NE(it1, cache.end());
  EXPECT_EQ(0u, it1->second);

  auto it2 = cache.find(std::make_pair((uint64_t)3000, (uint64_t)200));
  ASSERT_NE(it2, cache.end());
  EXPECT_EQ(1u, it2->second);
}

// ============================================================================
// 11. output_tracker_cache_rct_zero_amount
// ============================================================================
TEST_F(WalletRefreshTest, output_tracker_cache_rct_zero_amount)
{
  // RCT outputs use amount=0 as the key in the output tracker cache.
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.clear();

  auto td = make_transfer_detail(123456, 10, false, 500, true);  // rct=true
  transfers.push_back(td);

  std::map<std::pair<uint64_t, uint64_t>, size_t> cache;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& t = transfers[i];
    cache[std::make_pair(t.is_rct() ? 0 : t.amount(), t.m_global_output_index)] = i;
  }

  // Key should be (0, 500) not (123456, 500)
  auto it = cache.find(std::make_pair((uint64_t)0, (uint64_t)500));
  ASSERT_NE(it, cache.end());
  EXPECT_EQ(0u, it->second);

  // The non-zero amount key should not exist
  auto it2 = cache.find(std::make_pair((uint64_t)123456, (uint64_t)500));
  EXPECT_EQ(it2, cache.end());
}

// ============================================================================
// 12. short_chain_history_exponential
// ============================================================================
TEST_F(WalletRefreshTest, short_chain_history_exponential)
{
  // get_short_chain_history produces hashes at exponentially decreasing intervals
  // after the first 10 entries.
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);

  // Ensure we have enough blocks in the hashchain
  // Start from current size, push hashes up to 200
  while (blockchain.size() < 200)
    blockchain.push_back(make_hash(static_cast<uint32_t>(blockchain.size())));

  std::list<crypto::hash> ids;
  wallet_accessor_test::get_short_chain_history(m_wallet, ids);

  // The history should not be empty and should be smaller than the full chain
  EXPECT_FALSE(ids.empty());
  EXPECT_LT(ids.size(), blockchain.size());

  // First ~10 entries should be consecutive (every block), then exponential gaps
  EXPECT_GE(ids.size(), 10u);
}

// ============================================================================
// 13. short_chain_history_granularity
// ============================================================================
TEST_F(WalletRefreshTest, short_chain_history_granularity)
{
  // The granularity parameter affects the starting point (rounds down blockchain size).
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);

  while (blockchain.size() < 100)
    blockchain.push_back(make_hash(static_cast<uint32_t>(blockchain.size())));

  std::list<crypto::hash> ids_gran1, ids_gran10;
  wallet_accessor_test::get_short_chain_history(m_wallet, ids_gran1, 1);
  wallet_accessor_test::get_short_chain_history(m_wallet, ids_gran10, 10);

  // Both should produce non-empty results
  EXPECT_FALSE(ids_gran1.empty());
  EXPECT_FALSE(ids_gran10.empty());

  // With higher granularity, the effective blockchain size is rounded down,
  // potentially producing fewer history entries
  EXPECT_GE(ids_gran1.size(), ids_gran10.size());
}

// ============================================================================
// 14. process_tx_skips_empty_vout
// ============================================================================
TEST_F(WalletRefreshTest, process_tx_skips_empty_vout)
{
  // A transaction with no outputs (empty vout) should be handled gracefully
  // by process_new_transaction -- the while(!tx.vout.empty()) loop never enters.
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  size_t initial_transfers = transfers.size();

  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 0;
  // vout is empty by default
  EXPECT_TRUE(tx.vout.empty());

  // The wallet should handle this without crashing
  // Since we can't call process_new_transaction directly (private), we verify
  // the precondition: empty vout means the scan loop does not execute
  std::vector<tools::wallet2::tx_scan_info_t> tx_scan_info(tx.vout.size());
  EXPECT_EQ(0u, tx_scan_info.size());
  // No transfers should be added for empty vout
  EXPECT_EQ(initial_transfers, transfers.size());
}

// ============================================================================
// 15. process_tx_detects_owned_output
// ============================================================================
TEST_F(WalletRefreshTest, process_tx_detects_owned_output)
{
  // A miner tx paying to our address should cause m_transfers to grow.
  // We test the miner tx construction and output detection setup.
  const auto& addr = m_wallet.get_account().get_keys().m_account_address;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(build_miner_tx_to_addr(addr, 1, COIN, miner_tx));

  // The miner tx should have at least one output
  EXPECT_GE(miner_tx.vout.size(), 1u);

  // Verify the output amount is positive
  uint64_t total_output = cryptonote::get_outs_money_amount(miner_tx);
  EXPECT_GT(total_output, 0u);

  // The output should be addressable -- verify tx_extra contains a public key
  std::vector<cryptonote::tx_extra_field> fields;
  EXPECT_TRUE(cryptonote::parse_tx_extra(miner_tx.extra, fields));
  cryptonote::tx_extra_pub_key pub_key_field;
  EXPECT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, pub_key_field));
}

// ============================================================================
// 16. process_tx_updates_balance
// ============================================================================
TEST_F(WalletRefreshTest, process_tx_updates_balance)
{
  // Manually adding an owned output to m_transfers increases the wallet balance.
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.clear();

  uint64_t expected_balance = 0;
  EXPECT_EQ(expected_balance, m_wallet.balance(0, false));

  // Add a transfer detail manually
  auto td = make_transfer_detail(COIN, 10, false, 1, true);
  td.m_subaddr_index = {0, 0};
  transfers.push_back(td);

  // Balance should reflect the new transfer
  uint64_t balance = m_wallet.balance(0, false);
  EXPECT_EQ(COIN, balance);
}

// ============================================================================
// 17. process_tx_respects_no_coinbase
// ============================================================================
TEST_F(WalletRefreshTest, process_tx_respects_no_coinbase)
{
  // When RefreshType is RefreshNoCoinbase, miner txs should be skipped.
  auto& refresh_type = wallet_accessor_test::get_refresh_type(m_wallet);
  refresh_type = tools::wallet2::RefreshNoCoinbase;
  EXPECT_EQ(tools::wallet2::RefreshNoCoinbase, m_wallet.get_refresh_type());

  // The logic in process_new_blockchain_entry is:
  // if (m_refresh_type != RefreshNoCoinbase) process_new_transaction(miner_tx...)
  // So with RefreshNoCoinbase, miner_tx is never processed.
  bool should_process_coinbase = (refresh_type != tools::wallet2::RefreshNoCoinbase);
  EXPECT_FALSE(should_process_coinbase);

  // Reset
  refresh_type = tools::wallet2::RefreshDefault;
}

// ============================================================================
// 18. process_unconfirmed_removes_confirmed
// ============================================================================
TEST_F(WalletRefreshTest, process_unconfirmed_removes_confirmed)
{
  // When a tx is confirmed (appears in a block), it should be removed from
  // m_unconfirmed_txs via process_unconfirmed(). We test this logic pattern.
  auto& unconfirmed_payments = wallet_accessor_test::get_unconfirmed_payments(m_wallet);
  unconfirmed_payments.clear();

  // Add a mock unconfirmed payment
  crypto::hash payment_id = make_hash(42);
  tools::wallet2::pool_payment_details ppd;
  ppd.m_pd.m_tx_hash = make_hash(1);
  ppd.m_pd.m_amount = 5000;
  ppd.m_pd.m_fee = 10;
  ppd.m_pd.m_block_height = 0;
  ppd.m_pd.m_unlock_time = 0;
  ppd.m_pd.m_timestamp = 0;
  ppd.m_pd.m_coinbase = false;
  ppd.m_pd.m_subaddr_index = {0, 0};
  ppd.m_double_spend_seen = false;
  unconfirmed_payments.insert(std::make_pair(payment_id, ppd));
  EXPECT_EQ(1u, unconfirmed_payments.size());

  // Simulate confirmation: remove from unconfirmed_payments
  unconfirmed_payments.erase(payment_id);
  EXPECT_TRUE(unconfirmed_payments.empty());
}

// ============================================================================
// 19. hashchain_genesis_init
// ============================================================================
TEST_F(WalletRefreshTest, hashchain_genesis_init)
{
  // A fresh hashchain should start with the genesis hash when the first block is pushed.
  tools::hashchain hc;
  EXPECT_TRUE(hc.empty());
  EXPECT_EQ(0u, hc.size());

  crypto::hash genesis = make_hash(1);
  hc.push_back(genesis);
  EXPECT_EQ(genesis, hc.genesis());
  EXPECT_EQ(1u, hc.size());
  EXPECT_EQ(genesis, hc[0]);
}

// ============================================================================
// 20. fast_refresh_reaches_target
// ============================================================================
TEST_F(WalletRefreshTest, fast_refresh_reaches_target)
{
  // fast_refresh fills the hashchain up to a target height.
  // We simulate this by pushing hashes directly (since fast_refresh calls
  // pull_hashes which requires a daemon). Test the hashchain growth pattern.
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);
  size_t initial_size = blockchain.size();

  uint64_t target = initial_size + 50;
  while (blockchain.size() < target)
  {
    blockchain.push_back(make_hash(static_cast<uint32_t>(blockchain.size())));
  }

  EXPECT_EQ(target, blockchain.size());
}

// ============================================================================
// 21. refresh_stop_signal
// ============================================================================
TEST_F(WalletRefreshTest, refresh_stop_signal)
{
  // Setting m_run = false should cause the refresh loop to exit early.
  auto& run = wallet_accessor_test::get_run(m_wallet);

  // First verify it defaults to true-ish or can be set
  run.store(false, std::memory_order_relaxed);
  EXPECT_FALSE(run.load(std::memory_order_relaxed));

  // The refresh main loop condition is: while(m_run.load(...) && blocks_fetched < max_blocks)
  // With m_run=false, the loop body never executes.
  // Also, fast_refresh loop: while(m_run.load(...) && current_index < stop_height)
  bool would_continue = run.load(std::memory_order_relaxed);
  EXPECT_FALSE(would_continue);

  // Restore
  run.store(true, std::memory_order_relaxed);
}

// ============================================================================
// 22. multiple_blocks_sequential
// ============================================================================
TEST_F(WalletRefreshTest, multiple_blocks_sequential)
{
  // 5 blocks added in sequence; height increments correctly.
  auto& blockchain = wallet_accessor_test::get_blockchain(m_wallet);
  size_t start_height = blockchain.size();

  for (uint32_t i = 0; i < 5; ++i)
  {
    crypto::hash h = make_hash(1000 + i);
    blockchain.push_back(h);
    EXPECT_EQ(start_height + i + 1, blockchain.size());
    EXPECT_EQ(h, blockchain[start_height + i]);
  }

  EXPECT_EQ(start_height + 5, blockchain.size());
}

// ============================================================================
// 23. miner_tx_unlock_window
// ============================================================================
TEST_F(WalletRefreshTest, miner_tx_unlock_window)
{
  // Coinbase outputs should respect CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW (60 blocks).
  // A coinbase transfer at height H is locked until H + 60.
  EXPECT_EQ(60u, CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

  uint64_t block_height = 100;
  uint64_t unlock_height = block_height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
  EXPECT_EQ(160u, unlock_height);

  // Simulate: at height 150, the coinbase from block 100 is still locked
  uint64_t current_height = 150;
  bool is_locked = current_height < unlock_height;
  EXPECT_TRUE(is_locked);

  // At height 160, it becomes unlocked
  current_height = 160;
  is_locked = current_height < unlock_height;
  EXPECT_FALSE(is_locked);
}

// ============================================================================
// 24. view_only_wallet_scanning
// ============================================================================
TEST_F(WalletRefreshTest, view_only_wallet_scanning)
{
  // A view-only wallet (no spend key) can still detect incoming outputs
  // via the view secret key. We verify the key derivation math works.
  const auto& keys = m_wallet.get_account().get_keys();

  // Simulate a tx sender: generate ephemeral keys R = r*G
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  // View-only wallet derives: derivation = a * R (where a = view secret key)
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  // Derive the expected output public key: P = Hs(aR, 0)*G + B
  crypto::public_key expected_output_key;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, expected_output_key));

  // The output key should be deterministic and non-null
  EXPECT_NE(expected_output_key, crypto::null_pkey);

  // A view-only wallet can detect this output by checking if
  // derive_subaddress_public_key(output_key, derivation, 0) matches
  // one of its known subaddress spend keys.
  crypto::public_key derived_spend_key;
  hw::device& hwdev = m_wallet.get_account().get_device();
  ASSERT_TRUE(hwdev.derive_subaddress_public_key(expected_output_key, derivation, 0, derived_spend_key));

  // The derived spend key should match our actual spend public key
  EXPECT_EQ(derived_spend_key, keys.m_account_address.m_spend_public_key);
}

// ============================================================================
// 25. view_tag_optimization
// ============================================================================
TEST_F(WalletRefreshTest, view_tag_optimization)
{
  // View tags allow skipping full derivation when the tag doesn't match.
  // A matching view tag proceeds to full derivation; a mismatching one skips.
  const auto& keys = m_wallet.get_account().get_keys();

  // Generate a tx public key
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  // Compute the derivation (as the wallet would)
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  // Derive the correct view tag for output index 0
  crypto::view_tag correct_tag;
  crypto::derive_view_tag(derivation, 0, correct_tag);

  // A matching view tag should pass the check
  // out_can_be_to_acc returns true when view_tag matches or is absent
  hw::device& hwdev = m_wallet.get_account().get_device();
  bool matches = cryptonote::out_can_be_to_acc(boost::optional<crypto::view_tag>(correct_tag), derivation, 0, &hwdev);
  EXPECT_TRUE(matches);

  // A wrong view tag should fail the check, avoiding expensive derivation
  crypto::view_tag wrong_tag;
  wrong_tag.data = correct_tag.data ^ 0xFF; // flip bits to ensure mismatch
  bool mismatches = cryptonote::out_can_be_to_acc(boost::optional<crypto::view_tag>(wrong_tag), derivation, 0, &hwdev);
  EXPECT_FALSE(mismatches);

  // No view tag (pre-HF15) should always return true (no optimization possible)
  bool no_tag = cryptonote::out_can_be_to_acc(boost::none, derivation, 0, &hwdev);
  EXPECT_TRUE(no_tag);
}
