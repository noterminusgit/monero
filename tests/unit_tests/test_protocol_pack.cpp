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

#include "include_base_utils.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "storages/portable_storage_template_helper.h"
#include "crypto/crypto.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
  // Build a block_complete_entry with pruned=false
  cryptonote::block_complete_entry make_bce_unpruned()
  {
    cryptonote::block_complete_entry bce;
    bce.pruned = false;
    bce.block = "block_blob_data_1234";
    bce.block_weight = 0;
    bce.txs.push_back(cryptonote::tx_blob_entry{"tx_blob_aaa", crypto::null_hash});
    bce.txs.push_back(cryptonote::tx_blob_entry{"tx_blob_bbb", crypto::null_hash});
    return bce;
  }

  // Build a block_complete_entry with pruned=true
  cryptonote::block_complete_entry make_bce_pruned()
  {
    cryptonote::block_complete_entry bce;
    bce.pruned = true;
    bce.block = "pruned_block_blob";
    bce.block_weight = 4096;
    bce.txs.push_back(cryptonote::tx_blob_entry{"pruned_tx_1", crypto::rand<crypto::hash>()});
    bce.txs.push_back(cryptonote::tx_blob_entry{"pruned_tx_2", crypto::rand<crypto::hash>()});
    bce.txs.push_back(cryptonote::tx_blob_entry{"pruned_tx_3", crypto::rand<crypto::hash>()});
    return bce;
  }
}

// ===========================================================================
// Original test (preserved) – NOTIFY_RESPONSE_CHAIN_ENTRY scaling
// ===========================================================================
TEST(protocol_pack, protocol_pack_command)
{
  epee::byte_slice buff;
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  r.start_height = 1;
  r.total_height = 3;
  for(int i = 1; i < 10000; i += i*10)
  {
    r.m_block_ids.resize(i, crypto::hash{});
    bool res = epee::serialization::store_t_to_binary(r, buff);
    ASSERT_TRUE(res);

    cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request r2;
    res = epee::serialization::load_t_from_binary(r2, epee::to_span(buff));
    ASSERT_TRUE(res);
    ASSERT_TRUE(r.m_block_ids.size() == i);
    ASSERT_TRUE(r.start_height == 1);
    ASSERT_TRUE(r.total_height == 3);
  }
}

// ===========================================================================
// 1. NOTIFY_NEW_BLOCK – populated
// ===========================================================================
TEST(protocol_pack, notify_new_block_roundtrip)
{
  cryptonote::NOTIFY_NEW_BLOCK::request_t original;
  original.b = make_bce_unpruned();
  original.current_blockchain_height = 123456;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
  ASSERT_EQ(original.b.block, restored.b.block);
  ASSERT_EQ(original.b.pruned, restored.b.pruned);
  ASSERT_EQ(original.b.txs.size(), restored.b.txs.size());
  for (size_t i = 0; i < original.b.txs.size(); ++i)
    ASSERT_EQ(original.b.txs[i].blob, restored.b.txs[i].blob);
}

// 1b. NOTIFY_NEW_BLOCK – empty/default
TEST(protocol_pack, notify_new_block_empty)
{
  cryptonote::NOTIFY_NEW_BLOCK::request_t original;
  original.current_blockchain_height = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(0u, restored.current_blockchain_height);
  ASSERT_TRUE(restored.b.block.empty());
  ASSERT_TRUE(restored.b.txs.empty());
}

// ===========================================================================
// 2. NOTIFY_NEW_TRANSACTIONS – populated
// ===========================================================================
TEST(protocol_pack, notify_new_transactions_roundtrip)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  original.txs.push_back("tx_data_001");
  original.txs.push_back("tx_data_002");
  original.txs.push_back("tx_data_003");
  original._ = "padding_value";
  original.dandelionpp_fluff = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.txs.size(), restored.txs.size());
  for (size_t i = 0; i < original.txs.size(); ++i)
    ASSERT_EQ(original.txs[i], restored.txs[i]);
  ASSERT_EQ(original._, restored._);
  ASSERT_EQ(original.dandelionpp_fluff, restored.dandelionpp_fluff);
}

// 2b. NOTIFY_NEW_TRANSACTIONS – empty with default opt
TEST(protocol_pack, notify_new_transactions_empty)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  original.dandelionpp_fluff = true; // default value

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.txs.empty());
  ASSERT_TRUE(restored._.empty());
  ASSERT_EQ(true, restored.dandelionpp_fluff);
}

// 2c. NOTIFY_NEW_TRANSACTIONS – dandelionpp_fluff stem mode (false)
TEST(protocol_pack, notify_new_transactions_stem_mode)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  original.txs.push_back("stem_tx");
  original.dandelionpp_fluff = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(false, restored.dandelionpp_fluff);
  ASSERT_EQ(1u, restored.txs.size());
  ASSERT_EQ("stem_tx", restored.txs[0]);
}

// ===========================================================================
// 3. NOTIFY_REQUEST_GET_OBJECTS – populated
// ===========================================================================
TEST(protocol_pack, notify_request_get_objects_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t original;
  for (int i = 0; i < 5; ++i)
    original.blocks.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.blocks.size(), restored.blocks.size());
  for (size_t i = 0; i < original.blocks.size(); ++i)
    ASSERT_EQ(original.blocks[i], restored.blocks[i]);
  ASSERT_EQ(original.prune, restored.prune);
}

// 3b. NOTIFY_REQUEST_GET_OBJECTS – empty, prune default
TEST(protocol_pack, notify_request_get_objects_empty)
{
  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t original;
  original.prune = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.blocks.empty());
  ASSERT_EQ(false, restored.prune);
}

