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

// Phase F: Lightweight integration tests using mock blockchain databases.

#include "gtest/gtest.h"
#include "blockchain_db/testdb.h"
#include "mocks/mock_blockchain.h"
#include "mocks/mock_core.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

// ============================================================================
// InMemoryDB basic operations
// ============================================================================

TEST(core_integration, inmemorydb_initial_height)
{
  test::InMemoryDB db;
  EXPECT_EQ(0u, db.height());
}

TEST(core_integration, inmemorydb_top_hash_empty)
{
  test::InMemoryDB db;
  EXPECT_EQ(crypto::null_hash, db.top_block_hash());
}

TEST(core_integration, inmemorydb_add_block_increases_height)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash hash;
  memset(&hash, 1, sizeof(hash));
  db.add_test_block(0, blk, hash);
  EXPECT_EQ(1u, db.height());
}

TEST(core_integration, inmemorydb_add_multiple_blocks)
{
  test::InMemoryDB db;
  for (uint64_t i = 0; i < 10; ++i)
  {
    cryptonote::block blk;
    crypto::hash hash;
    memset(&hash, (int)(i + 1), sizeof(hash));
    db.add_test_block(i, blk, hash, i * 120, i + 1, i * 1000000, 128, 128);
  }
  EXPECT_EQ(10u, db.height());
}

TEST(core_integration, inmemorydb_hash_retrieval)
{
  test::InMemoryDB db;
  crypto::hash h1, h2;
  memset(&h1, 0xAA, sizeof(h1));
  memset(&h2, 0xBB, sizeof(h2));
  cryptonote::block blk;
  db.add_test_block(0, blk, h1);
  db.add_test_block(1, blk, h2);
  EXPECT_EQ(h1, db.get_block_hash_from_height(0));
  EXPECT_EQ(h2, db.get_block_hash_from_height(1));
}

TEST(core_integration, inmemorydb_top_block_hash)
{
  test::InMemoryDB db;
  crypto::hash h;
  memset(&h, 0xCC, sizeof(h));
  cryptonote::block blk;
  db.add_test_block(0, blk, h);
  EXPECT_EQ(h, db.top_block_hash());
}

TEST(core_integration, inmemorydb_timestamp_storage)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash hash;
  memset(&hash, 1, sizeof(hash));
  db.add_test_block(0, blk, hash, 1609459200);
  EXPECT_EQ(1609459200u, db.get_block_timestamp(0));
}

TEST(core_integration, inmemorydb_cumulative_difficulty)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash hash;
  memset(&hash, 1, sizeof(hash));
  db.add_test_block(0, blk, hash, 0, 100);
  db.add_test_block(1, blk, hash, 0, 250);
  EXPECT_EQ(100u, db.get_block_cumulative_difficulty(0));
  EXPECT_EQ(250u, db.get_block_cumulative_difficulty(1));
}

TEST(core_integration, inmemorydb_generated_coins)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash hash;
  memset(&hash, 1, sizeof(hash));
  db.add_test_block(0, blk, hash, 0, 0, 5000000000ULL);
  EXPECT_EQ(5000000000ULL, db.get_block_already_generated_coins(0));
}

TEST(core_integration, inmemorydb_block_weight)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash hash;
  memset(&hash, 1, sizeof(hash));
  db.add_test_block(0, blk, hash, 0, 0, 0, 256, 200);
  EXPECT_EQ(256u, db.get_block_weight(0));
  EXPECT_EQ(200u, db.get_block_long_term_weight(0));
}

TEST(core_integration, inmemorydb_block_exists)
{
  test::InMemoryDB db;
  crypto::hash h;
  memset(&h, 0xDD, sizeof(h));
  cryptonote::block blk;
  db.add_test_block(0, blk, h);
  EXPECT_TRUE(db.block_exists(h));

  crypto::hash other;
  memset(&other, 0xEE, sizeof(other));
  EXPECT_FALSE(db.block_exists(other));
}

TEST(core_integration, inmemorydb_block_exists_returns_height)
{
  test::InMemoryDB db;
  crypto::hash h;
  memset(&h, 0xDD, sizeof(h));
  cryptonote::block blk;
  db.add_test_block(5, blk, h);
  uint64_t height = 0;
  EXPECT_TRUE(db.block_exists(h, &height));
  EXPECT_EQ(5u, height);
}

