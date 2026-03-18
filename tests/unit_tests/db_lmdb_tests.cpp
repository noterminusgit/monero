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
#include "blockchain_db/lmdb/db_lmdb.h"
#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/hardfork.h"
#include "string_tools.h"
#include <boost/filesystem.hpp>
#include <cstring>

using namespace cryptonote;
using epee::string_tools::pod_to_hex;

namespace {

// ---- Hex-to-binary helper (same as blockchain_db.cpp tests) ----

std::string h2b(const std::string& s)
{
  bool upper = true;
  std::string result;
  unsigned char val = 0;
  for (char c : s)
  {
    if (upper)
    {
      val = 0;
      if (c <= 'f' && c >= 'a')
        val = ((c - 'a') + 10) << 4;
      else
        val = (c - '0') << 4;
    }
    else
    {
      if (c <= 'f' && c >= 'a')
        val |= (c - 'a') + 10;
      else
        val |= c - '0';
      result += (char)val;
    }
    upper = !upper;
  }
  return result;
}

// ---- Real block data from the Monero blockchain (same as blockchain_db.cpp tests) ----

const std::vector<std::string> t_blocks =
  {
    "0100d5adc49a053b8818b2b6023cd2d532c6774e164a8fcacd603651cb3ea0cb7f9340b28ec016b4bc4ca301aa0101ff6e08acbb2702eab03067870349139bee7eab2ca2e030a6bb73d4f68ab6a3b6ca937214054cdac0843d028bbe23b57ea9bae53f12da93bb57bf8a2e40598d9fccd10c2921576e987d93cd80b4891302468738e391f07c4f2b356f7957160968e0bfef6e907c3cee2d8c23cbf04b089680c6868f01025a0f41f063e195a966051e3a29e17130a9ce97d48f55285b9bb04bdd55a09ae78088aca3cf0202d0f26169290450fe17e08974789c3458910b4db18361cdc564f8f2d0bdd2cf568090cad2c60e02d6f3483ec45505cc3be841046c7a12bf953ac973939bc7b727e54258e1881d4d80e08d84ddcb0102dae6dfb16d3e28aaaf43e00170b90606b36f35f38f8a3dceb5ee18199dd8f17c80c0caf384a30202385d7e57a4daba4cdd9e550a92dcc188838386e7581f13f09de796cbed4716a42101c052492a077abf41996b50c1b2e67fd7288bcd8c55cdc657b4e22d0804371f6901beb76a82ea17400cd6d7f595f70e1667d2018ed8f5a78d1ce07484222618c3cd"
  , "0100f9adc49a057d3113f562eac36f14afa08c22ae20bbbf8cffa31a4466d24850732cb96f80e9762365ee01ab0101ff6f08cc953502be76deb845c431f2ed9a4862457654b914003693b8cd672abc935f0d97b16380c08db7010291819f2873e3efbae65ecd5a736f5e8a26318b591c21e39a03fb536520ac63ba80dac40902439a10fde02e39e48e0b31e57cc084a07eedbefb8cbea0143aedd0442b189caa80c6868f010227b84449de4cd7a48cbdce8974baf0b6646e03384e32055e705c243a86bef8a58088aca3cf0202fa7bd15e4e7e884307ab130bb9d50e33c5fcea6546042a26f948efd5952459ee8090cad2c60e028695583dbb8f8faab87e3ef3f88fa827db097bbf51761d91924f5c5b74c6631780e08d84ddcb010279d2f247b54690e3b491e488acff16014a825fd740c23988a25df7c4670c1f2580c0caf384a302022599dfa3f8788b66295051d85937816e1c320cdb347a0fba5219e3fe60c83b2421010576509c5672025d28fd5d3f38efce24e1f9aaf65dd3056b2504e6e2b7f19f7800"
  };

const std::vector<size_t> t_sizes =
  {
    1122
  , 347
  };

const std::vector<difficulty_type> t_diffs =
  {
    4003674
  , 4051757
  };

const std::vector<uint64_t> t_coins =
  {
    1952630229575370
  , 1970220553446486
  };

const std::vector<std::vector<std::string>> t_transactions =
  {
    {
      "0100010280e08d84ddcb0106010401110701f254220bb50d901a5523eaed438af5d43f8c6d0e54ba0632eb539884f6b7c02008c0a8a50402f9c7cf807ae74e56f4ec84db2bd93cfb02c2249b38e306f5b54b6e05d00d543b8095f52a02b6abb84e00f47f0a72e37b6b29392d906a38468404c57db3dbc5e8dd306a27a880d293ad0302cfc40a86723e7d459e90e45d47818dc0e81a1f451ace5137a4af8110a89a35ea80b4c4c321026b19c796338607d5a2c1ba240a167134142d72d1640ef07902da64fed0b10cfc8088aca3cf02021f6f655254fee84161118b32e7b6f8c31de5eb88aa00c29a8f57c0d1f95a24dd80d0b8e1981a023321af593163cea2ae37168ab926efd87f195756e3b723e886bdb7e618f751c480a094a58d1d0295ed2b08d1cf44482ae0060a5dcc4b7d810a85dea8c62e274f73862f3d59f8ed80a0e5b9c2910102dc50f2f28d7ceecd9a1147f7106c8d5b4e08b2ec77150f52dd7130ee4f5f50d42101d34f90ac861d0ee9fe3891656a234ea86a8a93bf51a237db65baa00d3f4aa196a9e1d89bc06b40e94ea9a26059efc7ba5b2de7ef7c139831ca62f3fe0bb252008f8c7ee810d3e1e06313edf2db362fc39431755779466b635f12f9f32e44470a3e85e08a28fcd90633efc94aa4ae39153dfaf661089d045521343a3d63e8da08d7916753c66aaebd4eefcfe8e58e5b3d266b752c9ca110749fa33fce7c44270386fcf2bed4f03dd5dadb2dc1fd4c505419f8217b9eaec07521f0d8963e104603c926745039cf38d31de6ed95ace8e8a451f5a36f818c151f517546d55ac0f500e54d07b30ea7452f2e93fa4f60bdb30d71a0a97f97eb121e662006780fbf69002228224a96bff37893d47ec3707b17383906c0cd7d9e7412b3e6c8ccf1419b093c06c26f96e3453b424713cdc5c9575f81cda4e157052df11f4c40809edf420f88a3dd1f7909bbf77c8b184a933389094a88e480e900bcdbf6d1824742ee520fc0032e7d892a2b099b8c6edfd1123ce58a34458ee20cad676a7f7cfd80a28f0cb0888af88838310db372986bdcf9bfcae2324480ca7360d22bff21fb569a530e"
    }
  , {
    }
  };

// ---- Basic test fixture (no blocks loaded) ----

class LMDBTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("monero-test-%%%%-%%%%");
    boost::filesystem::create_directories(m_dir);
  }

  void TearDown() override
  {
    boost::filesystem::remove_all(m_dir);
  }

  boost::filesystem::path m_dir;
};

// ---- Extended test fixture with block data and HardFork support ----

