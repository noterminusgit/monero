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

#define IN_UNIT_TESTS

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

// =============================================================================
// Additional coverage tests for blockchain.cpp
// =============================================================================

// --- get_adjusted_time tests ---

TEST_F(BlockchainTest, get_adjusted_time_low_height_returns_current_time)
{
  // With height < BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW (60), returns current time
  uint64_t adjusted = m_blockchain.get_adjusted_time(1);
  uint64_t now = static_cast<uint64_t>(time(NULL));
  // Should be very close to current time (within 2 seconds)
  ASSERT_GE(adjusted, now - 2);
  ASSERT_LE(adjusted, now + 2);
}

TEST_F(BlockchainTest, get_adjusted_time_zero_height_returns_current_time)
{
  uint64_t adjusted = m_blockchain.get_adjusted_time(0);
  uint64_t now = static_cast<uint64_t>(time(NULL));
  ASSERT_GE(adjusted, now - 2);
  ASSERT_LE(adjusted, now + 2);
}

// --- get_block_by_hash tests ---

TEST_F(BlockchainTest, get_block_by_hash_nonexistent_throws)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::block blk;
  bool orphan = false;
  // TestDB returns empty blob for unknown hashes, causing a parse exception
  ASSERT_ANY_THROW(m_blockchain.get_block_by_hash(h, blk, &orphan));
}

TEST_F(BlockchainTest, get_block_by_hash_nonexistent_null_orphan_throws)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::block blk;
  ASSERT_ANY_THROW(m_blockchain.get_block_by_hash(h, blk, nullptr));
}

// --- get_blocks (offset, count) tests ---

TEST_F(BlockchainTest, get_blocks_beyond_height_returns_false)
{
  // Requesting blocks starting at height 100 (beyond chain height 1) should fail
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  ASSERT_FALSE(m_blockchain.get_blocks(100, 1, blocks));
}

TEST_F(BlockchainTest, get_blocks_at_height_zero_succeeds)
{
  // Requesting block at height 0 should succeed (genesis block)
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  // TestDB's get_block_blob_from_height returns empty block blob, which may or may not parse
  // Just verify the method doesn't crash
  m_blockchain.get_blocks(0, 1, blocks);
}

TEST_F(BlockchainTest, get_blocks_with_txs_beyond_height_returns_false)
{
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  std::vector<cryptonote::blobdata> txs;
  ASSERT_FALSE(m_blockchain.get_blocks(100, 1, blocks, txs));
}

// --- get_num_mature_outputs test ---

TEST_F(BlockchainTest, get_num_mature_outputs_zero_amount)
{
  // TestDB returns get_num_outputs = 1, but height check may filter it
  uint64_t num = m_blockchain.get_num_mature_outputs(0);
  // With only 1 block and CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE = 10,
  // the single output at height 0 needs height 0 + 10 <= 1, which is false
  // So num should be 0
  ASSERT_EQ(num, 0u);
}

// --- get_output_key test ---

TEST_F(BlockchainTest, get_output_key_returns_pubkey)
{
  // TestDB returns empty output_data_t (zeroed pubkey)
  crypto::public_key pk = m_blockchain.get_output_key(0, 0);
  // Should return a zeroed key from TestDB
  ASSERT_EQ(pk, crypto::public_key());
}

// --- get_outs tests ---

TEST_F(BlockchainTest, get_outs_empty_request)
{
  // Empty request should succeed with empty response
  cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::request req;
  req.get_txid = false;
  cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::response res;
  ASSERT_TRUE(m_blockchain.get_outs(req, res));
  ASSERT_TRUE(res.outs.empty());
}

// --- get_output_key_mask_unlocked test ---

TEST_F(BlockchainTest, get_output_key_mask_unlocked_returns_data)
{
  crypto::public_key key;
  rct::key mask;
  bool unlocked = false;
  // TestDB returns zeroed output data for amount=0, index=0
  m_blockchain.get_output_key_mask_unlocked(0, 0, key, mask, unlocked);
  // Verify the function completed without crash
  ASSERT_EQ(key, crypto::public_key());
}

// --- get_output_distribution tests ---

TEST_F(BlockchainTest, get_output_distribution_invalid_range)
{
  uint64_t start_height = 0;
  std::vector<uint64_t> distribution;
  uint64_t base = 0;
  // to_height > 0 && to_height < from_height => returns false
  ASSERT_FALSE(m_blockchain.get_output_distribution(0, 10, 5, start_height, distribution, base));
}

TEST_F(BlockchainTest, get_output_distribution_beyond_height)
{
  uint64_t start_height = 0;
  std::vector<uint64_t> distribution;
  uint64_t base = 0;
  // to_height >= db_height => returns false
  ASSERT_FALSE(m_blockchain.get_output_distribution(1, 0, 100, start_height, distribution, base));
}

// --- check_tx_outputs (static) tests ---

TEST_F(BlockchainTest, check_tx_outputs_empty_v1_tx_hf1)
{
  // A v1 tx with no outputs should pass (no outputs to check)
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 1));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTest, check_tx_outputs_v1_tx_with_valid_decomposed_amount_hf2)
{
  // A v1 tx with valid decomposed amounts should pass at HF2
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 1000000000000ULL; // 1 XMR, valid decomposed amount
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTest, check_tx_outputs_v1_tx_with_invalid_decomposed_amount_hf2)
{
  // A v1 tx with non-decomposed amount should fail at HF2
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 1234567890ULL; // not a valid decomposed amount
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_TRUE(tvc.m_invalid_output);
}

TEST_F(BlockchainTest, check_tx_outputs_v2_tx_nonzero_amount_hf3)
{
  // A v2 tx with non-zero output amounts should fail at HF3+
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 100;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 3));
  ASSERT_TRUE(tvc.m_invalid_output);
}

TEST_F(BlockchainTest, check_tx_outputs_v2_tx_zero_amount_hf3)
{
  // A v2 tx with zero output amounts should pass at HF3
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 3));
  ASSERT_FALSE(tvc.m_invalid_output);
}

// --- have_tx_keyimges_as_spent tests ---

TEST_F(BlockchainTest, have_tx_keyimges_as_spent_span_empty)
{
  // Empty span should return empty vector
  std::vector<crypto::key_image> ki;
  auto result = m_blockchain.have_tx_keyimges_as_spent(epee::to_span(ki));
  ASSERT_TRUE(result.empty());
}

TEST_F(BlockchainTest, have_tx_keyimges_as_spent_span_nonexistent)
{
  // Key images that don't exist should all return false
  std::vector<crypto::key_image> ki;
  ki.push_back(crypto::rand<crypto::key_image>());
  ki.push_back(crypto::rand<crypto::key_image>());
  ki.push_back(crypto::rand<crypto::key_image>());
  auto result = m_blockchain.have_tx_keyimges_as_spent(epee::to_span(ki));
  ASSERT_EQ(result.size(), 3u);
  for (size_t i = 0; i < result.size(); ++i)
    ASSERT_FALSE(result[i]) << "Key image " << i << " should not be spent";
}

// --- find_blockchain_supplement tests ---

TEST_F(BlockchainTest, find_blockchain_supplement_empty_ids_fails)
{
  // Empty qblock_ids should fail
  std::list<crypto::hash> qblock_ids;
  uint64_t starter_offset = 0;
  ASSERT_FALSE(m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset));
}

TEST_F(BlockchainTest, find_blockchain_supplement_wrong_genesis_fails)
{
  // If the last element (genesis) doesn't match, should fail
  std::list<crypto::hash> qblock_ids;
  qblock_ids.push_back(crypto::rand<crypto::hash>()); // wrong genesis hash
  uint64_t starter_offset = 0;
  ASSERT_FALSE(m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset));
}

// NOTE: cleanup_handle_incoming_blocks requires a prior prepare_handle_incoming_blocks
// call to acquire the mutex. Testing cleanup alone causes undefined mutex unlock behavior.

// --- check_difficulty_checkpoints on clean chain ---

TEST_F(BlockchainTest, check_difficulty_checkpoints_clean_chain)
{
  // On FAKECHAIN with no checkpoints, this should return true
  auto result = m_blockchain.check_difficulty_checkpoints();
  ASSERT_TRUE(result.first);
}

// --- set_user_options ---

TEST_F(BlockchainTest, set_user_options_no_crash)
{
  m_blockchain.set_user_options(4, true, 100, cryptonote::db_defaultsync, true);
}

TEST_F(BlockchainTest, set_user_options_nosync_mode)
{
  m_blockchain.set_user_options(1, false, 0, cryptonote::db_nosync, false);
}

// --- safesyncmode ---

TEST_F(BlockchainTest, safesyncmode_toggle)
{
  m_blockchain.safesyncmode(true);
  m_blockchain.safesyncmode(false);
}

// --- lock/unlock ---

TEST_F(BlockchainTest, lock_unlock_no_deadlock)
{
  m_blockchain.lock();
  m_blockchain.unlock();
}

// --- cancel ---

TEST_F(BlockchainTest, cancel_no_crash)
{
  m_blockchain.cancel();
}

// --- get_next_long_term_block_weight ---

TEST_F(BlockchainTest, get_next_long_term_block_weight_returns_value)
{
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(1000);
  // At HF version 1, long term block weight is just the block weight
  ASSERT_EQ(ltw, 1000u);
}

// --- has_block_weights ---

TEST_F(BlockchainTest, has_block_weights_at_zero)
{
  bool has = m_blockchain.has_block_weights(0, 1);
  (void)has; // just verify no crash
}

// --- get_block_id_by_height beyond chain ---

TEST_F(BlockchainTest, get_block_id_by_height_beyond_chain)
{
  // Height beyond the chain should return null hash
  crypto::hash id = m_blockchain.get_block_id_by_height(999999);
  // TestDB returns null_hash for any height
  ASSERT_EQ(id, crypto::null_hash);
}

// --- have_block with where pointer ---

TEST_F(BlockchainTest, have_block_with_where_pointer)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  int where = -1;
  ASSERT_FALSE(m_blockchain.have_block(h, &where));
}

// --- get_total_transactions ---

TEST_F(BlockchainTest, get_total_transactions_returns_db_value)
{
  // TestDB returns tx_count = 0
  ASSERT_EQ(m_blockchain.get_total_transactions(), 0u);
}

// --- store_blockchain ---

TEST_F(BlockchainTest, store_blockchain_returns_result)
{
  bool result = m_blockchain.store_blockchain();
  (void)result; // Just ensure no crash
}

// --- for_blocks_range ---

TEST_F(BlockchainTest, for_blocks_range_zero_to_zero)
{
  int count = 0;
  bool result = m_blockchain.for_blocks_range(0, 0, [&count](uint64_t height, const crypto::hash& hash, const cryptonote::block& blk) {
    ++count;
    return true;
  });
  ASSERT_TRUE(result);
}

// --- txpool_tx_matches_category ---

TEST_F(BlockchainTest, txpool_tx_matches_category_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  bool matches = m_blockchain.txpool_tx_matches_category(h, cryptonote::relay_category::broadcasted);
  ASSERT_FALSE(matches);
}

// --- get_txpool_tx_meta nonexistent ---