// ===========================================================================
// 4. NOTIFY_RESPONSE_GET_OBJECTS – populated
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_roundtrip)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;

  original.blocks.push_back(make_bce_unpruned());
  original.blocks.push_back(make_bce_unpruned());

  for (int i = 0; i < 3; ++i)
    original.missed_ids.push_back(crypto::rand<crypto::hash>());
  original.current_blockchain_height = 999999;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.blocks.size(), restored.blocks.size());
  for (size_t i = 0; i < original.blocks.size(); ++i)
  {
    ASSERT_EQ(original.blocks[i].block, restored.blocks[i].block);
    ASSERT_EQ(original.blocks[i].txs.size(), restored.blocks[i].txs.size());
  }
  ASSERT_EQ(original.missed_ids.size(), restored.missed_ids.size());
  for (size_t i = 0; i < original.missed_ids.size(); ++i)
    ASSERT_EQ(original.missed_ids[i], restored.missed_ids[i]);
  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
}

// 4b. NOTIFY_RESPONSE_GET_OBJECTS – empty
TEST(protocol_pack, notify_response_get_objects_empty)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;
  original.current_blockchain_height = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.blocks.empty());
  ASSERT_TRUE(restored.missed_ids.empty());
  ASSERT_EQ(0u, restored.current_blockchain_height);
}

// ===========================================================================
// 5. NOTIFY_REQUEST_CHAIN – populated
// ===========================================================================
TEST(protocol_pack, notify_request_chain_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_CHAIN::request_t original;
  for (int i = 0; i < 7; ++i)
    original.block_ids.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_CHAIN::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.block_ids.size(), restored.block_ids.size());
  auto it_orig = original.block_ids.begin();
  auto it_rest = restored.block_ids.begin();
  for (; it_orig != original.block_ids.end(); ++it_orig, ++it_rest)
    ASSERT_EQ(*it_orig, *it_rest);
  ASSERT_EQ(original.prune, restored.prune);
}

// 5b. NOTIFY_REQUEST_CHAIN – empty
TEST(protocol_pack, notify_request_chain_empty)
{
  cryptonote::NOTIFY_REQUEST_CHAIN::request_t original;
  original.prune = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_CHAIN::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.block_ids.empty());
  ASSERT_EQ(false, restored.prune);
}

// ===========================================================================
// 6. NOTIFY_RESPONSE_CHAIN_ENTRY – populated (comprehensive)
// ===========================================================================
TEST(protocol_pack, notify_response_chain_entry_roundtrip)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = 100;
  original.total_height = 200;
  original.cumulative_difficulty = 0xDEADBEEFCAFEull;
  original.cumulative_difficulty_top64 = 0x42ull;
  original.first_block = "first_block_blob_data";

  for (int i = 0; i < 10; ++i)
  {
    original.m_block_ids.push_back(crypto::rand<crypto::hash>());
    original.m_block_weights.push_back(static_cast<uint64_t>(i) * 1000 + 500);
  }

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.start_height, restored.start_height);
  ASSERT_EQ(original.total_height, restored.total_height);
  ASSERT_EQ(original.cumulative_difficulty, restored.cumulative_difficulty);
  ASSERT_EQ(original.cumulative_difficulty_top64, restored.cumulative_difficulty_top64);
  ASSERT_EQ(original.first_block, restored.first_block);
  ASSERT_EQ(original.m_block_ids.size(), restored.m_block_ids.size());
  for (size_t i = 0; i < original.m_block_ids.size(); ++i)
    ASSERT_EQ(original.m_block_ids[i], restored.m_block_ids[i]);
  ASSERT_EQ(original.m_block_weights.size(), restored.m_block_weights.size());
  for (size_t i = 0; i < original.m_block_weights.size(); ++i)
    ASSERT_EQ(original.m_block_weights[i], restored.m_block_weights[i]);
}

// 6b. NOTIFY_RESPONSE_CHAIN_ENTRY – empty/defaults
TEST(protocol_pack, notify_response_chain_entry_empty)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = 0;
  original.total_height = 0;
  original.cumulative_difficulty = 0;
  original.cumulative_difficulty_top64 = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(0u, restored.start_height);
  ASSERT_EQ(0u, restored.total_height);
  ASSERT_EQ(0u, restored.cumulative_difficulty);
  ASSERT_EQ(0u, restored.cumulative_difficulty_top64);
  ASSERT_TRUE(restored.m_block_ids.empty());
  ASSERT_TRUE(restored.m_block_weights.empty());
  ASSERT_TRUE(restored.first_block.empty());
}

// 6c. NOTIFY_RESPONSE_CHAIN_ENTRY – max uint64 values
TEST(protocol_pack, notify_response_chain_entry_max_values)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = UINT64_MAX;
  original.total_height = UINT64_MAX;
  original.cumulative_difficulty = UINT64_MAX;
  original.cumulative_difficulty_top64 = UINT64_MAX;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(UINT64_MAX, restored.start_height);
  ASSERT_EQ(UINT64_MAX, restored.total_height);
  ASSERT_EQ(UINT64_MAX, restored.cumulative_difficulty);
  ASSERT_EQ(UINT64_MAX, restored.cumulative_difficulty_top64);
}

// ===========================================================================
// 7. NOTIFY_NEW_FLUFFY_BLOCK – populated
// ===========================================================================
TEST(protocol_pack, notify_new_fluffy_block_roundtrip)
{
  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t original;
  original.b = make_bce_unpruned();
  original.current_blockchain_height = 777777;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
  ASSERT_EQ(original.b.block, restored.b.block);
  ASSERT_EQ(original.b.pruned, restored.b.pruned);
  ASSERT_EQ(original.b.txs.size(), restored.b.txs.size());
  for (size_t i = 0; i < original.b.txs.size(); ++i)
    ASSERT_EQ(original.b.txs[i].blob, restored.b.txs[i].blob);
}

// 7b. NOTIFY_NEW_FLUFFY_BLOCK – empty
TEST(protocol_pack, notify_new_fluffy_block_empty)
{
  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t original;
  original.current_blockchain_height = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(0u, restored.current_blockchain_height);
  ASSERT_TRUE(restored.b.block.empty());
  ASSERT_TRUE(restored.b.txs.empty());
}