class LMDBTestWithBlocks : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("monero-test-%%%%-%%%%");
    boost::filesystem::create_directories(m_dir);

    // Parse test blocks
    for (auto& i : t_blocks)
    {
      block bl;
      blobdata bd = h2b(i);
      ASSERT_TRUE(parse_and_validate_block_from_blob(bd, bl));
      m_blocks.push_back(std::make_pair(bl, bd));
    }

    // Parse test transactions
    for (auto& i : t_transactions)
    {
      std::vector<std::pair<transaction, blobdata>> txs;
      for (auto& j : i)
      {
        transaction tx;
        blobdata bd = h2b(j);
        ASSERT_TRUE(parse_and_validate_tx_from_blob(bd, tx));
        txs.push_back(std::make_pair(tx, bd));
      }
      m_txs.push_back(txs);
    }
  }

  void TearDown() override
  {
    boost::filesystem::remove_all(m_dir);
  }

  void open_db()
  {
    m_db.open(m_dir.string());
    m_hardfork.reset(new HardFork(m_db, 1, 0));
    m_hardfork->init();
    m_db.set_hard_fork(m_hardfork.get());
  }

  void close_db()
  {
    m_db.set_hard_fork(nullptr);
    m_hardfork.reset();
    m_db.close();
  }

  // Add block 0 (genesis) within an existing write transaction
  void add_block_0()
  {
    m_db.add_block(m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], m_txs[0]);
  }

  // Add block 1 within an existing write transaction
  void add_block_1()
  {
    m_db.add_block(m_blocks[1], t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], m_txs[1]);
  }

  boost::filesystem::path m_dir;
  BlockchainLMDB m_db;
  std::unique_ptr<HardFork> m_hardfork;
  std::vector<std::pair<block, blobdata>> m_blocks;
  std::vector<std::vector<std::pair<transaction, blobdata>>> m_txs;
};

// ===========================================================================
// ---- Original LMDBTest tests (basic open/close/batch/query on empty DB) ----
// ===========================================================================

// ---- DB name ----

TEST_F(LMDBTest, GetDbName)
{
  cryptonote::BlockchainLMDB db;
  EXPECT_EQ("lmdb", db.get_db_name());
}

// ---- Open / Close lifecycle ----

TEST_F(LMDBTest, OpenAndClose)
{
  cryptonote::BlockchainLMDB db;
  EXPECT_NO_THROW(db.open(m_dir.string()));
  EXPECT_NO_THROW(db.close());
}

TEST_F(LMDBTest, OpenTwiceThrows)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  // Opening again should throw
  EXPECT_ANY_THROW(db.open(m_dir.string()));
  db.close();
}

TEST_F(LMDBTest, CloseWithoutOpen)
{
  cryptonote::BlockchainLMDB db;
  // Closing without opening throws DB_ERROR
  EXPECT_ANY_THROW(db.close());
}

TEST_F(LMDBTest, MultipleOpenCloseCycles)
{
  for (int i = 0; i < 3; ++i)
  {
    cryptonote::BlockchainLMDB db;
    EXPECT_NO_THROW(db.open(m_dir.string()));
    EXPECT_NO_THROW(db.close());
  }
}

// ---- Open with invalid path ----

TEST_F(LMDBTest, OpenInvalidPath)
{
  cryptonote::BlockchainLMDB db;
  EXPECT_ANY_THROW(db.open("/nonexistent/path/that/should/not/exist"));
}

// ---- Height on fresh DB ----

TEST_F(LMDBTest, HeightFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.height());
  db.close();
}

// ---- is_read_only ----

TEST_F(LMDBTest, IsNotReadOnly)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  // Access via base class pointer since override is private in BlockchainLMDB
  cryptonote::BlockchainDB* base = &db;
  EXPECT_FALSE(base->is_read_only());
  db.close();
}

// ---- Batch start / stop ----

TEST_F(LMDBTest, BatchStartStop)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start());
  EXPECT_NO_THROW(db.batch_stop());
  db.close();
}

TEST_F(LMDBTest, BatchStartAbort)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start());
  EXPECT_NO_THROW(db.batch_abort());
  db.close();
}

TEST_F(LMDBTest, BatchStartWithBlockCount)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start(100));
  EXPECT_NO_THROW(db.batch_stop());
  db.close();
}

TEST_F(LMDBTest, BatchStartWithBlockCountAndBytes)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start(100, 1024 * 1024));
  EXPECT_NO_THROW(db.batch_stop());
  db.close();
}

// ---- Double batch start ----

TEST_F(LMDBTest, DoubleBatchStartReturnsFalse)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start());
  // Second batch_start while batch is active should return false
  EXPECT_FALSE(db.batch_start());
  db.batch_stop();
  db.close();
}

// ---- get_filenames ----

TEST_F(LMDBTest, GetFilenamesNonEmpty)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  auto filenames = db.get_filenames();
  EXPECT_FALSE(filenames.empty());
  db.close();
}

// ---- get_database_size ----

TEST_F(LMDBTest, GetDatabaseSizeFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  cryptonote::BlockchainDB* base = &db;
  uint64_t size = base->get_database_size();
  // Fresh DB should still have some size (at minimum the map size)
  EXPECT_GT(size, 0u);
  db.close();
}

// ---- TX count on fresh DB ----

TEST_F(LMDBTest, TxCountFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.get_tx_count());
  db.close();
}

// ---- Block existence on fresh DB ----

TEST_F(LMDBTest, BlockExistsFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  crypto::hash h = crypto::null_hash;
  EXPECT_FALSE(db.block_exists(h));
  db.close();
}

// ---- Key image existence on fresh DB ----

TEST_F(LMDBTest, KeyImageExistsFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  crypto::key_image ki;
  memset(&ki, 0, sizeof(ki));
  EXPECT_FALSE(db.has_key_image(ki));
  db.close();
}

// ---- TX existence on fresh DB ----

TEST_F(LMDBTest, TxExistsFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  crypto::hash h = crypto::null_hash;
  EXPECT_FALSE(db.tx_exists(h));
  db.close();
}

// ---- TXPool count on fresh DB ----

TEST_F(LMDBTest, TxPoolCountFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.get_txpool_tx_count());
  db.close();
}

// ---- Alt block count on fresh DB ----

TEST_F(LMDBTest, AltBlockCountFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.get_alt_block_count());
  db.close();
}

// ---- Pruning seed on fresh DB ----

TEST_F(LMDBTest, PruningSeedFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.get_blockchain_pruning_seed());
  db.close();
}

// ---- Sync ----

TEST_F(LMDBTest, SyncDoesNotCrash)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_NO_THROW(db.sync());
  db.close();
}

// ---- Safe sync mode ----

TEST_F(LMDBTest, SafeSyncModeToggle)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_NO_THROW(db.safesyncmode(true));
  EXPECT_NO_THROW(db.safesyncmode(false));
  db.close();
}

// ---- Block read transaction ----

TEST_F(LMDBTest, BlockRtxnStartStop)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.block_rtxn_start());
  EXPECT_NO_THROW(db.block_rtxn_stop());
  db.close();
}

TEST_F(LMDBTest, BlockRtxnAbort)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_TRUE(db.block_rtxn_start());
  EXPECT_NO_THROW(db.block_rtxn_abort());
  db.close();
}

// ---- get_num_outputs on fresh DB ----

TEST_F(LMDBTest, NumOutputsFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_EQ(0u, db.get_num_outputs(0));
  db.close();
}

// ---- Output histogram on fresh DB ----

TEST_F(LMDBTest, OutputHistogramFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  auto hist = db.get_output_histogram({}, false, 0, 0);
  EXPECT_TRUE(hist.empty());
  db.close();
}

// ---- set_batch_transactions ----