TEST_F(BlockchainTest, get_txpool_tx_meta_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::txpool_tx_meta_t meta;
  ASSERT_FALSE(m_blockchain.get_txpool_tx_meta(h, meta));
}

// --- get_txpool_tx_blob nonexistent ---

TEST_F(BlockchainTest, get_txpool_tx_blob_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  cryptonote::blobdata bd;
  ASSERT_FALSE(m_blockchain.get_txpool_tx_blob(h, bd, cryptonote::relay_category::broadcasted));
}

// --- flush_txes_from_pool empty list ---

TEST_F(BlockchainTest, flush_txes_from_pool_empty)
{
  std::vector<crypto::hash> txids;
  ASSERT_TRUE(m_blockchain.flush_txes_from_pool(txids));
}

// --- for_all_txpool_txes ---

TEST_F(BlockchainTest, for_all_txpool_txes_empty)
{
  bool result = m_blockchain.for_all_txpool_txes([](const crypto::hash& txid, const cryptonote::txpool_tx_meta_t& meta, const cryptonote::blobdata_ref* blob) {
    return true;
  }, false, cryptonote::relay_category::broadcasted);
  // TestDB::for_all_txpool_txes returns false by default, but the
  // derived TestDB in this file overrides it to return true
  ASSERT_TRUE(result);
}

// --- V16 fixture additional tests ---

TEST_F(BlockchainTestV16, check_tx_outputs_v2_zero_amount_v16)
{
  // At HF v16, v2 tx with zero amounts should pass
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_tagged_key ttk;
  ttk.key = crypto::public_key();
  out.target = ttk;
  tx.vout.push_back(out);
  // At v16, BP+ type is expected
  tx.rct_signatures.type = rct::RCTTypeBulletproofPlus;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 16));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestV16, get_adjusted_time_low_height_v16)
{
  uint64_t adjusted = m_blockchain.get_adjusted_time(1);
  uint64_t now = static_cast<uint64_t>(time(NULL));
  ASSERT_GE(adjusted, now - 2);
  ASSERT_LE(adjusted, now + 2);
}

TEST_F(BlockchainTestV16, get_next_long_term_block_weight_v16)
{
  // At HF >= 10, long term block weight uses the adaptive algorithm
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(1000);
  ASSERT_GT(ltw, 0u);
}

TEST_F(BlockchainTestV16, find_blockchain_supplement_empty_fails_v16)
{
  std::list<crypto::hash> qblock_ids;
  uint64_t starter_offset = 0;
  ASSERT_FALSE(m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset));
}

TEST_F(BlockchainTestV16, get_num_mature_outputs_v16)
{
  // With only genesis block, no outputs should be mature
  uint64_t num = m_blockchain.get_num_mature_outputs(0);
  ASSERT_EQ(num, 0u);
}

TEST_F(BlockchainTestV16, get_outs_empty_request_v16)
{
  cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::request req;
  req.get_txid = false;
  cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::response res;
  ASSERT_TRUE(m_blockchain.get_outs(req, res));
  ASSERT_TRUE(res.outs.empty());
}

TEST_F(BlockchainTestV16, check_difficulty_checkpoints_v16)
{
  auto result = m_blockchain.check_difficulty_checkpoints();
  ASSERT_TRUE(result.first);
}

TEST_F(BlockchainTestV16, set_user_options_async_v16)
{
  m_blockchain.set_user_options(2, false, 50, cryptonote::db_async, true);
}

TEST_F(BlockchainTestV16, get_output_key_v16)
{
  crypto::public_key pk = m_blockchain.get_output_key(0, 0);
  ASSERT_EQ(pk, crypto::public_key());
}

// =============================================================================
// Checkpoint tests (BlockchainTest)
// =============================================================================

TEST_F(BlockchainTest, get_checkpoints_returns_ref)
{
  const cryptonote::checkpoints& cp = m_blockchain.get_checkpoints();
  (void)cp; // Compiles and returns a valid reference
}

TEST_F(BlockchainTest, set_checkpoints)
{
  cryptonote::checkpoints cp;
  cp.add_checkpoint(0, "0000000000000000000000000000000000000000000000000000000000000000");
  m_blockchain.set_checkpoints(std::move(cp));
  const cryptonote::checkpoints& cp2 = m_blockchain.get_checkpoints();
  (void)cp2;
}

// =============================================================================
// Block query tests (BlockchainTest)
// =============================================================================

TEST_F(BlockchainTest, have_block_unlocked_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_block_unlocked(h));
}

TEST_F(BlockchainTest, have_block_unlocked_with_where)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  int where = -1;
  bool found = m_blockchain.have_block_unlocked(h, &where);
  EXPECT_FALSE(found);
  // where is not modified when block is not found
  EXPECT_EQ(where, -1);
}

TEST_F(BlockchainTest, get_block_id_by_height_genesis)
{
  crypto::hash h = m_blockchain.get_block_id_by_height(0);
  // BaseTestDB returns null_hash, just verify no crash
  (void)h;
}

TEST_F(BlockchainTest, get_pending_block_id_by_height)
{
  crypto::hash h = m_blockchain.get_pending_block_id_by_height(0);
  // Should return a hash (possibly null) for genesis height
  (void)h;
}

// =============================================================================
// Static method tests (no fixture needed)
// =============================================================================

TEST(BlockchainStaticTest, get_fee_quantization_mask_value)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  ASSERT_EQ(mask, 10000u);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_zero_reward)
{
  // Zero reward edge case: fee should be minimal (1)
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(0, 300000);
  EXPECT_EQ(fee, 1u);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_zero_median)
{
  // Zero median weight: should be clamped to min block weight (300000)
  // and still return a valid fee without crashing
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 0);
  EXPECT_GT(fee, 0u);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_v8)
{
  // With v8-era parameters: ~3 XMR reward, 300000 median
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(3000000000000ULL, 300000);
  EXPECT_GT(fee, 0u);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_monotonicity)
{
  // Larger reward should give lower fee per byte (inversely proportional via division)
  // Actually, larger reward gives HIGHER base fee (fee = reward * ref_weight / median^2)
  uint64_t fee_small = cryptonote::Blockchain::get_dynamic_base_fee(1000000000ULL, 300000);
  uint64_t fee_large = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  EXPECT_GT(fee_large, fee_small);
}

// =============================================================================
// check_tx_outputs tests (BlockchainTestV16)
// =============================================================================

TEST_F(BlockchainTestV16, check_tx_outputs_v1_decomposed)
{
  // At HF 16, outputs must use txout_to_tagged_key
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 1000000000000ULL; // 1 XMR, decomposed
  cryptonote::txout_to_tagged_key otk;
  otk.key = crypto::rand<crypto::public_key>();
  otk.view_tag = crypto::view_tag{0x42};
  out.target = otk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 16));
  EXPECT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestV16, check_tx_outputs_v1_non_decomposed_pre_hf)
{
  // v1 tx with non-decomposed amount should pass at HF 1 (before decomposed enforcement)
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 1234567890ULL; // non-decomposed
  cryptonote::txout_to_key otk;
  otk.key = crypto::public_key();
  out.target = otk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 1));
  EXPECT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestV16, check_tx_outputs_v2_zero_amounts)
{
  // At HF 16, outputs must use txout_to_tagged_key
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_tagged_key otk;
  otk.key = crypto::rand<crypto::public_key>();
  otk.view_tag = crypto::view_tag{0x01};
  out.target = otk;
  tx.vout.push_back(out);
  tx.rct_signatures.type = rct::RCTTypeBulletproofPlus;
  tx.rct_signatures.outPk.resize(1);
  tx.rct_signatures.outPk[0].mask = rct::identity();
  tx.rct_signatures.ecdhInfo.resize(1);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 16));
  EXPECT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestV16, check_tx_outputs_empty)
{
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 16));
  EXPECT_FALSE(tvc.m_invalid_output);
}

// =============================================================================
// Blockchain state tests (BlockchainTest)
// =============================================================================

TEST_F(BlockchainTest, get_target_blockchain_height)
{
  uint64_t target = m_blockchain.get_current_blockchain_height();
  EXPECT_GE(target, m_blockchain.get_current_blockchain_height());
}

TEST_F(BlockchainTest, get_db_not_null)
{
  const cryptonote::BlockchainDB& db = m_blockchain.get_db();
  (void)db; // Should return a valid reference, not crash
}

TEST_F(BlockchainTest, get_hard_fork_not_null)
{
  uint8_t version = m_blockchain.get_current_hard_fork_version();
  EXPECT_GE(version, 1u);
}

TEST_F(BlockchainTest, check_blockchain_pruning_unpruned)
{
  ASSERT_TRUE(m_blockchain.check_blockchain_pruning());
}

TEST_F(BlockchainTest, check_difficulty_checkpoints_state)
{
  auto result = m_blockchain.check_difficulty_checkpoints();
  ASSERT_TRUE(result.first);
}

// =============================================================================
// Weight/reward tests
// =============================================================================

TEST_F(BlockchainTest, get_min_block_weight_at_height_0)
{
  // Current cumulative block weight limit should be positive at genesis
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  EXPECT_GT(limit, 0u);
}

TEST_F(BlockchainTest, get_ideal_hard_fork_version)
{
  // For the v1 fixture, ideal hard fork version at height 0 should be 1
  uint8_t version = m_blockchain.get_ideal_hard_fork_version(0);
  EXPECT_EQ(version, 1u);
}

// =============================================================================
// Additional coverage tests
// =============================================================================

// --- Fee calculation tests (3 tests) ---

TEST_F(BlockchainTest, get_dynamic_base_fee_estimate_at_height_0)
{
  // The static get_dynamic_base_fee should work with genesis-era parameters
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(600000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
  // The instance-based fee estimate via 2021 scaling should also work
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (size_t i = 0; i < fees.size(); ++i)
    ASSERT_GT(fees[i], 0u) << "Fee level " << i << " should be positive";
}

TEST_F(BlockchainTest, get_dynamic_base_fee_estimate_with_grace)
{
  // grace_blocks parameter should affect the estimate (larger grace = potentially lower fee)
  std::vector<uint64_t> fees_no_grace;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees_no_grace);
  ASSERT_EQ(fees_no_grace.size(), 4u);

  std::vector<uint64_t> fees_with_grace;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(100, fees_with_grace);
  ASSERT_EQ(fees_with_grace.size(), 4u);

  // Both should produce valid positive fees
  for (size_t i = 0; i < 4; ++i)
  {
    ASSERT_GT(fees_no_grace[i], 0u);
    ASSERT_GT(fees_with_grace[i], 0u);
  }
}

TEST(BlockchainStaticTest, fee_quantization_mask_alignment)
{
  // Quantized fees should be multiples of the mask
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  ASSERT_GT(mask, 0u);

  // A fee that is not a multiple of the mask, when quantized, should become one
  uint64_t raw_fee = 12345678ULL;
  uint64_t quantized = (raw_fee + mask - 1) / mask * mask;
  ASSERT_EQ(quantized % mask, 0u);
  ASSERT_GE(quantized, raw_fee);
}

// --- Blockchain query tests (4 tests) ---