// ===========================================================================
// 8. NOTIFY_REQUEST_FLUFFY_MISSING_TX – populated
// ===========================================================================
TEST(protocol_pack, notify_request_fluffy_missing_tx_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t original;
  original.block_hash = crypto::rand<crypto::hash>();
  original.current_blockchain_height = 888888;
  original.missing_tx_indices.push_back(0);
  original.missing_tx_indices.push_back(3);
  original.missing_tx_indices.push_back(7);
  original.missing_tx_indices.push_back(42);

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.block_hash, restored.block_hash);
  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
  ASSERT_EQ(original.missing_tx_indices.size(), restored.missing_tx_indices.size());
  for (size_t i = 0; i < original.missing_tx_indices.size(); ++i)
    ASSERT_EQ(original.missing_tx_indices[i], restored.missing_tx_indices[i]);
}

// 8b. NOTIFY_REQUEST_FLUFFY_MISSING_TX – empty indices
TEST(protocol_pack, notify_request_fluffy_missing_tx_empty)
{
  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t original;
  original.block_hash = crypto::null_hash;
  original.current_blockchain_height = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(crypto::null_hash, restored.block_hash);
  ASSERT_EQ(0u, restored.current_blockchain_height);
  ASSERT_TRUE(restored.missing_tx_indices.empty());
}

// ===========================================================================
// 9. NOTIFY_GET_TXPOOL_COMPLEMENT – populated
// ===========================================================================
TEST(protocol_pack, notify_get_txpool_complement_roundtrip)
{
  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t original;
  for (int i = 0; i < 8; ++i)
    original.hashes.push_back(crypto::rand<crypto::hash>());

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.hashes.size(), restored.hashes.size());
  for (size_t i = 0; i < original.hashes.size(); ++i)
    ASSERT_EQ(original.hashes[i], restored.hashes[i]);
}

// 9b. NOTIFY_GET_TXPOOL_COMPLEMENT – empty
TEST(protocol_pack, notify_get_txpool_complement_empty)
{
  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t original;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.hashes.empty());
}

// ===========================================================================
// 10. CORE_SYNC_DATA – populated
// ===========================================================================
TEST(protocol_pack, core_sync_data_roundtrip)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 500000;
  original.cumulative_difficulty = 0xABCDEF0123456789ull;
  original.cumulative_difficulty_top64 = 0x55ull;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 15;
  original.pruning_seed = 384;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.current_height, restored.current_height);
  ASSERT_EQ(original.cumulative_difficulty, restored.cumulative_difficulty);
  ASSERT_EQ(original.cumulative_difficulty_top64, restored.cumulative_difficulty_top64);
  ASSERT_EQ(original.top_id, restored.top_id);
  ASSERT_EQ(original.top_version, restored.top_version);
  ASSERT_EQ(original.pruning_seed, restored.pruning_seed);
}

// 10b. CORE_SYNC_DATA – defaults/zeros
TEST(protocol_pack, core_sync_data_defaults)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 0;
  original.cumulative_difficulty = 0;
  original.cumulative_difficulty_top64 = 0;
  original.top_id = crypto::null_hash;
  original.top_version = 0;
  original.pruning_seed = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(0u, restored.current_height);
  ASSERT_EQ(0u, restored.cumulative_difficulty);
  ASSERT_EQ(0u, restored.cumulative_difficulty_top64);
  ASSERT_EQ(crypto::null_hash, restored.top_id);
  ASSERT_EQ(0, restored.top_version);
  ASSERT_EQ(0u, restored.pruning_seed);
}

// 10c. CORE_SYNC_DATA – max values
TEST(protocol_pack, core_sync_data_max_values)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = UINT64_MAX;
  original.cumulative_difficulty = UINT64_MAX;
  original.cumulative_difficulty_top64 = UINT64_MAX;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = UINT8_MAX;
  original.pruning_seed = UINT32_MAX;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(UINT64_MAX, restored.current_height);
  ASSERT_EQ(UINT64_MAX, restored.cumulative_difficulty);
  ASSERT_EQ(UINT64_MAX, restored.cumulative_difficulty_top64);
  ASSERT_EQ(original.top_id, restored.top_id);
  ASSERT_EQ(UINT8_MAX, restored.top_version);
  ASSERT_EQ(UINT32_MAX, restored.pruning_seed);
}

// ===========================================================================
// 11. block_complete_entry – unpruned roundtrip
// ===========================================================================
TEST(protocol_pack, block_complete_entry_unpruned_roundtrip)
{
  cryptonote::block_complete_entry original = make_bce_unpruned();

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.pruned, restored.pruned);
  ASSERT_EQ(false, restored.pruned);
  ASSERT_EQ(original.block, restored.block);
  ASSERT_EQ(original.txs.size(), restored.txs.size());
  for (size_t i = 0; i < original.txs.size(); ++i)
  {
    ASSERT_EQ(original.txs[i].blob, restored.txs[i].blob);
    // When unpruned, prunable_hash is not serialized; restored gets null_hash
    ASSERT_EQ(crypto::null_hash, restored.txs[i].prunable_hash);
  }
}

// 12. block_complete_entry – pruned roundtrip
TEST(protocol_pack, block_complete_entry_pruned_roundtrip)
{
  cryptonote::block_complete_entry original = make_bce_pruned();

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(true, restored.pruned);
  ASSERT_EQ(original.block, restored.block);
  ASSERT_EQ(original.block_weight, restored.block_weight);
  ASSERT_EQ(original.txs.size(), restored.txs.size());
  for (size_t i = 0; i < original.txs.size(); ++i)
  {
    ASSERT_EQ(original.txs[i].blob, restored.txs[i].blob);
    ASSERT_EQ(original.txs[i].prunable_hash, restored.txs[i].prunable_hash);
  }
}