TEST_F(LMDBTest, SetBatchTransactions)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_NO_THROW(db.set_batch_transactions(true));
  EXPECT_NO_THROW(db.set_batch_transactions(false));
  db.close();
}

// ---- drop_alt_blocks ----

TEST_F(LMDBTest, DropAltBlocksFreshDB)
{
  cryptonote::BlockchainLMDB db;
  db.open(m_dir.string());
  EXPECT_NO_THROW(db.drop_alt_blocks());
  EXPECT_EQ(0u, db.get_alt_block_count());
  db.close();
}

// ---- Constructor with batch flag ----

TEST_F(LMDBTest, ConstructorBatchTrue)
{
  cryptonote::BlockchainLMDB db(true);
  db.open(m_dir.string());
  EXPECT_TRUE(db.batch_start());
  db.batch_stop();
  db.close();
}

TEST_F(LMDBTest, ConstructorBatchFalse)
{
  cryptonote::BlockchainLMDB db(false);
  db.open(m_dir.string());
  // When batch_transactions is false, batch_start throws
  EXPECT_ANY_THROW(db.batch_start());
  db.close();
}

// ===========================================================================
// ---- Block operations with real blocks ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, AddBlockAndCheckHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  EXPECT_EQ(0u, m_db.height());
  ASSERT_NO_THROW(add_block_0());
  EXPECT_EQ(1u, m_db.height());
  ASSERT_NO_THROW(add_block_1());
  EXPECT_EQ(2u, m_db.height());

  close_db();
}

TEST_F(LMDBTestWithBlocks, BlockExists)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);
  crypto::hash blk1_hash = get_block_hash(m_blocks[1].first);

  EXPECT_TRUE(m_db.block_exists(blk0_hash));
  EXPECT_FALSE(m_db.block_exists(blk1_hash));

  add_block_1();
  EXPECT_TRUE(m_db.block_exists(blk1_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, BlockExistsReturnsHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  uint64_t height = 0;
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);
  EXPECT_TRUE(m_db.block_exists(blk0_hash, &height));
  EXPECT_EQ(0u, height);

  crypto::hash blk1_hash = get_block_hash(m_blocks[1].first);
  EXPECT_TRUE(m_db.block_exists(blk1_hash, &height));
  EXPECT_EQ(1u, height);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);
  crypto::hash blk1_hash = get_block_hash(m_blocks[1].first);

  EXPECT_EQ(0u, m_db.get_block_height(blk0_hash));
  EXPECT_EQ(1u, m_db.get_block_height(blk1_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockHeader)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);

  block_header hdr = m_db.get_block_header(blk0_hash);
  EXPECT_EQ(m_blocks[0].first.major_version, hdr.major_version);
  EXPECT_EQ(m_blocks[0].first.minor_version, hdr.minor_version);
  EXPECT_EQ(m_blocks[0].first.timestamp, hdr.timestamp);
  EXPECT_EQ(m_blocks[0].first.nonce, hdr.nonce);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockBlob)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);

  blobdata bd = m_db.get_block_blob(blk0_hash);
  EXPECT_FALSE(bd.empty());
  // The blob should parse back into a valid block
  block parsed;
  EXPECT_TRUE(parse_and_validate_block_from_blob(bd, parsed));
  EXPECT_EQ(pod_to_hex(blk0_hash), pod_to_hex(get_block_hash(parsed)));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockBlobFromHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  blobdata bd0 = m_db.get_block_blob_from_height(0);
  EXPECT_FALSE(bd0.empty());

  blobdata bd1 = m_db.get_block_blob_from_height(1);
  EXPECT_FALSE(bd1.empty());

  // Different blocks should yield different blobs
  EXPECT_NE(bd0, bd1);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockBlobFromInvalidHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  // No blocks yet, accessing height 0 should throw
  EXPECT_ANY_THROW(m_db.get_block_blob_from_height(0));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockHashFromHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  crypto::hash h0 = m_db.get_block_hash_from_height(0);
  crypto::hash h1 = m_db.get_block_hash_from_height(1);

  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(h0));
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(h1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, TopBlockHash)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  crypto::hash top = m_db.top_block_hash();
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(top));

  add_block_1();
  top = m_db.top_block_hash();
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(top));

  close_db();
}

TEST_F(LMDBTestWithBlocks, TopBlockHashWithHeightReturn)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  uint64_t blk_height = 0;
  crypto::hash top = m_db.top_block_hash(&blk_height);
  EXPECT_EQ(1u, blk_height);
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(top));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTopBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  block top = m_db.get_top_block();
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(get_block_hash(top)));

  add_block_1();
  top = m_db.get_top_block();
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(get_block_hash(top)));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockTimestamp)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  uint64_t ts = m_db.get_block_timestamp(0);
  EXPECT_EQ(m_blocks[0].first.timestamp, ts);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTopBlockTimestamp)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  uint64_t ts = m_db.get_top_block_timestamp();
  EXPECT_EQ(m_blocks[0].first.timestamp, ts);

  add_block_1();
  ts = m_db.get_top_block_timestamp();
  EXPECT_EQ(m_blocks[1].first.timestamp, ts);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockWeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  EXPECT_EQ(t_sizes[0], m_db.get_block_weight(0));

  add_block_1();
  EXPECT_EQ(t_sizes[1], m_db.get_block_weight(1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockWeights)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<uint64_t> weights = m_db.get_block_weights(0, 2);
  ASSERT_EQ(2u, weights.size());
  EXPECT_EQ(t_sizes[0], weights[0]);
  EXPECT_EQ(t_sizes[1], weights[1]);

  // Request more than available
  weights = m_db.get_block_weights(0, 10);
  EXPECT_EQ(2u, weights.size());

  // Request from offset
  weights = m_db.get_block_weights(1, 1);
  ASSERT_EQ(1u, weights.size());
  EXPECT_EQ(t_sizes[1], weights[0]);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockCumulativeDifficulty)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  EXPECT_EQ(t_diffs[0], m_db.get_block_cumulative_difficulty(0));

  add_block_1();
  EXPECT_EQ(t_diffs[1], m_db.get_block_cumulative_difficulty(1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockDifficulty)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  EXPECT_EQ(t_diffs[0], m_db.get_block_difficulty(0));

  add_block_1();
  // Difficulty of block 1 = cumulative_diff[1] - cumulative_diff[0]
  EXPECT_EQ(t_diffs[1] - t_diffs[0], m_db.get_block_difficulty(1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockAlreadyGeneratedCoins)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  EXPECT_EQ(t_coins[0], m_db.get_block_already_generated_coins(0));

  add_block_1();
  EXPECT_EQ(t_coins[1], m_db.get_block_already_generated_coins(1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockLongTermWeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  // long_term_block_weight was passed as t_sizes[i] in add_block calls
  add_block_0();
  EXPECT_EQ(t_sizes[0], m_db.get_block_long_term_weight(0));

  add_block_1();
  EXPECT_EQ(t_sizes[1], m_db.get_block_long_term_weight(1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetLongTermBlockWeights)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<uint64_t> lts = m_db.get_long_term_block_weights(0, 2);
  ASSERT_EQ(2u, lts.size());
  EXPECT_EQ(t_sizes[0], lts[0]);
  EXPECT_EQ(t_sizes[1], lts[1]);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlocksRange)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<block> blks = m_db.get_blocks_range(0, 1);
  ASSERT_EQ(2u, blks.size());
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(get_block_hash(blks[0])));
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(get_block_hash(blks[1])));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetHashesRange)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<crypto::hash> hashes = m_db.get_hashes_range(0, 1);
  ASSERT_EQ(2u, hashes.size());
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(hashes[0]));
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(hashes[1]));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockCumulativeRctOutputs)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<uint64_t> heights = {0, 1};
  std::vector<uint64_t> rct_outs = m_db.get_block_cumulative_rct_outputs(heights);
  ASSERT_EQ(2u, rct_outs.size());
  // These are v1 blocks so no rct outputs expected
  EXPECT_EQ(0u, rct_outs[0]);
  EXPECT_EQ(0u, rct_outs[1]);

  close_db();
}

