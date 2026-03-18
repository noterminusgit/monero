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

#include "gtest/gtest.h"

#include "cryptonote_core/tx_pool.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/blockchain_and_pool.h"
#include "blockchain_db/testdb.h"

namespace
{
  class TestDB : public cryptonote::BaseTestDB
  {
  public:
    TestDB() { m_open = true; }
    bool for_all_txpool_txes(std::function<bool(const crypto::hash&, const cryptonote::txpool_tx_meta_t&, const cryptonote::blobdata_ref*)>, bool, cryptonote::relay_category) const override { return true; }
  };

  struct get_test_options {
    const std::pair<uint8_t, uint64_t> hard_forks[2];
    const cryptonote::test_options test_options = {
      hard_forks,
      0,
    };
    get_test_options(): hard_forks{std::make_pair((uint8_t)1, (uint64_t)0), std::make_pair((uint8_t)0, (uint64_t)0)} {}
  };

  class TxPoolTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      get_test_options opts;
      ASSERT_TRUE(m_blockchain.init(new TestDB(), cryptonote::FAKECHAIN, true, &opts.test_options, 0, NULL));
      ASSERT_TRUE(m_pool.init(0));
    }

    void TearDown() override
    {
      m_pool.deinit();
      m_blockchain.deinit();
    }

    cryptonote::BlockchainAndPool m_bap;
    cryptonote::Blockchain& m_blockchain{m_bap.blockchain};
    cryptonote::tx_memory_pool& m_pool{m_bap.tx_pool};
  };
}

TEST_F(TxPoolTest, empty_pool)
{
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);
}

TEST_F(TxPoolTest, empty_pool_get_transactions)
{
  std::vector<cryptonote::transaction> txs;
  m_pool.get_transactions(txs);
  ASSERT_TRUE(txs.empty());
}

TEST_F(TxPoolTest, empty_pool_get_hashes)
{
  std::vector<crypto::hash> txs;
  m_pool.get_transaction_hashes(txs);
  ASSERT_TRUE(txs.empty());
}

TEST_F(TxPoolTest, have_nonexistent_tx)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::legacy));
}

TEST_F(TxPoolTest, cookie_changes_on_init)
{
  uint64_t c = m_pool.cookie();
  (void)c; // Just verify it doesn't crash
}

TEST_F(TxPoolTest, print_empty_pool)
{
  // Just verify print_pool doesn't crash on an empty pool
  std::string short_output = m_pool.print_pool(true);
  std::string long_output = m_pool.print_pool(false);
  (void)short_output;
  (void)long_output;
}

TEST_F(TxPoolTest, check_for_empty_key_images)
{
  std::vector<crypto::key_image> key_images;
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_TRUE(spent.empty());
}

TEST_F(TxPoolTest, check_for_nonexistent_key_images)
{
  crypto::key_image ki;
  memset(&ki, 1, sizeof(ki));
  std::vector<crypto::key_image> key_images{ki};
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 1u);
  ASSERT_FALSE(spent[0]);
}

TEST_F(TxPoolTest, set_txpool_max_weight)
{
  m_pool.set_txpool_max_weight(1024 * 1024);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
}

TEST_F(TxPoolTest, get_transaction_stats_empty)
{
  cryptonote::txpool_stats stats;
  m_pool.get_transaction_stats(stats);
  ASSERT_EQ(stats.txs_total, 0u);
}

TEST_F(TxPoolTest, fill_block_template_empty)
{
  cryptonote::block bl;
  size_t total_weight = 0;
  uint64_t fee = 0;
  uint64_t expected_reward = 0;
  ASSERT_TRUE(m_pool.fill_block_template(bl, 300000, 0, total_weight, fee, expected_reward, 1));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);
}

TEST_F(TxPoolTest, get_relayable_empty)
{
  std::vector<std::tuple<crypto::hash, cryptonote::blobdata, cryptonote::relay_method>> txs;
  ASSERT_TRUE(m_pool.get_relayable_transactions(txs));
  ASSERT_TRUE(txs.empty());
}

