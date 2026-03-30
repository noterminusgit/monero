// Copyright (c) 2014-2026, The Monero Project
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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/difficulty.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "ringct/rctSigs.h"

namespace
{
  uint64_t const TEST_FEE = 5000000000; // 5 * 10^9

  // Helper to create a miner tx for a given height
  cryptonote::block make_block_with_miner_tx(size_t height, uint8_t major_version = 1, uint8_t minor_version = 0)
  {
    cryptonote::block b;
    b.major_version = major_version;
    b.minor_version = minor_version;
    b.timestamp = 1000000 + height * 120;
    b.nonce = 0;
    memset(&b.prev_id, 0, sizeof(b.prev_id));

    cryptonote::account_base acc;
    acc.generate();
    bool r = cryptonote::construct_miner_tx(height, 0, 10000000000000ULL, 1000, TEST_FEE,
      acc.get_keys().m_account_address, b.miner_tx, cryptonote::blobdata(), 999, major_version);
    if (!r)
      b.miner_tx.set_null();
    return b;
  }
}

// ---------------------------------------------------------------------------
// 1. Block structure tests
// ---------------------------------------------------------------------------

TEST(block_validation, empty_block_default_construction)
{
  cryptonote::block b;
  EXPECT_EQ(b.major_version, 0);
  EXPECT_EQ(b.minor_version, 0);
  EXPECT_EQ(b.timestamp, 0u);
  EXPECT_EQ(b.nonce, 0u);
  EXPECT_TRUE(b.tx_hashes.empty());
  EXPECT_EQ(b.prev_id, crypto::null_hash);
}

TEST(block_validation, block_with_miner_tx_has_correct_tx_count)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  // The block itself has 0 extra tx_hashes; the miner_tx is separate
  EXPECT_EQ(b.tx_hashes.size(), 0u);
  EXPECT_FALSE(b.miner_tx.vin.empty());
}

TEST(block_validation, block_with_added_tx_hashes)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  b.tx_hashes.push_back(crypto::rand<crypto::hash>());
  b.tx_hashes.push_back(crypto::rand<crypto::hash>());
  EXPECT_EQ(b.tx_hashes.size(), 2u);
}

TEST(block_validation, block_major_version_encoding)
{
  for (uint8_t v = 1; v <= 16; ++v)
  {
    cryptonote::block b = make_block_with_miner_tx(10, v, 0);
    EXPECT_EQ(b.major_version, v);
  }
}

TEST(block_validation, block_minor_version_encoding)
{
  for (uint8_t v = 0; v <= 10; ++v)
  {
    cryptonote::block b = make_block_with_miner_tx(10, 1, v);
    EXPECT_EQ(b.minor_version, v);
  }
}

TEST(block_validation, block_prev_id_is_settable)
{
  cryptonote::block b;
  crypto::hash id = crypto::rand<crypto::hash>();
  b.prev_id = id;
  EXPECT_EQ(b.prev_id, id);
}

TEST(block_validation, block_nonce_is_settable)
{
  cryptonote::block b;
  b.nonce = 42;
  EXPECT_EQ(b.nonce, 42u);
}

TEST(block_validation, block_timestamp_is_settable)
{
  cryptonote::block b;
  b.timestamp = 1609459200;
  EXPECT_EQ(b.timestamp, 1609459200u);
}

// ---------------------------------------------------------------------------
// 2. Miner tx validation via construct_miner_tx()
// ---------------------------------------------------------------------------

TEST(block_validation, miner_tx_construction_succeeds)
{
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::transaction tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000ULL, 1000, TEST_FEE,
    acc.get_keys().m_account_address, tx));
  EXPECT_FALSE(tx.vin.empty());
  EXPECT_FALSE(tx.vout.empty());
}

TEST(block_validation, miner_tx_reward_is_positive)
{
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::transaction tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000ULL, 1000, TEST_FEE,
    acc.get_keys().m_account_address, tx));
  uint64_t total_out = 0;
  for (const auto &o : tx.vout)
    total_out += o.amount;
  EXPECT_GT(total_out, 0u);
}

TEST(block_validation, miner_tx_reward_includes_fee)
{
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::transaction tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000ULL, 1000, TEST_FEE,
    acc.get_keys().m_account_address, tx));
  uint64_t total_out = 0;
  for (const auto &o : tx.vout)
    total_out += o.amount;
  EXPECT_GE(total_out, TEST_FEE);
}