// ===========================================================================
// ---- Pop block operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, PopBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();
  EXPECT_EQ(2u, m_db.height());

  block popped;
  std::vector<transaction> popped_txs;
  ASSERT_NO_THROW(m_db.pop_block(popped, popped_txs));
  EXPECT_EQ(1u, m_db.height());
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(get_block_hash(popped)));

  close_db();
}

TEST_F(LMDBTestWithBlocks, PopBlockToEmpty)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  EXPECT_EQ(1u, m_db.height());

  block popped;
  std::vector<transaction> popped_txs;
  ASSERT_NO_THROW(m_db.pop_block(popped, popped_txs));
  EXPECT_EQ(0u, m_db.height());

  // Verify block no longer exists
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);
  EXPECT_FALSE(m_db.block_exists(blk0_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, PopBlockWithTransactions)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  // Block 0 has transactions in t_txs[0]
  add_block_0();
  EXPECT_EQ(1u, m_db.height());

  // The miner tx should exist
  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  EXPECT_TRUE(m_db.tx_exists(miner_tx_hash));

  // Verify non-miner txs exist
  for (auto& h : m_blocks[0].first.tx_hashes)
  {
    EXPECT_TRUE(m_db.tx_exists(h));
  }

  block popped;
  std::vector<transaction> popped_txs;
  ASSERT_NO_THROW(m_db.pop_block(popped, popped_txs));
  EXPECT_EQ(0u, m_db.height());

  // After pop, miner tx should not exist
  EXPECT_FALSE(m_db.tx_exists(miner_tx_hash));

  // After pop, non-miner txs should not exist
  for (auto& h : m_blocks[0].first.tx_hashes)
  {
    EXPECT_FALSE(m_db.tx_exists(h));
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, PopAndReaddBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();
  EXPECT_EQ(2u, m_db.height());

  block popped;
  std::vector<transaction> popped_txs;
  m_db.pop_block(popped, popped_txs);
  EXPECT_EQ(1u, m_db.height());

  // Re-add block 1
  ASSERT_NO_THROW(add_block_1());
  EXPECT_EQ(2u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- Transaction storage (after adding blocks) ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, TxCountAfterAddingBlocks)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  EXPECT_EQ(0u, m_db.get_tx_count());

  add_block_0();
  // Block 0 has 1 miner tx + t_txs[0].size() non-miner txs
  uint64_t expected_count = 1 + m_txs[0].size();
  EXPECT_EQ(expected_count, m_db.get_tx_count());

  add_block_1();
  // Block 1 adds 1 miner tx + t_txs[1].size() non-miner txs
  expected_count += 1 + m_txs[1].size();
  EXPECT_EQ(expected_count, m_db.get_tx_count());

  close_db();
}

TEST_F(LMDBTestWithBlocks, TxExistsAfterAddingBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Miner tx should exist
  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  EXPECT_TRUE(m_db.tx_exists(miner_tx_hash));

  // Non-miner txs should exist
  for (auto& h : m_blocks[0].first.tx_hashes)
  {
    EXPECT_TRUE(m_db.tx_exists(h));
  }

  // Random hash should not exist
  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  EXPECT_FALSE(m_db.tx_exists(fake_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, TxExistsWithIndex)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  uint64_t tx_id = 0;
  EXPECT_TRUE(m_db.tx_exists(miner_tx_hash, tx_id));
  // tx_id should be assigned some value (exact value is implementation detail)

  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  EXPECT_FALSE(m_db.tx_exists(fake_hash, tx_id));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxBlob)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  blobdata tx_blob;
  EXPECT_TRUE(m_db.get_tx_blob(miner_tx_hash, tx_blob));
  EXPECT_FALSE(tx_blob.empty());

  // Verify the blob parses back into a valid transaction
  transaction parsed_tx;
  EXPECT_TRUE(parse_and_validate_tx_from_blob(tx_blob, parsed_tx));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxBlobNonExistent)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  blobdata tx_blob;
  EXPECT_FALSE(m_db.get_tx_blob(fake_hash, tx_blob));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetPrunedTxBlob)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  blobdata tx_blob;
  EXPECT_TRUE(m_db.get_pruned_tx_blob(miner_tx_hash, tx_blob));
  EXPECT_FALSE(tx_blob.empty());

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxUnlockTime)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  uint64_t unlock_time = m_db.get_tx_unlock_time(miner_tx_hash);
  EXPECT_EQ(m_blocks[0].first.miner_tx.unlock_time, unlock_time);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxBlockHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  crypto::hash miner_tx_hash0 = get_transaction_hash(m_blocks[0].first.miner_tx);
  EXPECT_EQ(0u, m_db.get_tx_block_height(miner_tx_hash0));

  crypto::hash miner_tx_hash1 = get_transaction_hash(m_blocks[1].first.miner_tx);
  EXPECT_EQ(1u, m_db.get_tx_block_height(miner_tx_hash1));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxViaBaseClass)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  BlockchainDB* base = &m_db;
  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);

  // get_tx returning transaction
  transaction tx;
  EXPECT_TRUE(base->get_tx(miner_tx_hash, tx));
  EXPECT_EQ(pod_to_hex(miner_tx_hash), pod_to_hex(get_transaction_hash(tx)));

  // get_tx throwing version
  EXPECT_NO_THROW(tx = base->get_tx(miner_tx_hash));

  // get_pruned_tx
  transaction pruned_tx;
  EXPECT_TRUE(base->get_pruned_tx(miner_tx_hash, pruned_tx));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxNonExistentThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  BlockchainDB* base = &m_db;
  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));

  EXPECT_THROW(base->get_tx(fake_hash), TX_DNE);

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxList)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Build list of tx hashes that should exist
  std::vector<crypto::hash> hlist;
  hlist.push_back(get_transaction_hash(m_blocks[0].first.miner_tx));
  for (auto& h : m_blocks[0].first.tx_hashes)
    hlist.push_back(h);

  std::vector<transaction> txs = m_db.get_tx_list(hlist);
  EXPECT_EQ(hlist.size(), txs.size());

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetPrunedTxBlobsFrom)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  std::vector<blobdata> blobs;
  bool res = m_db.get_pruned_tx_blobs_from(miner_tx_hash, 1, blobs);
  EXPECT_TRUE(res);
  EXPECT_EQ(1u, blobs.size());

  close_db();
}