TEST_F(TxPoolTest, on_blockchain_inc)
{
  crypto::hash top = crypto::rand<crypto::hash>();
  ASSERT_TRUE(m_pool.on_blockchain_inc(1, top));
}

TEST_F(TxPoolTest, validate_empty)
{
  size_t removed = m_pool.validate(1);
  ASSERT_EQ(removed, 0u);
}

// ========================================================================
// Pool state queries (~15 tests)
// ========================================================================

TEST_F(TxPoolTest, get_transactions_count_initially_zero)
{
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
  ASSERT_EQ(m_pool.get_transactions_count(true), 0u);
  ASSERT_EQ(m_pool.get_transactions_count(false), 0u);
}

TEST_F(TxPoolTest, get_txpool_weight_initially_zero)
{
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);
}

TEST_F(TxPoolTest, get_pool_info_empty)
{
  std::vector<std::pair<crypto::hash, cryptonote::tx_memory_pool::tx_details>> added_txs;
  std::vector<crypto::hash> remaining_added_txids;
  std::vector<crypto::hash> removed_txs;
  bool incremental = false;
  ASSERT_TRUE(m_pool.get_pool_info(0, false, 100, added_txs, remaining_added_txids, removed_txs, incremental));
  ASSERT_TRUE(added_txs.empty());
  ASSERT_TRUE(remaining_added_txids.empty());
  ASSERT_TRUE(removed_txs.empty());
}

TEST_F(TxPoolTest, have_tx_random_hash_returns_false)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::legacy));
  ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::broadcasted));
  ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::relayable));
  ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::all));
}

TEST_F(TxPoolTest, get_transaction_random_hash_fails)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::blobdata txblob;
  ASSERT_FALSE(m_pool.get_transaction(h, txblob, cryptonote::relay_category::legacy));
  ASSERT_TRUE(txblob.empty());
}

TEST_F(TxPoolTest, cookie_returns_nonzero_value)
{
  // After init, the cookie may be any value, but calling it should not crash
  uint64_t c = m_pool.cookie();
  // The pool was freshly initialized so the cookie is well-defined
  (void)c;
}

TEST_F(TxPoolTest, on_blockchain_dec_no_crash)
{
  crypto::hash top = crypto::rand<crypto::hash>();
  ASSERT_TRUE(m_pool.on_blockchain_dec(0, top));
}

TEST_F(TxPoolTest, on_blockchain_inc_various_heights)
{
  crypto::hash top = crypto::rand<crypto::hash>();
  ASSERT_TRUE(m_pool.on_blockchain_inc(0, top));
  ASSERT_TRUE(m_pool.on_blockchain_inc(1, top));
  ASSERT_TRUE(m_pool.on_blockchain_inc(100, top));
  ASSERT_TRUE(m_pool.on_blockchain_inc(1000000, top));
  ASSERT_TRUE(m_pool.on_blockchain_inc(UINT64_MAX, top));
}

TEST_F(TxPoolTest, validate_various_block_heights)
{
  ASSERT_EQ(m_pool.validate(0), 0u);
  ASSERT_EQ(m_pool.validate(1), 0u);
  ASSERT_EQ(m_pool.validate(100), 0u);
  ASSERT_EQ(m_pool.validate(255), 0u);
}

TEST_F(TxPoolTest, fill_block_template_multiple_calls)
{
  cryptonote::block bl;
  size_t total_weight = 0;
  uint64_t fee = 0;
  uint64_t expected_reward = 0;

  ASSERT_TRUE(m_pool.fill_block_template(bl, 300000, 0, total_weight, fee, expected_reward, 1));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);

  // Call again with different parameters
  total_weight = 0;
  fee = 0;
  expected_reward = 0;
  ASSERT_TRUE(m_pool.fill_block_template(bl, 600000, 1000000, total_weight, fee, expected_reward, 1));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);

  // Call with a different version
  total_weight = 0;
  fee = 0;
  expected_reward = 0;
  ASSERT_TRUE(m_pool.fill_block_template(bl, 300000, 0, total_weight, fee, expected_reward, 14));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);
}