// 13. block_complete_entry – empty (default constructed)
TEST(protocol_pack, block_complete_entry_empty)
{
  cryptonote::block_complete_entry original;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(false, restored.pruned);
  ASSERT_TRUE(restored.block.empty());
  ASSERT_EQ(0u, restored.block_weight);
  ASSERT_TRUE(restored.txs.empty());
}

// ===========================================================================
// 14. tx_blob_entry – standalone roundtrip
// ===========================================================================
TEST(protocol_pack, tx_blob_entry_roundtrip)
{
  cryptonote::tx_blob_entry original;
  original.blob = "serialized_transaction_data_here_1234567890";
  original.prunable_hash = crypto::rand<crypto::hash>();

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::tx_blob_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.blob, restored.blob);
  ASSERT_EQ(original.prunable_hash, restored.prunable_hash);
}

// 14b. tx_blob_entry – empty blob, null hash
TEST(protocol_pack, tx_blob_entry_empty)
{
  cryptonote::tx_blob_entry original;
  original.blob = "";
  original.prunable_hash = crypto::null_hash;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::tx_blob_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.blob.empty());
  ASSERT_EQ(crypto::null_hash, restored.prunable_hash);
}

// ===========================================================================
// 15. connection_info – populated
// ===========================================================================
TEST(protocol_pack, connection_info_roundtrip)
{
  cryptonote::connection_info original;
  original.incoming = true;
  original.localhost = false;
  original.local_ip = true;
  original.ssl = true; // Note: ssl is NOT in the KV_SERIALIZE_MAP
  original.address = "192.168.1.100:18080";
  original.host = "192.168.1.100";
  original.ip = "192.168.1.100";
  original.port = "18080";
  original.rpc_port = 18081;
  original.rpc_credits_per_hash = 100;
  original.peer_id = "abcdef0123456789";
  original.recv_count = 1024000;
  original.recv_idle_time = 30;
  original.send_count = 512000;
  original.send_idle_time = 15;
  original.state = "normal";
  original.live_time = 3600;
  original.avg_download = 500;
  original.current_download = 250;
  original.avg_upload = 400;
  original.current_upload = 200;
  original.support_flags = 1;
  original.connection_id = "conn-id-12345";
  original.height = 700000;
  original.pruning_seed = 384;
  original.address_type = 1;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::connection_info restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.incoming, restored.incoming);
  ASSERT_EQ(original.localhost, restored.localhost);
  ASSERT_EQ(original.local_ip, restored.local_ip);
  // ssl is NOT serialized, so we don't check it
  ASSERT_EQ(original.address, restored.address);
  ASSERT_EQ(original.host, restored.host);
  ASSERT_EQ(original.ip, restored.ip);
  ASSERT_EQ(original.port, restored.port);
  ASSERT_EQ(original.rpc_port, restored.rpc_port);
  ASSERT_EQ(original.rpc_credits_per_hash, restored.rpc_credits_per_hash);
  ASSERT_EQ(original.peer_id, restored.peer_id);
  ASSERT_EQ(original.recv_count, restored.recv_count);
  ASSERT_EQ(original.recv_idle_time, restored.recv_idle_time);
  ASSERT_EQ(original.send_count, restored.send_count);
  ASSERT_EQ(original.send_idle_time, restored.send_idle_time);
  ASSERT_EQ(original.state, restored.state);
  ASSERT_EQ(original.live_time, restored.live_time);
  ASSERT_EQ(original.avg_download, restored.avg_download);
  ASSERT_EQ(original.current_download, restored.current_download);
  ASSERT_EQ(original.avg_upload, restored.avg_upload);
  ASSERT_EQ(original.current_upload, restored.current_upload);
  ASSERT_EQ(original.support_flags, restored.support_flags);
  ASSERT_EQ(original.connection_id, restored.connection_id);
  ASSERT_EQ(original.height, restored.height);
  ASSERT_EQ(original.pruning_seed, restored.pruning_seed);
  ASSERT_EQ(original.address_type, restored.address_type);
}

// 15b. connection_info – empty/defaults
TEST(protocol_pack, connection_info_empty)
{
  cryptonote::connection_info original{};

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::connection_info restored{};
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(false, restored.incoming);
  ASSERT_EQ(false, restored.localhost);
  ASSERT_EQ(false, restored.local_ip);
  ASSERT_TRUE(restored.address.empty());
  ASSERT_TRUE(restored.host.empty());
  ASSERT_TRUE(restored.ip.empty());
  ASSERT_TRUE(restored.port.empty());
  ASSERT_EQ(0, restored.rpc_port);
  ASSERT_EQ(0u, restored.rpc_credits_per_hash);
  ASSERT_TRUE(restored.peer_id.empty());
  ASSERT_EQ(0u, restored.recv_count);
  ASSERT_EQ(0u, restored.send_count);
  ASSERT_TRUE(restored.state.empty());
  ASSERT_EQ(0u, restored.live_time);
  ASSERT_EQ(0u, restored.height);
  ASSERT_EQ(0u, restored.pruning_seed);
  ASSERT_EQ(0, restored.address_type);
}

// ===========================================================================
// 16. Large vector edge case – NOTIFY_REQUEST_GET_OBJECTS with many hashes
// ===========================================================================
TEST(protocol_pack, notify_request_get_objects_large_vector)
{
  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t original;
  const size_t count = 1000;
  for (size_t i = 0; i < count; ++i)
    original.blocks.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(count, restored.blocks.size());
  for (size_t i = 0; i < count; ++i)
    ASSERT_EQ(original.blocks[i], restored.blocks[i]);
  ASSERT_EQ(true, restored.prune);
}