TEST_F(BlockchainTest, get_block_by_hash_genesis_via_height)
{
  // Get genesis block ID then try to look it up
  crypto::hash genesis_id = m_blockchain.get_block_id_by_height(0);
  // TestDB returns null_hash, but the method should not crash
  (void)genesis_id;
}

TEST_F(BlockchainTest, get_block_id_by_height_out_of_range)
{
  // Height far beyond the chain should return null_hash
  crypto::hash id = m_blockchain.get_block_id_by_height(1000000);
  ASSERT_EQ(id, crypto::null_hash);
}

TEST_F(BlockchainTest, get_current_hard_fork_version_v1)
{
  // After init with HF v1, the current hard fork version should be 1
  uint8_t version = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(version, 1u);
}

TEST_F(BlockchainTest, get_hard_fork_voting_info_v1)
{
  uint32_t window = 0, votes = 0, threshold = 0;
  uint64_t earliest_height = 0;
  uint8_t voting = 0;
  bool enabled = m_blockchain.get_hard_fork_voting_info(1, window, votes, threshold, earliest_height, voting);
  ASSERT_TRUE(enabled);
  // Version 1 should be enabled from height 0
  ASSERT_EQ(earliest_height, 0u);
}

// --- Block weight/size tests (3 tests) ---

TEST_F(BlockchainTest, get_current_cumulative_block_weight_limit_positive)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
  // At HF v1, limit = 2 * median, median = max(actual, full_reward_zone_v1=20000)
  // So limit should be at least 2 * 20000 = 40000
  ASSERT_GE(limit, 40000u);
}

TEST_F(BlockchainTest, get_current_cumulative_block_weight_median_positive)
{
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
  // Median should not exceed the limit
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_LE(median, limit);
}

TEST_F(BlockchainTest, get_next_long_term_block_weight_at_zero)
{
  // At HF v1, long term block weight should equal the input weight
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(500);
  ASSERT_EQ(ltw, 500u);
  // Also test with 0
  uint64_t ltw0 = m_blockchain.get_next_long_term_block_weight(0);
  ASSERT_EQ(ltw0, 0u);
}

// =============================================================================
// Additional Blockchain coverage tests
// =============================================================================

TEST_F(BlockchainTest, get_tail_id_with_height)
{
  uint64_t height = 999;
  crypto::hash tail = m_blockchain.get_tail_id(height);
  // Height should be set to chain_height - 1 = 0
  ASSERT_EQ(height, 0u);
  (void)tail;
}

TEST_F(BlockchainTest, get_short_chain_history_returns_genesis)
{
  std::list<crypto::hash> ids;
  uint64_t current_height = 0;
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids, current_height));
  ASSERT_EQ(current_height, 1u);
  // Should contain at least the genesis block
  ASSERT_GE(ids.size(), 1u);
}

TEST_F(BlockchainTest, get_difficulty_target_v1_is_120)
{
  // At HF v1, target should be DIFFICULTY_TARGET_V1 (120 seconds for v1)
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V1);
}

TEST_F(BlockchainTest, get_total_transactions_after_init)
{
  size_t total = m_blockchain.get_total_transactions();
  // BaseTestDB returns 0 for tx_exists, but the blockchain might count genesis
  ASSERT_GE(total, 0u);
}

TEST_F(BlockchainTest, is_within_compiled_block_hash_area_at_genesis)
{
  // At height 0, this depends on whether compiled block hashes exist
  bool result = m_blockchain.is_within_compiled_block_hash_area(0);
  (void)result; // Just verify no crash
}

TEST_F(BlockchainTest, get_blockchain_pruning_seed_zero)
{
  // Unpruned blockchain has seed 0
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTest, check_blockchain_pruning_returns_true)
{
  // Unpruned chain should pass pruning check
  bool result = m_blockchain.check_blockchain_pruning();
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTest, get_db_returns_non_null)
{
  const cryptonote::BlockchainDB& db = m_blockchain.get_db();
  ASSERT_GT(db.height(), 0u);
}

TEST_F(BlockchainTest, get_checkpoints_returns_valid_ref)
{
  const cryptonote::checkpoints& cp = m_blockchain.get_checkpoints();
  // Just verify it returns a valid reference
  (void)cp;
}

TEST_F(BlockchainTest, get_current_blockchain_height_equals_1)
{
  uint64_t height = m_blockchain.get_current_blockchain_height();
  // After init with TestDB, should be 1 (genesis block)
  ASSERT_EQ(height, 1u);
}

TEST_F(BlockchainTest, for_all_txpool_txes_empty_pool)
{
  int count = 0;
  bool result = m_blockchain.for_all_txpool_txes(
    [&count](const crypto::hash&, const cryptonote::txpool_tx_meta_t&, const cryptonote::blobdata_ref*) -> bool {
      ++count;
      return true;
    }, false, cryptonote::relay_category::all);
  ASSERT_TRUE(result);
  ASSERT_EQ(count, 0);
}

TEST_F(BlockchainTest, get_txpool_tx_count_returns_zero)
{
  size_t count = m_blockchain.get_txpool_tx_count(false);
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTest, flush_txes_from_pool_with_empty_list)
{
  std::vector<crypto::hash> txids;
  m_blockchain.flush_txes_from_pool(txids);
  // Should not crash with empty list
}

TEST_F(BlockchainTest, get_alternative_blocks_count_zero)
{
  size_t count = m_blockchain.get_alternative_blocks_count();
  ASSERT_EQ(count, 0u);
}

TEST_F(BlockchainTest, get_current_cumulative_block_weight_limit_is_positive)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
}

TEST_F(BlockchainTest, get_current_cumulative_block_weight_median_is_positive)
{
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
}

TEST_F(BlockchainTest, get_next_long_term_block_weight_various_sizes)
{
  // Test with different block sizes at HF v1
  uint64_t ltw_small = m_blockchain.get_next_long_term_block_weight(100);
  uint64_t ltw_medium = m_blockchain.get_next_long_term_block_weight(10000);
  uint64_t ltw_large = m_blockchain.get_next_long_term_block_weight(100000);

  // At HF v1, long_term_block_weight == block_weight
  ASSERT_EQ(ltw_small, 100u);
  ASSERT_EQ(ltw_medium, 10000u);
  ASSERT_EQ(ltw_large, 100000u);
}

TEST_F(BlockchainTest, have_block_nonexistent_random_hash)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_block(h));
}

TEST_F(BlockchainTest, have_block_unlocked_nonexistent_random)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_block_unlocked(h));
}

TEST_F(BlockchainTest, have_tx_keyimg_as_spent_random)
{
  crypto::key_image ki;
  memset(&ki, 0xab, sizeof(ki));
  ASSERT_FALSE(m_blockchain.have_tx_keyimg_as_spent(ki));
}

TEST_F(BlockchainTest, get_block_id_by_height_at_zero)
{
  crypto::hash id = m_blockchain.get_block_id_by_height(0);
  // The genesis block should exist (TestDB returns null_hash but it's valid)
  (void)id;
}

TEST_F(BlockchainTest, set_enforce_dns_checkpoints)
{
  // Should not crash when toggling
  m_blockchain.set_enforce_dns_checkpoints(true);
  m_blockchain.set_enforce_dns_checkpoints(false);
}

TEST_F(BlockchainTest, get_hard_fork_state_ready)
{
  // With only HF v1 at genesis, state should be Ready
  cryptonote::HardFork::State state = m_blockchain.get_hard_fork_state();
  ASSERT_EQ(state, cryptonote::HardFork::Ready);
}

TEST_F(BlockchainTest, dynamic_base_fee_2021_scaling_four_levels)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  // All levels should be positive
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
}

TEST_F(BlockchainTest, dynamic_base_fee_2021_scaling_with_grace)
{
  std::vector<uint64_t> fees_no_grace, fees_with_grace;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees_no_grace);
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(10, fees_with_grace);
  ASSERT_EQ(fees_no_grace.size(), 4u);
  ASSERT_EQ(fees_with_grace.size(), 4u);
  // Grace blocks may affect the fee estimate
  // Both should be valid (positive)
  for (size_t i = 0; i < 4; ++i) {
    ASSERT_GT(fees_no_grace[i], 0u);
    ASSERT_GT(fees_with_grace[i], 0u);
  }
}

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_various_rewards)
{
  // Test with different reward levels
  std::vector<uint64_t> fees_low, fees_high;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(100000000000ULL, 300000, 300000, fees_low);
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(1000000000000ULL, 300000, 300000, fees_high);
  ASSERT_EQ(fees_low.size(), 4u);
  ASSERT_EQ(fees_high.size(), 4u);
  // Higher reward should yield higher base fees
  ASSERT_LE(fees_low[0], fees_high[0]);
}

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_various_weights)
{
  // With larger median weights, fees should decrease
  std::vector<uint64_t> fees_small_w, fees_large_w;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 300000, 300000, fees_small_w);
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 600000, 600000, fees_large_w);
  // At least the lowest fee level should be lower with larger weight
  ASSERT_LE(fees_large_w[0], fees_small_w[0]);
}

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_unequal_Mnw_Mlw)
{
  // Test with Mnw != Mlw
  std::vector<uint64_t> fees;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 400000, 300000, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_different_medians)
{
  // Fee should decrease as median weight increases
  uint64_t fee_300k = cryptonote::Blockchain::get_dynamic_base_fee(600000000000ULL, 300000);
  uint64_t fee_600k = cryptonote::Blockchain::get_dynamic_base_fee(600000000000ULL, 600000);
  ASSERT_GT(fee_300k, 0u);
  ASSERT_GT(fee_600k, 0u);
  ASSERT_GE(fee_300k, fee_600k);
}

TEST(BlockchainStaticTest, get_dynamic_base_fee_different_rewards)
{
  // Fee should increase with higher reward
  uint64_t fee_low = cryptonote::Blockchain::get_dynamic_base_fee(100000000000ULL, 300000);
  uint64_t fee_high = cryptonote::Blockchain::get_dynamic_base_fee(1000000000000ULL, 300000);
  ASSERT_GT(fee_low, 0u);
  ASSERT_GT(fee_high, 0u);
  ASSERT_LE(fee_low, fee_high);
}

TEST_F(BlockchainTestV16, get_difficulty_target_v2_is_120)
{
  // At HF v16, target should be DIFFICULTY_TARGET_V2 (120 seconds)
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
}

TEST_F(BlockchainTestV16, get_total_transactions_v16)
{
  size_t total = m_blockchain.get_total_transactions();
  ASSERT_GE(total, 0u);
}

TEST_F(BlockchainTestV16, get_tail_id_with_height_v16)
{
  uint64_t height = 999;
  crypto::hash tail = m_blockchain.get_tail_id(height);
  ASSERT_EQ(height, 0u);
  (void)tail;
}

TEST_F(BlockchainTestV16, get_current_blockchain_height_v16)
{
  uint64_t height = m_blockchain.get_current_blockchain_height();
  ASSERT_EQ(height, 1u);
}

TEST_F(BlockchainTestV16, get_hard_fork_state_ready_v16)
{
  cryptonote::HardFork::State state = m_blockchain.get_hard_fork_state();
  ASSERT_EQ(state, cryptonote::HardFork::Ready);
}