TEST(core_integration, inmemorydb_get_block_from_height)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  blk.timestamp = 12345;
  blk.nonce = 99;
  crypto::hash h;
  memset(&h, 1, sizeof(h));
  db.add_test_block(0, blk, h);
  cryptonote::block retrieved = db.get_block_from_height(0);
  EXPECT_EQ(12345u, retrieved.timestamp);
  EXPECT_EQ(99u, retrieved.nonce);
}

// ============================================================================
// Key image tracking
// ============================================================================

TEST(core_integration, inmemorydb_key_image_absent)
{
  test::InMemoryDB db;
  crypto::key_image ki;
  memset(&ki, 0xAA, sizeof(ki));
  EXPECT_FALSE(db.has_key_image(ki));
}

TEST(core_integration, inmemorydb_key_image_add_and_check)
{
  test::InMemoryDB db;
  crypto::key_image ki;
  memset(&ki, 0xAA, sizeof(ki));
  db.add_spent_key(ki);
  EXPECT_TRUE(db.has_key_image(ki));
}

TEST(core_integration, inmemorydb_key_image_remove)
{
  test::InMemoryDB db;
  crypto::key_image ki;
  memset(&ki, 0xAA, sizeof(ki));
  db.add_spent_key(ki);
  EXPECT_TRUE(db.has_key_image(ki));
  db.remove_spent_key(ki);
  EXPECT_FALSE(db.has_key_image(ki));
}

TEST(core_integration, inmemorydb_multiple_key_images)
{
  test::InMemoryDB db;
  crypto::key_image ki1, ki2, ki3;
  memset(&ki1, 1, sizeof(ki1));
  memset(&ki2, 2, sizeof(ki2));
  memset(&ki3, 3, sizeof(ki3));
  db.add_spent_key(ki1);
  db.add_spent_key(ki2);
  EXPECT_TRUE(db.has_key_image(ki1));
  EXPECT_TRUE(db.has_key_image(ki2));
  EXPECT_FALSE(db.has_key_image(ki3));
}

// ============================================================================
// Hard fork version
// ============================================================================

TEST(core_integration, inmemorydb_hard_fork_version_default)
{
  test::InMemoryDB db;
  EXPECT_EQ(16, db.get_hard_fork_version(0));
}

TEST(core_integration, inmemorydb_hard_fork_version_set)
{
  test::InMemoryDB db;
  db.set_hard_fork_version(0, 14);
  EXPECT_EQ(14, db.get_hard_fork_version(0));
}

TEST(core_integration, inmemorydb_tx_count)
{
  test::InMemoryDB db;
  EXPECT_EQ(0u, db.get_tx_count());
  cryptonote::transaction tx;
  crypto::hash txid;
  memset(&txid, 1, sizeof(txid));
  db.add_test_tx(txid, tx);
  EXPECT_EQ(1u, db.get_tx_count());
}

TEST(core_integration, inmemorydb_db_size)
{
  test::InMemoryDB db;
  EXPECT_EQ(0u, db.get_database_size());
  db.set_db_size(1024 * 1024);
  EXPECT_EQ(1024u * 1024u, db.get_database_size());
}

// ============================================================================
// TrackingTestDB
// ============================================================================

TEST(core_integration, trackingdb_initial_height)
{
  test::TrackingTestDB db;
  EXPECT_EQ(1u, db.height());
}

TEST(core_integration, trackingdb_set_height)
{
  test::TrackingTestDB db;
  db.set_height(100);
  EXPECT_EQ(100u, db.height());
}

TEST(core_integration, trackingdb_top_hash)
{
  test::TrackingTestDB db;
  EXPECT_EQ(crypto::null_hash, db.top_block_hash());
  crypto::hash h;
  memset(&h, 0xFF, sizeof(h));
  db.set_top_hash(h);
  EXPECT_EQ(h, db.top_block_hash());
}

TEST(core_integration, trackingdb_cumulative_difficulty)
{
  test::TrackingTestDB db;
  EXPECT_EQ(1u, db.get_block_cumulative_difficulty(0));
  db.set_cumulative_difficulty(12345);
  EXPECT_EQ(12345u, db.get_block_cumulative_difficulty(0));
}

TEST(core_integration, trackingdb_timestamps)
{
  test::TrackingTestDB db;
  db.set_block_timestamp(0, 1000);
  db.set_block_timestamp(5, 2000);
  EXPECT_EQ(1000u, db.get_block_timestamp(0));
  EXPECT_EQ(2000u, db.get_block_timestamp(5));
  EXPECT_EQ(0u, db.get_block_timestamp(99));
}

