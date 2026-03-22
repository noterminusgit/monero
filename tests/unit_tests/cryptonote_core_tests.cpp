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
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/account.h"
#include "ringct/rctSigs.h"
#include "cryptonote_config.h"

namespace
{
  cryptonote::account_public_address make_test_address()
  {
    cryptonote::account_public_address addr;
    memset(&addr, 0, sizeof(addr));
    // Generate a valid address by generating keys
    crypto::public_key spend_pkey, view_pkey;
    crypto::secret_key spend_skey, view_skey;
    crypto::generate_keys(spend_pkey, spend_skey);
    crypto::generate_keys(view_pkey, view_skey);
    addr.m_spend_public_key = spend_pkey;
    addr.m_view_public_key = view_pkey;
    return addr;
  }
}

// ---- construct_miner_tx basic tests ----

TEST(CryptonoteCore, ConstructMinerTxV1)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  EXPECT_TRUE(r);
  EXPECT_FALSE(tx.vout.empty());
}

TEST(CryptonoteCore, ConstructMinerTxV2)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(100, 0, UINT64_C(1000000000000), 0, 0, addr, tx, cryptonote::blobdata(), 999, 2);
  EXPECT_TRUE(r);
  EXPECT_FALSE(tx.vout.empty());
}

TEST(CryptonoteCore, ConstructMinerTxV4)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(200, 0, UINT64_C(5000000000000), 0, 0, addr, tx, cryptonote::blobdata(), 999, 4);
  EXPECT_TRUE(r);
}

TEST(CryptonoteCore, ConstructMinerTxV5)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(300, 0, UINT64_C(10000000000000), 0, 0, addr, tx, cryptonote::blobdata(), 999, 5);
  EXPECT_TRUE(r);
}

// ---- construct_miner_tx with extra nonce ----

TEST(CryptonoteCore, ConstructMinerTxWithExtraNonce)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  cryptonote::blobdata extra_nonce = "test_nonce";
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, extra_nonce, 999, 1);
  EXPECT_TRUE(r);
  // The extra field should contain the nonce
  EXPECT_FALSE(tx.extra.empty());
}

TEST(CryptonoteCore, ConstructMinerTxWithEmptyExtraNonce)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  EXPECT_TRUE(r);
}

// ---- construct_miner_tx with fee ----

TEST(CryptonoteCore, ConstructMinerTxWithFee)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(100, 0, UINT64_C(1000000000000), 0, UINT64_C(100000000), addr, tx, cryptonote::blobdata(), 999, 1);
  EXPECT_TRUE(r);
}

// ---- Miner tx has one input ----

TEST(CryptonoteCore, MinerTxHasOneInput)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  ASSERT_TRUE(r);
  EXPECT_EQ(1u, tx.vin.size());
}

// ---- Miner tx input is txin_gen type ----

TEST(CryptonoteCore, MinerTxInputIsTxinGen)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(42, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  ASSERT_TRUE(r);
  ASSERT_EQ(1u, tx.vin.size());
  EXPECT_TRUE(tx.vin[0].type() == typeid(cryptonote::txin_gen));
  const auto& gen_in = boost::get<cryptonote::txin_gen>(tx.vin[0]);
  EXPECT_EQ(42u, gen_in.height);
}

// ---- Max outs parameter ----

TEST(CryptonoteCore, ConstructMinerTxMaxOuts1)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 1, 1);
  EXPECT_TRUE(r);
  EXPECT_LE(tx.vout.size(), 1u);
}

// ---- generate_genesis_block determinism ----

TEST(CryptonoteCore, GenesisBlockDeterministic)
{
  cryptonote::block bl1, bl2;
  bool r1 = cryptonote::generate_genesis_block(bl1, config::GENESIS_TX, config::GENESIS_NONCE);
  bool r2 = cryptonote::generate_genesis_block(bl2, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);

  crypto::hash h1 = cryptonote::get_block_hash(bl1);
  crypto::hash h2 = cryptonote::get_block_hash(bl2);
  EXPECT_EQ(h1, h2);
}

TEST(CryptonoteCore, GenesisBlockNotEmpty)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);
  // Genesis block should have a miner tx
  EXPECT_FALSE(bl.miner_tx.vout.empty());
}

TEST(CryptonoteCore, GenesisBlockTimestampZero)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);
  EXPECT_EQ(0u, bl.timestamp);
}