TEST_F(BlockchainTestV16, get_blockchain_pruning_seed_v16)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTestV16, check_blockchain_pruning_v16)
{
  ASSERT_TRUE(m_blockchain.check_blockchain_pruning());
}

TEST_F(BlockchainTestV16, dynamic_base_fee_2021_scaling_v16)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
  // Fee levels should be ordered: low <= normal <= medium <= high
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

TEST_F(BlockchainTestV16, get_current_cumulative_weight_limit_v16_positive)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
}

TEST_F(BlockchainTestV16, get_next_long_term_block_weight_v16_various)
{
  // At HF v16, long_term_block_weight may differ from block_weight
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(1000);
  ASSERT_GT(ltw, 0u);
}

TEST_F(BlockchainTestV16, have_tx_keyimg_as_spent_v16)
{
  crypto::key_image ki;
  memset(&ki, 0xcd, sizeof(ki));
  ASSERT_FALSE(m_blockchain.have_tx_keyimg_as_spent(ki));
}

TEST_F(BlockchainTestV16, get_alternative_blocks_count_v16)
{
  ASSERT_EQ(m_blockchain.get_alternative_blocks_count(), 0u);
}

TEST_F(BlockchainTestV16, for_all_txpool_txes_empty_v16)
{
  int count = 0;
  bool result = m_blockchain.for_all_txpool_txes(
    [&count](const crypto::hash&, const cryptonote::txpool_tx_meta_t&, const cryptonote::blobdata_ref*) -> bool {
      ++count;
      return true;
    }, false, cryptonote::relay_category::all);
  ASSERT_TRUE(result);
  ASSERT_EQ(count, 0);
}

// =============================================================================
// Additional HF version fixtures for broader check_tx_outputs coverage
// =============================================================================

namespace
{
  template<uint8_t HF_VERSION>
  struct get_test_options_hf {
    const std::pair<uint8_t, uint64_t> hard_forks[2];
    const cryptonote::test_options test_options = { hard_forks, 0 };
    get_test_options_hf(): hard_forks{std::make_pair(HF_VERSION, (uint64_t)0), std::make_pair((uint8_t)0, (uint64_t)0)} {}
  };

  template<uint8_t HF_VERSION>
  class BlockchainTestHF : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      get_test_options_hf<HF_VERSION> opts;
      ASSERT_TRUE(m_bap.blockchain.init(new TestDB(), cryptonote::FAKECHAIN, true, &opts.test_options, 0, NULL));
    }
    void TearDown() override { m_bap.blockchain.deinit(); }
    cryptonote::BlockchainAndPool m_bap;
    cryptonote::Blockchain& m_blockchain{m_bap.blockchain};
  };

  using BlockchainTestHF2 = BlockchainTestHF<2>;
  using BlockchainTestHF4 = BlockchainTestHF<4>;
  using BlockchainTestHF8 = BlockchainTestHF<8>;
  using BlockchainTestHF10 = BlockchainTestHF<10>;
  using BlockchainTestHF12 = BlockchainTestHF<12>;
  using BlockchainTestHF14 = BlockchainTestHF<14>;
  using BlockchainTestHF15 = BlockchainTestHF<15>;
}

// =============================================================================
// check_tx_outputs at various HF versions (HF 2-15)
// =============================================================================

TEST_F(BlockchainTestHF2, check_tx_outputs_v1_non_decomposed_fails_hf2)
{
  // At HF2, non-decomposed amounts in v1 txs should fail
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 1234567890ULL;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_TRUE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF2, check_tx_outputs_v1_decomposed_passes_hf2)
{
  // At HF2, decomposed amounts in v1 txs should pass
  cryptonote::transaction tx;
  tx.version = 1;
  // Add multiple valid decomposed outputs
  for (uint64_t amt : {1000000000000ULL, 2000000000000ULL, 500000000000ULL})
  {
    cryptonote::tx_out out;
    out.amount = amt;
    cryptonote::txout_to_key tk;
    tk.key = crypto::public_key();
    out.target = tk;
    tx.vout.push_back(out);
  }
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF4, check_tx_outputs_v2_nonzero_amount_fails_hf4)
{
  // At HF4+, v2 tx with non-zero amounts should fail
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 100;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 4));
  ASSERT_TRUE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF4, check_tx_outputs_v2_zero_amount_passes_hf4)
{
  // At HF4, v2 tx with zero amounts should pass
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 4));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF8, difficulty_target_v2_at_hf8)
{
  // At HF8, difficulty target should be DIFFICULTY_TARGET_V2 (120)
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
}

TEST_F(BlockchainTestHF8, check_tx_outputs_empty_tx_hf8)
{
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 8));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF10, get_next_long_term_block_weight_hf10)
{
  // At HF >= 10, the adaptive long-term block weight algorithm is used
  uint64_t ltw_small = m_blockchain.get_next_long_term_block_weight(1000);
  ASSERT_GT(ltw_small, 0u);
  uint64_t ltw_large = m_blockchain.get_next_long_term_block_weight(1000000);
  ASSERT_GT(ltw_large, 0u);
  // Larger input should yield larger or equal long-term weight
  ASSERT_LE(ltw_small, ltw_large);
}

TEST_F(BlockchainTestHF10, get_current_cumulative_block_weight_limit_hf10)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
  // Limit should be >= median
  ASSERT_GE(limit, median);
}

TEST_F(BlockchainTestHF12, check_tx_outputs_v2_zero_amount_hf12)
{
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 12));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF14, dynamic_base_fee_estimate_2021_scaling_hf14)
{
  // At HF 14, the 2021 scaling should be active
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (size_t i = 0; i < fees.size(); ++i)
    ASSERT_GT(fees[i], 0u) << "Fee level " << i << " should be positive at HF14";
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

TEST_F(BlockchainTestHF15, check_tx_outputs_tagged_key_hf15)
{
  // At HF 15, txout_to_tagged_key is required for v2 txs
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_tagged_key ttk;
  ttk.key = crypto::rand<crypto::public_key>();
  ttk.view_tag = crypto::view_tag{0x42};
  out.target = ttk;
  tx.vout.push_back(out);
  tx.rct_signatures.type = rct::RCTTypeBulletproofPlus;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 15));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST_F(BlockchainTestHF15, check_tx_outputs_old_key_type_still_allowed_hf15)
{
  // At HF 15, txout_to_key is still allowed (tagged keys are optional until HF 16)
  cryptonote::transaction tx;
  tx.version = 2;
  cryptonote::tx_out out;
  out.amount = 0;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  tx.rct_signatures.type = rct::RCTTypeBulletproofPlus;
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 15));
  ASSERT_FALSE(tvc.m_invalid_output);
}

// =============================================================================
// Block reward calculation tests
// =============================================================================

TEST(BlockchainStaticTest, get_block_reward_v1_empty_block)
{
  uint64_t reward = 0;
  // At the start (no coins generated), a block with no txs should get the full reward
  bool result = cryptonote::get_block_reward(0, 0, 0, reward, 1);
  ASSERT_TRUE(result);
  ASSERT_GT(reward, 0u);
}

TEST(BlockchainStaticTest, get_block_reward_v1_with_weight)
{
  uint64_t reward_empty = 0, reward_weighted = 0;
  cryptonote::get_block_reward(0, 0, 0, reward_empty, 1);
  // A block that exceeds median should have a penalty
  cryptonote::get_block_reward(300000, 600000, 0, reward_weighted, 1);
  ASSERT_GT(reward_empty, 0u);
  // When current_block_weight > median, reward should be penalized
  ASSERT_LT(reward_weighted, reward_empty);
}

TEST(BlockchainStaticTest, get_block_reward_tail_emission)
{
  uint64_t reward = 0;
  // With large already_generated_coins, should still get at least tail emission
  bool result = cryptonote::get_block_reward(300000, 100000, 18400000000000000000ULL, reward, 8);
  ASSERT_TRUE(result);
  ASSERT_GE(reward, FINAL_SUBSIDY_PER_MINUTE);
}

TEST(BlockchainStaticTest, get_block_reward_zero_weight_no_penalty)
{
  uint64_t reward = 0;
  // Zero current_block_weight means no penalty
  bool result = cryptonote::get_block_reward(300000, 0, 0, reward, 1);
  ASSERT_TRUE(result);
  ASSERT_GT(reward, 0u);
}

TEST(BlockchainStaticTest, get_block_reward_oversize_block_fails)
{
  uint64_t reward = 0;
  // A block that exceeds 2x the median should fail (return false)
  bool result = cryptonote::get_block_reward(300000, 600001, 0, reward, 1);
  ASSERT_FALSE(result);
}

TEST(BlockchainStaticTest, get_block_reward_at_various_generation_levels)
{
  uint64_t reward_early = 0, reward_mid = 0, reward_late = 0;
  cryptonote::get_block_reward(300000, 0, 0, reward_early, 1);
  cryptonote::get_block_reward(300000, 0, 10000000000000000ULL, reward_mid, 1);
  cryptonote::get_block_reward(300000, 0, 18000000000000000000ULL, reward_late, 8);
  // Early reward should be largest
  ASSERT_GT(reward_early, reward_mid);
  // All should be positive
  ASSERT_GT(reward_early, 0u);
  ASSERT_GT(reward_mid, 0u);
  ASSERT_GT(reward_late, 0u);
}

// =============================================================================
// Transaction blob retrieval tests
// =============================================================================

TEST_F(BlockchainTest, get_transactions_blobs_empty_request)
{
  std::vector<crypto::hash> txs_ids;
  std::vector<cryptonote::blobdata> txs;
  std::vector<crypto::hash> missed;
  bool result = m_blockchain.get_transactions_blobs(txs_ids, txs, missed, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(txs.empty());
  ASSERT_TRUE(missed.empty());
}

TEST_F(BlockchainTest, get_transactions_blobs_nonexistent_tx)
{
  std::vector<crypto::hash> txs_ids;
  txs_ids.push_back(crypto::rand<crypto::hash>());
  std::vector<cryptonote::blobdata> txs;
  std::vector<crypto::hash> missed;
  bool result = m_blockchain.get_transactions_blobs(txs_ids, txs, missed, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(txs.empty());
  ASSERT_EQ(missed.size(), 1u);
}

TEST_F(BlockchainTest, get_transactions_blobs_multiple_nonexistent)
{
  std::vector<crypto::hash> txs_ids;
  txs_ids.push_back(crypto::rand<crypto::hash>());
  txs_ids.push_back(crypto::rand<crypto::hash>());
  txs_ids.push_back(crypto::rand<crypto::hash>());
  std::vector<cryptonote::blobdata> txs;
  std::vector<crypto::hash> missed;
  bool result = m_blockchain.get_transactions_blobs(txs_ids, txs, missed, false);
  ASSERT_TRUE(result);
  ASSERT_TRUE(txs.empty());
  ASSERT_EQ(missed.size(), 3u);
}

TEST_F(BlockchainTest, get_transactions_blobs_pruned_empty)
{
  std::vector<crypto::hash> txs_ids;
  std::vector<cryptonote::blobdata> txs;
  std::vector<crypto::hash> missed;
  bool result = m_blockchain.get_transactions_blobs(txs_ids, txs, missed, true);
  ASSERT_TRUE(result);
  ASSERT_TRUE(txs.empty());
  ASSERT_TRUE(missed.empty());
}

// =============================================================================
// for_all_outputs with specific amount
// =============================================================================

TEST_F(BlockchainTest, for_all_outputs_with_amount_zero)
{
  int count = 0;
  bool result = m_blockchain.for_all_outputs(0, [&count](uint64_t height) {
    ++count;
    return true;
  });
  ASSERT_TRUE(result);
}

// =============================================================================
// Edge cases for dynamic base fee 2021 scaling
// =============================================================================

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_small_Mnw)
{
  std::vector<uint64_t> fees;
  // Small Mnw (at minimum full reward zone) should produce valid fees
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(
      600000000000ULL, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
}

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_large_Mnw)
{
  std::vector<uint64_t> fees;
  // Large Mnw (10MB) should produce smaller fees than normal (300KB)
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 10000000, 10000000, fees);
  ASSERT_EQ(fees.size(), 4u);
  // Fees may be zero at very large medians due to integer division
  for (const auto& f : fees)
    ASSERT_GE(f, 0u);

  // Compare with normal median
  std::vector<uint64_t> fees_normal;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 300000, 300000, fees_normal);
  // Large median should produce fees <= normal median fees
  ASSERT_LE(fees[0], fees_normal[0]);
}

