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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "serialization/binary_utils.h"
#include "ringct/rctSigs.h"
#include "ringct/rctTypes.h"
#include <sstream>

// ---------------------------------------------------------------------------
// 1. Basic transaction serialization/deserialization
// ---------------------------------------------------------------------------

TEST(tx_serialization, empty_v1_tx_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 1;

  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx1, blob));
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.version, 1u);
  EXPECT_EQ(tx2.unlock_time, 0u);
  EXPECT_TRUE(tx2.vin.empty());
  EXPECT_TRUE(tx2.vout.empty());
  EXPECT_TRUE(tx2.extra.empty());
}

TEST(tx_serialization, empty_v2_tx_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 2;

  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx1, blob));
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.version, 2u);
  EXPECT_EQ(tx2.unlock_time, 0u);
  EXPECT_TRUE(tx2.vin.empty());
  EXPECT_TRUE(tx2.vout.empty());
  EXPECT_TRUE(tx2.extra.empty());
}

TEST(tx_serialization, version_field_preserved)
{
  for (size_t v = 1; v <= 2; ++v)
  {
    cryptonote::transaction tx1;
    tx1.version = v;
    cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(tx1);
    cryptonote::transaction tx2;
    ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
    EXPECT_EQ(tx2.version, v);
  }
}

TEST(tx_serialization, unlock_time_preserved)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 123456789;

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(tx1);
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.unlock_time, 123456789u);
}

TEST(tx_serialization, extra_field_preserved)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.extra = {0x01, 0x02, 0x03, 0xAA, 0xBB, 0xCC};

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(tx1);
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(tx2.extra.size(), tx1.extra.size());
  EXPECT_EQ(tx2.extra, tx1.extra);
}

// ---------------------------------------------------------------------------
// 2. Transaction prefix serialization
// ---------------------------------------------------------------------------

TEST(tx_serialization, tx_to_blob_and_parse_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 42;
  tx1.extra = {0xDE, 0xAD};

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.version, tx1.version);
  EXPECT_EQ(tx2.unlock_time, tx1.unlock_time);
  EXPECT_EQ(tx2.extra, tx1.extra);
}

TEST(tx_serialization, prefix_hash_consistency)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 100;
  tx1.extra = {0x01, 0x02};

  crypto::hash hash_before = cryptonote::get_transaction_prefix_hash(tx1);

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));

  crypto::hash hash_after = cryptonote::get_transaction_prefix_hash(tx2);
  EXPECT_EQ(hash_before, hash_after);
}

TEST(tx_serialization, blob_is_deterministic)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 999;
  tx1.extra = {0xFF, 0xFE, 0xFD};

  cryptonote::blobdata blob1 = cryptonote::tx_to_blob(tx1);
  cryptonote::blobdata blob2 = cryptonote::tx_to_blob(tx1);
  EXPECT_EQ(blob1, blob2);
}

// ---------------------------------------------------------------------------
// 3. RCT type serialization
// ---------------------------------------------------------------------------

// RCT type field encoding: verify different types produce different blobs.
// Full RCT serialization with complete prunable data is tested in ringct.cpp.

// RCT type field encoding: verify different types produce different blobs
// Full RCT serialization with complete prunable data is tested in ringct.cpp

TEST(tx_serialization, rct_type_null_roundtrip)
{
  // RCTTypeNull is used for coinbase-like v2 txs; it roundtrips cleanly
  cryptonote::transaction tx1;
  tx1.version = 2;
  tx1.rct_signatures.type = rct::RCTTypeNull;

  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx1, blob));
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.rct_signatures.type, rct::RCTTypeNull);
}

TEST(tx_serialization, v2_rct_null_with_outputs_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 2;
  tx1.rct_signatures.type = rct::RCTTypeNull;

  // Add an output
  cryptonote::tx_out out;
  out.amount = 100;
  cryptonote::txout_to_key out_key;
  memset(&out_key.key, 0xAA, sizeof(out_key.key));
  out.target = out_key;
  tx1.vout.push_back(out);

  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(tx1, blob));
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  EXPECT_EQ(tx2.version, 2u);
  EXPECT_EQ(tx2.rct_signatures.type, rct::RCTTypeNull);
  ASSERT_EQ(tx2.vout.size(), 1u);
  EXPECT_EQ(tx2.vout[0].amount, 100u);
}

