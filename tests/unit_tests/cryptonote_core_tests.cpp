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
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
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