// ===========================================================================
// ---- Block retrieved via base class get_block / get_block_from_height ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetBlockViaBaseClass)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  BlockchainDB* base = &m_db;
  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);

  block b = base->get_block(blk0_hash);
  EXPECT_EQ(pod_to_hex(blk0_hash), pod_to_hex(get_block_hash(b)));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockFromHeight)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  BlockchainDB* base = &m_db;
  block b0 = base->get_block_from_height(0);
  block b1 = base->get_block_from_height(1);

  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[0].first)), pod_to_hex(get_block_hash(b0)));
  EXPECT_EQ(pod_to_hex(get_block_hash(m_blocks[1].first)), pod_to_hex(get_block_hash(b1)));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockFromInvalidHeightThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  BlockchainDB* base = &m_db;
  EXPECT_ANY_THROW(base->get_block_from_height(999));

  close_db();
}

// ===========================================================================
// ---- Hard fork info ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, HardForkVersionRoundTrip)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // HardFork should track the version; query via the HardFork object
  // The block's major_version is 1, HardFork was initialized with original_version=1
  EXPECT_EQ(1u, m_hardfork->get(0));

  close_db();
}

TEST_F(LMDBTestWithBlocks, HardForkVersionMultipleBlocks)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  EXPECT_EQ(1u, m_hardfork->get(0));
  EXPECT_EQ(1u, m_hardfork->get(1));

  close_db();
}

// ===========================================================================
// ---- Alt blocks ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, AddAndGetAltBlock)
{
  open_db();

  // Alt block operations need a write transaction
  m_db.block_wtxn_start();

  crypto::hash blkid;
  memset(&blkid, 0xAA, sizeof(blkid));

  alt_block_data_t data;
  memset(&data, 0, sizeof(data));
  data.height = 42;
  data.cumulative_weight = 1000;
  data.cumulative_difficulty_low = 5000;
  data.cumulative_difficulty_high = 0;
  data.already_generated_coins = 999999;

  std::string blob_data = "test_alt_block_blob_data_here";
  m_db.add_alt_block(blkid, data, blob_data);

  EXPECT_EQ(1u, m_db.get_alt_block_count());

  // Retrieve it
  alt_block_data_t retrieved_data;
  blobdata retrieved_blob;
  EXPECT_TRUE(m_db.get_alt_block(blkid, &retrieved_data, &retrieved_blob));
  EXPECT_EQ(42u, retrieved_data.height);
  EXPECT_EQ(1000u, retrieved_data.cumulative_weight);
  EXPECT_EQ(5000u, retrieved_data.cumulative_difficulty_low);
  EXPECT_EQ(999999u, retrieved_data.already_generated_coins);
  EXPECT_EQ(blob_data, retrieved_blob);

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, GetAltBlockNullParams)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash blkid;
  memset(&blkid, 0xBB, sizeof(blkid));

  alt_block_data_t data;
  memset(&data, 0, sizeof(data));
  data.height = 10;

  std::string blob_data = "another_alt_block";
  m_db.add_alt_block(blkid, data, blob_data);

  // Get with data only (null blob)
  alt_block_data_t retrieved_data;
  EXPECT_TRUE(m_db.get_alt_block(blkid, &retrieved_data, nullptr));
  EXPECT_EQ(10u, retrieved_data.height);

  // Get with blob only (null data)
  blobdata retrieved_blob;
  EXPECT_TRUE(m_db.get_alt_block(blkid, nullptr, &retrieved_blob));
  EXPECT_EQ(blob_data, retrieved_blob);

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, GetAltBlockNonExistent)
{
  open_db();

  crypto::hash blkid;
  memset(&blkid, 0xCC, sizeof(blkid));

  alt_block_data_t data;
  blobdata blob;
  EXPECT_FALSE(m_db.get_alt_block(blkid, &data, &blob));

  close_db();
}

TEST_F(LMDBTestWithBlocks, RemoveAltBlock)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash blkid;
  memset(&blkid, 0xDD, sizeof(blkid));

  alt_block_data_t data;
  memset(&data, 0, sizeof(data));
  data.height = 5;

  m_db.add_alt_block(blkid, data, "blob");
  EXPECT_EQ(1u, m_db.get_alt_block_count());

  m_db.remove_alt_block(blkid);
  EXPECT_EQ(0u, m_db.get_alt_block_count());

  // Should no longer be retrievable
  EXPECT_FALSE(m_db.get_alt_block(blkid, nullptr, nullptr));

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, MultipleAltBlocks)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 5; ++i)
  {
    crypto::hash blkid;
    memset(&blkid, i + 1, sizeof(blkid));

    alt_block_data_t data;
    memset(&data, 0, sizeof(data));
    data.height = i;

    m_db.add_alt_block(blkid, data, "blob_" + std::to_string(i));
  }

  EXPECT_EQ(5u, m_db.get_alt_block_count());

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, DropAltBlocks)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 3; ++i)
  {
    crypto::hash blkid;
    memset(&blkid, i + 1, sizeof(blkid));

    alt_block_data_t data;
    memset(&data, 0, sizeof(data));
    data.height = i;

    m_db.add_alt_block(blkid, data, "blob");
  }

  EXPECT_EQ(3u, m_db.get_alt_block_count());

  m_db.drop_alt_blocks();
  EXPECT_EQ(0u, m_db.get_alt_block_count());

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllAltBlocks)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 3; ++i)
  {
    crypto::hash blkid;
    memset(&blkid, i + 1, sizeof(blkid));

    alt_block_data_t data;
    memset(&data, 0, sizeof(data));
    data.height = i;

    m_db.add_alt_block(blkid, data, "blob_" + std::to_string(i));
  }

  m_db.block_wtxn_stop();

  // Iterate without blob
  int count = 0;
  bool result = m_db.for_all_alt_blocks([&count](const crypto::hash&, const alt_block_data_t&, const blobdata_ref*) {
    ++count;
    return true;
  }, false);
  EXPECT_TRUE(result);
  EXPECT_EQ(3, count);

  // Iterate with blob
  count = 0;
  result = m_db.for_all_alt_blocks([&count](const crypto::hash&, const alt_block_data_t&, const blobdata_ref* blob) {
    ++count;
    EXPECT_NE(nullptr, blob);
    return true;
  }, true);
  EXPECT_TRUE(result);
  EXPECT_EQ(3, count);

  close_db();
}