// ===========================================================================
// 17. Large vector edge case – NOTIFY_GET_TXPOOL_COMPLEMENT
// ===========================================================================
TEST(protocol_pack, notify_get_txpool_complement_large_vector)
{
  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t original;
  const size_t count = 500;
  for (size_t i = 0; i < count; ++i)
    original.hashes.push_back(crypto::rand<crypto::hash>());

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(count, restored.hashes.size());
  for (size_t i = 0; i < count; ++i)
    ASSERT_EQ(original.hashes[i], restored.hashes[i]);
}

// ===========================================================================
// 18. NOTIFY_RESPONSE_GET_OBJECTS with multiple blocks including mixed entries
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_multiple_blocks)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;

  // Add several blocks with varying tx counts
  for (int b = 0; b < 5; ++b)
  {
    cryptonote::block_complete_entry bce;
    bce.pruned = false;
    bce.block = "block_data_" + std::to_string(b);
    bce.block_weight = 0;
    for (int t = 0; t <= b; ++t)
      bce.txs.push_back(cryptonote::tx_blob_entry{"tx_" + std::to_string(b) + "_" + std::to_string(t), crypto::null_hash});
    original.blocks.push_back(bce);
  }
  original.current_blockchain_height = 42;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(5u, restored.blocks.size());
  for (size_t b = 0; b < 5; ++b)
  {
    ASSERT_EQ(original.blocks[b].block, restored.blocks[b].block);
    ASSERT_EQ(original.blocks[b].txs.size(), restored.blocks[b].txs.size());
    for (size_t t = 0; t < original.blocks[b].txs.size(); ++t)
      ASSERT_EQ(original.blocks[b].txs[t].blob, restored.blocks[b].txs[t].blob);
  }
  ASSERT_EQ(42u, restored.current_blockchain_height);
}

// ===========================================================================
// 19. NOTIFY_NEW_BLOCK with pruned block_complete_entry
// ===========================================================================
TEST(protocol_pack, notify_new_block_pruned)
{
  cryptonote::NOTIFY_NEW_BLOCK::request_t original;
  original.b = make_bce_pruned();
  original.current_blockchain_height = 555555;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
  ASSERT_EQ(true, restored.b.pruned);
  ASSERT_EQ(original.b.block, restored.b.block);
  ASSERT_EQ(original.b.block_weight, restored.b.block_weight);
  ASSERT_EQ(original.b.txs.size(), restored.b.txs.size());
  for (size_t i = 0; i < original.b.txs.size(); ++i)
  {
    ASSERT_EQ(original.b.txs[i].blob, restored.b.txs[i].blob);
    ASSERT_EQ(original.b.txs[i].prunable_hash, restored.b.txs[i].prunable_hash);
  }
}

// ===========================================================================
// 20. NOTIFY_NEW_FLUFFY_BLOCK with pruned block_complete_entry
// ===========================================================================
TEST(protocol_pack, notify_new_fluffy_block_pruned)
{
  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t original;
  original.b = make_bce_pruned();
  original.current_blockchain_height = 333333;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.current_blockchain_height, restored.current_blockchain_height);
  ASSERT_EQ(true, restored.b.pruned);
  ASSERT_EQ(original.b.block, restored.b.block);
  ASSERT_EQ(original.b.block_weight, restored.b.block_weight);
  ASSERT_EQ(original.b.txs.size(), restored.b.txs.size());
  for (size_t i = 0; i < original.b.txs.size(); ++i)
  {
    ASSERT_EQ(original.b.txs[i].blob, restored.b.txs[i].blob);
    ASSERT_EQ(original.b.txs[i].prunable_hash, restored.b.txs[i].prunable_hash);
  }
}

// ===========================================================================
// 21. NOTIFY_REQUEST_FLUFFY_MISSING_TX – large missing_tx_indices
// ===========================================================================
TEST(protocol_pack, notify_request_fluffy_missing_tx_large_indices)
{
  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t original;
  original.block_hash = crypto::rand<crypto::hash>();
  original.current_blockchain_height = UINT64_MAX;
  for (uint64_t i = 0; i < 200; ++i)
    original.missing_tx_indices.push_back(i * 7);

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(original.block_hash, restored.block_hash);
  ASSERT_EQ(UINT64_MAX, restored.current_blockchain_height);
  ASSERT_EQ(200u, restored.missing_tx_indices.size());
  for (size_t i = 0; i < 200; ++i)
    ASSERT_EQ(original.missing_tx_indices[i], restored.missing_tx_indices[i]);
}

// ===========================================================================
// 22. NOTIFY_NEW_TRANSACTIONS – large number of transactions
// ===========================================================================
TEST(protocol_pack, notify_new_transactions_large_vector)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  for (int i = 0; i < 100; ++i)
    original.txs.push_back("tx_payload_" + std::to_string(i) + "_data_padding_to_make_it_bigger");
  original._ = "";
  original.dandelionpp_fluff = true;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(100u, restored.txs.size());
  for (size_t i = 0; i < 100; ++i)
    ASSERT_EQ(original.txs[i], restored.txs[i]);
  ASSERT_EQ(true, restored.dandelionpp_fluff);
}

// ===========================================================================
// 23. NOTIFY_REQUEST_CHAIN – single element list
// ===========================================================================
TEST(protocol_pack, notify_request_chain_single_element)
{
  cryptonote::NOTIFY_REQUEST_CHAIN::request_t original;
  original.block_ids.push_back(crypto::rand<crypto::hash>());
  original.prune = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_CHAIN::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(1u, restored.block_ids.size());
  ASSERT_EQ(original.block_ids.front(), restored.block_ids.front());
  ASSERT_EQ(false, restored.prune);
}