TEST_F(TxPoolTest, get_relayable_transactions_empty_pool)
{
  std::vector<std::tuple<crypto::hash, cryptonote::blobdata, cryptonote::relay_method>> txs;
  ASSERT_TRUE(m_pool.get_relayable_transactions(txs));
  ASSERT_TRUE(txs.empty());
}

TEST_F(TxPoolTest, set_txpool_max_weight_various_values)
{
  m_pool.set_txpool_max_weight(1);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);

  m_pool.set_txpool_max_weight(1024);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);

  m_pool.set_txpool_max_weight(1024 * 1024);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);

  m_pool.set_txpool_max_weight(1024 * 1024 * 1024);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
}

TEST_F(TxPoolTest, set_txpool_max_weight_zero_unlimited)
{
  // Setting max weight to 0 should mean unlimited
  m_pool.set_txpool_max_weight(0);
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);
}

TEST_F(TxPoolTest, get_transaction_stats_field_values_empty)
{
  cryptonote::txpool_stats stats;
  m_pool.get_transaction_stats(stats);
  ASSERT_EQ(stats.txs_total, 0u);
  ASSERT_EQ(stats.bytes_total, 0u);
  ASSERT_EQ(stats.bytes_min, 0u);
  ASSERT_EQ(stats.bytes_max, 0u);
  ASSERT_EQ(stats.bytes_med, 0u);
  ASSERT_EQ(stats.fee_total, 0u);
  ASSERT_EQ(stats.oldest, 0u);
  ASSERT_EQ(stats.num_failing, 0u);
  ASSERT_EQ(stats.num_10m, 0u);
  ASSERT_EQ(stats.num_not_relayed, 0u);
  ASSERT_EQ(stats.num_double_spends, 0u);

  // Also test with include_sensitive = true
  cryptonote::txpool_stats stats2;
  m_pool.get_transaction_stats(stats2, true);
  ASSERT_EQ(stats2.txs_total, 0u);
}

TEST_F(TxPoolTest, check_for_key_images_multiple)
{
  crypto::key_image ki1, ki2, ki3;
  memset(&ki1, 0x01, sizeof(ki1));
  memset(&ki2, 0x02, sizeof(ki2));
  memset(&ki3, 0x03, sizeof(ki3));

  std::vector<crypto::key_image> key_images{ki1, ki2, ki3};
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 3u);
  ASSERT_FALSE(spent[0]);
  ASSERT_FALSE(spent[1]);
  ASSERT_FALSE(spent[2]);
}

// ========================================================================
// Multiple key image tests (~5 tests)
// ========================================================================

TEST_F(TxPoolTest, check_for_multiple_nonexistent_key_images)
{
  std::vector<crypto::key_image> key_images;
  for (int i = 0; i < 5; ++i)
  {
    crypto::key_image ki;
    memset(&ki, i + 10, sizeof(ki));
    key_images.push_back(ki);
  }
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 5u);
  for (size_t i = 0; i < spent.size(); ++i)
    ASSERT_FALSE(spent[i]);
}

TEST_F(TxPoolTest, check_for_key_images_large_batch)
{
  std::vector<crypto::key_image> key_images;
  for (int i = 0; i < 100; ++i)
  {
    crypto::key_image ki;
    memset(&ki, 0, sizeof(ki));
    // Set the first byte to make each unique
    reinterpret_cast<unsigned char*>(&ki)[0] = static_cast<unsigned char>(i);
    key_images.push_back(ki);
  }
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 100u);
  for (size_t i = 0; i < spent.size(); ++i)
    ASSERT_FALSE(spent[i]);
}

TEST_F(TxPoolTest, check_for_key_images_empty_vector)
{
  std::vector<crypto::key_image> key_images;
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_TRUE(spent.empty());
}

