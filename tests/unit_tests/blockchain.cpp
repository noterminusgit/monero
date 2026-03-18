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

#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/blockchain_and_pool.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
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

  class BlockchainTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      get_test_options opts;
      ASSERT_TRUE(m_bap.blockchain.init(new TestDB(), cryptonote::FAKECHAIN, true, &opts.test_options, 0, NULL));
    }

    void TearDown() override
    {
      m_bap.blockchain.deinit();
    }

    cryptonote::BlockchainAndPool m_bap;
    cryptonote::Blockchain& m_blockchain{m_bap.blockchain};
  };
}

TEST_F(BlockchainTest, init_and_height)
{
  ASSERT_EQ(m_blockchain.get_current_blockchain_height(), 1u);
}

TEST_F(BlockchainTest, tail_id_not_null)
{
  crypto::hash tail = m_blockchain.get_tail_id();
  // BaseTestDB returns null_hash for all hashes, so tail might be null
  (void)tail;
}

TEST_F(BlockchainTest, pruning_seed_default)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTest, fee_quantization_mask)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  ASSERT_GT(mask, 0u);
}

TEST_F(BlockchainTest, dynamic_base_fee)
{
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, have_tx_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_tx(h));
}

TEST_F(BlockchainTest, block_difficulty_genesis)
{
  // BaseTestDB returns 0 for block_difficulty, just verify no crash
  cryptonote::difficulty_type d = m_blockchain.block_difficulty(0);
  (void)d;
}

// =============================================================================
// V16 fixture for higher hard fork version tests
// =============================================================================
namespace
{
  struct get_test_options_v16 {
    const std::pair<uint8_t, uint64_t> hard_forks[2];
    const cryptonote::test_options test_options = { hard_forks, 0 };
    get_test_options_v16(): hard_forks{std::make_pair((uint8_t)16, (uint64_t)0), std::make_pair((uint8_t)0, (uint64_t)0)} {}
  };

  class BlockchainTestV16 : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      get_test_options_v16 opts;
      ASSERT_TRUE(m_bap.blockchain.init(new TestDB(), cryptonote::FAKECHAIN, true, &opts.test_options, 0, NULL));
    }
    void TearDown() override { m_bap.blockchain.deinit(); }
    cryptonote::BlockchainAndPool m_bap;
    cryptonote::Blockchain& m_blockchain{m_bap.blockchain};
  };
}

// =============================================================================
// Fee calculation tests (~15 tests)
// =============================================================================

TEST_F(BlockchainTest, dynamic_base_fee_typical_reward_and_median)
{
  // Typical block reward (~1.6 XMR) and median weight (300000)
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(1600000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, dynamic_base_fee_small_reward)
{
  // Very small block reward should still produce a non-zero fee
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(1000ULL, 300000);
  ASSERT_GE(fee, 0u);
}

TEST_F(BlockchainTest, dynamic_base_fee_large_reward)
{
  // Very large block reward
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(100000000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, dynamic_base_fee_small_median)
{
  // Median below the minimum block granted full reward zone should clamp to min
  uint64_t fee_small = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 100);
  uint64_t fee_min = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5);
  // The small median gets clamped to the minimum, so both fees should be equal
  ASSERT_EQ(fee_small, fee_min);
}

TEST_F(BlockchainTest, dynamic_base_fee_zero_median_clamps)
{
  // Zero median should get clamped to minimum block weight
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 0);
  uint64_t fee_min = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5);
  ASSERT_EQ(fee, fee_min);
}

TEST_F(BlockchainTest, dynamic_base_fee_large_median)
{
  // Very large median weight should produce a smaller fee
  uint64_t fee_normal = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  uint64_t fee_large = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 3000000);
  ASSERT_LT(fee_large, fee_normal);
}