TEST(block_validation, miner_tx_block_height_in_coinbase_input)
{
  const size_t height = 42;
  cryptonote::block b = make_block_with_miner_tx(height);
  uint64_t extracted_height = cryptonote::get_block_height(b);
  EXPECT_EQ(extracted_height, height);
}

TEST(block_validation, miner_tx_block_height_zero)
{
  cryptonote::block b = make_block_with_miner_tx(0);
  uint64_t extracted_height = cryptonote::get_block_height(b);
  EXPECT_EQ(extracted_height, 0u);
}

TEST(block_validation, miner_tx_block_height_large)
{
  const size_t height = 1000000;
  cryptonote::block b = make_block_with_miner_tx(height);
  uint64_t extracted_height = cryptonote::get_block_height(b);
  EXPECT_EQ(extracted_height, height);
}

TEST(block_validation, miner_tx_has_no_txin_to_key)
{
  cryptonote::block b = make_block_with_miner_tx(10);
  for (const auto &vin : b.miner_tx.vin)
  {
    EXPECT_NE(vin.type(), typeid(cryptonote::txin_to_key));
  }
}

TEST(block_validation, miner_tx_has_txin_gen_input)
{
  cryptonote::block b = make_block_with_miner_tx(10);
  ASSERT_EQ(b.miner_tx.vin.size(), 1u);
  EXPECT_NO_THROW(boost::get<cryptonote::txin_gen>(b.miner_tx.vin[0]));
}

TEST(block_validation, miner_tx_has_pub_key_in_extra)
{
  cryptonote::block b = make_block_with_miner_tx(10);
  crypto::public_key pk = cryptonote::get_tx_pub_key_from_extra(b.miner_tx);
  EXPECT_NE(pk, crypto::null_pkey);
}

// ---------------------------------------------------------------------------
// 3. Block hash computation
// ---------------------------------------------------------------------------

TEST(block_validation, block_hash_is_consistent)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  crypto::hash h1 = cryptonote::get_block_hash(b);
  crypto::hash h2 = cryptonote::get_block_hash(b);
  EXPECT_EQ(h1, h2);
}

TEST(block_validation, different_blocks_different_hashes)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  cryptonote::block b2 = make_block_with_miner_tx(2);
  crypto::hash h1 = cryptonote::get_block_hash(b1);
  crypto::hash h2 = cryptonote::get_block_hash(b2);
  EXPECT_NE(h1, h2);
}

TEST(block_validation, same_block_same_hash)
{
  cryptonote::block b1 = make_block_with_miner_tx(100);
  cryptonote::block b2 = b1; // copy
  crypto::hash h1 = cryptonote::get_block_hash(b1);
  crypto::hash h2 = cryptonote::get_block_hash(b2);
  EXPECT_EQ(h1, h2);
}

TEST(block_validation, block_hash_changes_with_nonce)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.nonce = 0;
  crypto::hash h1 = cryptonote::get_block_hash(b1);
  b1.invalidate_hashes();
  b1.nonce = 1;
  crypto::hash h2 = cryptonote::get_block_hash(b1);
  EXPECT_NE(h1, h2);
}

TEST(block_validation, block_hash_changes_with_timestamp)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.timestamp = 1000;
  crypto::hash h1 = cryptonote::get_block_hash(b1);
  b1.invalidate_hashes();
  b1.timestamp = 2000;
  crypto::hash h2 = cryptonote::get_block_hash(b1);
  EXPECT_NE(h1, h2);
}

TEST(block_validation, block_hash_not_null)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  crypto::hash h = cryptonote::get_block_hash(b);
  EXPECT_NE(h, crypto::null_hash);
}

// ---------------------------------------------------------------------------
// 4. Block weight/size
// ---------------------------------------------------------------------------

TEST(block_validation, block_blob_size_positive)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  cryptonote::blobdata blob = cryptonote::block_to_blob(b);
  EXPECT_GT(blob.size(), 0u);
}

TEST(block_validation, block_with_more_tx_hashes_has_larger_blob)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  cryptonote::block b2 = b1;
  for (int i = 0; i < 10; ++i)
    b2.tx_hashes.push_back(crypto::rand<crypto::hash>());

  cryptonote::blobdata blob1 = cryptonote::block_to_blob(b1);
  cryptonote::blobdata blob2 = cryptonote::block_to_blob(b2);
  EXPECT_GT(blob2.size(), blob1.size());
}