TEST(core_integration, trackingdb_hard_fork_default)
{
  test::TrackingTestDB db;
  EXPECT_EQ(16, db.get_hard_fork_version(0));
  db.set_default_hf_version(14);
  EXPECT_EQ(14, db.get_hard_fork_version(0));
}

TEST(core_integration, trackingdb_generated_coins)
{
  test::TrackingTestDB db;
  EXPECT_EQ(10000000000ULL, db.get_block_already_generated_coins(0));
  db.set_already_generated_coins(99999);
  EXPECT_EQ(99999u, db.get_block_already_generated_coins(0));
}

TEST(core_integration, trackingdb_block_weight)
{
  test::TrackingTestDB db;
  EXPECT_EQ(128u, db.get_block_weight(0));
  db.set_block_weight(512);
  EXPECT_EQ(512u, db.get_block_weight(0));
}

TEST(core_integration, trackingdb_key_images)
{
  test::TrackingTestDB db;
  crypto::key_image ki;
  memset(&ki, 0xAA, sizeof(ki));
  EXPECT_FALSE(db.has_key_image(ki));
  db.add_spent_key(ki);
  EXPECT_TRUE(db.has_key_image(ki));
  db.remove_spent_key(ki);
  EXPECT_FALSE(db.has_key_image(ki));
}

TEST(core_integration, trackingdb_num_outputs)
{
  test::TrackingTestDB db;
  EXPECT_EQ(1u, db.get_num_outputs(0));
  db.set_num_outputs(500);
  EXPECT_EQ(500u, db.get_num_outputs(0));
}

TEST(core_integration, trackingdb_tx_count)
{
  test::TrackingTestDB db;
  EXPECT_EQ(0u, db.get_tx_count());
  db.set_tx_count(42);
  EXPECT_EQ(42u, db.get_tx_count());
}

// ============================================================================
// Miner transaction construction
// ============================================================================

TEST(core_integration, construct_miner_tx_v1)
{
  cryptonote::transaction tx;
  cryptonote::account_base acc;
  acc.generate();
  // height=0, median_weight=0, already_generated_coins=0, current_block_weight=200, fee=0
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 200, 0, acc.get_keys().m_account_address, tx);
  EXPECT_TRUE(r);
  EXPECT_FALSE(tx.vout.empty());
}

TEST(core_integration, construct_miner_tx_v2)
{
  cryptonote::transaction tx;
  cryptonote::account_base acc;
  acc.generate();
  // hard_fork_version=12
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 200, 0, acc.get_keys().m_account_address, tx, cryptonote::blobdata(), 999, 12);
  EXPECT_TRUE(r);
  EXPECT_FALSE(tx.vout.empty());
}

TEST(core_integration, construct_miner_tx_height_encoded)
{
  cryptonote::transaction tx;
  cryptonote::account_base acc;
  acc.generate();
  uint64_t height = 42;
  bool r = cryptonote::construct_miner_tx(height, 0, 0, 200, 0, acc.get_keys().m_account_address, tx);
  EXPECT_TRUE(r);
  ASSERT_FALSE(tx.vin.empty());
  ASSERT_TRUE(tx.vin[0].type() == typeid(cryptonote::txin_gen));
  EXPECT_EQ(height, boost::get<cryptonote::txin_gen>(tx.vin[0]).height);
}

TEST(core_integration, construct_miner_tx_has_output)
{
  cryptonote::transaction tx;
  cryptonote::account_base acc;
  acc.generate();
  bool r = cryptonote::construct_miner_tx(0, 0, 0, 200, 0, acc.get_keys().m_account_address, tx);
  EXPECT_TRUE(r);
  EXPECT_GE(tx.vout.size(), 1u);
}

// ============================================================================
// Block hash operations
// ============================================================================

TEST(core_integration, block_hash_deterministic)
{
  cryptonote::block blk;
  blk.major_version = 1;
  blk.minor_version = 0;
  blk.timestamp = 1000;
  blk.nonce = 42;
  crypto::hash h1 = cryptonote::get_block_hash(blk);
  crypto::hash h2 = cryptonote::get_block_hash(blk);
  EXPECT_EQ(h1, h2);
}

