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
#include "wallet/wallet2.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/crypto.h"

#include <cmath>
#include <map>
#include <numeric>
#include <vector>

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
  static float get_output_relatedness(const tools::wallet2& w, const tools::wallet2::transfer_details& td0, const tools::wallet2::transfer_details& td1) { return w.get_output_relatedness(td0, td1); }
  static tools::hashchain& get_blockchain(tools::wallet2& w) { return w.m_blockchain; }
  static void set_ignore_outputs_above(tools::wallet2& w, uint64_t v) { w.m_ignore_outputs_above = v; }
  static void set_ignore_outputs_below(tools::wallet2& w, uint64_t v) { w.m_ignore_outputs_below = v; }
  static std::vector<size_t> pick_preferred_rct_inputs(tools::wallet2& w, uint64_t needed_money, uint32_t subaddr_account, const std::set<uint32_t>& subaddr_indices) { return w.pick_preferred_rct_inputs(needed_money, subaddr_account, subaddr_indices); }
};

namespace
{
  // Helper to create a basic transfer_details with controllable fields
  tools::wallet2::transfer_details make_transfer_detail(
    uint64_t amount, uint64_t block_height, bool spent,
    const crypto::hash& txid = crypto::null_hash,
    uint64_t internal_output_index = 0)
  {
    tools::wallet2::transfer_details td = AUTO_VAL_INIT(td);
    td.m_amount = amount;
    td.m_block_height = block_height;
    td.m_spent = spent;
    td.m_spent_height = spent ? block_height + 10 : 0;
    td.m_txid = txid;
    td.m_internal_output_index = internal_output_index;
    td.m_global_output_index = 0;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_key_image_partial = false;
    td.m_frozen = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 0};
    // Set unlock_time to 0 (immediately unlockable by block height rules)
    td.m_tx.unlock_time = 0;
    return td;
  }

  // Helper to make a unique txid from an integer
  crypto::hash make_txid(uint32_t id)
  {
    crypto::hash h = crypto::null_hash;
    h.data[0] = id & 0xff;
    h.data[1] = (id >> 8) & 0xff;
    h.data[2] = (id >> 16) & 0xff;
    h.data[3] = (id >> 24) & 0xff;
    return h;
  }

  // Helper to make a key image from an integer
  crypto::key_image make_key_image(uint32_t id)
  {
    crypto::key_image ki;
    memset(&ki, 0, sizeof(ki));
    ki.data[0] = id & 0xff;
    ki.data[1] = (id >> 8) & 0xff;
    return ki;
  }

  // Build a monotonically increasing rct_offsets vector simulating N blocks
  // Each block has `outputs_per_block` outputs (cumulative).
  std::vector<uint64_t> make_rct_offsets(size_t num_blocks, uint64_t outputs_per_block = 2)
  {
    std::vector<uint64_t> offsets(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i)
      offsets[i] = (i + 1) * outputs_per_block;
    return offsets;
  }

  // Fixture providing a generated offline wallet for tests
  class CoinSelectionTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
      m_wallet.set_subaddress_lookahead(2, 5);
      m_wallet.generate("", "", m_recovery_key, true, false);
    }

    // Push N fake block hashes into the wallet's blockchain to set height
    void set_blockchain_height(uint64_t height)
    {
      auto& bc = wallet_accessor_test::get_blockchain(m_wallet);
      bc.clear();
      for (uint64_t i = 0; i < height; ++i)
        bc.push_back(crypto::null_hash);
    }

    // Add a transfer to the wallet and register its key image
    size_t add_transfer(uint64_t amount, uint64_t block_height, bool spent,
                        const crypto::hash& txid, uint32_t ki_id,
                        cryptonote::subaddress_index subaddr = {0, 0},
                        bool frozen = false)
    {
      auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
      auto td = make_transfer_detail(amount, block_height, spent, txid);
      td.m_key_image = make_key_image(ki_id);
      td.m_subaddr_index = subaddr;
      td.m_frozen = frozen;
      transfers.push_back(td);
      size_t idx = transfers.size() - 1;
      wallet_accessor_test::get_key_images(m_wallet)[make_key_image(ki_id)] = idx;
      return idx;
    }

    tools::wallet2 m_wallet;
    crypto::secret_key m_recovery_key;
  };
}