// ===========================================================================
// 24. NOTIFY_RESPONSE_CHAIN_ENTRY – large block weights
// ===========================================================================
TEST(protocol_pack, notify_response_chain_entry_large_weights)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = 1;
  original.total_height = 1000000;
  original.cumulative_difficulty = 1;
  original.cumulative_difficulty_top64 = 0;
  original.first_block = "";

  // Add many block IDs and weights
  for (uint64_t i = 0; i < 500; ++i)
  {
    original.m_block_ids.push_back(crypto::rand<crypto::hash>());
    original.m_block_weights.push_back(UINT64_MAX - i);
  }

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(500u, restored.m_block_ids.size());
  ASSERT_EQ(500u, restored.m_block_weights.size());
  for (size_t i = 0; i < 500; ++i)
  {
    ASSERT_EQ(original.m_block_ids[i], restored.m_block_ids[i]);
    ASSERT_EQ(original.m_block_weights[i], restored.m_block_weights[i]);
  }
}

// ===========================================================================
// 25. block_complete_entry – unpruned with many transactions
// ===========================================================================
TEST(protocol_pack, block_complete_entry_many_txs)
{
  cryptonote::block_complete_entry original;
  original.pruned = false;
  original.block = "large_block_blob";
  original.block_weight = 0;
  for (int i = 0; i < 50; ++i)
    original.txs.push_back(cryptonote::tx_blob_entry{"tx_number_" + std::to_string(i), crypto::null_hash});

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(50u, restored.txs.size());
  for (size_t i = 0; i < 50; ++i)
    ASSERT_EQ(original.txs[i].blob, restored.txs[i].blob);
}

// ===========================================================================
// 26. block_complete_entry – pruned with block_weight
// ===========================================================================
TEST(protocol_pack, block_complete_entry_pruned_with_weight)
{
  cryptonote::block_complete_entry original;
  original.pruned = true;
  original.block = "pruned_block";
  original.block_weight = UINT64_MAX;
  original.txs.push_back(cryptonote::tx_blob_entry{"ptx", crypto::rand<crypto::hash>()});

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(true, restored.pruned);
  ASSERT_EQ(UINT64_MAX, restored.block_weight);
  ASSERT_EQ(1u, restored.txs.size());
  ASSERT_EQ(original.txs[0].blob, restored.txs[0].blob);
  ASSERT_EQ(original.txs[0].prunable_hash, restored.txs[0].prunable_hash);
}

// ===========================================================================
// 27. NOTIFY_RESPONSE_GET_OBJECTS with only missed_ids
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_only_missed)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;
  for (int i = 0; i < 10; ++i)
    original.missed_ids.push_back(crypto::rand<crypto::hash>());
  original.current_blockchain_height = 1;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_TRUE(restored.blocks.empty());
  ASSERT_EQ(10u, restored.missed_ids.size());
  for (size_t i = 0; i < 10; ++i)
    ASSERT_EQ(original.missed_ids[i], restored.missed_ids[i]);
  ASSERT_EQ(1u, restored.current_blockchain_height);
}

// ===========================================================================
// 28. Double roundtrip – serialize, deserialize, re-serialize, compare buffers
// ===========================================================================
TEST(protocol_pack, core_sync_data_double_roundtrip)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 123456;
  original.cumulative_difficulty = 0xFFFFFFFFull;
  original.cumulative_difficulty_top64 = 1;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 14;
  original.pruning_seed = 256;

  // First roundtrip
  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  // Second roundtrip
  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA final_val;
  res = epee::serialization::load_t_from_binary(final_val, epee::to_span(buff2));
  ASSERT_TRUE(res);

  // Both buffers must produce the same data
  ASSERT_EQ(original.current_height, final_val.current_height);
  ASSERT_EQ(original.cumulative_difficulty, final_val.cumulative_difficulty);
  ASSERT_EQ(original.cumulative_difficulty_top64, final_val.cumulative_difficulty_top64);
  ASSERT_EQ(original.top_id, final_val.top_id);
  ASSERT_EQ(original.top_version, final_val.top_version);
  ASSERT_EQ(original.pruning_seed, final_val.pruning_seed);

  // The serialized buffers should be identical
  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 29. NOTIFY_REQUEST_CHAIN – large list
// ===========================================================================
TEST(protocol_pack, notify_request_chain_large_list)
{
  cryptonote::NOTIFY_REQUEST_CHAIN::request_t original;
  for (int i = 0; i < 300; ++i)
    original.block_ids.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_CHAIN::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(300u, restored.block_ids.size());
  auto it_orig = original.block_ids.begin();
  auto it_rest = restored.block_ids.begin();
  for (; it_orig != original.block_ids.end(); ++it_orig, ++it_rest)
    ASSERT_EQ(*it_orig, *it_rest);
  ASSERT_EQ(true, restored.prune);
}

// ===========================================================================
// 30. NOTIFY_NEW_BLOCK – max blockchain height
// ===========================================================================
TEST(protocol_pack, notify_new_block_max_height)
{
  cryptonote::NOTIFY_NEW_BLOCK::request_t original;
  original.current_blockchain_height = UINT64_MAX;
  original.b.block = "b";

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(UINT64_MAX, restored.current_blockchain_height);
  ASSERT_EQ("b", restored.b.block);
}

// ===========================================================================
// 31. Double roundtrip – NOTIFY_RESPONSE_CHAIN_ENTRY buffer stability
// ===========================================================================
TEST(protocol_pack, notify_response_chain_entry_double_roundtrip)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = 42;
  original.total_height = 100;
  original.cumulative_difficulty = 999;
  original.cumulative_difficulty_top64 = 7;
  original.first_block = "genesis";
  for (int i = 0; i < 5; ++i)
  {
    original.m_block_ids.push_back(crypto::rand<crypto::hash>());
    original.m_block_weights.push_back(i * 100);
  }

  // First roundtrip
  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  // Second roundtrip
  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 32. NOTIFY_REQUEST_GET_OBJECTS with single hash
// ===========================================================================
TEST(protocol_pack, notify_request_get_objects_single_hash)
{
  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t original;
  crypto::hash h = crypto::rand<crypto::hash>();
  original.blocks.push_back(h);
  original.prune = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(1u, restored.blocks.size());
  ASSERT_EQ(h, restored.blocks[0]);
  ASSERT_EQ(false, restored.prune);
}