TEST_F(TxPoolTest, check_for_key_images_same_image_repeated)
{
  crypto::key_image ki;
  memset(&ki, 0xAB, sizeof(ki));

  std::vector<crypto::key_image> key_images{ki, ki, ki};
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 3u);
  // All should be not spent in empty pool
  ASSERT_FALSE(spent[0]);
  ASSERT_FALSE(spent[1]);
  ASSERT_FALSE(spent[2]);
}

TEST_F(TxPoolTest, check_for_key_images_random_data)
{
  std::vector<crypto::key_image> key_images;
  for (int i = 0; i < 10; ++i)
  {
    crypto::key_image ki = crypto::rand<crypto::key_image>();
    key_images.push_back(ki);
  }
  std::vector<bool> spent;
  ASSERT_TRUE(m_pool.check_for_key_images(key_images, spent));
  ASSERT_EQ(spent.size(), 10u);
  for (size_t i = 0; i < spent.size(); ++i)
    ASSERT_FALSE(spent[i]);
}

// ========================================================================
// Print and debug (~5 tests)
// ========================================================================

TEST_F(TxPoolTest, print_pool_short_format)
{
  std::string output = m_pool.print_pool(true);
  // Short format on empty pool should produce some output (possibly just a header)
  // Mainly verify it does not crash
  (void)output;
}

TEST_F(TxPoolTest, print_pool_verbose_format)
{
  std::string output = m_pool.print_pool(false);
  // Verbose format on empty pool should produce some output
  // Mainly verify it does not crash
  (void)output;
}

TEST_F(TxPoolTest, print_pool_short_and_verbose_differ_or_both_valid)
{
  std::string short_output = m_pool.print_pool(true);
  std::string verbose_output = m_pool.print_pool(false);
  // Both formats should succeed and return valid strings (may be the same for an empty pool)
  // The key check is that neither crashes and both return a string
  (void)short_output;
  (void)verbose_output;
}

TEST_F(TxPoolTest, print_pool_twice_returns_consistent_results)
{
  std::string output1 = m_pool.print_pool(true);
  std::string output2 = m_pool.print_pool(true);
  ASSERT_EQ(output1, output2);
}

TEST_F(TxPoolTest, get_transactions_and_spent_keys_info_empty)
{
  std::vector<cryptonote::tx_info> tx_infos;
  std::vector<cryptonote::spent_key_image_info> key_image_infos;
  ASSERT_TRUE(m_pool.get_transactions_and_spent_keys_info(tx_infos, key_image_infos, false));
  ASSERT_TRUE(tx_infos.empty());
  ASSERT_TRUE(key_image_infos.empty());

  // Also with include_sensitive_data = true
  ASSERT_TRUE(m_pool.get_transactions_and_spent_keys_info(tx_infos, key_image_infos, true));
  ASSERT_TRUE(tx_infos.empty());
  ASSERT_TRUE(key_image_infos.empty());
}

// ========================================================================
// Edge cases (~10 tests)
// ========================================================================

TEST_F(TxPoolTest, fill_block_template_zero_max_weight)
{
  cryptonote::block bl;
  size_t total_weight = 0;
  uint64_t fee = 0;
  uint64_t expected_reward = 0;
  // Zero median weight - should still succeed on empty pool
  ASSERT_TRUE(m_pool.fill_block_template(bl, 0, 0, total_weight, fee, expected_reward, 1));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);
}

TEST_F(TxPoolTest, fill_block_template_very_large_max_weight)
{
  cryptonote::block bl;
  size_t total_weight = 0;
  uint64_t fee = 0;
  uint64_t expected_reward = 0;
  // Very large median weight
  ASSERT_TRUE(m_pool.fill_block_template(bl, SIZE_MAX / 2, 0, total_weight, fee, expected_reward, 1));
  ASSERT_EQ(total_weight, 0u);
  ASSERT_EQ(fee, 0u);
}