TEST(CryptonoteCore, GenesisBlockNonce)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);
  EXPECT_EQ(config::GENESIS_NONCE, bl.nonce);
}

TEST(CryptonoteCore, GenesisBlockNoTxHashes)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);
  // Genesis block should not have any transaction hashes (only miner tx)
  EXPECT_TRUE(bl.tx_hashes.empty());
}

TEST(CryptonoteCore, TestnetGenesisBlockDiffers)
{
  cryptonote::block mainnet_bl, testnet_bl;
  bool r1 = cryptonote::generate_genesis_block(mainnet_bl, config::GENESIS_TX, config::GENESIS_NONCE);
  bool r2 = cryptonote::generate_genesis_block(testnet_bl, config::testnet::GENESIS_TX, config::testnet::GENESIS_NONCE);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);

  crypto::hash h1 = cryptonote::get_block_hash(mainnet_bl);
  crypto::hash h2 = cryptonote::get_block_hash(testnet_bl);
  EXPECT_NE(h1, h2);
}

// ---- get_max_tx_size ----

TEST(CryptonoteCore, MaxTxSizeReasonable)
{
  size_t max_size = cryptonote::get_max_tx_size();
  EXPECT_GT(max_size, 0u);
  // Should be at least somewhat large (the actual value is half the block size)
  EXPECT_GE(max_size, 1000u);
}

// ---- get_block_reward ----

TEST(CryptonoteCore, BlockRewardFirstBlock)
{
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(0, 0, 0, reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, BlockRewardDecreasesWithGeneratedCoins)
{
  uint64_t reward1 = 0, reward2 = 0;
  bool r1 = cryptonote::get_block_reward(0, 0, 0, reward1, 1);
  bool r2 = cryptonote::get_block_reward(0, 0, UINT64_C(1000000000000000), reward2, 1);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);
  EXPECT_GT(reward1, reward2);
}

TEST(CryptonoteCore, BlockRewardWithMedianWeight)
{
  uint64_t reward = 0;
  // When current block weight equals median, no penalty
  bool r = cryptonote::get_block_reward(300000, 300000, UINT64_C(1000000000000), reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, BlockRewardPenaltyForLargeBlock)
{
  uint64_t reward_normal = 0, reward_large = 0;
  // Normal size block
  bool r1 = cryptonote::get_block_reward(300000, 100000, UINT64_C(1000000000000), reward_normal, 1);
  // Very large block (should get penalty)
  bool r2 = cryptonote::get_block_reward(300000, 600000, UINT64_C(1000000000000), reward_large, 1);
  ASSERT_TRUE(r1);
  // A block 2x the median gets zero reward
  if (r2)
  {
    EXPECT_LE(reward_large, reward_normal);
  }
}

TEST(CryptonoteCore, BlockRewardZeroForHugeBlock)
{
  uint64_t reward = 0;
  // Block weight = 2 * median: should return false (block too big)
  bool r = cryptonote::get_block_reward(300000, 600001, UINT64_C(1000000000000), reward, 1);
  EXPECT_FALSE(r);
}

TEST(CryptonoteCore, BlockRewardFinalSubsidy)
{
  uint64_t reward = 0;
  // With nearly all coins generated, reward should be the final subsidy
  bool r = cryptonote::get_block_reward(0, 0, MONEY_SUPPLY, reward, 1);
  ASSERT_TRUE(r);
  EXPECT_EQ(FINAL_SUBSIDY_PER_MINUTE, reward);
}

TEST(CryptonoteCore, BlockRewardVersion8)
{
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(300000, 100000, UINT64_C(1000000000000), reward, 8);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

// ---- Transaction weight ----

TEST(CryptonoteCore, TransactionWeightMinerTx)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  ASSERT_TRUE(r);
  uint64_t weight = cryptonote::get_transaction_weight(tx);
  EXPECT_GT(weight, 0u);
}

// ---- get_min_block_weight ----

TEST(CryptonoteCore, MinBlockWeightV1)
{
  size_t min_weight = cryptonote::get_min_block_weight(1);
  EXPECT_GT(min_weight, 0u);
}

TEST(CryptonoteCore, MinBlockWeightV5)
{
  size_t min_weight = cryptonote::get_min_block_weight(5);
  EXPECT_GT(min_weight, 0u);
}

// ---- Miner tx serialization round-trip ----

TEST(CryptonoteCore, MinerTxSerializationRoundTrip)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 0, 0, addr, tx, cryptonote::blobdata(), 999, 1);
  ASSERT_TRUE(r);

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx);
  EXPECT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  EXPECT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));

  crypto::hash h1 = cryptonote::get_transaction_hash(tx);
  crypto::hash h2 = cryptonote::get_transaction_hash(tx2);
  EXPECT_EQ(h1, h2);
}