TEST(tx_serialization, v2_no_inputs_different_from_v1)
{
  cryptonote::transaction tx_v1, tx_v2;
  tx_v1.version = 1;
  tx_v2.version = 2;

  cryptonote::blobdata blob_v1 = cryptonote::t_serializable_object_to_blob(tx_v1);
  cryptonote::blobdata blob_v2 = cryptonote::t_serializable_object_to_blob(tx_v2);
  EXPECT_NE(blob_v1, blob_v2);
}

TEST(tx_serialization, v2_version_preserved_in_blob)
{
  cryptonote::transaction tx1;
  tx1.version = 2;
  tx1.rct_signatures.type = rct::RCTTypeNull;

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(tx1);
  ASSERT_FALSE(blob.empty());
  // First byte should encode version 2
  EXPECT_EQ(static_cast<uint8_t>(blob[0]), 2u);
}

TEST(tx_serialization, v2_null_blob_consistency)
{
  // Two identical v2 RCTTypeNull txs should produce identical blobs
  cryptonote::transaction tx1, tx2;
  tx1.version = 2;
  tx2.version = 2;
  tx1.rct_signatures.type = rct::RCTTypeNull;
  tx2.rct_signatures.type = rct::RCTTypeNull;

  cryptonote::blobdata blob1 = cryptonote::t_serializable_object_to_blob(tx1);
  cryptonote::blobdata blob2 = cryptonote::t_serializable_object_to_blob(tx2);
  EXPECT_EQ(blob1, blob2);
}

// ---------------------------------------------------------------------------
// 4. Block serialization
// ---------------------------------------------------------------------------

TEST(tx_serialization, empty_block_roundtrip)
{
  cryptonote::block b1;
  b1.major_version = 1;
  b1.minor_version = 0;
  b1.timestamp = 0;
  b1.nonce = 0;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));
  // miner_tx defaults to a valid v1 tx with empty vin/vout

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  ASSERT_FALSE(blob.empty());

  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.major_version, b1.major_version);
  EXPECT_EQ(b2.minor_version, b1.minor_version);
}

TEST(tx_serialization, block_with_miner_tx_roundtrip)
{
  cryptonote::block b1;
  b1.major_version = 14;
  b1.minor_version = 14;
  b1.timestamp = 1600000000;
  b1.nonce = 0x12345678;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  // Set up a coinbase (miner) tx
  b1.miner_tx.version = 2;
  b1.miner_tx.unlock_time = 60;
  cryptonote::txin_gen gen_in;
  gen_in.height = 100;
  b1.miner_tx.vin.push_back(gen_in);
  cryptonote::tx_out out;
  out.amount = 1000000;
  cryptonote::txout_to_key out_key;
  memset(&out_key.key, 0xAB, sizeof(out_key.key));
  out.target = out_key;
  b1.miner_tx.vout.push_back(out);

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  ASSERT_FALSE(blob.empty());

  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.miner_tx.version, 2u);
  EXPECT_EQ(b2.miner_tx.unlock_time, 60u);
  ASSERT_EQ(b2.miner_tx.vin.size(), 1u);
  ASSERT_EQ(b2.miner_tx.vout.size(), 1u);
  EXPECT_EQ(b2.miner_tx.vout[0].amount, 1000000u);
}

TEST(tx_serialization, block_timestamp_preserved)
{
  cryptonote::block b1;
  b1.major_version = 1;
  b1.minor_version = 0;
  b1.timestamp = 1700000000;
  b1.nonce = 0;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.timestamp, 1700000000u);
}

TEST(tx_serialization, block_nonce_preserved)
{
  cryptonote::block b1;
  b1.major_version = 1;
  b1.minor_version = 0;
  b1.timestamp = 0;
  b1.nonce = 0xDEADBEEF;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.nonce, 0xDEADBEEFu);
}

TEST(tx_serialization, block_major_minor_version_preserved)
{
  cryptonote::block b1;
  b1.major_version = 15;
  b1.minor_version = 3;
  b1.timestamp = 0;
  b1.nonce = 0;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.major_version, 15);
  EXPECT_EQ(b2.minor_version, 3);
}