// ===========================================================================
// ---- TXPool operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, AddAndGetTxPoolTx)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xAA, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.weight = 1234;
  meta.fee = 5678;
  meta.receive_time = 1000000;
  meta.set_relay_method(relay_method::fluff);

  std::string blob = "test_txpool_blob_data";

  m_db.add_txpool_tx(txid, blob, meta);
  EXPECT_EQ(1u, m_db.get_txpool_tx_count());

  // Get the blob back
  blobdata retrieved;
  EXPECT_TRUE(m_db.get_txpool_tx_blob(txid, retrieved, relay_category::all));
  EXPECT_EQ(blob, retrieved);

  // Get meta
  txpool_tx_meta_t retrieved_meta;
  EXPECT_TRUE(m_db.get_txpool_tx_meta(txid, retrieved_meta));
  EXPECT_EQ(1234u, retrieved_meta.weight);
  EXPECT_EQ(5678u, retrieved_meta.fee);

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, TxPoolTxBlobOverload)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xBB, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.weight = 100;
  meta.fee = 200;
  meta.set_relay_method(relay_method::fluff);

  std::string blob = "overload_test_blob";
  m_db.add_txpool_tx(txid, blob, meta);

  // Use the overload that returns blobdata directly
  blobdata bd = m_db.get_txpool_tx_blob(txid, relay_category::all);
  EXPECT_EQ(blob, bd);

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, TxPoolHasTx)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xCC, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.set_relay_method(relay_method::fluff);

  m_db.add_txpool_tx(txid, "blob", meta);

  EXPECT_TRUE(m_db.txpool_has_tx(txid, relay_category::all));
  EXPECT_TRUE(m_db.txpool_has_tx(txid, relay_category::broadcasted));

  crypto::hash fake_txid;
  memset(&fake_txid, 0xDD, sizeof(fake_txid));
  EXPECT_FALSE(m_db.txpool_has_tx(fake_txid, relay_category::all));

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, RemoveTxPoolTx)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xEE, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.set_relay_method(relay_method::fluff);

  m_db.add_txpool_tx(txid, "blob_to_remove", meta);
  EXPECT_EQ(1u, m_db.get_txpool_tx_count());

  m_db.remove_txpool_tx(txid);
  EXPECT_EQ(0u, m_db.get_txpool_tx_count());
  EXPECT_FALSE(m_db.txpool_has_tx(txid, relay_category::all));

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, UpdateTxPoolTx)
{
  open_db();
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xFF, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.weight = 100;
  meta.fee = 50;
  meta.set_relay_method(relay_method::fluff);

  m_db.add_txpool_tx(txid, "some_blob", meta);

  // Update with new meta
  meta.weight = 200;
  meta.fee = 100;
  m_db.update_txpool_tx(txid, meta);

  txpool_tx_meta_t retrieved;
  EXPECT_TRUE(m_db.get_txpool_tx_meta(txid, retrieved));
  EXPECT_EQ(200u, retrieved.weight);
  EXPECT_EQ(100u, retrieved.fee);

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, MultipleTxPoolTxs)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 5; ++i)
  {
    crypto::hash txid;
    memset(&txid, i + 1, sizeof(txid));

    txpool_tx_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.weight = 100 * (i + 1);
    meta.fee = 10 * (i + 1);
    meta.set_relay_method(relay_method::fluff);

    m_db.add_txpool_tx(txid, "blob_" + std::to_string(i), meta);
  }

  EXPECT_EQ(5u, m_db.get_txpool_tx_count());
  EXPECT_EQ(5u, m_db.get_txpool_tx_count(relay_category::all));

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, TxPoolRelayCategoryFiltering)
{
  open_db();
  m_db.block_wtxn_start();

  // Add a fluff tx (broadcasted)
  crypto::hash txid1;
  memset(&txid1, 0x01, sizeof(txid1));
  txpool_tx_meta_t meta1;
  memset(&meta1, 0, sizeof(meta1));
  meta1.set_relay_method(relay_method::fluff);
  m_db.add_txpool_tx(txid1, "blob1", meta1);

  // Add a local tx (not broadcasted)
  crypto::hash txid2;
  memset(&txid2, 0x02, sizeof(txid2));
  txpool_tx_meta_t meta2;
  memset(&meta2, 0, sizeof(meta2));
  meta2.set_relay_method(relay_method::local);
  m_db.add_txpool_tx(txid2, "blob2", meta2);

  // All should see both
  EXPECT_EQ(2u, m_db.get_txpool_tx_count(relay_category::all));
  // Broadcasted should see only fluff
  EXPECT_EQ(1u, m_db.get_txpool_tx_count(relay_category::broadcasted));

  m_db.block_wtxn_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllTxPoolTxes)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 3; ++i)
  {
    crypto::hash txid;
    memset(&txid, i + 1, sizeof(txid));

    txpool_tx_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.weight = 100 * (i + 1);
    meta.set_relay_method(relay_method::fluff);

    m_db.add_txpool_tx(txid, "blob_" + std::to_string(i), meta);
  }

  m_db.block_wtxn_stop();

  // Iterate without blob
  int count = 0;
  bool result = m_db.for_all_txpool_txes([&count](const crypto::hash&, const txpool_tx_meta_t&, const blobdata_ref*) {
    ++count;
    return true;
  }, false, relay_category::all);
  EXPECT_TRUE(result);
  EXPECT_EQ(3, count);

  // Iterate with blob
  count = 0;
  result = m_db.for_all_txpool_txes([&count](const crypto::hash&, const txpool_tx_meta_t&, const blobdata_ref* blob) {
    ++count;
    EXPECT_NE(nullptr, blob);
    return true;
  }, true, relay_category::all);
  EXPECT_TRUE(result);
  EXPECT_EQ(3, count);

  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllTxPoolTxesEarlyStop)
{
  open_db();
  m_db.block_wtxn_start();

  for (int i = 0; i < 5; ++i)
  {
    crypto::hash txid;
    memset(&txid, i + 1, sizeof(txid));

    txpool_tx_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.set_relay_method(relay_method::fluff);

    m_db.add_txpool_tx(txid, "blob_" + std::to_string(i), meta);
  }

  m_db.block_wtxn_stop();

  // Return false to stop early
  int count = 0;
  bool result = m_db.for_all_txpool_txes([&count](const crypto::hash&, const txpool_tx_meta_t&, const blobdata_ref*) {
    ++count;
    return count < 2; // stop after 2
  }, false, relay_category::all);
  EXPECT_FALSE(result);
  EXPECT_EQ(2, count);

  close_db();
}

// ===========================================================================
// ---- Properties / Stats ----
// ===========================================================================

// fixup() is private in BlockchainLMDB, can't test directly

TEST_F(LMDBTestWithBlocks, SetBatchTransactionsThenBatch)
{
  open_db();
  m_db.set_batch_transactions(true);
  EXPECT_TRUE(m_db.batch_start());
  EXPECT_NO_THROW(m_db.batch_stop());
  close_db();
}

TEST_F(LMDBTestWithBlocks, BatchCommit)
{
  open_db();
  m_db.batch_start();
  add_block_0();
  m_db.batch_commit();
  // After commit, the batch is still active, data should be persisted
  EXPECT_EQ(1u, m_db.height());
  m_db.batch_stop();
  close_db();
}

TEST_F(LMDBTestWithBlocks, ResetDB)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
    EXPECT_EQ(1u, m_db.height());
  }
  // reset clears everything
  m_db.reset();
  EXPECT_EQ(0u, m_db.height());
  close_db();
}

TEST_F(LMDBTestWithBlocks, IsOpenState)
{
  EXPECT_FALSE(m_db.is_open());
  open_db();
  EXPECT_TRUE(m_db.is_open());
  close_db();
  EXPECT_FALSE(m_db.is_open());
}

TEST_F(LMDBTestWithBlocks, GetDatabaseSizeGrowsAfterAddingBlocks)
{
  open_db();

  BlockchainDB* base = &m_db;
  uint64_t size_before = base->get_database_size();

  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  uint64_t size_after = base->get_database_size();
  // Size should be at least as large (map size might not change, but used pages increase)
  EXPECT_GE(size_after, size_before);

  close_db();
}

TEST_F(LMDBTestWithBlocks, RemoveDataFile)
{
  open_db();
  auto filenames = m_db.get_filenames();
  EXPECT_FALSE(filenames.empty());
  close_db();

  // remove_data_file should succeed
  EXPECT_TRUE(m_db.remove_data_file(m_dir.string()));
}

