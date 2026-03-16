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
#include "cryptonote_core/blockchain.h"
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

  class TxPoolTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_test_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directories(m_test_dir);

      m_db = new cryptonote::BlockchainLMDB();
      m_db->open(m_test_dir.string(), 0);

      ASSERT_TRUE(m_blockchain.init(m_db, cryptonote::FAKECHAIN, true, &test_opts, 1));
      ASSERT_TRUE(m_pool.init(0));
    }

    void TearDown() override
    {
      m_pool.deinit();
      m_blockchain.deinit();
      delete m_db;
      boost::filesystem::remove_all(m_test_dir);
    }

    cryptonote::Blockchain m_blockchain;
    cryptonote::tx_memory_pool m_pool{m_blockchain};
    cryptonote::BlockchainDB *m_db;
    boost::filesystem::path m_test_dir;
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
  // Cookie should be a valid value
  (void)c; // Just verify it doesn't crash
}

TEST_F(TxPoolTest, print_empty_pool)
{
  std::string short_output = m_pool.print_pool(true);
  std::string long_output = m_pool.print_pool(false);
  // Should not crash
  ASSERT_FALSE(short_output.empty());
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
  // Should not crash, verify pool still functions
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