// ---- get_block_hash ----

TEST(CryptonoteCore, GenesisBlockHashConsistent)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);

  crypto::hash h1, h2;
  EXPECT_TRUE(cryptonote::get_block_hash(bl, h1));
  EXPECT_TRUE(cryptonote::get_block_hash(bl, h2));
  EXPECT_EQ(h1, h2);
}

// ---- Block hashing blob ----

TEST(CryptonoteCore, BlockHashingBlobNotEmpty)
{
  cryptonote::block bl;
  bool r = cryptonote::generate_genesis_block(bl, config::GENESIS_TX, config::GENESIS_NONCE);
  ASSERT_TRUE(r);

  cryptonote::blobdata hashing_blob = cryptonote::get_block_hashing_blob(bl);
  EXPECT_FALSE(hashing_blob.empty());
}

// ---- Various hard fork versions for miner tx ----

TEST(CryptonoteCore, ConstructMinerTxV12)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(500000, 300000, UINT64_C(15000000000000000), 200000, UINT64_C(50000000), addr, tx, cryptonote::blobdata(), 999, 12);
  EXPECT_TRUE(r);
  if (r)
  {
    // V12+ should produce RCT outputs
    EXPECT_FALSE(tx.vout.empty());
  }
}

TEST(CryptonoteCore, ConstructMinerTxV14)
{
  cryptonote::transaction tx;
  auto addr = make_test_address();
  bool r = cryptonote::construct_miner_tx(600000, 300000, UINT64_C(16000000000000000), 200000, UINT64_C(50000000), addr, tx, cryptonote::blobdata(), 999, 14);
  EXPECT_TRUE(r);
  if (r)
  {
    EXPECT_FALSE(tx.vout.empty());
  }
}

// =============================================================================
// Additional coverage tests for cryptonote_core.cpp
// =============================================================================

// --- check_tx_inputs_keyimages_diff (static) tests ---

TEST(CryptonoteCore, CheckTxInputsKeyimagesDiffEmptyTx)
{
  // An empty tx has no inputs, so the method returns false because
  // CHECKED_GET_SPECIFIC_VARIANT fails on non-txin_to_key types.
  // Actually, with no inputs, the loop body never runs, so it returns true.
  cryptonote::transaction tx;
  // We need at least one txin_to_key input for it to do anything useful
  // Empty vin means the loop is skipped, return true
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_keyimages_diff(tx));
}

TEST(CryptonoteCore, CheckTxInputsKeyimagesDiffSingleInput)
{
  cryptonote::transaction tx;
  cryptonote::txin_to_key in;
  in.amount = 1000;
  in.k_image = crypto::rand<crypto::key_image>();
  in.key_offsets.push_back(0);
  tx.vin.push_back(in);
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_keyimages_diff(tx));
}

TEST(CryptonoteCore, CheckTxInputsKeyimagesDiffDuplicateKeyImage)
{
  cryptonote::transaction tx;
  crypto::key_image ki = crypto::rand<crypto::key_image>();
  cryptonote::txin_to_key in1;
  in1.amount = 1000;
  in1.k_image = ki;
  in1.key_offsets.push_back(0);
  tx.vin.push_back(in1);

  cryptonote::txin_to_key in2;
  in2.amount = 2000;
  in2.k_image = ki; // same key image
  in2.key_offsets.push_back(1);
  tx.vin.push_back(in2);

  EXPECT_FALSE(cryptonote::core::check_tx_inputs_keyimages_diff(tx));
}

TEST(CryptonoteCore, CheckTxInputsKeyimagesDiffDistinctKeyImages)
{
  cryptonote::transaction tx;
  for (int i = 0; i < 5; ++i)
  {
    cryptonote::txin_to_key in;
    in.amount = 1000 * (i + 1);
    in.k_image = crypto::rand<crypto::key_image>();
    in.key_offsets.push_back(i);
    tx.vin.push_back(in);
  }
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_keyimages_diff(tx));
}