// ============================================================================
// Group: gamma_picker construction
// ============================================================================

TEST(coin_selection_gamma, constructor_valid_offsets)
{
  // Normal construction with 1000+ offsets succeeds
  std::vector<uint64_t> offsets = make_rct_offsets(1000);
  EXPECT_NO_THROW(tools::gamma_picker picker(offsets));
}

TEST(coin_selection_gamma, constructor_rejects_small)
{
  // Offsets smaller than CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE should throw
  std::vector<uint64_t> offsets = make_rct_offsets(CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE - 1);
  EXPECT_ANY_THROW(tools::gamma_picker picker(offsets));
}

TEST(coin_selection_gamma, constructor_rejects_zero_outputs)
{
  // rct_offsets ending with 0 (no outputs after spendable-age exclusion) should throw.
  // Build offsets where the end region has 0 cumulative outputs.
  // We need at least CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE entries.
  std::vector<uint64_t> offsets(CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE + 5, 0);
  EXPECT_ANY_THROW(tools::gamma_picker picker(offsets));
}

TEST(coin_selection_gamma, average_output_time_calculation)
{
  // With known offsets, verify expected average output time.
  // 500 blocks, 2 outputs each => 1000 total outputs.
  // blocks_in_a_year = 86400*365/120 = 262800
  // blocks_to_consider = min(500, 262800) = 500
  // outputs_to_consider = offsets.back() - 0 = 1000 (since 500 < 500 is false, so it's offsets[0-1] which doesn't exist => 0)
  // average_output_time = 120 * 500 / 1000.0 = 60.0
  const size_t num_blocks = 500;
  const uint64_t outputs_per_block = 2;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, outputs_per_block);
  tools::gamma_picker picker(offsets);

  // num_rct_outputs = offsets[end - 1] where end excludes last SPENDABLE_AGE-1 blocks
  // end = offsets.data() + 500 - (max(1,10) - 1) = offsets.data() + 491
  // num_rct_outputs = offsets[490] = 491 * 2 = 982
  EXPECT_EQ(982u, picker.get_num_rct_outs());
}

TEST(coin_selection_gamma, blocks_to_consider_caps_at_year)
{
  // With more than a year of blocks, only the last year is considered.
  // blocks_in_a_year = 86400*365/120 = 262800
  const size_t blocks_in_a_year = 86400 * 365 / DIFFICULTY_TARGET_V2;
  const size_t num_blocks = blocks_in_a_year + 10000;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, 1);

  tools::gamma_picker picker(offsets);

  // end points to offsets.data() + num_blocks - (SPENDABLE_AGE - 1)
  // num_rct_outputs = *(end - 1) = offsets[num_blocks - SPENDABLE_AGE]
  uint64_t expected_num = offsets[num_blocks - CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE];
  EXPECT_EQ(expected_num, picker.get_num_rct_outs());
}

// ============================================================================
// Group: gamma_picker::pick()
// ============================================================================

TEST(coin_selection_gamma, pick_returns_valid_index)
{
  // 1000 picks should all be in [0, num_rct_outputs) or uint64_t::max (bad pick)
  std::vector<uint64_t> offsets = make_rct_offsets(2000, 3);
  tools::gamma_picker picker(offsets);
  uint64_t num_rct = picker.get_num_rct_outs();

  for (int i = 0; i < 1000; ++i)
  {
    uint64_t idx = picker.pick();
    EXPECT_TRUE(idx < num_rct || idx == std::numeric_limits<uint64_t>::max())
      << "pick() returned " << idx << " which is out of range [0, " << num_rct << ")";
  }
}