TEST(tx_serialization, block_prev_id_preserved)
{
  cryptonote::block b1;
  b1.major_version = 1;
  b1.minor_version = 0;
  b1.timestamp = 0;
  b1.nonce = 0;
  // Set prev_id to a recognizable pattern
  memset(&b1.prev_id, 0xCD, sizeof(b1.prev_id));

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  EXPECT_EQ(b2.prev_id, b1.prev_id);
}

TEST(tx_serialization, block_tx_hashes_preserved)
{
  cryptonote::block b1;
  b1.major_version = 1;
  b1.minor_version = 0;
  b1.timestamp = 0;
  b1.nonce = 0;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  // Add a few recognizable tx hashes
  crypto::hash h1, h2, h3;
  memset(&h1, 0x11, sizeof(h1));
  memset(&h2, 0x22, sizeof(h2));
  memset(&h3, 0x33, sizeof(h3));
  b1.tx_hashes.push_back(h1);
  b1.tx_hashes.push_back(h2);
  b1.tx_hashes.push_back(h3);

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  ASSERT_EQ(b2.tx_hashes.size(), 3u);
  EXPECT_EQ(b2.tx_hashes[0], h1);
  EXPECT_EQ(b2.tx_hashes[1], h2);
  EXPECT_EQ(b2.tx_hashes[2], h3);
}

// ---------------------------------------------------------------------------
// 5. Edge cases
// ---------------------------------------------------------------------------

TEST(tx_serialization, large_extra_field)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.extra.resize(1000, 0x42);

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(tx2.extra.size(), 1000u);
  for (size_t i = 0; i < 1000; ++i)
  {
    EXPECT_EQ(tx2.extra[i], 0x42);
  }
}

TEST(tx_serialization, many_vin_entries)
{
  cryptonote::transaction tx1;
  tx1.version = 1;

  // Add 50 txin_gen inputs (coinbase-style)
  for (size_t i = 0; i < 50; ++i)
  {
    cryptonote::txin_gen in;
    in.height = i + 1;
    tx1.vin.push_back(in);
  }

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(tx2.vin.size(), 50u);
  for (size_t i = 0; i < 50; ++i)
  {
    ASSERT_EQ(tx2.vin[i].type(), typeid(cryptonote::txin_gen));
    EXPECT_EQ(boost::get<cryptonote::txin_gen>(tx2.vin[i]).height, i + 1);
  }
}

TEST(tx_serialization, many_vout_entries)
{
  cryptonote::transaction tx1;
  tx1.version = 1;

  // Add 50 outputs
  for (size_t i = 0; i < 50; ++i)
  {
    cryptonote::tx_out out;
    out.amount = (i + 1) * 1000;
    cryptonote::txout_to_key out_key;
    memset(&out_key.key, static_cast<int>(i & 0xFF), sizeof(out_key.key));
    out.target = out_key;
    tx1.vout.push_back(out);
  }

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(tx2.vout.size(), 50u);
  for (size_t i = 0; i < 50; ++i)
  {
    EXPECT_EQ(tx2.vout[i].amount, (i + 1) * 1000);
  }
}

TEST(tx_serialization, malformed_blob_fails_to_parse)
{
  // Random garbage should not parse as a valid transaction
  cryptonote::blobdata garbage(64, '\0');
  for (size_t i = 0; i < garbage.size(); ++i)
    garbage[i] = static_cast<char>((i * 37 + 13) & 0xFF);

  cryptonote::transaction tx;
  EXPECT_FALSE(cryptonote::parse_and_validate_tx_from_blob(garbage, tx));
}

TEST(tx_serialization, empty_blob_fails_to_parse)
{
  cryptonote::blobdata empty_blob;
  cryptonote::transaction tx;
  EXPECT_FALSE(cryptonote::parse_and_validate_tx_from_blob(empty_blob, tx));
}