// ===========================================================================
// 33. NOTIFY_GET_TXPOOL_COMPLEMENT – single hash
// ===========================================================================
TEST(protocol_pack, notify_get_txpool_complement_single_hash)
{
  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t original;
  crypto::hash h = crypto::rand<crypto::hash>();
  original.hashes.push_back(h);

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(1u, restored.hashes.size());
  ASSERT_EQ(h, restored.hashes[0]);
}

// ===========================================================================
// 34. NOTIFY_REQUEST_FLUFFY_MISSING_TX with max uint64 indices
// ===========================================================================
TEST(protocol_pack, notify_request_fluffy_missing_tx_max_indices)
{
  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t original;
  original.block_hash = crypto::rand<crypto::hash>();
  original.current_blockchain_height = 1;
  original.missing_tx_indices.push_back(UINT64_MAX);
  original.missing_tx_indices.push_back(UINT64_MAX - 1);
  original.missing_tx_indices.push_back(0);

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(3u, restored.missing_tx_indices.size());
  ASSERT_EQ(UINT64_MAX, restored.missing_tx_indices[0]);
  ASSERT_EQ(UINT64_MAX - 1, restored.missing_tx_indices[1]);
  ASSERT_EQ(0u, restored.missing_tx_indices[2]);
}

// ===========================================================================
// 35. tx_blob_entry with large blob
// ===========================================================================
TEST(protocol_pack, tx_blob_entry_large_blob)
{
  cryptonote::tx_blob_entry original;
  // Create a large-ish blob (10KB)
  original.blob.resize(10240, 'X');
  original.prunable_hash = crypto::rand<crypto::hash>();

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::tx_blob_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(10240u, restored.blob.size());
  ASSERT_EQ(original.blob, restored.blob);
  ASSERT_EQ(original.prunable_hash, restored.prunable_hash);
}

// ===========================================================================
// 36. NOTIFY_NEW_TRANSACTIONS with binary-like blobs
// ===========================================================================
TEST(protocol_pack, notify_new_transactions_binary_blobs)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  // Create blobs with null bytes and various byte values
  std::string blob1(256, '\0');
  for (int i = 0; i < 256; ++i)
    blob1[i] = static_cast<char>(i);
  original.txs.push_back(blob1);

  std::string blob2(32, '\xFF');
  original.txs.push_back(blob2);

  original.dandelionpp_fluff = false;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(2u, restored.txs.size());
  ASSERT_EQ(blob1, restored.txs[0]);
  ASSERT_EQ(blob2, restored.txs[1]);
  ASSERT_EQ(false, restored.dandelionpp_fluff);
}

// ===========================================================================
// 37. CORE_SYNC_DATA – null hash top_id
// ===========================================================================
TEST(protocol_pack, core_sync_data_null_hash)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 1;
  original.cumulative_difficulty = 1;
  original.cumulative_difficulty_top64 = 0;
  original.top_id = crypto::null_hash;
  original.top_version = 1;
  original.pruning_seed = 0;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::CORE_SYNC_DATA restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(crypto::null_hash, restored.top_id);
  ASSERT_EQ(1u, restored.current_height);
}

// ===========================================================================
// 38. Double roundtrip – NOTIFY_NEW_TRANSACTIONS buffer stability
// ===========================================================================
TEST(protocol_pack, notify_new_transactions_double_roundtrip)
{
  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t original;
  original.txs.push_back("tx_data_1");
  original.txs.push_back("tx_data_2");
  original._ = "pad";
  original.dandelionpp_fluff = false;

  // First roundtrip
  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_TRANSACTIONS::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  // Second roundtrip
  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 39. NOTIFY_RESPONSE_GET_OBJECTS with pruned blocks
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_pruned_blocks)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;

  original.blocks.push_back(make_bce_pruned());
  original.blocks.push_back(make_bce_pruned());
  original.current_blockchain_height = 100000;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(2u, restored.blocks.size());
  for (size_t i = 0; i < 2; ++i)
  {
    ASSERT_EQ(true, restored.blocks[i].pruned);
    ASSERT_EQ(original.blocks[i].block, restored.blocks[i].block);
    ASSERT_EQ(original.blocks[i].block_weight, restored.blocks[i].block_weight);
    ASSERT_EQ(original.blocks[i].txs.size(), restored.blocks[i].txs.size());
    for (size_t t = 0; t < original.blocks[i].txs.size(); ++t)
    {
      ASSERT_EQ(original.blocks[i].txs[t].blob, restored.blocks[i].txs[t].blob);
      ASSERT_EQ(original.blocks[i].txs[t].prunable_hash, restored.blocks[i].txs[t].prunable_hash);
    }
  }
  ASSERT_EQ(100000u, restored.current_blockchain_height);
}

// ===========================================================================
// 40. NOTIFY_RESPONSE_GET_OBJECTS with mixed pruned and unpruned blocks
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_mixed_pruned)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;

  original.blocks.push_back(make_bce_unpruned());
  original.blocks.push_back(make_bce_pruned());
  original.blocks.push_back(make_bce_unpruned());
  original.current_blockchain_height = 50000;

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(3u, restored.blocks.size());
  ASSERT_EQ(false, restored.blocks[0].pruned);
  ASSERT_EQ(true, restored.blocks[1].pruned);
  ASSERT_EQ(false, restored.blocks[2].pruned);
  ASSERT_EQ(50000u, restored.current_blockchain_height);
}