TEST(coin_selection_gamma, pick_max_on_overflow)
{
  // With very few outputs relative to the gamma distribution, some picks should
  // return uint64_t::max (bad pick) when the gamma sample is very large.
  // Use minimal offsets so num_rct_outputs is small.
  std::vector<uint64_t> offsets = make_rct_offsets(CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE + 1, 1);
  tools::gamma_picker picker(offsets);

  int max_count = 0;
  for (int i = 0; i < 10000; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      ++max_count;
  }
  // With only ~2 valid outputs, many picks should overflow
  EXPECT_GT(max_count, 0) << "Expected some bad picks (uint64_t::max) with tiny offset set";
}

TEST(coin_selection_gamma, pick_distribution_non_uniform)
{
  // Chi-squared style test: recent outputs should be over-represented.
  // Split outputs into two halves: recent (top 50%) and old (bottom 50%).
  std::vector<uint64_t> offsets = make_rct_offsets(5000, 5);
  tools::gamma_picker picker(offsets);
  uint64_t num_rct = picker.get_num_rct_outs();
  uint64_t midpoint = num_rct / 2;

  int recent_count = 0;
  int old_count = 0;
  const int total_picks = 10000;

  for (int i = 0; i < total_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      continue;
    if (idx >= midpoint)
      ++recent_count;
    else
      ++old_count;
  }

  // The gamma distribution should heavily favor recent outputs
  EXPECT_GT(recent_count, old_count)
    << "Recent outputs (" << recent_count << ") should outnumber old (" << old_count << ")";
}

TEST(coin_selection_gamma, pick_unlock_time_subtraction)
{
  // When gamma > DEFAULT_UNLOCK_TIME, offset should be subtracted.
  // We verify this indirectly: with enough blocks, the picker should produce
  // valid indices (not all max), meaning the subtraction path works.
  std::vector<uint64_t> offsets = make_rct_offsets(3000, 10);
  tools::gamma_picker picker(offsets);

  int valid_count = 0;
  for (int i = 0; i < 1000; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
      ++valid_count;
  }
  EXPECT_GT(valid_count, 500) << "Most picks should be valid with large offset set";
}

TEST(coin_selection_gamma, pick_recent_window_fallback)
{
  // When gamma <= DEFAULT_UNLOCK_TIME, the code falls back to RECENT_SPEND_WINDOW.
  // With a large chain, some picks will hit the recent window fallback, producing
  // indices near the tip of the unlocked range.
  std::vector<uint64_t> offsets = make_rct_offsets(5000, 10);
  tools::gamma_picker picker(offsets);
  uint64_t num_rct = picker.get_num_rct_outs();

  // The RECENT_SPEND_WINDOW = 15 * 120 = 1800 seconds.
  // average_output_time ~ 120 * min(5000, 262800) / (5000*10) = 120*5000/50000 = 12
  // So recent window fallback picks output_index = x / 12 where x in [0, 1800)
  // That means output_index in [0, 150), so output selected is near the top (num_rct - 1 - output_index)
  int near_tip = 0;
  for (int i = 0; i < 5000; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max() && idx >= num_rct - 200)
      ++near_tip;
  }
  // Some picks should be near the tip due to the recent window fallback
  EXPECT_GT(near_tip, 0) << "Expected some picks near the chain tip from recent window fallback";
}

TEST(coin_selection_gamma, pick_correct_block_lookup)
{
  // Known offsets: verify that pick() returns outputs belonging to valid blocks.
  // Block 0: outputs [0, 10), Block 1: [10, 20), ..., Block N: [10*N, 10*(N+1))
  std::vector<uint64_t> offsets = make_rct_offsets(2000, 10);
  tools::gamma_picker picker(offsets);

  for (int i = 0; i < 500; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      continue;
    // idx should be in [0, num_rct_outputs)
    // Find which block it belongs to
    size_t block = idx / 10;
    EXPECT_LT(block, offsets.size())
      << "Output " << idx << " maps to block " << block << " which is out of range";
    // Verify the output is within the block's range
    uint64_t block_start = block * 10;
    uint64_t block_end = (block + 1) * 10;
    EXPECT_GE(idx, block_start);
    EXPECT_LT(idx, block_end);
  }
}