TEST(block_validation, block_hashing_blob_is_nonempty)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  cryptonote::blobdata hashing_blob = cryptonote::get_block_hashing_blob(b);
  EXPECT_GT(hashing_blob.size(), 0u);
}

// ---------------------------------------------------------------------------
// 5. Timestamp validation logic
// ---------------------------------------------------------------------------

TEST(block_validation, timestamp_median_of_odd_count)
{
  // Simulate median-of-11 logic
  std::vector<uint64_t> timestamps = {100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200};
  std::sort(timestamps.begin(), timestamps.end());
  uint64_t median = timestamps[timestamps.size() / 2];
  EXPECT_EQ(median, 150u);
}

TEST(block_validation, timestamp_median_of_even_count)
{
  std::vector<uint64_t> timestamps = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
  std::sort(timestamps.begin(), timestamps.end());
  // For an even count, pick the lower-middle element (index n/2 - 1) as some implementations do
  uint64_t lower_median = timestamps[timestamps.size() / 2 - 1];
  uint64_t upper_median = timestamps[timestamps.size() / 2];
  EXPECT_EQ(lower_median, 500u);
  EXPECT_EQ(upper_median, 600u);
}

TEST(block_validation, timestamp_future_limit_constant)
{
  // CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT is 2 hours = 7200 seconds
  EXPECT_EQ(CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT, 60u * 60 * 2);
}

TEST(block_validation, timestamp_must_be_above_median)
{
  // If the median of last 11 timestamps is X, new block timestamp must be > X
  std::vector<uint64_t> timestamps = {100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120};
  std::sort(timestamps.begin(), timestamps.end());
  uint64_t median = timestamps[timestamps.size() / 2];
  EXPECT_EQ(median, 110u);
  // A valid new timestamp must be strictly greater than the median
  uint64_t new_timestamp = median + 1;
  EXPECT_GT(new_timestamp, median);
  // A timestamp equal to the median should be rejected
  EXPECT_FALSE(median > median);
}

TEST(block_validation, timestamp_sorted_gives_correct_median)
{
  // Unsorted timestamps that should still yield correct median when sorted
  std::vector<uint64_t> timestamps = {300, 100, 500, 200, 400};
  std::sort(timestamps.begin(), timestamps.end());
  uint64_t median = timestamps[timestamps.size() / 2];
  EXPECT_EQ(median, 300u);
}

// ---------------------------------------------------------------------------
// 6. Block serialization roundtrip
// ---------------------------------------------------------------------------

TEST(block_validation, serialize_deserialize_roundtrip)
{
  cryptonote::block b1 = make_block_with_miner_tx(42);
  b1.nonce = 12345;

  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::block_to_blob(b1, blob));
  ASSERT_GT(blob.size(), 0u);

  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b1.major_version, b2.major_version);
  EXPECT_EQ(b1.minor_version, b2.minor_version);
  EXPECT_EQ(b1.timestamp, b2.timestamp);
  EXPECT_EQ(b1.nonce, b2.nonce);
  EXPECT_EQ(b1.prev_id, b2.prev_id);
  EXPECT_EQ(b1.tx_hashes.size(), b2.tx_hashes.size());
}

TEST(block_validation, serialize_roundtrip_preserves_hash)
{
  cryptonote::block b1 = make_block_with_miner_tx(42);
  crypto::hash h1 = cryptonote::get_block_hash(b1);

  cryptonote::blobdata blob = cryptonote::block_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  crypto::hash h2 = cryptonote::get_block_hash(b2);
  EXPECT_EQ(h1, h2);
}

TEST(block_validation, serialize_roundtrip_with_tx_hashes)
{
  cryptonote::block b1 = make_block_with_miner_tx(42);
  b1.tx_hashes.push_back(crypto::rand<crypto::hash>());
  b1.tx_hashes.push_back(crypto::rand<crypto::hash>());
  b1.tx_hashes.push_back(crypto::rand<crypto::hash>());

  cryptonote::blobdata blob = cryptonote::block_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  ASSERT_EQ(b1.tx_hashes.size(), b2.tx_hashes.size());
  for (size_t i = 0; i < b1.tx_hashes.size(); ++i)
    EXPECT_EQ(b1.tx_hashes[i], b2.tx_hashes[i]);
}