TEST(tx_serialization, truncated_blob_fails_to_parse)
{
  // A single byte cannot be a valid transaction — version varint needs more context
  cryptonote::blobdata truncated(1, '\x80');  // 0x80 = varint continuation byte without termination

  cryptonote::transaction tx2;
  EXPECT_FALSE(cryptonote::parse_and_validate_tx_from_blob(truncated, tx2));
}

// ---------------------------------------------------------------------------
// 6. Binary archive operations
// ---------------------------------------------------------------------------

TEST(tx_serialization, crypto_hash_binary_roundtrip)
{
  crypto::hash h1;
  memset(&h1, 0, sizeof(h1));
  // Fill with a recognizable pattern
  for (size_t i = 0; i < sizeof(h1.data); ++i)
    h1.data[i] = static_cast<char>(i);

  std::string blob;
  ASSERT_TRUE(serialization::dump_binary(h1, blob));
  ASSERT_FALSE(blob.empty());

  crypto::hash h2;
  ASSERT_TRUE(serialization::parse_binary(blob, h2));
  EXPECT_EQ(h1, h2);
}

TEST(tx_serialization, crypto_public_key_binary_roundtrip)
{
  crypto::public_key pk1;
  memset(&pk1, 0, sizeof(pk1));
  // Fill with a recognizable pattern
  for (size_t i = 0; i < sizeof(pk1.data); ++i)
    pk1.data[i] = static_cast<char>(0xFF - i);

  std::string blob;
  ASSERT_TRUE(serialization::dump_binary(pk1, blob));
  ASSERT_FALSE(blob.empty());

  crypto::public_key pk2;
  ASSERT_TRUE(serialization::parse_binary(blob, pk2));
  EXPECT_EQ(pk1, pk2);
}

// ---------------------------------------------------------------------------
// Additional comprehensive tests
// ---------------------------------------------------------------------------

TEST(tx_serialization, v2_tx_with_no_inputs_rct_null)
{
  // A v2 transaction with no inputs should have RCTTypeNull after roundtrip
  cryptonote::transaction tx1;
  tx1.version = 2;

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(tx1);
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  // With no vin, rct_signatures are not serialized; type stays RCTTypeNull
  EXPECT_EQ(tx2.rct_signatures.type, rct::RCTTypeNull);
}

TEST(tx_serialization, tx_hash_preserved_through_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 55555;
  tx1.extra = {0x10, 0x20, 0x30};

  crypto::hash hash1 = cryptonote::get_transaction_hash(tx1);

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));

  crypto::hash hash2 = cryptonote::get_transaction_hash(tx2);
  EXPECT_EQ(hash1, hash2);
}

TEST(tx_serialization, parse_with_hash_output)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 77;

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);

  cryptonote::transaction tx2;
  crypto::hash tx_hash;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2, tx_hash));
  EXPECT_EQ(tx2.version, 1u);

  // The returned hash should match what we compute independently
  crypto::hash expected_hash = cryptonote::get_transaction_hash(tx2);
  EXPECT_EQ(tx_hash, expected_hash);
}

TEST(tx_serialization, block_blob_deterministic)
{
  cryptonote::block b1;
  b1.major_version = 7;
  b1.minor_version = 7;
  b1.timestamp = 1234567890;
  b1.nonce = 42;
  memset(&b1.prev_id, 0xAA, sizeof(b1.prev_id));

  cryptonote::blobdata blob1 = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::blobdata blob2 = cryptonote::t_serializable_object_to_blob(b1);
  EXPECT_EQ(blob1, blob2);
}

TEST(tx_serialization, malformed_blob_fails_block_parse)
{
  cryptonote::blobdata garbage(100, '\0');
  for (size_t i = 0; i < garbage.size(); ++i)
    garbage[i] = static_cast<char>((i * 53 + 7) & 0xFF);

  cryptonote::block b;
  EXPECT_FALSE(cryptonote::parse_and_validate_block_from_blob(garbage, b));
}

TEST(tx_serialization, empty_blob_fails_block_parse)
{
  cryptonote::blobdata empty_blob;
  cryptonote::block b;
  EXPECT_FALSE(cryptonote::parse_and_validate_block_from_blob(empty_blob, b));
}