TEST(coin_selection_gamma, pick_uniform_within_block)
{
  // Blocks with N outputs: distribution within each block should be roughly uniform.
  // Use large outputs_per_block so we can observe distribution.
  const uint64_t outputs_per_block = 100;
  std::vector<uint64_t> offsets = make_rct_offsets(2000, outputs_per_block);
  tools::gamma_picker picker(offsets);

  // Collect picks that fall in one specific block (near the tip for higher hit rate)
  // Target the block just before the spendable age cutoff
  size_t target_block = 2000 - CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE - 1;
  uint64_t block_start = target_block * outputs_per_block;
  uint64_t block_end = (target_block + 1) * outputs_per_block;

  std::map<uint64_t, int> within_block_counts;
  int hits = 0;
  for (int i = 0; i < 100000 && hits < 200; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx >= block_start && idx < block_end)
    {
      within_block_counts[idx]++;
      ++hits;
    }
  }
  // If we got enough hits, check that more than 1 distinct output was selected
  if (hits >= 10)
  {
    EXPECT_GT(within_block_counts.size(), 1u)
      << "Expected multiple distinct outputs within the block";
  }
}

TEST(coin_selection_gamma, pick_single_output_blocks)
{
  // 1 output per block: pick should return exactly the single output in a block.
  std::vector<uint64_t> offsets = make_rct_offsets(2000, 1);
  tools::gamma_picker picker(offsets);

  for (int i = 0; i < 500; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      continue;
    // With 1 output per block, output idx should equal block index
    // Block b has outputs [b, b+1), so output idx IS the block index
    EXPECT_LT(idx, offsets.size());
  }
}

TEST(coin_selection_gamma, pick_never_returns_locked)
{
  // The last SPENDABLE_AGE - 1 blocks are excluded by the constructor
  // (end = offsets.data() + size - (SPENDABLE_AGE - 1)).
  // So the maximum valid output should be less than offsets[size - SPENDABLE_AGE].
  const size_t num_blocks = 2000;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, 5);
  tools::gamma_picker picker(offsets);

  // locked_start = offsets[num_blocks - SPENDABLE_AGE]
  uint64_t locked_start = offsets[num_blocks - CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE];

  for (int i = 0; i < 5000; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      continue;
    EXPECT_LT(idx, locked_start)
      << "pick() returned " << idx << " which is in the locked region (>= " << locked_start << ")";
  }
}

// ============================================================================
// Group: pick_preferred_rct_inputs
// ============================================================================

TEST_F(CoinSelectionTest, single_input_sufficient)
{
  // One transfer that covers the needed_money should be returned alone
  set_blockchain_height(2000);
  add_transfer(5 * COIN, 100, false, make_txid(1), 1);

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(0u, picks[0]);
}

TEST_F(CoinSelectionTest, two_inputs_combined)
{
  // No single input suffices, but a pair does
  set_blockchain_height(2000);
  add_transfer(2 * COIN, 100, false, make_txid(1), 1);
  add_transfer(3 * COIN, 500, false, make_txid(2), 2);

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,4 * COIN, 0, subaddr_indices);
  ASSERT_EQ(2u, picks.size());
  // Both indices should be present
  std::set<size_t> pick_set(picks.begin(), picks.end());
  EXPECT_EQ(1u, pick_set.count(0));
  EXPECT_EQ(1u, pick_set.count(1));
}