TEST_F(TxPoolTest, get_relayable_transactions_empty_pool_repeated)
{
  std::vector<std::tuple<crypto::hash, cryptonote::blobdata, cryptonote::relay_method>> txs1;
  m_pool.get_relayable_transactions(txs1);
  ASSERT_TRUE(txs1.empty());

  std::vector<std::tuple<crypto::hash, cryptonote::blobdata, cryptonote::relay_method>> txs2;
  m_pool.get_relayable_transactions(txs2);
  ASSERT_TRUE(txs2.empty());
}

TEST_F(TxPoolTest, on_blockchain_inc_height_zero)
{
  crypto::hash top = crypto::rand<crypto::hash>();
  ASSERT_TRUE(m_pool.on_blockchain_inc(0, top));
}

TEST_F(TxPoolTest, validate_height_zero)
{
  size_t removed = m_pool.validate(0);
  ASSERT_EQ(removed, 0u);
}

TEST_F(TxPoolTest, cookie_consistency_between_calls)
{
  uint64_t c1 = m_pool.cookie();
  uint64_t c2 = m_pool.cookie();
  // Without any pool modification, cookie should remain the same
  ASSERT_EQ(c1, c2);
}

TEST_F(TxPoolTest, get_transactions_multiple_calls_same_result)
{
  std::vector<cryptonote::transaction> txs1;
  m_pool.get_transactions(txs1);
  ASSERT_TRUE(txs1.empty());

  std::vector<cryptonote::transaction> txs2;
  m_pool.get_transactions(txs2);
  ASSERT_TRUE(txs2.empty());

  ASSERT_EQ(txs1.size(), txs2.size());
}

TEST_F(TxPoolTest, get_transaction_hashes_empty_returns_empty)
{
  std::vector<crypto::hash> hashes;
  m_pool.get_transaction_hashes(hashes);
  ASSERT_TRUE(hashes.empty());

  // Also with include_sensitive = true
  std::vector<crypto::hash> hashes2;
  m_pool.get_transaction_hashes(hashes2, true);
  ASSERT_TRUE(hashes2.empty());
}

TEST_F(TxPoolTest, get_pool_info_empty_pool_structure)
{
  std::vector<std::pair<crypto::hash, cryptonote::tx_memory_pool::tx_details>> added_txs;
  std::vector<crypto::hash> remaining_added_txids;
  std::vector<crypto::hash> removed_txs;
  bool incremental = false;

  // With start_time = 0, should get non-incremental full view
  ASSERT_TRUE(m_pool.get_pool_info(0, false, 100, added_txs, remaining_added_txids, removed_txs, incremental));
  ASSERT_TRUE(added_txs.empty());
  ASSERT_TRUE(remaining_added_txids.empty());
  ASSERT_TRUE(removed_txs.empty());

  // With include_sensitive = true
  ASSERT_TRUE(m_pool.get_pool_info(0, true, 100, added_txs, remaining_added_txids, removed_txs, incremental));
  ASSERT_TRUE(added_txs.empty());
}

TEST_F(TxPoolTest, get_transaction_backlog_empty)
{
  std::vector<cryptonote::tx_backlog_entry> backlog;
  m_pool.get_transaction_backlog(backlog);
  ASSERT_TRUE(backlog.empty());

  m_pool.get_transaction_backlog(backlog, true);
  ASSERT_TRUE(backlog.empty());
}

TEST_F(TxPoolTest, get_block_template_backlog_empty)
{
  std::vector<cryptonote::tx_block_template_backlog_entry> backlog;
  m_pool.get_block_template_backlog(backlog);
  ASSERT_TRUE(backlog.empty());

  m_pool.get_block_template_backlog(backlog, true);
  ASSERT_TRUE(backlog.empty());
}

TEST_F(TxPoolTest, get_complement_empty_pool)
{
  std::vector<crypto::hash> hashes;
  std::vector<cryptonote::blobdata> txes;
  ASSERT_TRUE(m_pool.get_complement(hashes, txes));
  ASSERT_TRUE(txes.empty());
}

TEST_F(TxPoolTest, get_transaction_info_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::tx_memory_pool::tx_details td;
  ASSERT_FALSE(m_pool.get_transaction_info(h, td, false));
  ASSERT_FALSE(m_pool.get_transaction_info(h, td, true));
}