// --- check_tx_inputs_ring_members_diff (static) tests ---

TEST(CryptonoteCore, CheckTxInputsRingMembersDiffEmptyTx)
{
  cryptonote::transaction tx;
  // Empty tx, loop skipped, returns true
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_ring_members_diff(tx, 6));
}

TEST(CryptonoteCore, CheckTxInputsRingMembersDiffValidOffsets)
{
  cryptonote::transaction tx;
  cryptonote::txin_to_key in;
  in.amount = 1000;
  in.k_image = crypto::rand<crypto::key_image>();
  // Key offsets: first is absolute, rest are relative (should be > 0)
  in.key_offsets = {100, 5, 10, 3};
  tx.vin.push_back(in);
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_ring_members_diff(tx, 6));
}

TEST(CryptonoteCore, CheckTxInputsRingMembersDiffZeroOffset)
{
  cryptonote::transaction tx;
  cryptonote::txin_to_key in;
  in.amount = 1000;
  in.k_image = crypto::rand<crypto::key_image>();
  // A zero offset at position > 0 means duplicate ring member
  in.key_offsets = {100, 0, 10};
  tx.vin.push_back(in);
  // At HF >= 6, this should fail
  EXPECT_FALSE(cryptonote::core::check_tx_inputs_ring_members_diff(tx, 6));
}

TEST(CryptonoteCore, CheckTxInputsRingMembersDiffZeroOffsetBeforeHF6)
{
  cryptonote::transaction tx;
  cryptonote::txin_to_key in;
  in.amount = 1000;
  in.k_image = crypto::rand<crypto::key_image>();
  in.key_offsets = {100, 0, 10};
  tx.vin.push_back(in);
  // Before HF 6, zero offsets are allowed
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_ring_members_diff(tx, 5));
}

// --- check_tx_inputs_keyimages_domain (static) tests ---

TEST(CryptonoteCore, CheckTxInputsKeyimagesDomainEmptyTx)
{
  cryptonote::transaction tx;
  EXPECT_TRUE(cryptonote::core::check_tx_inputs_keyimages_domain(tx));
}

TEST(CryptonoteCore, CheckTxInputsKeyimagesDomainIdentityKeyImage)
{
  // A key image that is the identity point should be rejected
  cryptonote::transaction tx;
  cryptonote::txin_to_key in;
  in.amount = 1000;
  // Set key image to identity (all zeros is the identity in ed25519)
  memset(&in.k_image, 0, sizeof(in.k_image));
  // The identity check uses rct::ki2rct(k_image) == rct::identity()
  // rct::identity() returns { {1, 0, 0, ...} } (the curve identity point encoding)
  // A zeroed key_image might not be the identity in the ed25519 encoding.
  // Let's construct the actual identity point:
  rct::key identity = rct::identity();
  memcpy(&in.k_image, identity.bytes, sizeof(in.k_image));
  in.key_offsets.push_back(0);
  tx.vin.push_back(in);
  EXPECT_FALSE(cryptonote::core::check_tx_inputs_keyimages_domain(tx));
}

// --- check_tx_semantic (static) tests ---

TEST(CryptonoteCore, CheckTxSemanticEmptyInputs)
{
  // A tx with no inputs should fail semantic check
  cryptonote::transaction tx;
  tx.version = 1;
  cryptonote::tx_verification_context tvc = {};
  EXPECT_FALSE(cryptonote::core::check_tx_semantic(tx, tvc, 1));
  EXPECT_TRUE(tvc.m_verifivation_failed);
  EXPECT_TRUE(tvc.m_invalid_input);
}

TEST(CryptonoteCore, CheckTxSemanticV1TxZeroMoneyOut)
{
  // A v1 tx where amount_in <= amount_out should fail
  cryptonote::transaction tx;
  tx.version = 1;

  // Add a valid input type
  cryptonote::txin_to_key in;
  in.amount = 100;
  in.k_image = crypto::rand<crypto::key_image>();
  in.key_offsets.push_back(0);
  tx.vin.push_back(in);

  // Add output with same amount (inputs must be > outputs for v1)
  cryptonote::tx_out out;
  out.amount = 100;
  cryptonote::txout_to_key tk;
  crypto::generate_keys(tk.key, *reinterpret_cast<crypto::secret_key*>(&tk.key));
  out.target = tk;
  tx.vout.push_back(out);

  cryptonote::tx_verification_context tvc = {};
  EXPECT_FALSE(cryptonote::core::check_tx_semantic(tx, tvc, 1));
  // Either m_overspend or some other check triggers
  EXPECT_TRUE(tvc.m_verifivation_failed);
}