// ===========================================================================
// ---- Read transaction management ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, MultipleReadTransactionCycles)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  // Multiple read transaction cycles
  for (int i = 0; i < 5; ++i)
  {
    EXPECT_TRUE(m_db.block_rtxn_start());
    EXPECT_EQ(1u, m_db.height());
    m_db.block_rtxn_stop();
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, ReadTransactionAbortCycles)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  for (int i = 0; i < 5; ++i)
  {
    EXPECT_TRUE(m_db.block_rtxn_start());
    EXPECT_EQ(1u, m_db.height());
    m_db.block_rtxn_abort();
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, DbRtxnGuard)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  {
    db_rtxn_guard guard(&m_db);
    EXPECT_EQ(1u, m_db.height());
  }
  // After guard goes out of scope, read txn should be stopped

  close_db();
}

TEST_F(LMDBTestWithBlocks, WriteTransactionStartStop)
{
  open_db();

  m_db.block_wtxn_start();
  add_block_0();
  m_db.block_wtxn_stop();

  // Data should be visible now
  EXPECT_EQ(1u, m_db.height());

  close_db();
}

TEST_F(LMDBTestWithBlocks, WriteTransactionAbort)
{
  open_db();

  m_db.block_wtxn_start();
  add_block_0();
  m_db.block_wtxn_abort();

  // After abort, data should not be visible
  EXPECT_EQ(0u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- Output operations (after adding blocks) ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetNumOutputsAfterAddingBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // The block has outputs at specific amounts
  // The miner tx in block 0 has several outputs with different amounts
  // We can check that at least some output amounts have counts > 0
  bool found_any = false;
  for (const auto& out : m_blocks[0].first.miner_tx.vout)
  {
    uint64_t count = m_db.get_num_outputs(out.amount);
    if (count > 0)
      found_any = true;
  }
  EXPECT_TRUE(found_any);

  close_db();
}

TEST_F(LMDBTestWithBlocks, OutputHistogramAfterAddingBlock)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Get histogram for all amounts (empty vector means all)
  auto hist = m_db.get_output_histogram({}, false, 0, 0);
  EXPECT_FALSE(hist.empty());

  close_db();
}

TEST_F(LMDBTestWithBlocks, OutputHistogramSpecificAmounts)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Collect amounts from the block's miner tx
  std::vector<uint64_t> amounts;
  for (const auto& out : m_blocks[0].first.miner_tx.vout)
    amounts.push_back(out.amount);

  auto hist = m_db.get_output_histogram(amounts, false, 0, 0);
  // We should get entries for the amounts that have outputs
  EXPECT_FALSE(hist.empty());

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetOutputKey)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Find an amount that has outputs
  for (const auto& out : m_blocks[0].first.miner_tx.vout)
  {
    uint64_t count = m_db.get_num_outputs(out.amount);
    if (count > 0)
    {
      output_data_t odata = m_db.get_output_key(out.amount, 0, false);
      // The output should be at height 0
      EXPECT_EQ(0u, odata.height);
      break;
    }
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxAmountOutputIndices)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Get the tx_id for the miner tx
  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  uint64_t tx_id = 0;
  ASSERT_TRUE(m_db.tx_exists(miner_tx_hash, tx_id));

  auto indices = m_db.get_tx_amount_output_indices(tx_id, 1);
  ASSERT_EQ(1u, indices.size());
  // The miner tx has multiple outputs
  EXPECT_EQ(m_blocks[0].first.miner_tx.vout.size(), indices[0].size());

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetOutputTxAndIndex)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Find an amount with outputs
  for (const auto& out : m_blocks[0].first.miner_tx.vout)
  {
    uint64_t count = m_db.get_num_outputs(out.amount);
    if (count > 0)
    {
      tx_out_index toi = m_db.get_output_tx_and_index(out.amount, 0);
      // Should be from the miner tx at height 0
      crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
      EXPECT_EQ(pod_to_hex(miner_tx_hash), pod_to_hex(toi.first));
      break;
    }
  }

  close_db();
}

// ===========================================================================
// ---- Key image operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, KeyImageExistsAfterAddingBlockWithTxs)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Block 0 has transactions with key image inputs
  // Check if key images from non-miner txs are stored
  for (const auto& txpair : m_txs[0])
  {
    const transaction& tx = txpair.first;
    for (const auto& vin : tx.vin)
    {
      if (vin.type() == typeid(txin_to_key))
      {
        const txin_to_key& in = boost::get<txin_to_key>(vin);
        EXPECT_TRUE(m_db.has_key_image(in.k_image));
      }
    }
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, HasKeyImages)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Collect key images from txs
  std::vector<crypto::key_image> key_images;
  for (const auto& txpair : m_txs[0])
  {
    for (const auto& vin : txpair.first.vin)
    {
      if (vin.type() == typeid(txin_to_key))
        key_images.push_back(boost::get<txin_to_key>(vin).k_image);
    }
  }

  if (!key_images.empty())
  {
    std::vector<bool> results = m_db.has_key_images(epee::span<const crypto::key_image>(key_images.data(), key_images.size()));
    ASSERT_EQ(key_images.size(), results.size());
    for (bool r : results)
      EXPECT_TRUE(r);
  }

  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllKeyImages)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  int count = 0;
  bool result = m_db.for_all_key_images([&count](const crypto::key_image&) {
    ++count;
    return true;
  });
  EXPECT_TRUE(result);
  // Should have at least the key images from the non-miner transactions
  // Count of key images = number of txin_to_key inputs across all non-miner txs
  int expected = 0;
  for (const auto& txpair : m_txs[0])
    for (const auto& vin : txpair.first.vin)
      if (vin.type() == typeid(txin_to_key))
        ++expected;
  EXPECT_EQ(expected, count);

  close_db();
}

// ===========================================================================
// ---- for_blocks_range / for_all_transactions / for_all_outputs ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, ForBlocksRange)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  int count = 0;
  bool result = m_db.for_blocks_range(0, 1, [&count](uint64_t height, const crypto::hash&, const block&) {
    ++count;
    return true;
  });
  EXPECT_TRUE(result);
  EXPECT_EQ(2, count);

  close_db();
}

TEST_F(LMDBTestWithBlocks, ForBlocksRangeEarlyStop)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  int count = 0;
  bool result = m_db.for_blocks_range(0, 1, [&count](uint64_t, const crypto::hash&, const block&) {
    ++count;
    return false; // stop after first
  });
  EXPECT_FALSE(result);
  EXPECT_EQ(1, count);

  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllTransactions)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  int count = 0;
  bool result = m_db.for_all_transactions([&count](const crypto::hash&, const transaction&) {
    ++count;
    return true;
  }, false);
  EXPECT_TRUE(result);
  // Should have miner tx + non-miner txs
  EXPECT_EQ(static_cast<int>(1 + m_txs[0].size()), count);

  close_db();
}

TEST_F(LMDBTestWithBlocks, ForAllOutputs)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  int count = 0;
  bool result = m_db.for_all_outputs([&count](uint64_t, const crypto::hash&, uint64_t, size_t) {
    ++count;
    return true;
  });
  EXPECT_TRUE(result);
  EXPECT_GT(count, 0);

  close_db();
}

// ===========================================================================
// ---- Duplicate block addition ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, AddDuplicateBlockThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  // Adding the same block again should throw (TX_EXISTS because miner tx already stored)
  EXPECT_ANY_THROW(add_block_0());

  close_db();
}