TEST(core_integration, block_hash_changes_with_nonce)
{
  cryptonote::block blk1, blk2;
  blk1.major_version = blk2.major_version = 1;
  blk1.minor_version = blk2.minor_version = 0;
  blk1.timestamp = blk2.timestamp = 1000;
  blk1.nonce = 42;
  blk2.nonce = 43;
  EXPECT_NE(cryptonote::get_block_hash(blk1), cryptonote::get_block_hash(blk2));
}

TEST(core_integration, block_hash_changes_with_timestamp)
{
  cryptonote::block blk1, blk2;
  blk1.major_version = blk2.major_version = 1;
  blk1.minor_version = blk2.minor_version = 0;
  blk1.nonce = blk2.nonce = 42;
  blk1.timestamp = 1000;
  blk2.timestamp = 2000;
  EXPECT_NE(cryptonote::get_block_hash(blk1), cryptonote::get_block_hash(blk2));
}

TEST(core_integration, block_hash_not_null)
{
  cryptonote::block blk;
  blk.major_version = 1;
  blk.nonce = 1;
  crypto::hash h = cryptonote::get_block_hash(blk);
  EXPECT_NE(crypto::null_hash, h);
}

TEST(core_integration, block_blob_roundtrip)
{
  cryptonote::block blk;
  blk.major_version = 14;
  blk.minor_version = 14;
  blk.timestamp = 1609459200;
  blk.nonce = 12345;

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(blk);
  EXPECT_FALSE(blob.empty());

  cryptonote::block blk2;
  EXPECT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, blk2));
  EXPECT_EQ(blk.major_version, blk2.major_version);
  EXPECT_EQ(blk.minor_version, blk2.minor_version);
  EXPECT_EQ(blk.timestamp, blk2.timestamp);
  EXPECT_EQ(blk.nonce, blk2.nonce);
}

TEST(core_integration, block_blob_size_positive)
{
  cryptonote::block blk;
  blk.major_version = 1;
  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(blk);
  EXPECT_GT(blob.size(), 0u);
}

// ============================================================================
// TestBlockchainContext
// ============================================================================

// TestBlockchainContext::init requires full Blockchain::init which needs
// genesis block storage support. Test DB setup operations directly instead.

TEST(core_integration, test_db_open_flag)
{
  test::TrackingTestDB db;
  db.open("");
  EXPECT_TRUE(db.is_open());
}

TEST(core_integration, test_db_operations_after_open)
{
  test::TrackingTestDB db;
  db.open("");
  db.set_height(100);
  EXPECT_EQ(100u, db.height());
  db.set_block_timestamp(50, 1609459200);
  EXPECT_EQ(1609459200u, db.get_block_timestamp(50));
  db.set_default_hf_version(14);
  EXPECT_EQ(14, db.get_hard_fork_version(0));
}

TEST(core_integration, inmemorydb_open_flag)
{
  test::InMemoryDB db;
  db.open("");
  EXPECT_TRUE(db.is_open());
}

TEST(core_integration, test_db_batch_operations)
{
  test::TrackingTestDB db;
  db.open("");
  EXPECT_TRUE(db.batch_start());
  db.batch_stop();
  db.batch_abort();
}

// ============================================================================
// Bug #10 regression: missed_ids dual-purpose (blocks + transactions)
// Tests verify InMemoryDB block_exists, tx_exists, get_tx_blob which underlie
// the Blockchain::get_blocks / get_transactions_blobs logic that populates
// rsp.missed_ids for both missed blocks and missed transactions.
// ============================================================================

TEST(core_integration, inmemorydb_block_exists_valid_hash)
{
  test::InMemoryDB db;
  crypto::hash h1, h2, h3;
  memset(&h1, 0x11, sizeof(h1));
  memset(&h2, 0x22, sizeof(h2));
  memset(&h3, 0x33, sizeof(h3));
  cryptonote::block blk;
  db.add_test_block(0, blk, h1);
  db.add_test_block(1, blk, h2);
  db.add_test_block(2, blk, h3);

  // All three hashes should be found
  EXPECT_TRUE(db.block_exists(h1));
  EXPECT_TRUE(db.block_exists(h2));
  EXPECT_TRUE(db.block_exists(h3));
}