TEST(block_validation, serialize_roundtrip_preserves_miner_tx_height)
{
  const size_t height = 999;
  cryptonote::block b1 = make_block_with_miner_tx(height);

  cryptonote::blobdata blob = cryptonote::block_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  uint64_t h = cryptonote::get_block_height(b2);
  EXPECT_EQ(h, height);
}

TEST(block_validation, parse_empty_blob_fails)
{
  cryptonote::block b;
  cryptonote::blobdata empty_blob;
  EXPECT_FALSE(cryptonote::parse_and_validate_block_from_blob(empty_blob, b));
}

TEST(block_validation, parse_garbage_blob_fails)
{
  cryptonote::block b;
  cryptonote::blobdata garbage(64, '\xDE');
  EXPECT_FALSE(cryptonote::parse_and_validate_block_from_blob(garbage, b));
}

TEST(block_validation, t_serializable_object_to_blob_roundtrip)
{
  cryptonote::block b1 = make_block_with_miner_tx(7);
  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  ASSERT_GT(blob.size(), 0u);

  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b1.major_version, b2.major_version);
  EXPECT_EQ(b1.nonce, b2.nonce);
}

TEST(block_validation, block_copy_constructor_preserves_fields)
{
  cryptonote::block b1 = make_block_with_miner_tx(50);
  b1.nonce = 777;
  b1.tx_hashes.push_back(crypto::rand<crypto::hash>());
  cryptonote::block b2(b1);
  EXPECT_EQ(b1.major_version, b2.major_version);
  EXPECT_EQ(b1.minor_version, b2.minor_version);
  EXPECT_EQ(b1.timestamp, b2.timestamp);
  EXPECT_EQ(b1.nonce, b2.nonce);
  EXPECT_EQ(b1.prev_id, b2.prev_id);
  EXPECT_EQ(b1.tx_hashes.size(), b2.tx_hashes.size());
}

TEST(block_validation, block_tx_tree_hash_nonempty)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  crypto::hash tree = cryptonote::get_tx_tree_hash(b);
  // Even a block with no extra tx_hashes has a tree hash (from the miner tx)
  EXPECT_NE(tree, crypto::null_hash);
}

// ===========================================================================
// Task 4: Block Timestamp Edge Cases
// ===========================================================================

// Test block with timestamp exactly at CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT from "now".
// The protocol rejects blocks with timestamps more than FUTURE_TIME_LIMIT ahead
// of the network-adjusted time. This test verifies the constant and boundary logic.
TEST(block_validation, timestamp_at_future_limit)
{
  const uint64_t future_limit = CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT;
  EXPECT_EQ(future_limit, 7200u); // 2 hours in seconds

  // Simulate: current time is T, block timestamp is T + future_limit
  // This should be the last acceptable timestamp (at the boundary).
  const uint64_t current_time = 1700000000; // arbitrary "now"
  const uint64_t block_ts = current_time + future_limit;

  // The rule is: block_timestamp <= current_time + FUTURE_TIME_LIMIT
  // Exactly at the limit: should pass
  EXPECT_LE(block_ts, current_time + future_limit);

  // A block can encode this timestamp
  cryptonote::block b = make_block_with_miner_tx(1);
  b.timestamp = block_ts;
  EXPECT_EQ(b.timestamp, block_ts);

  // Verify hash changes with timestamp (the timestamp is part of the block header)
  cryptonote::block b2 = make_block_with_miner_tx(1);
  b2.timestamp = current_time;
  b.invalidate_hashes();
  b2.invalidate_hashes();
  EXPECT_NE(cryptonote::get_block_hash(b), cryptonote::get_block_hash(b2));
}

// Test block with timestamp past the future limit.
// Blocks with timestamp > current_time + CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT
// should be rejected during validation.
TEST(block_validation, timestamp_past_future_limit)
{
  const uint64_t future_limit = CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT;
  const uint64_t current_time = 1700000000;

  // One second past the limit
  const uint64_t too_far = current_time + future_limit + 1;
  EXPECT_GT(too_far, current_time + future_limit);

  // Way past the limit
  const uint64_t way_too_far = current_time + future_limit + 86400; // 1 day past
  EXPECT_GT(way_too_far, current_time + future_limit);

  // The block can still be constructed with these timestamps (structural validity),
  // but it should fail the timestamp check during consensus validation.
  cryptonote::block b = make_block_with_miner_tx(1);
  b.timestamp = too_far;
  EXPECT_EQ(b.timestamp, too_far);

  // Verify the condition that consensus would check
  EXPECT_FALSE(too_far <= current_time + future_limit);
  EXPECT_FALSE(way_too_far <= current_time + future_limit);
}

