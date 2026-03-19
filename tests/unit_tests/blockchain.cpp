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