TEST(core_integration, inmemorydb_block_exists_invalid_hash_populates_miss)
{
  test::InMemoryDB db;
  crypto::hash h1;
  memset(&h1, 0x11, sizeof(h1));
  cryptonote::block blk;
  db.add_test_block(0, blk, h1);

  // A hash that was never added should not be found
  crypto::hash missing;
  memset(&missing, 0xFF, sizeof(missing));
  EXPECT_FALSE(db.block_exists(missing));

  // Simulate what get_blocks does: check block_exists, record misses
  std::vector<crypto::hash> requested = {h1, missing};
  std::vector<crypto::hash> missed_ids;
  for (const auto& hash : requested)
  {
    if (!db.block_exists(hash))
      missed_ids.push_back(hash);
  }
  ASSERT_EQ(1u, missed_ids.size());
  EXPECT_EQ(missing, missed_ids[0]);
}

TEST(core_integration, inmemorydb_block_exists_mixed_valid_invalid)
{
  test::InMemoryDB db;
  cryptonote::block blk;
  // Add blocks at heights 0, 2, 4 (gaps at 1, 3)
  crypto::hash h0, h2, h4;
  memset(&h0, 0x10, sizeof(h0));
  memset(&h2, 0x20, sizeof(h2));
  memset(&h4, 0x40, sizeof(h4));
  db.add_test_block(0, blk, h0);
  db.add_test_block(2, blk, h2);
  db.add_test_block(4, blk, h4);

  // Request all including some invalid
  crypto::hash bad1, bad2;
  memset(&bad1, 0xAA, sizeof(bad1));
  memset(&bad2, 0xBB, sizeof(bad2));

  std::vector<crypto::hash> requested = {h0, bad1, h2, bad2, h4};
  std::vector<crypto::hash> found;
  std::vector<crypto::hash> missed;
  for (const auto& hash : requested)
  {
    if (db.block_exists(hash))
      found.push_back(hash);
    else
      missed.push_back(hash);
  }
  EXPECT_EQ(3u, found.size());
  EXPECT_EQ(2u, missed.size());
  EXPECT_EQ(bad1, missed[0]);
  EXPECT_EQ(bad2, missed[1]);
}

TEST(core_integration, inmemorydb_tx_exists_present)
{
  test::InMemoryDB db;
  cryptonote::transaction tx;
  crypto::hash txid;
  memset(&txid, 0xAB, sizeof(txid));
  db.add_test_tx(txid, tx);

  EXPECT_TRUE(db.tx_exists(txid));
}

TEST(core_integration, inmemorydb_tx_exists_absent)
{
  test::InMemoryDB db;
  crypto::hash txid;
  memset(&txid, 0xAB, sizeof(txid));

  EXPECT_FALSE(db.tx_exists(txid));
}

TEST(core_integration, inmemorydb_tx_exists_with_index)
{
  test::InMemoryDB db;
  cryptonote::transaction tx;
  crypto::hash txid;
  memset(&txid, 0xCD, sizeof(txid));
  db.add_test_tx(txid, tx);

  uint64_t tx_index = 999;
  EXPECT_TRUE(db.tx_exists(txid, tx_index));
  EXPECT_EQ(0u, tx_index);
}

TEST(core_integration, inmemorydb_tx_exists_with_index_absent)
{
  test::InMemoryDB db;
  crypto::hash txid;
  memset(&txid, 0xCD, sizeof(txid));

  uint64_t tx_index = 999;
  EXPECT_FALSE(db.tx_exists(txid, tx_index));
  // tx_index should remain unchanged on failure
  EXPECT_EQ(999u, tx_index);
}

TEST(core_integration, inmemorydb_get_tx_blob_present)
{
  test::InMemoryDB db;
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 0;
  crypto::hash txid;
  memset(&txid, 0xEF, sizeof(txid));
  db.add_test_tx(txid, tx);

  cryptonote::blobdata blob;
  EXPECT_TRUE(db.get_tx_blob(txid, blob));
  EXPECT_FALSE(blob.empty());
}

TEST(core_integration, inmemorydb_get_tx_blob_absent)
{
  test::InMemoryDB db;
  crypto::hash txid;
  memset(&txid, 0xEF, sizeof(txid));

  cryptonote::blobdata blob;
  EXPECT_FALSE(db.get_tx_blob(txid, blob));
}