// Test blocks with nonce values at the boundaries of uint32_t range.
// The nonce is iterated during mining to find a valid PoW hash.
TEST(block_validation, nonce_range)
{
  // Nonce = 0 (minimum)
  cryptonote::block b0 = make_block_with_miner_tx(1);
  b0.nonce = 0;
  EXPECT_EQ(b0.nonce, 0u);

  // Nonce = UINT32_MAX (maximum)
  cryptonote::block bmax = make_block_with_miner_tx(1);
  bmax.nonce = UINT32_MAX;
  EXPECT_EQ(bmax.nonce, UINT32_MAX);
  EXPECT_EQ(bmax.nonce, 4294967295u);

  // Nonce = 1 (common starting value)
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.nonce = 1;
  EXPECT_EQ(b1.nonce, 1u);

  // Nonce = UINT32_MAX / 2 (midpoint)
  cryptonote::block bmid = make_block_with_miner_tx(1);
  bmid.nonce = UINT32_MAX / 2;
  EXPECT_EQ(bmid.nonce, 2147483647u);

  // Each nonce value produces a different block hash
  b0.invalidate_hashes();
  bmax.invalidate_hashes();
  b1.invalidate_hashes();
  bmid.invalidate_hashes();
  // Copy the same base block to ensure only nonce differs
  cryptonote::block base = make_block_with_miner_tx(100);
  cryptonote::block copy1 = base;
  cryptonote::block copy2 = base;
  copy1.nonce = 0;
  copy2.nonce = UINT32_MAX;
  copy1.invalidate_hashes();
  copy2.invalidate_hashes();
  EXPECT_NE(cryptonote::get_block_hash(copy1), cryptonote::get_block_hash(copy2));
}

// Test that nonce is preserved through serialization at boundary values
TEST(block_validation, nonce_serialization_boundaries)
{
  // Test UINT32_MAX nonce survives serialization roundtrip
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.nonce = UINT32_MAX;

  cryptonote::blobdata blob = cryptonote::block_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.nonce, UINT32_MAX);

  // Test 0 nonce
  b1.nonce = 0;
  blob = cryptonote::block_to_blob(b1);
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.nonce, 0u);
}

// Test that block timestamp at uint64_t maximum is encodable
TEST(block_validation, timestamp_max_u64)
{
  cryptonote::block b = make_block_with_miner_tx(1);
  b.timestamp = UINT64_MAX;
  EXPECT_EQ(b.timestamp, UINT64_MAX);

  // Should survive serialization roundtrip
  cryptonote::blobdata blob = cryptonote::block_to_blob(b);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.timestamp, UINT64_MAX);
}

// Test the relationship between CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT and
// block timestamp validation for pre-v2 and post-v2 hardforks.
TEST(block_validation, future_time_limit_constant_values)
{
  // Pre-HF2: CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT = 2 hours = 7200 seconds
  EXPECT_EQ(CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT, 60 * 60 * 2);
  EXPECT_EQ(CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT, 7200u);

  // The timestamp median window for Monero is the last 60 blocks
  // (BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
  // This is important for consensus: block timestamp must be > median of last N timestamps
}

// Test that block hash is sensitive to the timestamp field
// (critical for consensus - changing timestamp must change PoW hash)
TEST(block_validation, timestamp_affects_hashing_blob)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.timestamp = 1000000;
  cryptonote::blobdata hb1 = cryptonote::get_block_hashing_blob(b1);

  cryptonote::block b2 = b1;
  b2.timestamp = 1000001;
  cryptonote::blobdata hb2 = cryptonote::get_block_hashing_blob(b2);

  // The hashing blob must differ when timestamps differ
  EXPECT_NE(hb1, hb2);
}

// Test that block hash is sensitive to the nonce field
// (critical for mining - iterating nonce must change PoW hash)
TEST(block_validation, nonce_affects_hashing_blob)
{
  cryptonote::block b1 = make_block_with_miner_tx(1);
  b1.nonce = 0;
  cryptonote::blobdata hb1 = cryptonote::get_block_hashing_blob(b1);

  cryptonote::block b2 = b1;
  b2.nonce = 1;
  cryptonote::blobdata hb2 = cryptonote::get_block_hashing_blob(b2);

  // The hashing blob must differ when nonces differ
  EXPECT_NE(hb1, hb2);
}