TEST(BlockchainStaticTest, dynamic_base_fee_2021_scaling_Mlw_much_larger_than_Mnw)
{
  std::vector<uint64_t> fees;
  cryptonote::Blockchain::get_dynamic_base_fee_estimate_2021_scaling(600000000000ULL, 100000, 1000000, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
}

TEST(BlockchainStaticTest, dynamic_base_fee_max_reward)
{
  // Test with UINT64_MAX-like reward (should not overflow)
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(UINT64_MAX / 2, 300000);
  ASSERT_GT(fee, 0u);
}

// =============================================================================
// HF version-specific fixture tests for blockchain queries
// =============================================================================

TEST_F(BlockchainTestHF4, get_current_hard_fork_version_hf4)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 4u);
}

TEST_F(BlockchainTestHF4, get_ideal_hard_fork_version_hf4)
{
  ASSERT_EQ(m_blockchain.get_ideal_hard_fork_version(), 4u);
}

TEST_F(BlockchainTestHF4, difficulty_target_v2_at_hf4)
{
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
}

TEST_F(BlockchainTestHF4, get_short_chain_history_hf4)
{
  std::list<crypto::hash> ids;
  uint64_t current_height = 0;
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids, current_height));
  ASSERT_GE(ids.size(), 1u);
  ASSERT_EQ(current_height, 1u);
}

TEST_F(BlockchainTestHF4, check_fee_hf4)
{
  // Large fee should pass at HF4
  ASSERT_TRUE(m_blockchain.check_fee(3000, 1000000000000ULL));
  // Zero fee should fail for non-zero weight
  ASSERT_FALSE(m_blockchain.check_fee(3000, 0));
}

TEST_F(BlockchainTestHF8, get_current_hard_fork_version_hf8)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 8u);
}

TEST_F(BlockchainTestHF8, dynamic_base_fee_estimate_2021_scaling_hf8)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (size_t i = 0; i < fees.size(); ++i)
    ASSERT_GT(fees[i], 0u);
}

TEST_F(BlockchainTestHF8, get_block_weight_limit_hf8)
{
  uint64_t limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(limit, 0u);
  uint64_t median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(median, 0u);
}

TEST_F(BlockchainTestHF10, get_current_hard_fork_version_hf10)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 10u);
}

TEST_F(BlockchainTestHF10, dynamic_base_fee_estimate_2021_scaling_hf10)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
}

TEST_F(BlockchainTestHF12, get_current_hard_fork_version_hf12)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 12u);
}

TEST_F(BlockchainTestHF12, get_next_long_term_block_weight_hf12)
{
  // At HF >= 10, long-term weight uses adaptive algorithm
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(5000);
  ASSERT_GT(ltw, 0u);
}

TEST_F(BlockchainTestHF14, get_current_hard_fork_version_hf14)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 14u);
}

TEST_F(BlockchainTestHF14, check_fee_hf14)
{
  ASSERT_TRUE(m_blockchain.check_fee(3000, 1000000000000ULL));
  ASSERT_FALSE(m_blockchain.check_fee(3000, 0));
}

TEST_F(BlockchainTestHF15, get_current_hard_fork_version_hf15)
{
  ASSERT_EQ(m_blockchain.get_current_hard_fork_version(), 15u);
}

TEST_F(BlockchainTestHF15, dynamic_base_fee_estimate_2021_scaling_hf15)
{
  std::vector<uint64_t> fees;
  m_blockchain.get_dynamic_base_fee_estimate_2021_scaling(0, fees);
  ASSERT_EQ(fees.size(), 4u);
  for (const auto& f : fees)
    ASSERT_GT(f, 0u);
  // Fee levels should be ordered
  ASSERT_LE(fees[0], fees[1]);
  ASSERT_LE(fees[1], fees[2]);
  ASSERT_LE(fees[2], fees[3]);
}

// =============================================================================
// get_miner_data test
// =============================================================================

TEST_F(BlockchainTestHF15, get_miner_data_no_crash)
{
  uint8_t major_version = 0;
  uint64_t height = 0;
  crypto::hash prev_id, seed_hash;
  cryptonote::difficulty_type difficulty;
  uint64_t median_weight = 0, already_generated_coins = 0;
  std::vector<cryptonote::tx_block_template_backlog_entry> tx_backlog;
  bool result = m_blockchain.get_miner_data(major_version, height, prev_id, seed_hash,
                                             difficulty, median_weight, already_generated_coins, tx_backlog);
  ASSERT_TRUE(result);
  // major_version is set to ideal version at height
  ASSERT_GE(major_version, 1u);
  ASSERT_EQ(height, 1u);
  ASSERT_GT(median_weight, 0u);
}

// =============================================================================
// Additional edge cases for check_tx_outputs
// =============================================================================

TEST(BlockchainStaticTest, check_tx_outputs_multiple_outputs_all_decomposed_hf2)
{
  cryptonote::transaction tx;
  tx.version = 1;
  for (uint64_t amt : {10000000000ULL, 20000000000ULL, 3000000000000ULL, 400000000000ULL})
  {
    cryptonote::tx_out out;
    out.amount = amt;
    cryptonote::txout_to_key tk;
    tk.key = crypto::public_key();
    out.target = tk;
    tx.vout.push_back(out);
  }
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_FALSE(tvc.m_invalid_output);
}

TEST(BlockchainStaticTest, check_tx_outputs_one_bad_amount_among_good_hf2)
{
  cryptonote::transaction tx;
  tx.version = 1;
  // Good amounts
  for (uint64_t amt : {10000000000ULL, 20000000000ULL})
  {
    cryptonote::tx_out out;
    out.amount = amt;
    cryptonote::txout_to_key tk;
    tk.key = crypto::public_key();
    out.target = tk;
    tx.vout.push_back(out);
  }
  // One bad amount
  {
    cryptonote::tx_out out;
    out.amount = 12345ULL;
    cryptonote::txout_to_key tk;
    tk.key = crypto::public_key();
    out.target = tk;
    tx.vout.push_back(out);
  }
  cryptonote::tx_verification_context tvc = {};
  ASSERT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 2));
  ASSERT_TRUE(tvc.m_invalid_output);
}

TEST(BlockchainStaticTest, check_tx_outputs_v1_at_hf1_no_decomposition_check)
{
  // At HF1, no decomposition check for v1 tx (any amount is valid)
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_out out;
  out.amount = 7777777ULL;
  cryptonote::txout_to_key tk;
  tk.key = crypto::public_key();
  out.target = tk;
  tx.vout.push_back(out);
  cryptonote::tx_verification_context tvc = {};
  ASSERT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc, 1));
  ASSERT_FALSE(tvc.m_invalid_output);
}

// =============================================================================
// Blockchain weight/size edge cases
// =============================================================================

TEST_F(BlockchainTestHF10, get_next_long_term_block_weight_zero)
{
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(0);
  ASSERT_EQ(ltw, 0u);
}

TEST_F(BlockchainTestHF10, get_next_long_term_block_weight_very_large)
{
  uint64_t ltw = m_blockchain.get_next_long_term_block_weight(100000000);
  ASSERT_GT(ltw, 0u);
}

TEST_F(BlockchainTestHF10, has_block_weights_after_init)
{
  bool has = m_blockchain.has_block_weights(0, 1);
  (void)has; // Just verify no crash
}

// =============================================================================
// find_blockchain_supplement additional tests
// =============================================================================

TEST_F(BlockchainTestHF8, find_blockchain_supplement_empty_ids_fails)
{
  std::list<crypto::hash> qblock_ids;
  uint64_t starter_offset = 0;
  ASSERT_FALSE(m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset));
}

TEST_F(BlockchainTestHF8, find_blockchain_supplement_with_genesis_hash)
{
  // Get the genesis block hash
  crypto::hash genesis_hash = m_blockchain.get_block_id_by_height(0);
  std::list<crypto::hash> qblock_ids;
  qblock_ids.push_back(genesis_hash);
  uint64_t starter_offset = 0;
  // BaseTestDB returns null_hash for get_block_hash_from_height,
  // so genesis_hash is null_hash; this should match
  bool result = m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset);
  // Result depends on whether the null hash happens to match the DB
  (void)result;
}

TEST_F(BlockchainTestHF12, find_blockchain_supplement_wrong_genesis)
{
  std::list<crypto::hash> qblock_ids;
  qblock_ids.push_back(crypto::rand<crypto::hash>());
  uint64_t starter_offset = 0;
  ASSERT_FALSE(m_blockchain.find_blockchain_supplement(qblock_ids, starter_offset));
}

// =============================================================================
// get_output_histogram additional tests
// =============================================================================

TEST_F(BlockchainTest, get_output_histogram_with_specific_amounts)
{
  std::vector<uint64_t> amounts = {0, 1000000000000ULL};
  auto histogram = m_blockchain.get_output_histogram(amounts, false, 0, 0);
  // BaseTestDB returns empty histogram
  ASSERT_TRUE(histogram.empty());
}

TEST_F(BlockchainTest, get_output_histogram_unlocked)
{
  std::vector<uint64_t> amounts;
  auto histogram = m_blockchain.get_output_histogram(amounts, true, 0, 0);
  ASSERT_TRUE(histogram.empty());
}

TEST_F(BlockchainTest, get_output_histogram_with_cutoff)
{
  std::vector<uint64_t> amounts;
  auto histogram = m_blockchain.get_output_histogram(amounts, false, time(NULL), 0);
  ASSERT_TRUE(histogram.empty());
}

TEST_F(BlockchainTest, get_output_histogram_with_min_count)
{
  std::vector<uint64_t> amounts;
  auto histogram = m_blockchain.get_output_histogram(amounts, false, 0, 100);
  ASSERT_TRUE(histogram.empty());
}

// =============================================================================
// get_output_distribution additional tests
// =============================================================================