// --- get_max_tx_size additional ---

TEST(CryptonoteCore, MaxTxSizeLessThanHalfBlockSize)
{
  size_t max_size = cryptonote::get_max_tx_size();
  // The max tx size should be exactly CRYPTONOTE_MAX_TX_SIZE
  EXPECT_EQ(max_size, CRYPTONOTE_MAX_TX_SIZE);
}

// --- is_valid_decomposed_amount tests ---

TEST(CryptonoteCore, ValidDecomposedAmountZero)
{
  // Zero might not be in the decomposed list
  // Actually the list starts from 1, so 0 is not valid
  EXPECT_FALSE(cryptonote::is_valid_decomposed_amount(0));
}

TEST(CryptonoteCore, ValidDecomposedAmountOne)
{
  EXPECT_TRUE(cryptonote::is_valid_decomposed_amount(1));
}

TEST(CryptonoteCore, ValidDecomposedAmountTen)
{
  EXPECT_TRUE(cryptonote::is_valid_decomposed_amount(10));
}

TEST(CryptonoteCore, ValidDecomposedAmountHundred)
{
  EXPECT_TRUE(cryptonote::is_valid_decomposed_amount(100));
}

TEST(CryptonoteCore, ValidDecomposedAmountInvalid)
{
  // 15 is not a decomposed amount (not of the form d * 10^n)
  EXPECT_FALSE(cryptonote::is_valid_decomposed_amount(15));
}

TEST(CryptonoteCore, ValidDecomposedAmount1XMR)
{
  // 1 XMR = 1e12
  EXPECT_TRUE(cryptonote::is_valid_decomposed_amount(1000000000000ULL));
}

// --- get_block_reward edge cases ---