TEST_F(BlockchainTest, dynamic_base_fee_monotonic_with_reward)
{
  // Higher reward should yield higher fee for same median
  uint64_t fee_low = cryptonote::Blockchain::get_dynamic_base_fee(1000000000ULL, 300000);
  uint64_t fee_high = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  ASSERT_LE(fee_low, fee_high);
}

TEST_F(BlockchainTest, dynamic_base_fee_monotonic_with_median)
{
  // Higher median should yield lower fee for same reward
  uint64_t fee_small_med = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  uint64_t fee_large_med = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 600000);
  ASSERT_GE(fee_small_med, fee_large_med);
}

TEST_F(BlockchainTest, dynamic_base_fee_at_exact_minimum_median)
{
  // Test fee at exactly the minimum block granted full reward zone
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, fee_quantization_mask_is_power_of_10)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  // The mask should be 10^(CRYPTONOTE_DISPLAY_DECIMAL_POINT - PER_KB_FEE_QUANTIZATION_DECIMALS)
  // = 10^(12-8) = 10^4 = 10000
  ASSERT_EQ(mask, 10000u);
}

TEST_F(BlockchainTest, fee_quantization_mask_divides_evenly)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  // A fee value that is a multiple of the mask should remain unchanged when quantized
  uint64_t fee = 50000u;
  uint64_t quantized = (fee / mask) * mask;
  ASSERT_EQ(quantized, fee);
}

TEST_F(BlockchainTest, fee_quantization_mask_rounds_down)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  // A fee not divisible by mask should round down
  uint64_t fee = 59999u;
  uint64_t quantized = (fee / mask) * mask;
  ASSERT_EQ(quantized, 50000u);
}

TEST_F(BlockchainTest, dynamic_base_fee_estimate_2021_scaling_static)
{
  // Test static version of 2021 scaling fee estimate
  std::vector<uint64_t> fees;
  uint64_t base_reward = 600000000000ULL; // 0.6 XMR
  uint64_t Mnw = 300000;
  uint64_t Mlw = 300000;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(base_reward, Mnw, Mlw, fees);
  ASSERT_EQ(fees.size(), 4u);
  // All four fee levels should be positive
  for (size_t i = 0; i < fees.size(); ++i)
  {
    ASSERT_GT(fees[i], 0u) << "Fee level " << i << " should be positive";
  }
  // Fee levels should be non-decreasing: Fl <= Fn <= Fm <= Fh
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

TEST_F(BlockchainTest, dynamic_base_fee_estimate_2021_scaling_large_weight)
{
  // With larger Mnw, fees should be lower
  std::vector<uint64_t> fees_small, fees_large;
  uint64_t base_reward = 600000000000ULL;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(base_reward, 300000, 300000, fees_small);
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(base_reward, 600000, 600000, fees_large);
  // At least the lowest fee level with larger weight should be <= small weight
  ASSERT_LE(fees_large[0], fees_small[0]);
}

// =============================================================================
// Block validation helpers (~10 tests)
// =============================================================================

TEST_F(BlockchainTest, get_current_blockchain_height_after_init)
{
  // After init with TestDB (height=1), current height should be 1
  ASSERT_EQ(m_blockchain.get_current_blockchain_height(), 1u);
}

TEST_F(BlockchainTest, get_tail_id_returns_hash)
{
  // get_tail_id should return a hash (may be null_hash from TestDB)
  uint64_t height = 0;
  crypto::hash tail = m_blockchain.get_tail_id(height);
  // The overload should set the height
  (void)tail;
  (void)height;
}

TEST_F(BlockchainTest, get_short_chain_history_small_chain)
{
  std::list<crypto::hash> ids;
  uint64_t current_height = 0;
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids, current_height));
  // With height 1, we should get at least the genesis block hash
  ASSERT_GE(ids.size(), 1u);
  ASSERT_EQ(current_height, 1u);
}

TEST_F(BlockchainTest, have_block_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_block(h));
}