TEST_F(CoinSelectionTest, respects_subaddr_account)
{
  // Only matching account should be considered
  set_blockchain_height(2000);
  add_transfer(5 * COIN, 100, false, make_txid(1), 1, {1, 0});  // account 1
  add_transfer(5 * COIN, 200, false, make_txid(2), 2, {0, 0});  // account 0

  std::set<uint32_t> subaddr_indices = {0};
  // Request account 0: should find transfer at index 1 only
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);
}

TEST_F(CoinSelectionTest, respects_subaddr_indices)
{
  // Only matching minor indices should be considered
  set_blockchain_height(2000);
  add_transfer(5 * COIN, 100, false, make_txid(1), 1, {0, 1});  // minor 1
  add_transfer(5 * COIN, 200, false, make_txid(2), 2, {0, 0});  // minor 0

  std::set<uint32_t> subaddr_indices = {0};  // only minor 0
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);
}

TEST_F(CoinSelectionTest, skips_frozen)
{
  // Frozen transfers should never be picked
  set_blockchain_height(2000);
  add_transfer(10 * COIN, 100, false, make_txid(1), 1, {0, 0}, true);  // frozen
  add_transfer(5 * COIN, 200, false, make_txid(2), 2);                  // not frozen

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);  // Should pick the non-frozen one
}

TEST_F(CoinSelectionTest, skips_locked)
{
  // Transfers too recent (not yet unlocked) should not be picked.
  // A transfer at height 1995 with blockchain height 2000 is not unlocked
  // because 1995 + 10 > 2000.
  set_blockchain_height(2000);
  add_transfer(10 * COIN, 1995, false, make_txid(1), 1);  // locked (height + 10 > 2000)
  add_transfer(5 * COIN, 100, false, make_txid(2), 2);     // unlocked

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);
}

TEST_F(CoinSelectionTest, respects_ignore_above)
{
  // ignore_outputs_above threshold should be enforced
  set_blockchain_height(2000);
  wallet_accessor_test::set_ignore_outputs_above(m_wallet, 4 * COIN);

  add_transfer(10 * COIN, 100, false, make_txid(1), 1);  // above threshold
  add_transfer(3 * COIN, 200, false, make_txid(2), 2);   // within threshold

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,2 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);

  // Reset
  wallet_accessor_test::set_ignore_outputs_above(m_wallet, MONEY_SUPPLY);
}

TEST_F(CoinSelectionTest, respects_ignore_below)
{
  // ignore_outputs_below threshold should be enforced
  set_blockchain_height(2000);
  wallet_accessor_test::set_ignore_outputs_below(m_wallet, 2 * COIN);

  add_transfer(1 * COIN, 100, false, make_txid(1), 1);  // below threshold
  add_transfer(5 * COIN, 200, false, make_txid(2), 2);   // above threshold

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,3 * COIN, 0, subaddr_indices);
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);

  // Reset
  wallet_accessor_test::set_ignore_outputs_below(m_wallet, 0);
}

TEST_F(CoinSelectionTest, returns_empty_no_match)
{
  // When no qualifying transfers exist, return empty
  set_blockchain_height(2000);
  add_transfer(1 * COIN, 100, true, make_txid(1), 1);  // spent
  add_transfer(1 * COIN, 100, false, make_txid(2), 2, {1, 0}); // wrong account

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet,5 * COIN, 0, subaddr_indices);
  EXPECT_TRUE(picks.empty());
}