TEST(CryptonoteCore, BlockRewardVersion1WithSmallMedian)
{
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(1000, 500, UINT64_C(10000000000000), reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, BlockRewardVersion1BlockEqualMedian)
{
  uint64_t reward = 0;
  // Block weight == median: no penalty
  bool r = cryptonote::get_block_reward(300000, 300000, UINT64_C(10000000000000), reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, BlockRewardVersion1BlockSlightlyOverMedian)
{
  uint64_t reward_at = 0, reward_over = 0;
  cryptonote::get_block_reward(300000, 300000, UINT64_C(10000000000000), reward_at, 1);
  cryptonote::get_block_reward(300000, 310000, UINT64_C(10000000000000), reward_over, 1);
  // Slightly over median should have slightly reduced reward
  EXPECT_LE(reward_over, reward_at);
}

// =============================================================================
// Tests for cryptonote_basic_impl.cpp functions
// =============================================================================

// ---- get_block_reward additional tests ----

TEST(CryptonoteCore, get_block_reward_genesis)
{
  // At height 0 (no coins generated yet), reward should be non-zero
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(0, 0, 0, reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, get_block_reward_zero_median)
{
  // median_weight = 0 should use the default full reward zone
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(0, 0, UINT64_C(5000000000000), reward, 1);
  ASSERT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

TEST(CryptonoteCore, get_block_reward_oversized_block)
{
  // current_block_weight > 2*median: penalty applies, function returns false
  uint64_t reward = 0;
  size_t median = 300000;
  size_t oversized = 2 * median + 1;
  bool r = cryptonote::get_block_reward(median, oversized, UINT64_C(5000000000000), reward, 1);
  EXPECT_FALSE(r);
}

TEST(CryptonoteCore, get_block_reward_at_median)
{
  // current_block_weight == median: full reward, no penalty
  uint64_t reward_at_median = 0;
  uint64_t reward_below_median = 0;
  size_t median = 300000;
  bool r1 = cryptonote::get_block_reward(median, median, UINT64_C(5000000000000), reward_at_median, 1);
  bool r2 = cryptonote::get_block_reward(median, median / 2, UINT64_C(5000000000000), reward_below_median, 1);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);
  // At median and below median should both get the full base reward
  EXPECT_EQ(reward_at_median, reward_below_median);
}

TEST(CryptonoteCore, get_block_reward_returns_true)
{
  // Valid params should return true
  uint64_t reward = 0;
  bool r = cryptonote::get_block_reward(300000, 100000, UINT64_C(1000000000000), reward, 8);
  EXPECT_TRUE(r);
  EXPECT_GT(reward, 0u);
}

// ---- get_min_block_weight tests ----

TEST(CryptonoteCore, get_min_block_weight_v1)
{
  // At HF version 1, should return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1
  size_t min_weight = cryptonote::get_min_block_weight(1);
  EXPECT_EQ(min_weight, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1);
}

TEST(CryptonoteCore, get_min_block_weight_v5_larger)
{
  // At HF version 5, min weight should be larger than v1
  size_t weight_v1 = cryptonote::get_min_block_weight(1);
  size_t weight_v5 = cryptonote::get_min_block_weight(5);
  EXPECT_GT(weight_v5, weight_v1);
  EXPECT_EQ(weight_v5, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5);
}

// ---- get_max_tx_size test ----

TEST(CryptonoteCore, get_max_tx_size_positive)
{
  size_t max_size = cryptonote::get_max_tx_size();
  EXPECT_GT(max_size, 0u);
}

// ---- address roundtrip tests ----

TEST(CryptonoteCore, address_roundtrip_mainnet)
{
  cryptonote::account_base acc;
  acc.generate();
  const auto& keys = acc.get_keys();

  std::string addr_str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, keys.m_account_address);
  EXPECT_FALSE(addr_str.empty());

  cryptonote::address_parse_info info;
  bool r = cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, addr_str);
  ASSERT_TRUE(r);
  EXPECT_FALSE(info.is_subaddress);
  EXPECT_FALSE(info.has_payment_id);
  EXPECT_EQ(info.address.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  EXPECT_EQ(info.address.m_view_public_key, keys.m_account_address.m_view_public_key);
}

TEST(CryptonoteCore, address_roundtrip_testnet)
{
  cryptonote::account_base acc;
  acc.generate();
  const auto& keys = acc.get_keys();

  std::string addr_str = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, keys.m_account_address);
  EXPECT_FALSE(addr_str.empty());

  cryptonote::address_parse_info info;
  bool r = cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, addr_str);
  ASSERT_TRUE(r);
  EXPECT_FALSE(info.is_subaddress);
  EXPECT_FALSE(info.has_payment_id);
  EXPECT_EQ(info.address.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  EXPECT_EQ(info.address.m_view_public_key, keys.m_account_address.m_view_public_key);
}

TEST(CryptonoteCore, address_roundtrip_stagenet)
{
  cryptonote::account_base acc;
  acc.generate();
  const auto& keys = acc.get_keys();

  std::string addr_str = cryptonote::get_account_address_as_str(cryptonote::STAGENET, false, keys.m_account_address);
  EXPECT_FALSE(addr_str.empty());

  cryptonote::address_parse_info info;
  bool r = cryptonote::get_account_address_from_str(info, cryptonote::STAGENET, addr_str);
  ASSERT_TRUE(r);
  EXPECT_FALSE(info.is_subaddress);
  EXPECT_FALSE(info.has_payment_id);
  EXPECT_EQ(info.address.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  EXPECT_EQ(info.address.m_view_public_key, keys.m_account_address.m_view_public_key);
}

TEST(CryptonoteCore, integrated_address_roundtrip)
{
  cryptonote::account_base acc;
  acc.generate();
  const auto& keys = acc.get_keys();

  crypto::hash8 payment_id;
  memset(&payment_id, 0xAB, sizeof(payment_id));

  std::string integrated_str = cryptonote::get_account_integrated_address_as_str(
    cryptonote::MAINNET, keys.m_account_address, payment_id);
  EXPECT_FALSE(integrated_str.empty());

  cryptonote::address_parse_info info;
  bool r = cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, integrated_str);
  ASSERT_TRUE(r);
  EXPECT_TRUE(info.has_payment_id);
  EXPECT_FALSE(info.is_subaddress);
  EXPECT_EQ(info.address.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  EXPECT_EQ(info.address.m_view_public_key, keys.m_account_address.m_view_public_key);
  EXPECT_EQ(info.payment_id, payment_id);
}

TEST(CryptonoteCore, invalid_address_string)
{
  cryptonote::address_parse_info info;
  bool r = cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, "this_is_not_a_valid_address");
  EXPECT_FALSE(r);
}