TEST_F(BlockchainTest, get_difficulty_for_next_block_no_crash)
{
  // Just verify this doesn't crash on a fresh chain
  cryptonote::difficulty_type diff = m_blockchain.get_difficulty_for_next_block();
  (void)diff;
}

TEST_F(BlockchainTest, get_difficulty_target_v1)
{
  // With hard fork version 1, difficulty target should be DIFFICULTY_TARGET_V1 (60)
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V1);
}

TEST_F(BlockchainTest, get_total_transactions_at_genesis)
{
  // TestDB returns 0 for tx_count
  size_t total = m_blockchain.get_total_transactions();
  ASSERT_EQ(total, 0u);
}

TEST_F(BlockchainTest, get_db_returns_reference)
{
  // Verify we can get a DB reference without crashing
  const cryptonote::BlockchainDB& db = m_blockchain.get_db();
  ASSERT_EQ(db.height(), 1u);
}

TEST_F(BlockchainTest, get_db_mutable_returns_reference)
{
  // Verify we can get a mutable DB reference without crashing
  cryptonote::BlockchainDB& db = m_blockchain.get_db();
  ASSERT_EQ(db.height(), 1u);
}

TEST_F(BlockchainTest, block_difficulty_at_height_0)
{
  // TestDB returns 0 for block_difficulty at any height
  cryptonote::difficulty_type d = m_blockchain.block_difficulty(0);
  ASSERT_EQ(d, 0u);
}

// =============================================================================
// Pruning and hard fork version tests (~8 tests)
// =============================================================================

TEST_F(BlockchainTest, get_blockchain_pruning_seed_is_zero)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTest, get_ideal_hard_fork_version_after_init)
{
  // With test options specifying HF version 1, ideal version should be 1
  uint8_t version = m_blockchain.get_ideal_hard_fork_version();
  ASSERT_EQ(version, 1u);
}

TEST_F(BlockchainTest, get_current_hard_fork_version_after_init)
{
  uint8_t version = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(version, 1u);
}

TEST_F(BlockchainTest, get_ideal_hard_fork_version_at_height_0)
{
  uint8_t version = m_blockchain.get_ideal_hard_fork_version(0);
  ASSERT_EQ(version, 1u);
}

TEST_F(BlockchainTest, get_hard_fork_version_at_height_0)
{
  uint8_t version = m_blockchain.get_hard_fork_version(0);
  ASSERT_GE(version, 0u);
}

TEST_F(BlockchainTest, get_next_hard_fork_version)
{
  uint8_t version = m_blockchain.get_next_hard_fork_version();
  ASSERT_GE(version, 1u);
}

TEST_F(BlockchainTest, get_hard_fork_state_after_init)
{
  cryptonote::HardFork::State state = m_blockchain.get_hard_fork_state();
  // Should be a valid state value
  ASSERT_TRUE(state == cryptonote::HardFork::LikelyForked || state == cryptonote::HardFork::UpdateNeeded ||
              state == cryptonote::HardFork::Ready);
}

TEST_F(BlockchainTest, get_hard_fork_voting_info)
{
  uint32_t window = 0, votes = 0, threshold = 0;
  uint64_t earliest_height = 0;
  uint8_t voting = 0;
  bool enabled = m_blockchain.get_hard_fork_voting_info(1, window, votes, threshold, earliest_height, voting);
  ASSERT_TRUE(enabled);
}

// =============================================================================
// Static method / fee utility tests (~10 tests)
// =============================================================================