// ===========================================================================
// 41. Double roundtrip – NOTIFY_REQUEST_FLUFFY_MISSING_TX
// ===========================================================================
TEST(protocol_pack, notify_request_fluffy_missing_tx_double_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t original;
  original.block_hash = crypto::rand<crypto::hash>();
  original.current_blockchain_height = 12345;
  original.missing_tx_indices.push_back(1);
  original.missing_tx_indices.push_back(5);
  original.missing_tx_indices.push_back(10);

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 42. Double roundtrip – NOTIFY_GET_TXPOOL_COMPLEMENT
// ===========================================================================
TEST(protocol_pack, notify_get_txpool_complement_double_roundtrip)
{
  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t original;
  for (int i = 0; i < 3; ++i)
    original.hashes.push_back(crypto::rand<crypto::hash>());

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 43. Double roundtrip – block_complete_entry pruned
// ===========================================================================
TEST(protocol_pack, block_complete_entry_pruned_double_roundtrip)
{
  cryptonote::block_complete_entry original = make_bce_pruned();

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 44. NOTIFY_NEW_FLUFFY_BLOCK – large height
// ===========================================================================
TEST(protocol_pack, notify_new_fluffy_block_max_height)
{
  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t original;
  original.current_blockchain_height = UINT64_MAX;
  original.b.block = "b";

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(UINT64_MAX, restored.current_blockchain_height);
}

// ===========================================================================
// 45. block_complete_entry – single tx unpruned
// ===========================================================================
TEST(protocol_pack, block_complete_entry_single_tx_unpruned)
{
  cryptonote::block_complete_entry original;
  original.pruned = false;
  original.block = "single_tx_block";
  original.block_weight = 0;
  original.txs.push_back(cryptonote::tx_blob_entry{"the_only_tx", crypto::null_hash});

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::block_complete_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ(1u, restored.txs.size());
  ASSERT_EQ("the_only_tx", restored.txs[0].blob);
  ASSERT_EQ(false, restored.pruned);
}

// ===========================================================================
// 46. NOTIFY_RESPONSE_CHAIN_ENTRY – first_block non-empty
// ===========================================================================
TEST(protocol_pack, notify_response_chain_entry_first_block)
{
  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t original;
  original.start_height = 0;
  original.total_height = 1;
  original.cumulative_difficulty = 1;
  original.cumulative_difficulty_top64 = 0;
  original.first_block = "genesis_block_blob_data_here";

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ("genesis_block_blob_data_here", restored.first_block);
}

// ===========================================================================
// 47. connection_info with only string fields populated
// ===========================================================================
TEST(protocol_pack, connection_info_string_fields)
{
  cryptonote::connection_info original{};
  original.address = "1.2.3.4:18080";
  original.host = "1.2.3.4";
  original.ip = "1.2.3.4";
  original.port = "18080";
  original.peer_id = "abcdef";
  original.state = "synchronizing";
  original.connection_id = "uuid-here";

  epee::byte_slice buff;
  bool res = epee::serialization::store_t_to_binary(original, buff);
  ASSERT_TRUE(res);

  cryptonote::connection_info restored{};
  res = epee::serialization::load_t_from_binary(restored, epee::to_span(buff));
  ASSERT_TRUE(res);

  ASSERT_EQ("1.2.3.4:18080", restored.address);
  ASSERT_EQ("1.2.3.4", restored.host);
  ASSERT_EQ("1.2.3.4", restored.ip);
  ASSERT_EQ("18080", restored.port);
  ASSERT_EQ("abcdef", restored.peer_id);
  ASSERT_EQ("synchronizing", restored.state);
  ASSERT_EQ("uuid-here", restored.connection_id);
}

// ===========================================================================
// 48. NOTIFY_RESPONSE_GET_OBJECTS double roundtrip
// ===========================================================================
TEST(protocol_pack, notify_response_get_objects_double_roundtrip)
{
  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t original;
  original.blocks.push_back(make_bce_unpruned());
  original.missed_ids.push_back(crypto::rand<crypto::hash>());
  original.current_blockchain_height = 42;

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 49. NOTIFY_REQUEST_GET_OBJECTS double roundtrip
// ===========================================================================
TEST(protocol_pack, notify_request_get_objects_double_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t original;
  for (int i = 0; i < 5; ++i)
    original.blocks.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 50. NOTIFY_NEW_BLOCK double roundtrip
// ===========================================================================
TEST(protocol_pack, notify_new_block_double_roundtrip)
{
  cryptonote::NOTIFY_NEW_BLOCK::request_t original;
  original.b = make_bce_unpruned();
  original.current_blockchain_height = 777;

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_BLOCK::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 51. tx_blob_entry double roundtrip
// ===========================================================================
TEST(protocol_pack, tx_blob_entry_double_roundtrip)
{
  cryptonote::tx_blob_entry original;
  original.blob = "test_blob_data";
  original.prunable_hash = crypto::rand<crypto::hash>();

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::tx_blob_entry middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 52. NOTIFY_NEW_FLUFFY_BLOCK double roundtrip
// ===========================================================================
TEST(protocol_pack, notify_new_fluffy_block_double_roundtrip)
{
  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t original;
  original.b = make_bce_unpruned();
  original.current_blockchain_height = 555;

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}

// ===========================================================================
// 53. NOTIFY_REQUEST_CHAIN double roundtrip
// ===========================================================================
TEST(protocol_pack, notify_request_chain_double_roundtrip)
{
  cryptonote::NOTIFY_REQUEST_CHAIN::request_t original;
  for (int i = 0; i < 3; ++i)
    original.block_ids.push_back(crypto::rand<crypto::hash>());
  original.prune = true;

  epee::byte_slice buff1;
  bool res = epee::serialization::store_t_to_binary(original, buff1);
  ASSERT_TRUE(res);

  cryptonote::NOTIFY_REQUEST_CHAIN::request_t middle;
  res = epee::serialization::load_t_from_binary(middle, epee::to_span(buff1));
  ASSERT_TRUE(res);

  epee::byte_slice buff2;
  res = epee::serialization::store_t_to_binary(middle, buff2);
  ASSERT_TRUE(res);

  ASSERT_EQ(buff1.size(), buff2.size());
  ASSERT_EQ(0, memcmp(buff1.data(), buff2.data(), buff1.size()));
}
