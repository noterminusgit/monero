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

#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "blockchain_db/lmdb/db_lmdb.h"
#include <boost/filesystem.hpp>

namespace
{
  static const cryptonote::test_options test_opts = {
    {
      std::make_pair(1, 0),
    },
    0
  };

  class BlockchainTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_test_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directories(m_test_dir);

      m_db = new cryptonote::BlockchainLMDB();
      m_db->open(m_test_dir.string(), 0);

      ASSERT_TRUE(m_blockchain.init(m_db, cryptonote::FAKECHAIN, true, &test_opts, 1));
    }

    void TearDown() override
    {
      m_blockchain.deinit();
      delete m_db;
      boost::filesystem::remove_all(m_test_dir);
    }

    cryptonote::Blockchain m_blockchain;
    cryptonote::BlockchainDB *m_db;
    boost::filesystem::path m_test_dir;
  };
}

TEST_F(BlockchainTest, init_and_height)
{
  // After init with genesis, height should be 1
  ASSERT_EQ(m_blockchain.get_current_blockchain_height(), 1u);
}

TEST_F(BlockchainTest, tail_id_not_null)
{
  crypto::hash tail = m_blockchain.get_tail_id();
  ASSERT_NE(tail, crypto::null_hash);
}

TEST_F(BlockchainTest, get_block_by_invalid_height)
{
  // Getting a block at an invalid height should fail
  cryptonote::block bl;
  try {
    crypto::hash h = m_blockchain.get_block_id_by_height(99999);
    ASSERT_EQ(h, crypto::null_hash);
  } catch (...) {
    // Expected for out-of-range
  }
}

TEST_F(BlockchainTest, pruning_seed_default)
{
  ASSERT_EQ(m_blockchain.get_blockchain_pruning_seed(), 0u);
}

TEST_F(BlockchainTest, fee_quantization_mask)
{
  uint64_t mask = cryptonote::Blockchain::get_fee_quantization_mask();
  ASSERT_GT(mask, 0u);
  // Should be a power of 10 or similar quantization
}

TEST_F(BlockchainTest, dynamic_base_fee)
{
  // Test static method with known values
  uint64_t fee = cryptonote::Blockchain::get_dynamic_base_fee(10000000000ULL, 300000);
  ASSERT_GT(fee, 0u);
}

TEST_F(BlockchainTest, check_block_timestamp_valid)
{
  // Genesis block timestamp should be valid
  cryptonote::block genesis;
  crypto::hash genesis_hash = m_blockchain.get_block_id_by_height(0);
  ASSERT_TRUE(m_blockchain.get_block_by_hash(genesis_hash, genesis));
  ASSERT_TRUE(m_blockchain.check_block_timestamp(genesis));
}

TEST_F(BlockchainTest, check_block_timestamp_with_timestamps)
{
  std::vector<uint64_t> timestamps;
  // With empty timestamps, block timestamp check should pass for reasonable timestamps
  cryptonote::block b;
  b.timestamp = time(NULL);
  uint64_t median_ts;
  ASSERT_TRUE(m_blockchain.check_block_timestamp(timestamps, b, median_ts));
}

TEST_F(BlockchainTest, check_block_timestamp_future)
{
  std::vector<uint64_t> timestamps;
  // Generate enough timestamps for the check
  uint64_t now = time(NULL);
  for (int i = 0; i < 60; ++i)
    timestamps.push_back(now - 120 + i * 2);

  cryptonote::block b;
  // Set timestamp far in the future
  b.timestamp = now + 7200; // 2 hours ahead
  uint64_t median_ts;
  ASSERT_FALSE(m_blockchain.check_block_timestamp(timestamps, b, median_ts));
}

TEST_F(BlockchainTest, have_tx_nonexistent)
{
  crypto::hash h = crypto::rand<crypto::hash>();
  ASSERT_FALSE(m_blockchain.have_tx(h));
}

TEST_F(BlockchainTest, block_difficulty_genesis)
{
  // Genesis should have difficulty = fixed difficulty (1 in our test opts)
  cryptonote::difficulty_type d = m_blockchain.block_difficulty(0);
  ASSERT_EQ(d, 1u);
}