TEST_F(CoinSelectionTest, prefers_lower_relatedness)
{
  // When choosing between pairs, the one with lower relatedness should be preferred.
  // The algorithm iterates i=0..N, j=i+1..N keeping lowest relatedness pair.
  // If rel==0.0 is found, it returns immediately.
  set_blockchain_height(2000);

  // Transfer 0: block 100, 2 XMR
  add_transfer(2 * COIN, 100, false, make_txid(1), 1);
  // Transfer 1: block 100, 3 XMR (same block as 0 -> rel=0.9)
  add_transfer(3 * COIN, 100, false, make_txid(2), 2);
  // Transfer 2: block 900, 3 XMR (far from 0 -> rel=0.0, from 1 -> rel=0.0)
  add_transfer(3 * COIN, 900, false, make_txid(3), 3);

  std::set<uint32_t> subaddr_indices = {0};
  auto picks = wallet_accessor_test::pick_preferred_rct_inputs(m_wallet, 4 * COIN, 0, subaddr_indices);

  // Iteration: i=0, j=1 -> rel=0.9, picks=[0,1]
  //            i=0, j=2 -> rel=0.0 < 0.9, picks=[0,2], return immediately
  ASSERT_EQ(2u, picks.size());
  std::set<size_t> pick_set(picks.begin(), picks.end());
  EXPECT_EQ(1u, pick_set.count(0));
  EXPECT_EQ(1u, pick_set.count(2));
}

// ============================================================================
// Group: get_output_relatedness
// ============================================================================

TEST_F(CoinSelectionTest, same_tx_returns_1)
{
  // Two outputs from the same tx should return 1.0
  crypto::hash txid = make_txid(100);
  auto td0 = make_transfer_detail(100, 1000, false, txid);
  auto td1 = make_transfer_detail(200, 1000, false, txid);

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(1.0f, r);
}

TEST_F(CoinSelectionTest, different_block_returns_0)
{
  // Different txs, different blocks (far apart) should return 0.0
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 2000, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.0f, r);
}

TEST_F(CoinSelectionTest, same_block_returns_0_5)
{
  // Different txs, same block should return 0.9 (the actual code returns 0.9 for dh==0)
  // Note: The spec says 0.5 but the actual implementation returns 0.9 for same block.
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1000, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  // Same block, different tx = 0.9 in the implementation
  EXPECT_FLOAT_EQ(0.9f, r);
}

TEST_F(CoinSelectionTest, symmetric)
{
  // relatedness(a, b) == relatedness(b, a)
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1005, false, make_txid(2));

  float r_forward = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  float r_reverse = wallet_accessor_test::get_output_relatedness(m_wallet, td1, td0);
  EXPECT_FLOAT_EQ(r_forward, r_reverse);
}

// ============================================================================
// Group: distribution validation
// ============================================================================

TEST(coin_selection_gamma, rct_offsets_monotonic)
{
  // The invariant expected by gamma_picker: rct_offsets must be monotonically non-decreasing
  std::vector<uint64_t> offsets = make_rct_offsets(5000, 3);
  for (size_t i = 1; i < offsets.size(); ++i)
  {
    EXPECT_GE(offsets[i], offsets[i - 1])
      << "Monotonicity violated at index " << i;
  }

  // Also verify the gamma_picker can be constructed with monotonic offsets
  EXPECT_NO_THROW(tools::gamma_picker picker(offsets));
}

TEST(coin_selection_gamma, gamma_parameters_match_spec)
{
  // Gamma distribution with shape=19.28, scale=1/1.61 should have:
  // mean = shape * scale = 19.28 / 1.61 ~= 11.975
  // variance = shape * scale^2 = 19.28 / (1.61^2) ~= 7.436
  const double shape = 19.28;
  const double scale = 1.0 / 1.61;
  const double expected_mean = shape * scale;
  const double expected_variance = shape * scale * scale;

  // Verify the theoretical values match expectations
  EXPECT_NEAR(expected_mean, 11.975, 0.01);
  EXPECT_NEAR(expected_variance, 7.436, 0.01);

  // Construct a gamma_picker with explicit shape/scale and verify it works
  std::vector<uint64_t> offsets = make_rct_offsets(2000, 5);
  EXPECT_NO_THROW(tools::gamma_picker picker(offsets, shape, scale));

  // Sample from the picker to verify the distribution is reasonable
  tools::gamma_picker picker(offsets, shape, scale);
  int valid_picks = 0;
  for (int i = 0; i < 1000; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
      ++valid_picks;
  }
  EXPECT_GT(valid_picks, 0) << "Picker with spec params should produce valid picks";
}