TEST(tx_serialization, v2_null_tx_fee_in_blob)
{
  // For RCTTypeNull, txnFee is not serialized, but for v2 txs the
  // rct_signatures type byte IS serialized. Verify blob changes with version.
  cryptonote::transaction tx1;
  tx1.version = 1;
  cryptonote::blobdata blob1 = cryptonote::t_serializable_object_to_blob(tx1);

  cryptonote::transaction tx2;
  tx2.version = 2;
  tx2.rct_signatures.type = rct::RCTTypeNull;
  cryptonote::blobdata blob2 = cryptonote::t_serializable_object_to_blob(tx2);

  EXPECT_NE(blob1, blob2);
}

TEST(tx_serialization, block_hash_consistency_after_roundtrip)
{
  cryptonote::block b1;
  b1.major_version = 14;
  b1.minor_version = 14;
  b1.timestamp = 1650000000;
  b1.nonce = 0xCAFEBABE;
  memset(&b1.prev_id, 0x55, sizeof(b1.prev_id));

  crypto::hash hash1 = cryptonote::get_block_hash(b1);

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));

  crypto::hash hash2 = cryptonote::get_block_hash(b2);
  EXPECT_EQ(hash1, hash2);
}

TEST(tx_serialization, parse_block_with_hash_output)
{
  cryptonote::block b1;
  b1.major_version = 10;
  b1.minor_version = 10;
  b1.timestamp = 1500000000;
  b1.nonce = 1234;
  memset(&b1.prev_id, 0, sizeof(b1.prev_id));

  cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(b1);
  cryptonote::block b2;
  crypto::hash block_hash;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2, block_hash));

  crypto::hash expected_hash = cryptonote::get_block_hash(b2);
  EXPECT_EQ(block_hash, expected_hash);
}

TEST(tx_serialization, v1_tx_with_txin_to_key)
{
  cryptonote::transaction tx1;
  tx1.version = 1;

  // Add a txin_to_key input
  cryptonote::txin_to_key in;
  in.amount = 50000;
  in.key_offsets = {10, 20, 30};
  memset(&in.k_image, 0xBB, sizeof(in.k_image));
  tx1.vin.push_back(in);

  // Add an output
  cryptonote::tx_out out;
  out.amount = 49000;
  cryptonote::txout_to_key out_key;
  memset(&out_key.key, 0xCC, sizeof(out_key.key));
  out.target = out_key;
  tx1.vout.push_back(out);

  // Add matching signature (one ring sig per input, size = key_offsets.size())
  std::vector<crypto::signature> sigs(3);
  for (auto& s : sigs)
    memset(&s, 0xDD, sizeof(s));
  tx1.signatures.push_back(sigs);

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);
  ASSERT_FALSE(blob.empty());

  cryptonote::transaction tx2;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(tx2.vin.size(), 1u);
  ASSERT_EQ(tx2.vin[0].type(), typeid(cryptonote::txin_to_key));

  const auto& parsed_in = boost::get<cryptonote::txin_to_key>(tx2.vin[0]);
  EXPECT_EQ(parsed_in.amount, 50000u);
  ASSERT_EQ(parsed_in.key_offsets.size(), 3u);
  EXPECT_EQ(parsed_in.key_offsets[0], 10u);
  EXPECT_EQ(parsed_in.key_offsets[1], 20u);
  EXPECT_EQ(parsed_in.key_offsets[2], 30u);
  EXPECT_EQ(tx2.vout[0].amount, 49000u);
  ASSERT_EQ(tx2.signatures.size(), 1u);
  ASSERT_EQ(tx2.signatures[0].size(), 3u);
}

TEST(tx_serialization, tx_prefix_from_blob_roundtrip)
{
  cryptonote::transaction tx1;
  tx1.version = 1;
  tx1.unlock_time = 888;
  tx1.extra = {0xAA, 0xBB, 0xCC, 0xDD};

  cryptonote::blobdata blob = cryptonote::tx_to_blob(tx1);

  cryptonote::transaction_prefix prefix;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_prefix_from_blob(blob, prefix));
  EXPECT_EQ(prefix.version, 1u);
  EXPECT_EQ(prefix.unlock_time, 888u);
  EXPECT_EQ(prefix.extra, tx1.extra);
}