TEST(core_integration, inmemorydb_get_tx_present)
{
  test::InMemoryDB db;
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 42;
  crypto::hash txid;
  memset(&txid, 0x77, sizeof(txid));
  db.add_test_tx(txid, tx);

  cryptonote::transaction retrieved;
  EXPECT_TRUE(db.get_tx(txid, retrieved));
  EXPECT_EQ(2u, retrieved.version);
  EXPECT_EQ(42u, retrieved.unlock_time);
}

TEST(core_integration, inmemorydb_get_tx_absent)
{
  test::InMemoryDB db;
  crypto::hash txid;
  memset(&txid, 0x77, sizeof(txid));

  cryptonote::transaction retrieved;
  EXPECT_FALSE(db.get_tx(txid, retrieved));
}

TEST(core_integration, inmemorydb_missed_ids_dual_purpose_simulation)
{
  // Simulate the dual-purpose missed_ids behavior from handle_get_objects:
  // First collect missed block hashes, then collect missed tx hashes into
  // the same container.
  test::InMemoryDB db;
  cryptonote::block blk;
  crypto::hash blk_hash;
  memset(&blk_hash, 0x11, sizeof(blk_hash));
  db.add_test_block(0, blk, blk_hash);

  // Add one tx but not another
  cryptonote::transaction tx;
  crypto::hash tx_present, tx_missing;
  memset(&tx_present, 0xAA, sizeof(tx_present));
  memset(&tx_missing, 0xBB, sizeof(tx_missing));
  db.add_test_tx(tx_present, tx);

  // Phase 1: Check blocks (simulating get_blocks)
  std::vector<crypto::hash> missed_ids;
  crypto::hash missing_block;
  memset(&missing_block, 0xFF, sizeof(missing_block));

  std::vector<crypto::hash> block_requests = {blk_hash, missing_block};
  for (const auto& h : block_requests)
  {
    if (!db.block_exists(h))
      missed_ids.push_back(h);
  }
  ASSERT_EQ(1u, missed_ids.size());
  EXPECT_EQ(missing_block, missed_ids[0]);

  // Phase 2: Check transactions (simulating get_transactions_blobs)
  std::vector<crypto::hash> missed_tx_ids;
  std::vector<crypto::hash> tx_requests = {tx_present, tx_missing};
  for (const auto& h : tx_requests)
  {
    if (!db.tx_exists(h))
      missed_tx_ids.push_back(h);
  }

  // Append missed tx hashes to missed_ids (dual-purpose, like handle_get_objects)
  missed_ids.insert(missed_ids.end(), missed_tx_ids.begin(), missed_tx_ids.end());

  // missed_ids now contains both a missed block hash and a missed tx hash
  ASSERT_EQ(2u, missed_ids.size());
  EXPECT_EQ(missing_block, missed_ids[0]);
  EXPECT_EQ(tx_missing, missed_ids[1]);
}

TEST(core_integration, inmemorydb_get_tx_blob_roundtrip)
{
  // Verify that get_tx_blob returns a consistent blob for the stored tx
  test::InMemoryDB db;
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 100;
  crypto::hash txid;
  memset(&txid, 0x55, sizeof(txid));
  db.add_test_tx(txid, tx);

  cryptonote::blobdata blob;
  ASSERT_TRUE(db.get_tx_blob(txid, blob));
  EXPECT_FALSE(blob.empty());

  // The blob should match what t_serializable_object_to_blob produces
  cryptonote::blobdata expected_blob = cryptonote::t_serializable_object_to_blob(tx);
  EXPECT_EQ(expected_blob, blob);

  // Fetching a second time should return the same blob
  cryptonote::blobdata blob2;
  ASSERT_TRUE(db.get_tx_blob(txid, blob2));
  EXPECT_EQ(blob, blob2);
}

TEST(core_integration, inmemorydb_multiple_txs_independent)
{
  test::InMemoryDB db;
  crypto::hash txid1, txid2, txid3;
  memset(&txid1, 0x01, sizeof(txid1));
  memset(&txid2, 0x02, sizeof(txid2));
  memset(&txid3, 0x03, sizeof(txid3));

  cryptonote::transaction tx1, tx2;
  tx1.version = 1;
  tx2.version = 2;
  db.add_test_tx(txid1, tx1);
  db.add_test_tx(txid2, tx2);

  EXPECT_TRUE(db.tx_exists(txid1));
  EXPECT_TRUE(db.tx_exists(txid2));
  EXPECT_FALSE(db.tx_exists(txid3));

  EXPECT_EQ(2u, db.get_tx_count());
}