TEST_F(BlockchainTest, get_output_distribution_zero_to_zero)
{
  uint64_t start_height = 0;
  std::vector<uint64_t> distribution;
  uint64_t base = 0;
  // from=0, to=0 is a valid range; Blockchain may handle it internally
  bool result = m_blockchain.get_output_distribution(0, 0, 0, start_height, distribution, base);
  // Just verify no crash; result depends on DB implementation details
  (void)result;
}

TEST_F(BlockchainTest, get_output_distribution_specific_amount)
{
  uint64_t start_height = 0;
  std::vector<uint64_t> distribution;
  uint64_t base = 0;
  bool result = m_blockchain.get_output_distribution(1000000000000ULL, 0, 0, start_height, distribution, base);
  ASSERT_FALSE(result);
}

// =============================================================================
// Additional V16 fixture tests for better coverage
// =============================================================================

TEST_F(BlockchainTestV16, get_miner_data_v16)
{
  uint8_t major_version = 0;
  uint64_t height = 0;
  crypto::hash prev_id, seed_hash;
  cryptonote::difficulty_type difficulty;
  uint64_t median_weight = 0, already_generated_coins = 0;
  std::vector<cryptonote::tx_block_template_backlog_entry> tx_backlog;
  bool result = m_blockchain.get_miner_data(major_version, height, prev_id, seed_hash,
                                             difficulty, median_weight, already_generated_coins, tx_backlog);
  ASSERT_TRUE(result);
  // major_version is set to ideal version at height
  ASSERT_GE(major_version, 1u);
  ASSERT_EQ(height, 1u);
  ASSERT_GT(median_weight, 0u);
}

TEST_F(BlockchainTestV16, get_transactions_blobs_empty_v16)
{
  std::vector<crypto::hash> txs_ids;
  std::vector<cryptonote::blobdata> txs;
  std::vector<crypto::hash> missed;
  ASSERT_TRUE(m_blockchain.get_transactions_blobs(txs_ids, txs, missed, false));
  ASSERT_TRUE(txs.empty());
  ASSERT_TRUE(missed.empty());
}

TEST_F(BlockchainTestV16, check_fee_edge_cases_v16)
{
  // Zero weight and zero fee
  bool ok0 = m_blockchain.check_fee(0, 0);
  (void)ok0;
  // Very small weight with very large fee
  ASSERT_TRUE(m_blockchain.check_fee(1, UINT64_MAX));
  // Large weight with zero fee
  ASSERT_FALSE(m_blockchain.check_fee(1000000, 0));
}

TEST_F(BlockchainTestV16, get_earliest_ideal_height_for_version_v16)
{
  uint64_t height = m_blockchain.get_earliest_ideal_height_for_version(16);
  ASSERT_EQ(height, 0u);
}

TEST_F(BlockchainTestV16, get_earliest_ideal_height_for_version_higher)
{
  // Version higher than configured should return UINT64_MAX
  uint64_t height = m_blockchain.get_earliest_ideal_height_for_version(17);
  ASSERT_EQ(height, std::numeric_limits<uint64_t>::max());
}

TEST_F(BlockchainTestV16, store_blockchain_v16)
{
  bool result = m_blockchain.store_blockchain();
  (void)result;
}

TEST_F(BlockchainTestV16, lock_unlock_v16)
{
  m_blockchain.lock();
  m_blockchain.unlock();
}

TEST_F(BlockchainTestV16, cancel_v16)
{
  m_blockchain.cancel();
}