// ===========================================================================
// ---- Block DNE errors ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetBlockHeightNonExistentThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  EXPECT_ANY_THROW(m_db.get_block_height(fake_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockHeaderNonExistentThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  EXPECT_ANY_THROW(m_db.get_block_header(fake_hash));

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetBlockBlobNonExistentThrows)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  crypto::hash fake_hash;
  memset(&fake_hash, 0x42, sizeof(fake_hash));
  EXPECT_ANY_THROW(m_db.get_block_blob(fake_hash));

  close_db();
}

// ===========================================================================
// ---- Correct block cumulative difficulties ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, CorrectBlockCumulativeDifficulties)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  // Correct the difficulty of block 1
  std::vector<difficulty_type> new_diffs = { 9999999 };
  EXPECT_NO_THROW(m_db.correct_block_cumulative_difficulties(1, new_diffs));
  EXPECT_EQ(9999999u, m_db.get_block_cumulative_difficulty(1));

  close_db();
}

// ===========================================================================
// ---- get_blocks_from ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetBlocksFrom)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();
  add_block_1();

  std::vector<std::pair<std::pair<blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, blobdata>>>> blocks;
  bool result = m_db.get_blocks_from(0, 1, 10, 100, 1024*1024, blocks, false, false);
  EXPECT_TRUE(result);
  EXPECT_GE(blocks.size(), 1u);

  close_db();
}

// ===========================================================================
// ---- Pruning (basic) ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, CheckPruningOnFreshDB)
{
  open_db();
  EXPECT_TRUE(m_db.check_pruning());
  close_db();
}

TEST_F(LMDBTestWithBlocks, UpdatePruningOnFreshDB)
{
  open_db();
  EXPECT_TRUE(m_db.update_pruning());
  close_db();
}

// ===========================================================================
// ---- get_txids_loose ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetTxidsLooseNoMatch)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Search with a template that is very unlikely to match
  crypto::hash fake_template;
  memset(&fake_template, 0xFF, sizeof(fake_template));
  // With many bits, unlikely to match
  auto results = m_db.get_txids_loose(fake_template, 256, 100);
  // May or may not match depending on actual tx hashes
  // Just verify it does not crash
  (void)results;

  close_db();
}

TEST_F(LMDBTestWithBlocks, GetTxidsLooseZeroBits)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // 0 bits means everything matches
  crypto::hash any_template = crypto::null_hash;
  auto results = m_db.get_txids_loose(any_template, 0, 100);
  // Should match all txids in the database
  uint64_t tx_count = m_db.get_tx_count();
  EXPECT_EQ(tx_count, results.size());

  close_db();
}

// ===========================================================================
// ---- Statistics / profiling ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, ResetAndShowStats)
{
  open_db();

  BlockchainDB* base = &m_db;
  EXPECT_NO_THROW(base->reset_stats());
  EXPECT_NO_THROW(base->show_stats());

  close_db();
}

// ===========================================================================
// ---- Batch abort rollback ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, BatchAbortRollsBackBlock)
{
  open_db();

  m_db.batch_start();
  add_block_0();
  EXPECT_EQ(1u, m_db.height());

  m_db.batch_abort();

  // After abort, the block should be rolled back
  EXPECT_EQ(0u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- Output distribution (after adding blocks) ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, GetOutputDistribution)
{
  open_db();
  db_wtxn_guard guard(&m_db);

  add_block_0();

  // Find an amount that has outputs
  for (const auto& out : m_blocks[0].first.miner_tx.vout)
  {
    uint64_t count = m_db.get_num_outputs(out.amount);
    if (count > 0)
    {
      std::vector<uint64_t> distribution;
      uint64_t base = 0;
      bool result = m_db.get_output_distribution(out.amount, 0, 0, distribution, base);
      EXPECT_TRUE(result);
      break;
    }
  }

  close_db();
}

// ===========================================================================
// ---- Sync after write operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, SyncAfterAddingBlock)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }
  EXPECT_NO_THROW(m_db.sync());
  EXPECT_EQ(1u, m_db.height());
  close_db();
}

// ===========================================================================
// ---- SafeSyncMode with block operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, SafeSyncModeWithBlocks)
{
  open_db();

  m_db.safesyncmode(false);
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }
  EXPECT_EQ(1u, m_db.height());

  m_db.safesyncmode(true);
  {
    db_wtxn_guard guard(&m_db);
    add_block_1();
  }
  EXPECT_EQ(2u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- Persistence across close/open ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, DataPersistsAcrossCloseOpen)
{
  open_db();
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
    add_block_1();
  }
  EXPECT_EQ(2u, m_db.height());
  close_db();

  // Re-open the same database
  open_db();
  EXPECT_EQ(2u, m_db.height());

  crypto::hash blk0_hash = get_block_hash(m_blocks[0].first);
  EXPECT_TRUE(m_db.block_exists(blk0_hash));

  crypto::hash miner_tx_hash = get_transaction_hash(m_blocks[0].first.miner_tx);
  EXPECT_TRUE(m_db.tx_exists(miner_tx_hash));

  close_db();
}

// ===========================================================================
// ---- Batch with block operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, BatchAddMultipleBlocks)
{
  open_db();

  m_db.batch_start();
  add_block_0();
  add_block_1();
  m_db.batch_stop();

  EXPECT_EQ(2u, m_db.height());

  close_db();
}

TEST_F(LMDBTestWithBlocks, BatchStartStopMultipleCycles)
{
  open_db();

  m_db.batch_start();
  add_block_0();
  m_db.batch_stop();
  EXPECT_EQ(1u, m_db.height());

  m_db.batch_start();
  add_block_1();
  m_db.batch_stop();
  EXPECT_EQ(2u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- TXPool mixed with block operations ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, TxPoolWithBlockData)
{
  open_db();

  // Add a block first
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  // Then add txpool entries
  m_db.block_wtxn_start();

  crypto::hash txid;
  memset(&txid, 0xAB, sizeof(txid));

  txpool_tx_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  meta.weight = 500;
  meta.fee = 100;
  meta.set_relay_method(relay_method::fluff);

  m_db.add_txpool_tx(txid, "pool_blob", meta);
  EXPECT_EQ(1u, m_db.get_txpool_tx_count());

  m_db.block_wtxn_stop();

  // Block data should still be accessible
  EXPECT_EQ(1u, m_db.height());

  close_db();
}

// ===========================================================================
// ---- Alt blocks with block data present ----
// ===========================================================================

TEST_F(LMDBTestWithBlocks, AltBlocksWithMainChain)
{
  open_db();

  // Add main chain block
  {
    db_wtxn_guard guard(&m_db);
    add_block_0();
  }

  // Add alt block
  m_db.block_wtxn_start();

  crypto::hash alt_blkid;
  memset(&alt_blkid, 0xDE, sizeof(alt_blkid));

  alt_block_data_t alt_data;
  memset(&alt_data, 0, sizeof(alt_data));
  alt_data.height = 1;
  alt_data.cumulative_weight = 500;

  m_db.add_alt_block(alt_blkid, alt_data, "alt_block_blob");
  EXPECT_EQ(1u, m_db.get_alt_block_count());

  m_db.block_wtxn_stop();

  // Main chain should still have 1 block
  EXPECT_EQ(1u, m_db.height());

  // Drop alt blocks, main chain unaffected
  m_db.block_wtxn_start();
  m_db.drop_alt_blocks();
  m_db.block_wtxn_stop();

  EXPECT_EQ(0u, m_db.get_alt_block_count());
  EXPECT_EQ(1u, m_db.height());

  close_db();
}

} // anonymous namespace