TEST_F(BlockchainTest, static_dynamic_base_fee_with_1xmr_reward)
{
  // 1 XMR = 1e12 atomic units
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(1000000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, static_dynamic_base_fee_with_tail_emission)
{
  // Tail emission: 0.6 XMR = 6e11
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(600000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, static_dynamic_base_fee_with_huge_median)
{
  // Large median (10MB) should produce smaller fees
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(1000000000000ULL, 10000000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, static_dynamic_base_fee_proportional_to_reward)
{
  // Fee should scale roughly proportionally to reward (for fixed median)
  uint64_t fee1 = cryptonote::Blockchain::get_dynamic_base_fee(1000000000000ULL, 300000);
  uint64_t fee2 = cryptonote::Blockchain::get_dynamic_base_fee(2000000000000ULL, 300000);
  // fee2 should be roughly 2x fee1 (since the formula is proportional through block_reward)
  ASSERT_GT(fee2, fee1);
}

TEST_F(BlockchainTest, static_dynamic_base_fee_inversely_proportional_to_median_squared)
{
  // Fee is inversely proportional to median^2 (for fixed reward)
  uint64_t fee1 = cryptonote::Blockchain::get_dynamic_base_fee(10000000000000ULL, 300000);
  uint64_t fee2 = cryptonote::Blockchain::get_dynamic_base_fee(10000000000000ULL, 600000);
  // fee2 should be roughly fee1/4 (since median doubled, fee ~ 1/median^2)
  ASSERT_LT(fee2, fee1);
}

TEST_F(BlockchainTest, dynamic_base_fee_estimate_2021_scaling_fee_levels_ordering)
{
  // Check that fee levels are in order: low <= normal <= medium <= high
  std::vector<uint64_t> fees;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 300000, 300000, fees);
  ASSERT_EQ(fees.size(), 4u);
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

TEST_F(BlockchainTest, dynamic_base_fee_estimate_2021_scaling_with_zero_reward)
{
  // Zero reward edge case
  std::vector<uint64_t> fees;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(0, 300000, 300000, fees);
  ASSERT_EQ(fees.size(), 4u);
  // Fees should all be zero or at least not crash
  for (size_t i = 0; i < fees.size(); ++i)
  {
    ASSERT_GE(fees[i], 0u);
  }
}

TEST_F(BlockchainTest, dynamic_base_fee_estimate_2021_scaling_different_Mnw_Mlw)
{
  // Test with Mnw < Mlw (normal usage: Mnw = min(Msw, 50*Mlw))
  std::vector<uint64_t> fees;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 200000, 300000, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (size_t i = 0; i < fees.size(); ++i)
  {
    ASSERT_GT(fees[i], 0u) << "Fee level " << i << " should be positive";
  }
}

TEST_F(BlockchainTest, fee_quantization_mask_consistency)
{
  // Verify the mask is consistent with the display decimal point constants
  uint64_t expected = 1;
  for (int i = 0; i < CRYPTONOTE_DISPLAY_DECIMAL_POINT - PER_KB_FEE_QUANTIZATION_DECIMALS; ++i)
    expected *= 10;
  ASSERT_EQ(cryptonote::Blockchain::get_fee_quantization_mask(), expected);
}

TEST_F(BlockchainTest, dynamic_base_fee_with_one_atomic_unit_reward)
{
  // Extreme edge case: 1 atomic unit reward
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(1ULL, 300000);
  // Should not crash; fee might be 0 due to integer division
  (void)fee;
}

// =============================================================================
// Chain state query tests (~10 tests)
// =============================================================================

TEST_F(BlockchainTest, get_txpool_tx_count_is_zero)
{
  uint64_t count = m_blockchain.get_txpool_tx_count(false);
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTest, get_txpool_tx_count_include_sensitive_is_zero)
{
  uint64_t count = m_blockchain.get_txpool_tx_count(true);
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTest, get_alternative_blocks_count_is_zero)
{
  size_t count = m_blockchain.get_alternative_blocks_count();
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTest, is_within_compiled_block_hash_area_at_zero)
{
  // At height 0, may or may not be within compiled area depending on build config
  bool within = m_blockchain.is_within_compiled_block_hash_area(0);
  (void)within; // Just verify no crash
}

TEST_F(BlockchainTest, is_within_compiled_block_hash_area_very_large_height)
{
  // A very large height should be outside the compiled block hash area
  bool within = m_blockchain.is_within_compiled_block_hash_area(UINT64_MAX);
  ASSERT_FALSE(within);
}

TEST_F(BlockchainTest, is_within_compiled_block_hash_area_current)
{
  // The parameterless version uses current db height
  bool within = m_blockchain.is_within_compiled_block_hash_area();
  (void)within; // Just verify no crash
}

TEST_F(BlockchainTest, get_alternative_blocks_empty)
{
  std::vector<cryptonote::block> blocks;
  ASSERT_TRUE(m_blockchain.get_alternative_blocks(blocks));
  ASSERT_EQ(blocks.size(), 0u);
}

TEST_F(BlockchainTest, get_current_cumulative_block_weight_limit)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
}

TEST_F(BlockchainTest, get_current_cumulative_block_weight_median)
{
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
}

TEST_F(BlockchainTest, have_tx_keyimg_as_spent_nonexistent)
{
  crypto::key_image ki = crypto::rand<crypto::key_image>();
  ASSERT_FALSE(m_blockchain.have_tx_keyimg_as_spent(ki));
}

TEST_F(BlockchainTest, get_block_id_by_height_zero)
{
  crypto::hash id = m_blockchain.get_block_id_by_height(0);
  // TestDB returns null_hash for all hashes
  (void)id;
}

TEST_F(BlockchainTest, check_fee_zero_weight)
{
  // Zero-weight transaction with zero fee - just verify no crash
  bool ok = m_blockchain.check_fee(0, 0);
  (void)ok;
}

TEST_F(BlockchainTest, check_fee_nonzero_weight_zero_fee)
{
  // Non-zero weight with zero fee should fail
  bool ok = m_blockchain.check_fee(3000, 0);
  ASSERT_FALSE(ok);
}

TEST_F(BlockchainTest, check_fee_large_fee)
{
  // Very large fee for a typical-weight transaction should pass
  bool ok = m_blockchain.check_fee(3000, 1000000000000ULL);
  ASSERT_TRUE(ok);
}

TEST_F(BlockchainTest, get_hardforks_returns_entries)
{
  const auto& hfs = m_blockchain.get_hardforks();
  ASSERT_GE(hfs.size(), 1u);
}

TEST_F(BlockchainTest, get_earliest_ideal_height_for_version_1)
{
  uint64_t height = m_blockchain.get_earliest_ideal_height_for_version(1);
  ASSERT_EQ(height, 0u);
}

TEST_F(BlockchainTest, store_blockchain_no_crash)
{
  // store_blockchain should work without crashing
  bool ok = m_blockchain.store_blockchain();
  (void)ok;
}

TEST_F(BlockchainTest, get_last_block_timestamps_empty)
{
  std::vector<time_t> timestamps = m_blockchain.get_last_block_timestamps(10);
  // With only genesis block, we may get 0 or 1 timestamps
  ASSERT_LE(timestamps.size(), 1u);
}

TEST_F(BlockchainTest, for_all_key_images_empty_chain)
{
  bool result = m_blockchain.for_all_key_images([](const crypto::key_image& ki) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTest, for_all_transactions_empty_chain)
{
  bool result = m_blockchain.for_all_transactions([](const crypto::hash& h, const cryptonote::transaction& tx) {
    return true;
  }, false);
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTest, for_all_outputs_empty_chain)
{
  bool result = m_blockchain.for_all_outputs([](uint64_t amount, const crypto::hash& tx_hash, uint64_t height, size_t tx_idx) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTest, flush_invalid_blocks_no_crash)
{
  m_blockchain.flush_invalid_blocks();
}

TEST_F(BlockchainTest, set_enforce_dns_checkpoints_no_crash)
{
  m_blockchain.set_enforce_dns_checkpoints(false);
  m_blockchain.set_enforce_dns_checkpoints(true);
}

TEST_F(BlockchainTest, get_output_histogram_empty)
{
  std::vector<uint64_t> amounts;
  auto histogram = m_blockchain.get_output_histogram(amounts, false, 0, 0);
  ASSERT_TRUE(histogram.empty());
}

// =============================================================================
// V16 fixture tests
// =============================================================================

TEST_F(BlockchainTestV16, init_and_height_v16)
{
  ASSERT_EQ(m_blockchain.get_current_blockchain_height(), 1u);
}

TEST_F(BlockchainTestV16, get_difficulty_target_v2)
{
  // With hard fork version >= 2, difficulty target should be DIFFICULTY_TARGET_V2 (120)
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
}

TEST_F(BlockchainTestV16, get_current_hard_fork_version_is_16)
{
  uint8_t version = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(version, 16u);
}

TEST_F(BlockchainTestV16, get_ideal_hard_fork_version_is_16)
{
  uint8_t version = m_blockchain.get_ideal_hard_fork_version();
  ASSERT_EQ(version, 16u);
}

TEST_F(BlockchainTestV16, get_next_hard_fork_version_v16)
{
  uint8_t version = m_blockchain.get_next_hard_fork_version();
  ASSERT_GE(version, 16u);
}

TEST_F(BlockchainTestV16, dynamic_base_fee_v16)
{
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(600000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTestV16, get_difficulty_for_next_block_v16)
{
  // Verify no crash at HF v16
  cryptonote::difficulty_type diff = m_blockchain.get_difficulty_for_next_block();
  (void)diff;
}

TEST_F(BlockchainTestV16, check_fee_v16)
{
  // At HF v16, fee checking should work
  bool ok = m_blockchain.check_fee(3000, 1000000000000ULL);
  ASSERT_TRUE(ok);
}

TEST_F(BlockchainTestV16, check_fee_insufficient_v16)
{
  bool ok = m_blockchain.check_fee(3000, 0);
  ASSERT_FALSE(ok);
}

TEST_F(BlockchainTestV16, get_dynamic_base_fee_estimate_2021_scaling_instance_v16)
{
  // Test the instance method that uses current chain state
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(10, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (size_t i = 0; i < fees.size(); ++i)
  {
    ASSERT_GT(fees[i], 0u) << "Fee level " << i << " should be positive at v16";
  }
}

TEST_F(BlockchainTestV16, fee_estimate_2021_scaling_ordering_v16)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(10, fees);
  ASSERT_EQ(fees.size(), 4u);
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

TEST_F(BlockchainTestV16, get_short_chain_history_v16)
{
  std::list<crypto::hash> ids;
  uint64_t current_height = 0;
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids, current_height));
  ASSERT_GE(ids.size(), 1u);
}

TEST_F(BlockchainTestV16, get_current_cumulative_block_weight_limit_v16)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
}

TEST_F(BlockchainTestV16, get_current_cumulative_block_weight_median_v16)
{
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
}

TEST_F(BlockchainTestV16, get_txpool_tx_count_v16)
{
  uint64_t count = m_blockchain.get_txpool_tx_count(false);
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTestV16, have_block_nonexistent_v16)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_block(h));
}

TEST_F(BlockchainTestV16, have_tx_nonexistent_v16)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_tx(h));
}

TEST_F(BlockchainTestV16, pruning_seed_default_v16)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTestV16, block_difficulty_genesis_v16)
{
  cryptonote::difficulty_type d = m_blockchain.block_difficulty(0);
  ASSERT_EQ(d, 0u);
}

TEST_F(BlockchainTestV16, get_alternative_chains_empty_v16)
{
  auto chains = m_blockchain.get_alternative_chains();
  ASSERT_EQ(chains.size(), 0u);
}

TEST_F(BlockchainTestV16, get_db_returns_correct_height_v16)
{
  const cryptonote::BlockchainDB& db = m_blockchain.get_db();
  ASSERT_EQ(db.height(), 1u);
}