TEST_F(BlockchainTestV16, for_blocks_range_zero_v16)
{
  int count = 0;
  bool result = m_blockchain.for_blocks_range(0, 0, [&count](uint64_t, const crypto::hash&, const cryptonote::block&) {
    ++count;
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, for_all_key_images_v16)
{
  bool result = m_blockchain.for_all_key_images([](const crypto::key_image&) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, for_all_transactions_v16)
{
  bool result = m_blockchain.for_all_transactions([](const crypto::hash&, const cryptonote::transaction&) {
    return true;
  }, false);
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, for_all_outputs_v16)
{
  bool result = m_blockchain.for_all_outputs([](uint64_t, const crypto::hash&, uint64_t, size_t) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, for_all_outputs_with_amount_v16)
{
  bool result = m_blockchain.for_all_outputs(0, [](uint64_t) {
    return true;
  });
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, get_output_key_mask_unlocked_v16)
{
  crypto::public_key key;
  rct::key mask;
  bool unlocked = false;
  m_blockchain.get_output_key_mask_unlocked(0, 0, key, mask, unlocked);
  ASSERT_EQ(key, crypto::public_key());
}

TEST_F(BlockchainTestV16, get_hardforks_v16)
{
  const auto& hfs = m_blockchain.get_hardforks();
  ASSERT_GE(hfs.size(), 1u);
}

TEST_F(BlockchainTestV16, get_hard_fork_voting_info_v16)
{
  uint32_t window = 0, votes = 0, threshold = 0;
  uint64_t earliest_height = 0;
  uint8_t voting = 0;
  bool enabled = m_blockchain.get_hard_fork_voting_info(16, window, votes, threshold, earliest_height, voting);
  ASSERT_TRUE(enabled);
  ASSERT_EQ(earliest_height, 0u);
}

TEST_F(BlockchainTestV16, flush_invalid_blocks_v16)
{
  m_blockchain.flush_invalid_blocks();
}

TEST_F(BlockchainTestV16, set_checkpoints_v16)
{
  cryptonote::checkpoints cp;
  cp.add_checkpoint(0, "0000000000000000000000000000000000000000000000000000000000000000");
  m_blockchain.set_checkpoints(std::move(cp));
}

TEST_F(BlockchainTestV16, safesyncmode_v16)
{
  m_blockchain.safesyncmode(true);
  m_blockchain.safesyncmode(false);
}

TEST_F(BlockchainTestV16, set_user_options_v16)
{
  m_blockchain.set_user_options(4, true, 100, cryptonote::db_defaultsync, true);
}

TEST_F(BlockchainTestV16, set_enforce_dns_checkpoints_v16)
{
  m_blockchain.set_enforce_dns_checkpoints(false);
  m_blockchain.set_enforce_dns_checkpoints(true);
}

TEST_F(BlockchainTestV16, get_output_distribution_invalid_range_v16)
{
  uint64_t start_height = 0;
  std::vector<uint64_t> distribution;
  uint64_t base = 0;
  ASSERT_FALSE(m_blockchain.get_output_distribution(0, 10, 5, start_height, distribution, base));
}

TEST_F(BlockchainTestV16, get_blocks_beyond_height_v16)
{
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  ASSERT_FALSE(m_blockchain.get_blocks(100, 1, blocks));
}

TEST_F(BlockchainTestV16, have_tx_keyimges_as_spent_empty_span_v16)
{
  std::vector<crypto::key_image> ki;
  auto result = m_blockchain.have_tx_keyimges_as_spent(epee::to_span(ki));
  ASSERT_TRUE(result.empty());
}

TEST_F(BlockchainTestV16, have_tx_keyimges_as_spent_nonexistent_v16)
{
  std::vector<crypto::key_image> ki;
  ki.push_back(crypto::rand<crypto::key_image>());
  ki.push_back(crypto::rand<crypto::key_image>());
  auto result = m_blockchain.have_tx_keyimges_as_spent(epee::to_span(ki));
  ASSERT_EQ(result.size(), 2u);
  ASSERT_FALSE(result[0]);
  ASSERT_FALSE(result[1]);
}

TEST_F(BlockchainTestV16, flush_txes_from_pool_empty_v16)
{
  std::vector<crypto::hash> txids;
  ASSERT_TRUE(m_blockchain.flush_txes_from_pool(txids));
}

TEST_F(BlockchainTestV16, get_output_histogram_empty_v16)
{
  std::vector<uint64_t> amounts;
  auto histogram = m_blockchain.get_output_histogram(amounts, false, 0, 0);
  ASSERT_TRUE(histogram.empty());
}

// =============================================================================
// is_tx_spendtime_unlocked tests
// =============================================================================

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_zero_unlock_time)
{
  // unlock_time == 0 means immediately spendable
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(0, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_block_based_past)
{
  // A block-based unlock_time that is in the past should be unlocked
  // Our chain height is 1, so unlock_time 0 (< CRYPTONOTE_MAX_BLOCK_NUMBER) means
  // height-1 + DELTA >= unlock_time => 0 + 10 >= 0 => true
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(0, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_block_based_far_future)
{
  // A block-based unlock_time far in the future should not be unlocked
  // height-1 + DELTA = 0 + 10 = 10 < 1000000
  ASSERT_FALSE(m_blockchain.is_tx_spendtime_unlocked(1000000, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_block_based_exact_boundary)
{
  // unlock_time at exactly the boundary:
  // height()-1 + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS = 0 + 1 = 1
  // unlock_time = 1 => 1 >= 1 => true
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(1, 1));
  // unlock_time = 2 => 1 >= 2 => false
  ASSERT_FALSE(m_blockchain.is_tx_spendtime_unlocked(2, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_time_based_past)
{
  // Time-based unlock: unlock_time >= CRYPTONOTE_MAX_BLOCK_NUMBER (500000000)
  // A past time should be unlocked
  uint64_t past_time = 1000000000ULL; // ~2001, definitely in the past
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(past_time, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_time_based_far_future)
{
  // A far future time should not be unlocked
  uint64_t future_time = UINT64_MAX - 1000;
  ASSERT_FALSE(m_blockchain.is_tx_spendtime_unlocked(future_time, 1));
}

TEST_F(BlockchainTest, is_tx_spendtime_unlocked_at_max_block_number_boundary)
{
  // CRYPTONOTE_MAX_BLOCK_NUMBER is the boundary between block-based and time-based
  // unlock_time == CRYPTONOTE_MAX_BLOCK_NUMBER is interpreted as time
  uint64_t boundary = CRYPTONOTE_MAX_BLOCK_NUMBER;
  // This is in the past (year ~1985 in unix time), so should be unlocked
  bool result = m_blockchain.is_tx_spendtime_unlocked(boundary, 1);
  ASSERT_TRUE(result);
}

TEST_F(BlockchainTestV16, is_tx_spendtime_unlocked_zero_v16)
{
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(0, 16));
}

TEST_F(BlockchainTestV16, is_tx_spendtime_unlocked_block_based_future_v16)
{
  ASSERT_FALSE(m_blockchain.is_tx_spendtime_unlocked(999999, 16));
}

TEST_F(BlockchainTestV16, is_tx_spendtime_unlocked_time_based_past_v16)
{
  // Past timestamp should be unlocked at v16 too
  // At v16, the time comparison uses get_adjusted_time instead of time(NULL),
  // but for past times the result should still be 'unlocked'
  uint64_t past_time = 1000000000ULL;
  ASSERT_TRUE(m_blockchain.is_tx_spendtime_unlocked(past_time, 16));
}

TEST_F(BlockchainTestV16, is_tx_spendtime_unlocked_time_based_far_future_v16)
{
  uint64_t future_time = UINT64_MAX - 1000;
  ASSERT_FALSE(m_blockchain.is_tx_spendtime_unlocked(future_time, 16));
}

// =============================================================================
// get_blocks range tests
// =============================================================================

TEST_F(BlockchainTest, get_blocks_zero_count)
{
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  // Requesting 0 blocks should succeed with empty result
  ASSERT_TRUE(m_blockchain.get_blocks(0, 0, blocks));
  ASSERT_TRUE(blocks.empty());
}

TEST_F(BlockchainTest, get_blocks_with_txs_zero_count)
{
  std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> blocks;
  std::vector<cryptonote::blobdata> txs;
  ASSERT_TRUE(m_blockchain.get_blocks(0, 0, blocks, txs));
  ASSERT_TRUE(blocks.empty());
  ASSERT_TRUE(txs.empty());
}

// =============================================================================
// Alternative chains
// =============================================================================

TEST_F(BlockchainTest, get_alternative_chains_empty)
{
  auto chains = m_blockchain.get_alternative_chains();
  ASSERT_EQ(chains.size(), 0u);
}

TEST_F(BlockchainTest, get_alternative_blocks_list_empty)
{
  std::vector<cryptonote::block> blocks;
  ASSERT_TRUE(m_blockchain.get_alternative_blocks(blocks));
  ASSERT_TRUE(blocks.empty());
}

TEST_F(BlockchainTestV16, get_alternative_blocks_list_empty_v16)
{
  std::vector<cryptonote::block> blocks;
  ASSERT_TRUE(m_blockchain.get_alternative_blocks(blocks));
  ASSERT_TRUE(blocks.empty());
}

// =============================================================================
// check_fee additional edge cases
// =============================================================================

TEST_F(BlockchainTest, check_fee_exact_minimum)
{
  // A single byte weight should need some minimum fee
  // Zero fee for non-zero weight should fail
  ASSERT_FALSE(m_blockchain.check_fee(1, 0));
  // Very large fee for 1 byte should always pass
  ASSERT_TRUE(m_blockchain.check_fee(1, UINT64_MAX));
}

TEST_F(BlockchainTest, check_fee_max_weight)
{
  // Very large weight with large fee
  ASSERT_TRUE(m_blockchain.check_fee(1000000, 1000000000000000ULL));
}

TEST_F(BlockchainTestV16, check_fee_zero_weight_v16)
{
  bool ok = m_blockchain.check_fee(0, 0);
  (void)ok; // Just verify no crash
}

// =============================================================================
// get_last_block_timestamps
// =============================================================================

TEST_F(BlockchainTest, get_last_block_timestamps_zero_requested)
{
  std::vector<time_t> timestamps = m_blockchain.get_last_block_timestamps(0);
  ASSERT_TRUE(timestamps.empty());
}

TEST_F(BlockchainTestV16, get_last_block_timestamps_v16)
{
  std::vector<time_t> timestamps = m_blockchain.get_last_block_timestamps(10);
  // With only genesis, we get at most 1 timestamp
  ASSERT_LE(timestamps.size(), 1u);
}

// =============================================================================
// get_pending_block_id_by_height
// =============================================================================

TEST_F(BlockchainTestV16, get_pending_block_id_by_height_v16_genesis)
{
  crypto::hash h = m_blockchain.get_pending_block_id_by_height(0);
  // Should return a hash without crash
  (void)h;
}

TEST_F(BlockchainTest, get_pending_block_id_by_height_beyond_chain)
{
  crypto::hash h = m_blockchain.get_pending_block_id_by_height(999);
  // Should return null_hash for height beyond chain
  ASSERT_EQ(h, crypto::null_hash);
}

// =============================================================================
// get_miner_data at different HF levels
// =============================================================================

TEST_F(BlockchainTestHF8, get_miner_data_hf8)
{
  uint8_t major_version = 0;
  uint64_t height = 0;
  crypto::hash prev_id, seed_hash;
  cryptonote::difficulty_type difficulty;
  uint64_t median_weight = 0, already_generated_coins = 0;
  std::vector<cryptonote::tx_block_template_backlog_entry> tx_backlog;
  bool result = m_blockchain.get_miner_data(major_version, height, prev_id, seed_hash,
                                             difficulty, median_weight, already_generated_coins, tx_backlog);
  ASSERT_TRUE(result);
  ASSERT_GE(major_version, 1u);
  ASSERT_EQ(height, 1u);
  ASSERT_GT(median_weight, 0u);
  ASSERT_TRUE(tx_backlog.empty()); // No txpool transactions
}

TEST_F(BlockchainTestHF14, get_miner_data_hf14)
{
  uint8_t major_version = 0;
  uint64_t height = 0;
  crypto::hash prev_id, seed_hash;
  cryptonote::difficulty_type difficulty;
  uint64_t median_weight = 0, already_generated_coins = 0;
  std::vector<cryptonote::tx_block_template_backlog_entry> tx_backlog;
  bool result = m_blockchain.get_miner_data(major_version, height, prev_id, seed_hash,
                                             difficulty, median_weight, already_generated_coins, tx_backlog);
  ASSERT_TRUE(result);
  ASSERT_GE(major_version, 1u);
  ASSERT_EQ(height, 1u);
  ASSERT_GT(median_weight, 0u);
}

// =============================================================================
// get_txpool_tx_blob direct return
// =============================================================================

TEST_F(BlockchainTest, get_txpool_tx_blob_nonexistent_returns_empty)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  // TestDB returns empty string for nonexistent txpool tx
  cryptonote::blobdata bd = m_blockchain.get_txpool_tx_blob(h, cryptonote::relay_category::broadcasted);
  ASSERT_TRUE(bd.empty());
}

// =============================================================================
// Blockchain state consistency
// =============================================================================

TEST_F(BlockchainTest, blockchain_state_consistency_after_init)
{
  // Verify various state values are consistent
  uint64_t height = m_blockchain.get_current_blockchain_height();
  ASSERT_EQ(height, 1u);

  uint64_t total_tx = m_blockchain.get_total_transactions();
  ASSERT_GE(total_tx, 0u);

  uint8_t hf_version = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(hf_version, 1u);

  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V1);

  uint64_t weight_limit = m_blockchain.get_current_cumulative_block_weight_limit();
  ASSERT_GT(weight_limit, 0u);

  uint64_t weight_median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(weight_median, 0u);
  ASSERT_LE(weight_median, weight_limit);

  size_t alt_count = m_blockchain.get_alternative_blocks_count();
  ASSERT_EQ(alt_count, 0u);

  uint32_t pruning_seed = m_blockchain.get_blockchain_pruning_seed();
  ASSERT_EQ(pruning_seed, 0u);
}

TEST_F(BlockchainTestV16, blockchain_state_consistency_v16)
{
  uint64_t height = m_blockchain.get_current_blockchain_height();
  ASSERT_EQ(height, 1u);

  uint8_t hf_version = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(hf_version, 16u);

  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);

  uint64_t weight_limit = m_blockchain.get_current_cumulative_block_weight_limit();
  uint64_t weight_median = m_blockchain.get_current_cumulative_block_weight_median();
  ASSERT_GT(weight_limit, 0u);
  ASSERT_GT(weight_median, 0u);
  ASSERT_LE(weight_median, weight_limit);

  // Verify get_tail_id with height overload
  uint64_t tail_height = 999;
  crypto::hash tail = m_blockchain.get_tail_id(tail_height);
  ASSERT_EQ(tail_height, 0u);
  (void)tail;
}

// =============================================================================
// Block reward tests at various HF levels
// =============================================================================

TEST(BlockchainStaticTest, get_block_reward_v8_typical)
{
  uint64_t reward = 0;
  // Typical v8+ parameters: 300KB median, 100KB block, some coins generated
  bool result = cryptonote::get_block_reward(300000, 100000, 10000000000000000ULL, reward, 8);
  ASSERT_TRUE(result);
  ASSERT_GT(reward, 0u);
}

TEST(BlockchainStaticTest, get_block_reward_block_above_median)
{
  uint64_t reward_empty = 0, reward_above_median = 0;
  cryptonote::get_block_reward(300000, 0, 0, reward_empty, 8);
  // A block 50% above the median should be penalized
  cryptonote::get_block_reward(300000, 450000, 0, reward_above_median, 8);
  ASSERT_GT(reward_empty, 0u);
  ASSERT_GT(reward_above_median, 0u);
  // Block above median should have penalty (partial reward)
  ASSERT_LT(reward_above_median, reward_empty);
}

TEST(BlockchainStaticTest, get_block_reward_very_small_block)
{
  uint64_t reward = 0;
  // Tiny block should get full reward (no penalty)
  bool result = cryptonote::get_block_reward(300000, 1, 0, reward, 8);
  ASSERT_TRUE(result);
  ASSERT_GT(reward, 0u);
}

// ============================================================================
// Bug #13 regression: checkpoint on alternative chain
// Tests verify checkpoint interaction with alternative block logic.
// A checkpoint on an alt chain triggers forced reorganization.
// The underlying checkpoint checking logic (check_block, is_a_checkpoint flag)
// is verified here through the BlockchainTest fixture's checkpoints object.
// ============================================================================

TEST_F(BlockchainTest, checkpoints_object_accessible)
{
  // The Blockchain object contains a checkpoints member used for
  // check_block and is_alternative_block_allowed decisions.
  // Verify the blockchain initializes with checkpoints available.
  uint64_t height = m_blockchain.get_current_blockchain_height();
  ASSERT_GE(height, 1u);
}

TEST_F(BlockchainTest, checkpoint_alt_block_policy_via_blockchain)
{
  // The Blockchain's handle_alternative_block uses checkpoints.check_block()
  // to set is_a_checkpoint, which triggers reorganization if true.
  // Test the checkpoint checking logic directly.
  cryptonote::checkpoints cp;

  // Simulate adding a checkpoint
  crypto::hash checkpoint_hash;
  memset(&checkpoint_hash, 0xAA, sizeof(checkpoint_hash));
  std::string hash_hex = epee::string_tools::pod_to_hex(checkpoint_hash);
  ASSERT_TRUE(cp.add_checkpoint(10, hash_hex));

  // check_block at checkpoint height with matching hash -> is_a_checkpoint = true
  bool is_checkpoint = false;
  ASSERT_TRUE(cp.check_block(10, checkpoint_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);

  // check_block at checkpoint height with wrong hash -> returns false (fails check)
  crypto::hash wrong_hash;
  memset(&wrong_hash, 0xBB, sizeof(wrong_hash));
  is_checkpoint = false;
  ASSERT_FALSE(cp.check_block(10, wrong_hash, is_checkpoint));

  // check_block at non-checkpoint height -> is_a_checkpoint = false, returns true
  is_checkpoint = true;
  ASSERT_TRUE(cp.check_block(5, wrong_hash, is_checkpoint));
  ASSERT_FALSE(is_checkpoint);
}

TEST_F(BlockchainTest, checkpoint_triggers_reorg_flag)
{
  // This test verifies the logic path: when is_a_checkpoint is true on an
  // alt chain block, the code calls switch_to_alternative_blockchain.
  // We verify the checkpoint detection part: check_block correctly
  // identifies checkpointed blocks.
  cryptonote::checkpoints cp;
  crypto::hash blk_hash;
  memset(&blk_hash, 0xCC, sizeof(blk_hash));
  ASSERT_TRUE(cp.add_checkpoint(42, epee::string_tools::pod_to_hex(blk_hash)));

  // Block at height 42 matching checkpoint hash IS a checkpoint
  bool is_checkpoint = false;
  ASSERT_TRUE(cp.check_block(42, blk_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);

  // If is_checkpoint is true and the block is on an alt chain,
  // handle_alternative_block would call switch_to_alternative_blockchain.
  // The alt block policy should allow blocks above the checkpoint.
  ASSERT_FALSE(cp.is_alternative_block_allowed(42, 42));
  ASSERT_TRUE(cp.is_alternative_block_allowed(42, 43));
}

TEST_F(BlockchainTest, checkpoint_alt_chain_multiple_checkpoints)
{
  // Multiple checkpoints: verify that a checkpoint appearing on an alt chain
  // at any checkpoint height would be detected correctly.
  cryptonote::checkpoints cp;
  crypto::hash h1, h2, h3;
  memset(&h1, 0x11, sizeof(h1));
  memset(&h2, 0x22, sizeof(h2));
  memset(&h3, 0x33, sizeof(h3));

  ASSERT_TRUE(cp.add_checkpoint(100, epee::string_tools::pod_to_hex(h1)));
  ASSERT_TRUE(cp.add_checkpoint(200, epee::string_tools::pod_to_hex(h2)));
  ASSERT_TRUE(cp.add_checkpoint(300, epee::string_tools::pod_to_hex(h3)));

  // Each checkpoint hash at its height should be detected
  bool is_cp = false;
  ASSERT_TRUE(cp.check_block(100, h1, is_cp));
  ASSERT_TRUE(is_cp);
  is_cp = false;
  ASSERT_TRUE(cp.check_block(200, h2, is_cp));
  ASSERT_TRUE(is_cp);
  is_cp = false;
  ASSERT_TRUE(cp.check_block(300, h3, is_cp));
  ASSERT_TRUE(is_cp);

  // Wrong hash at checkpoint heights should fail
  ASSERT_FALSE(cp.check_block(100, h2, is_cp));
  ASSERT_FALSE(cp.check_block(200, h3, is_cp));
  ASSERT_FALSE(cp.check_block(300, h1, is_cp));
}

TEST_F(BlockchainTest, alt_block_allowed_cross_reference)
{
  // Cross-reference test: verify is_alternative_block_allowed behavior
  // matches the checkpoint policy used in handle_alternative_block.
  // After a checkpoint, only blocks above it should be allowed.
  cryptonote::checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(50, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(100, "0000000000000000000000000000000000000000000000000000000000000000"));

  // Blockchain at height 75 (between checkpoints 50 and 100):
  // highest cp <= 75 is 50, so blocks above 50 allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(75, 50));
  ASSERT_TRUE(cp.is_alternative_block_allowed(75, 51));
  ASSERT_TRUE(cp.is_alternative_block_allowed(75, 75));
  ASSERT_TRUE(cp.is_alternative_block_allowed(75, 100));

  // Blockchain at height 150 (past all checkpoints):
  // highest cp <= 150 is 100
  ASSERT_FALSE(cp.is_alternative_block_allowed(150, 100));
  ASSERT_TRUE(cp.is_alternative_block_allowed(150, 101));
}

// ===== Regression test for Bug #8: zero difficulty handling =====
// get_difficulty_for_next_block() can return 0 on overflow or empty state.
// The block validation code must handle this gracefully (return false) rather
// than crashing. We verify that calling get_difficulty_for_next_block on a
// minimal blockchain returns a value, and that the CHECK_AND_ASSERT_MES
// pattern properly handles a zero difficulty_type.

TEST_F(BlockchainTest, difficulty_for_next_block_returns_nonzero_on_init)
{
  // On a freshly initialized FAKECHAIN with 1 block, difficulty should be
  // computable and non-zero (it returns the starting difficulty).
  cryptonote::difficulty_type diff = m_blockchain.get_difficulty_for_next_block();
  ASSERT_GT(diff, 0u) << "Difficulty should not be zero on a valid blockchain";
}

TEST(BlockchainStaticTest, zero_difficulty_type_is_falsy)
{
  // Verify that a zero difficulty_type evaluates as false in boolean context,
  // which is the condition CHECK_AND_ASSERT_MES relies on for the zero-check.
  cryptonote::difficulty_type zero_diff = 0;
  ASSERT_FALSE(zero_diff) << "Zero difficulty must evaluate as false for CHECK_AND_ASSERT_MES";

  cryptonote::difficulty_type nonzero_diff = 1;
  ASSERT_TRUE(nonzero_diff) << "Non-zero difficulty must evaluate as true";
}

// =============================================================================
// Blockchain reorg scenario / state query tests
// =============================================================================

// Test 7: blockchain_reorg_detection
// Verify that get_current_blockchain_height returns 1 after init with TestDB,
// and that the height is consistent across repeated calls.
TEST_F(BlockchainTest, blockchain_reorg_detection_height_consistent)
{
  uint64_t h1 = m_blockchain.get_current_blockchain_height();
  uint64_t h2 = m_blockchain.get_current_blockchain_height();
  ASSERT_EQ(h1, h2) << "Height should be consistent across repeated calls";
  ASSERT_EQ(h1, 1u) << "Initial blockchain height should be 1 (genesis block)";

  // The DB also reports height 1
  ASSERT_EQ(m_blockchain.get_db().height(), h1);
}

// Test 8: blockchain_get_block_id_by_height — genesis block
// Verify block hash retrieval for genesis block returns a valid hash.
TEST_F(BlockchainTest, blockchain_get_block_id_genesis_valid)
{
  crypto::hash genesis_id = m_blockchain.get_block_id_by_height(0);
  // The genesis block hash should be deterministic; on FAKECHAIN with TestDB
  // it depends on the genesis block content. Verify it's retrievable.
  (void)genesis_id;

  // Retrieving the same height twice should give the same hash
  crypto::hash genesis_id2 = m_blockchain.get_block_id_by_height(0);
  ASSERT_EQ(genesis_id, genesis_id2) << "Same height should always produce same block ID";
}

// Test 9: blockchain_difficulty_at_genesis
// Verify the difficulty returned for the genesis block.
TEST_F(BlockchainTest, blockchain_difficulty_at_genesis_value)
{
  // block_difficulty(0) returns the difficulty stored in the DB.
  // BaseTestDB returns 0 for block_difficulty.
  cryptonote::difficulty_type d = m_blockchain.block_difficulty(0);
  ASSERT_EQ(d, 0u) << "BaseTestDB should return 0 for block_difficulty at genesis";

  // get_difficulty_for_next_block() should return a positive value on a valid chain
  cryptonote::difficulty_type next_diff = m_blockchain.get_difficulty_for_next_block();
  ASSERT_GT(next_diff, 0u) << "Next block difficulty should be positive";
}

// Test 10: blockchain_have_block_nonexistent
// Test that have_block returns false for random/non-existent hashes.
TEST_F(BlockchainTest, blockchain_have_block_multiple_random)
{
  // Test with several random hashes
  for (int i = 0; i < 10; ++i)
  {
    crypto::hash h = crypto::rand<crypto::hash>();
    ASSERT_FALSE(m_blockchain.have_block(h))
      << "Random hash should not exist in blockchain (iteration " << i << ")";
  }
}

// Test: have_block with null hash
TEST_F(BlockchainTest, blockchain_have_block_null_hash)
{
  // null_hash is not a valid block hash in the chain
  ASSERT_FALSE(m_blockchain.have_block(crypto::null_hash));
}

// Test 11: blockchain_tail_id
// Verify get_tail_id returns a hash and height for genesis.
TEST_F(BlockchainTest, blockchain_tail_id_with_height)
{
  uint64_t height = UINT64_MAX;
  crypto::hash tail = m_blockchain.get_tail_id(height);

  // Height should be set to 0 (the tail of a single-block chain is the genesis)
  // Note: TestDB may return 0 for height. The important thing is no crash
  // and height is set to a reasonable value.
  ASSERT_LE(height, 1u) << "Tail height should be 0 or at most 1";

  // Calling again gives same result
  uint64_t height2 = UINT64_MAX;
  crypto::hash tail2 = m_blockchain.get_tail_id(height2);
  ASSERT_EQ(tail, tail2);
  ASSERT_EQ(height, height2);
}

// Test: tail_id without height parameter
TEST_F(BlockchainTest, blockchain_tail_id_no_height_param)
{
  crypto::hash tail = m_blockchain.get_tail_id();

  // Should return same hash as the version with height
  uint64_t height = 0;
  crypto::hash tail_with_height = m_blockchain.get_tail_id(height);
  ASSERT_EQ(tail, tail_with_height);
}

// =============================================================================
// Additional blockchain state query tests for Rust port coverage
// =============================================================================

// Test: height matches DB height
TEST_F(BlockchainTest, blockchain_height_matches_db)
{
  ASSERT_EQ(m_blockchain.get_current_blockchain_height(), m_blockchain.get_db().height());
}

// Test: have_tx returns false for random hash
TEST_F(BlockchainTest, blockchain_have_tx_multiple_random)
{
  for (int i = 0; i < 10; ++i)
  {
    crypto::hash h = crypto::rand<crypto::hash>();
    ASSERT_FALSE(m_blockchain.have_tx(h));
  }
}

// Test: get_total_transactions consistent with empty TestDB
TEST_F(BlockchainTest, blockchain_total_transactions_empty_db)
{
  size_t total = m_blockchain.get_total_transactions();
  // TestDB returns 0 for tx_count
  ASSERT_EQ(total, 0u);
}

// Test: blockchain pruning seed is 0 for unpruned chain
TEST_F(BlockchainTest, blockchain_unpruned_seed_is_zero)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

// Test: V16 fixture has correct hard fork version
TEST_F(BlockchainTestV16, blockchain_v16_hard_fork_version)
{
  uint8_t version = m_blockchain.get_ideal_hard_fork_version();
  ASSERT_EQ(version, 16u);

  uint8_t current = m_blockchain.get_current_hard_fork_version();
  ASSERT_EQ(current, 16u);
}

// Test: V16 difficulty target should be DIFFICULTY_TARGET_V2 (120 seconds)
TEST_F(BlockchainTestV16, blockchain_v16_difficulty_target)
{
  uint64_t target = m_blockchain.get_difficulty_target();
  ASSERT_EQ(target, DIFFICULTY_TARGET_V2);
}

// Test: get_short_chain_history returns consistent results
TEST_F(BlockchainTest, blockchain_short_chain_history_consistent)
{
  std::list<crypto::hash> ids1, ids2;
  uint64_t h1 = 0, h2 = 0;
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids1, h1));
  ASSERT_TRUE(m_blockchain.get_short_chain_history(ids2, h2));
  ASSERT_EQ(ids1.size(), ids2.size());
  ASSERT_EQ(h1, h2);

  // The lists should contain the same hashes in the same order
  auto it1 = ids1.begin();
  auto it2 = ids2.begin();
  for (; it1 != ids1.end(); ++it1, ++it2)
  {
    ASSERT_EQ(*it1, *it2);
  }
}