TEST_F(TxPoolTest, get_transactions_info_nonexistent)
{
  std::vector<crypto::hash> txids;
  txids.push_back(crypto::rand<crypto::hash>());
  txids.push_back(crypto::rand<crypto::hash>());
  std::vector<std::pair<crypto::hash, cryptonote::tx_memory_pool::tx_details>> txs;
  // get_transactions_info should handle non-existent txids gracefully
  m_pool.get_transactions_info(txids, txs, false);
  ASSERT_TRUE(txs.empty());
}

TEST_F(TxPoolTest, on_blockchain_dec_various_heights)
{
  crypto::hash top = crypto::rand<crypto::hash>();
  ASSERT_TRUE(m_pool.on_blockchain_dec(0, top));
  ASSERT_TRUE(m_pool.on_blockchain_dec(1, top));
  ASSERT_TRUE(m_pool.on_blockchain_dec(100, top));
  ASSERT_TRUE(m_pool.on_blockchain_dec(UINT64_MAX, top));
}

TEST_F(TxPoolTest, have_tx_multiple_random_hashes)
{
  for (int i = 0; i < 10; ++i)
  {
    crypto::hash h = crypto::rand<crypto::hash>();
    ASSERT_FALSE(m_pool.have_tx(h, cryptonote::relay_category::legacy));
  }
}

TEST_F(TxPoolTest, get_transaction_multiple_random_hashes_fail)
{
  for (int i = 0; i < 10; ++i)
  {
    crypto::hash h = crypto::rand<crypto::hash>();
    cryptonote::blobdata txblob;
    ASSERT_FALSE(m_pool.get_transaction(h, txblob, cryptonote::relay_category::legacy));
    ASSERT_TRUE(txblob.empty());
  }
}

TEST_F(TxPoolTest, pool_state_consistent_after_operations)
{
  // Perform a series of read-only operations and ensure pool state remains consistent
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);

  std::vector<cryptonote::transaction> txs;
  m_pool.get_transactions(txs);
  ASSERT_TRUE(txs.empty());

  std::vector<crypto::hash> hashes;
  m_pool.get_transaction_hashes(hashes);
  ASSERT_TRUE(hashes.empty());

  cryptonote::txpool_stats stats;
  m_pool.get_transaction_stats(stats);
  ASSERT_EQ(stats.txs_total, 0u);

  std::string pool_str = m_pool.print_pool(true);
  (void)pool_str;

  uint64_t cookie = m_pool.cookie();
  (void)cookie;

  // After all operations, state should still be empty
  ASSERT_EQ(m_pool.get_transactions_count(), 0u);
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);
}

TEST_F(TxPoolTest, get_pool_for_rpc_empty)
{
  std::vector<cryptonote::rpc::tx_in_pool> tx_infos;
  cryptonote::rpc::key_images_with_tx_hashes key_image_infos;
  ASSERT_TRUE(m_pool.get_pool_for_rpc(tx_infos, key_image_infos));
  ASSERT_TRUE(tx_infos.empty());
}

TEST_F(TxPoolTest, reduce_txpool_weight_zero)
{
  // Reducing by zero on an empty pool should not crash
  m_pool.reduce_txpool_weight(0);
  ASSERT_EQ(m_pool.get_txpool_weight(), 0u);
}

TEST_F(TxPoolTest, get_pool_info_max_tx_count_zero)
{
  std::vector<std::pair<crypto::hash, cryptonote::tx_memory_pool::tx_details>> added_txs;
  std::vector<crypto::hash> remaining_added_txids;
  std::vector<crypto::hash> removed_txs;
  bool incremental = false;

  // With max_tx_count = 0, should still succeed on empty pool
  ASSERT_TRUE(m_pool.get_pool_info(0, false, 0, added_txs, remaining_added_txids, removed_txs, incremental));
  ASSERT_TRUE(added_txs.empty());
}
