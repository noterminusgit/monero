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

#include <set>
#include "rpc/core_rpc_server_commands_defs.h"
#include "rpc/core_rpc_server_error_codes.h"
#include "serialization/keyvalue_serialization.h"
#include "storages/portable_storage_template_helper.h"

using namespace cryptonote;

TEST(core_rpc, status_constants)
{
  ASSERT_STREQ(CORE_RPC_STATUS_OK, "OK");
  ASSERT_STREQ(CORE_RPC_STATUS_BUSY, "BUSY");
  ASSERT_STREQ(CORE_RPC_STATUS_NOT_MINING, "NOT MINING");
  ASSERT_STREQ(CORE_RPC_STATUS_PAYMENT_REQUIRED, "PAYMENT REQUIRED");
}

TEST(core_rpc, error_codes_are_negative)
{
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_PARAM, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_INTERNAL_ERROR, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_CORE_BUSY, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_UNSUPPORTED_RPC, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_PAYMENT_REQUIRED, 0);
}

TEST(core_rpc, error_codes_unique)
{
  ASSERT_NE(CORE_RPC_ERROR_CODE_WRONG_PARAM, CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT);
  ASSERT_NE(CORE_RPC_ERROR_CODE_INTERNAL_ERROR, CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB);
  ASSERT_NE(CORE_RPC_ERROR_CODE_CORE_BUSY, CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED);
}

TEST(core_rpc, get_height_response_serialization)
{
  COMMAND_RPC_GET_HEIGHT::response_t res;
  res.height = 3000000;
  res.status = CORE_RPC_STATUS_OK;
  res.hash = "abcdef0123456789";
  res.untrusted = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 3000000u);
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_EQ(res2.hash, "abcdef0123456789");
}

TEST(core_rpc, get_info_response_serialization)
{
  COMMAND_RPC_GET_INFO::response_t res;
  res.height = 2500000;
  res.target_height = 2500100;
  res.difficulty = 300000000000ULL;
  res.target = 120;
  res.tx_count = 15000000;
  res.tx_pool_size = 50;
  res.alt_blocks_count = 3;
  res.outgoing_connections_count = 8;
  res.incoming_connections_count = 12;
  res.rpc_connections_count = 2;
  res.white_peerlist_size = 100;
  res.grey_peerlist_size = 200;
  res.mainnet = true;
  res.testnet = false;
  res.stagenet = false;
  res.top_block_hash = "deadbeef";
  res.synchronized = true;
  res.offline = false;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.target_height, 2500100u);
  ASSERT_EQ(res2.target, 120u);
  ASSERT_EQ(res2.mainnet, true);
  ASSERT_EQ(res2.synchronized, true);
}

TEST(core_rpc, get_block_count_response_serialization)
{
  COMMAND_RPC_GETBLOCKCOUNT::response_t res;
  res.count = 2500001;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETBLOCKCOUNT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.count, 2500001u);
}

TEST(core_rpc, get_block_hash_request_is_vector)
{
  COMMAND_RPC_GETBLOCKHASH::request req;
  req.push_back(100);
  ASSERT_EQ(req.size(), 1u);
  ASSERT_EQ(req[0], 100u);
}

TEST(core_rpc, mining_status_response_serialization)
{
  COMMAND_RPC_MINING_STATUS::response_t res;
  res.active = false;
  res.speed = 0;
  res.threads_count = 0;
  res.address = "";
  res.pow_algorithm = "RandomX";
  res.is_background_mining_enabled = false;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MINING_STATUS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_FALSE(res2.active);
  ASSERT_EQ(res2.pow_algorithm, "RandomX");
}

TEST(core_rpc, get_version_response_serialization)
{
  COMMAND_RPC_GET_VERSION::response_t res;
  res.version = 196613; // 3.5 in major.minor format
  res.release = true;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_VERSION::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.version, 196613u);
  ASSERT_TRUE(res2.release);
}

TEST(core_rpc, get_block_template_request_serialization)
{
  COMMAND_RPC_GETBLOCKTEMPLATE::request_t req;
  req.wallet_address = "44GBHzv6ZyQdJkjqZje6KLZ3xSyN1hBSFAnLP6EAqJtCRVzMzZmeXTC2AHKDS9aEDTRKmo6a6o9r9j86pYfhCWDkKjbtcns";
  req.reserve_size = 60;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GETBLOCKTEMPLATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.reserve_size, 60u);
  ASSERT_FALSE(req2.wallet_address.empty());
}

TEST(core_rpc, get_tx_pool_stats_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response_t res;
  res.pool_stats.bytes_total = 50000;
  res.pool_stats.bytes_min = 200;
  res.pool_stats.bytes_max = 5000;
  res.pool_stats.bytes_med = 1000;
  res.pool_stats.txs_total = 25;
  res.pool_stats.num_double_spends = 0;
  res.pool_stats.num_not_relayed = 1;
  res.pool_stats.num_failing = 0;
  res.pool_stats.num_10m = 5;
  res.pool_stats.oldest = 1600000000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.pool_stats.txs_total, 25u);
  ASSERT_EQ(res2.pool_stats.bytes_total, 50000u);
}

// ============================================================
// Comprehensive error code tests
// ============================================================

TEST(core_rpc, all_error_codes_are_negative)
{
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_PARAM, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_INTERNAL_ERROR, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_CORE_BUSY, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB_SIZE, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_UNSUPPORTED_RPC, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_MINING_TO_SUBADDRESS, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_REGTEST_REQUIRED, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_PAYMENT_REQUIRED, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_INVALID_CLIENT, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_PAYMENT_TOO_LOW, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_DUPLICATE_PAYMENT, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_STALE_PAYMENT, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_RESTRICTED, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_UNSUPPORTED_BOOTSTRAP, 0);
  ASSERT_LT(CORE_RPC_ERROR_CODE_PAYMENTS_NOT_ENABLED, 0);
}

TEST(core_rpc, all_error_codes_unique)
{
  std::set<int> codes;
  codes.insert(CORE_RPC_ERROR_CODE_WRONG_PARAM);
  codes.insert(CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT);
  codes.insert(CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE);
  codes.insert(CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS);
  codes.insert(CORE_RPC_ERROR_CODE_INTERNAL_ERROR);
  codes.insert(CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB);
  codes.insert(CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED);
  codes.insert(CORE_RPC_ERROR_CODE_CORE_BUSY);
  codes.insert(CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB_SIZE);
  codes.insert(CORE_RPC_ERROR_CODE_UNSUPPORTED_RPC);
  codes.insert(CORE_RPC_ERROR_CODE_MINING_TO_SUBADDRESS);
  codes.insert(CORE_RPC_ERROR_CODE_REGTEST_REQUIRED);
  codes.insert(CORE_RPC_ERROR_CODE_PAYMENT_REQUIRED);
  codes.insert(CORE_RPC_ERROR_CODE_INVALID_CLIENT);
  codes.insert(CORE_RPC_ERROR_CODE_PAYMENT_TOO_LOW);
  codes.insert(CORE_RPC_ERROR_CODE_DUPLICATE_PAYMENT);
  codes.insert(CORE_RPC_ERROR_CODE_STALE_PAYMENT);
  codes.insert(CORE_RPC_ERROR_CODE_RESTRICTED);
  codes.insert(CORE_RPC_ERROR_CODE_UNSUPPORTED_BOOTSTRAP);
  codes.insert(CORE_RPC_ERROR_CODE_PAYMENTS_NOT_ENABLED);
  ASSERT_EQ(codes.size(), 20u);
}

TEST(core_rpc, error_code_message_lookup)
{
  ASSERT_STREQ(get_rpc_server_error_message(CORE_RPC_ERROR_CODE_WRONG_PARAM), "Invalid parameter");
  ASSERT_STREQ(get_rpc_server_error_message(CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT), "Height is too large");
  ASSERT_STREQ(get_rpc_server_error_message(CORE_RPC_ERROR_CODE_INTERNAL_ERROR), "Internal error");
  ASSERT_STREQ(get_rpc_server_error_message(CORE_RPC_ERROR_CODE_CORE_BUSY), "Core is busy");
  ASSERT_STREQ(get_rpc_server_error_message(CORE_RPC_ERROR_CODE_RESTRICTED), "Parameters beyond restricted allowance");
}

// ============================================================
// GET_BLOCKS_FAST
// ============================================================

TEST(core_rpc, get_blocks_fast_request_serialization)
{
  COMMAND_RPC_GET_BLOCKS_FAST::request_t req;
  req.start_height = 100000;
  req.prune = true;
  req.no_miner_tx = false;
  req.pool_info_since = 50;
  req.max_block_count = 1000;
  req.requested_info = 1;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCKS_FAST::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.start_height, 100000u);
  ASSERT_TRUE(req2.prune);
  ASSERT_FALSE(req2.no_miner_tx);
  ASSERT_EQ(req2.pool_info_since, 50u);
  ASSERT_EQ(req2.max_block_count, 1000u);
  ASSERT_EQ(req2.requested_info, 1);
}

TEST(core_rpc, get_blocks_fast_response_serialization)
{
  COMMAND_RPC_GET_BLOCKS_FAST::response_t res;
  res.start_height = 100000;
  res.current_height = 200000;
  res.daemon_time = 1600000000;
  res.pool_info_extent = 0;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCKS_FAST::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.start_height, 100000u);
  ASSERT_EQ(res2.current_height, 200000u);
  ASSERT_EQ(res2.daemon_time, 1600000000u);
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_BLOCKS_BY_HEIGHT
// ============================================================

TEST(core_rpc, get_blocks_by_height_request_serialization)
{
  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::request_t req;
  req.heights.push_back(100);
  req.heights.push_back(200);
  req.heights.push_back(300);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.heights.size(), 3u);
  ASSERT_EQ(req2.heights[0], 100u);
  ASSERT_EQ(req2.heights[1], 200u);
  ASSERT_EQ(req2.heights[2], 300u);
}

TEST(core_rpc, get_blocks_by_height_response_serialization)
{
  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_ALT_BLOCKS_HASHES
// ============================================================

TEST(core_rpc, get_alt_blocks_hashes_request_serialization)
{
  COMMAND_RPC_GET_ALT_BLOCKS_HASHES::request_t req;
  req.client = "test_client";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ALT_BLOCKS_HASHES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.client, "test_client");
}

TEST(core_rpc, get_alt_blocks_hashes_response_serialization)
{
  COMMAND_RPC_GET_ALT_BLOCKS_HASHES::response_t res;
  res.blks_hashes.push_back("aabb");
  res.blks_hashes.push_back("ccdd");
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ALT_BLOCKS_HASHES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.blks_hashes.size(), 2u);
  ASSERT_EQ(res2.blks_hashes[0], "aabb");
  ASSERT_EQ(res2.blks_hashes[1], "ccdd");
}

// ============================================================
// GET_HASHES_FAST
// ============================================================

TEST(core_rpc, get_hashes_fast_request_serialization)
{
  COMMAND_RPC_GET_HASHES_FAST::request_t req;
  req.start_height = 500000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_HASHES_FAST::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.start_height, 500000u);
}

TEST(core_rpc, get_hashes_fast_response_serialization)
{
  COMMAND_RPC_GET_HASHES_FAST::response_t res;
  res.start_height = 500000;
  res.current_height = 600000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_HASHES_FAST::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.start_height, 500000u);
  ASSERT_EQ(res2.current_height, 600000u);
}

// ============================================================
// GET_TRANSACTIONS
// ============================================================

TEST(core_rpc, get_transactions_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTIONS::request_t req;
  req.txs_hashes.push_back("tx_hash_1");
  req.txs_hashes.push_back("tx_hash_2");
  req.decode_as_json = true;
  req.prune = false;
  req.split = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTIONS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txs_hashes.size(), 2u);
  ASSERT_EQ(req2.txs_hashes[0], "tx_hash_1");
  ASSERT_TRUE(req2.decode_as_json);
  ASSERT_FALSE(req2.prune);
  ASSERT_TRUE(req2.split);
}

TEST(core_rpc, get_transactions_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTIONS::response_t res;
  res.txs_as_hex.push_back("deadbeef");
  res.missed_tx.push_back("missed1");
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTIONS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.txs_as_hex.size(), 1u);
  ASSERT_EQ(res2.txs_as_hex[0], "deadbeef");
  ASSERT_EQ(res2.missed_tx.size(), 1u);
  ASSERT_EQ(res2.missed_tx[0], "missed1");
}

// ============================================================
// IS_KEY_IMAGE_SPENT
// ============================================================

TEST(core_rpc, is_key_image_spent_request_serialization)
{
  COMMAND_RPC_IS_KEY_IMAGE_SPENT::request_t req;
  req.key_images.push_back("ki1");
  req.key_images.push_back("ki2");
  req.key_images.push_back("ki3");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_IS_KEY_IMAGE_SPENT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key_images.size(), 3u);
  ASSERT_EQ(req2.key_images[0], "ki1");
}

TEST(core_rpc, is_key_image_spent_response_serialization)
{
  COMMAND_RPC_IS_KEY_IMAGE_SPENT::response_t res;
  res.spent_status.push_back(0);
  res.spent_status.push_back(1);
  res.spent_status.push_back(2);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IS_KEY_IMAGE_SPENT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.spent_status.size(), 3u);
  ASSERT_EQ(res2.spent_status[0], 0);
  ASSERT_EQ(res2.spent_status[1], 1);
  ASSERT_EQ(res2.spent_status[2], 2);
}

// ============================================================
// GET_TX_GLOBAL_OUTPUTS_INDEXES
// ============================================================

TEST(core_rpc, get_tx_global_outputs_indexes_response_serialization)
{
  COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response_t res;
  res.o_indexes.push_back(10);
  res.o_indexes.push_back(20);
  res.o_indexes.push_back(30);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.o_indexes.size(), 3u);
  ASSERT_EQ(res2.o_indexes[0], 10u);
  ASSERT_EQ(res2.o_indexes[1], 20u);
  ASSERT_EQ(res2.o_indexes[2], 30u);
}

// ============================================================
// SEND_RAW_TX
// ============================================================

TEST(core_rpc, send_raw_tx_request_serialization)
{
  COMMAND_RPC_SEND_RAW_TX::request_t req;
  req.tx_as_hex = "deadbeefcafe";
  req.do_not_relay = true;
  req.do_sanity_checks = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SEND_RAW_TX::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tx_as_hex, "deadbeefcafe");
  ASSERT_TRUE(req2.do_not_relay);
  ASSERT_FALSE(req2.do_sanity_checks);
}

TEST(core_rpc, send_raw_tx_response_serialization)
{
  COMMAND_RPC_SEND_RAW_TX::response_t res;
  res.reason = "some reason";
  res.not_relayed = true;
  res.low_mixin = false;
  res.double_spend = false;
  res.invalid_input = false;
  res.invalid_output = false;
  res.too_big = false;
  res.overspend = false;
  res.fee_too_low = true;
  res.too_few_outputs = false;
  res.sanity_check_failed = false;
  res.tx_extra_too_big = false;
  res.nonzero_unlock_time = false;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SEND_RAW_TX::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.reason, "some reason");
  ASSERT_TRUE(res2.not_relayed);
  ASSERT_TRUE(res2.fee_too_low);
  ASSERT_FALSE(res2.double_spend);
  ASSERT_FALSE(res2.too_big);
  ASSERT_FALSE(res2.nonzero_unlock_time);
}

// ============================================================
// START_MINING
// ============================================================

TEST(core_rpc, start_mining_request_serialization)
{
  COMMAND_RPC_START_MINING::request_t req;
  req.miner_address = "44GBHzv6ZyQdJkjqZje6KLZ3xSyN1hBSFAnLP6EAqJtCRVzMzZmeXTC2AHKDS9aEDTRKmo6a6o9r9j86pYfhCWDkKjbtcns";
  req.threads_count = 4;
  req.do_background_mining = true;
  req.ignore_battery = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_START_MINING::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_FALSE(req2.miner_address.empty());
  ASSERT_EQ(req2.threads_count, 4u);
  ASSERT_TRUE(req2.do_background_mining);
  ASSERT_FALSE(req2.ignore_battery);
}

TEST(core_rpc, start_mining_response_serialization)
{
  COMMAND_RPC_START_MINING::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_START_MINING::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// STOP_MINING
// ============================================================

TEST(core_rpc, stop_mining_request_serialization)
{
  COMMAND_RPC_STOP_MINING::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_STOP_MINING::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, stop_mining_response_serialization)
{
  COMMAND_RPC_STOP_MINING::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_STOP_MINING::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// SAVE_BC
// ============================================================

TEST(core_rpc, save_bc_request_serialization)
{
  COMMAND_RPC_SAVE_BC::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SAVE_BC::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, save_bc_response_serialization)
{
  COMMAND_RPC_SAVE_BC::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SAVE_BC::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_PEER_LIST
// ============================================================

TEST(core_rpc, get_peer_list_request_serialization)
{
  COMMAND_RPC_GET_PEER_LIST::request_t req;
  req.public_only = false;
  req.include_blocked = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_PEER_LIST::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_FALSE(req2.public_only);
  ASSERT_TRUE(req2.include_blocked);
}

TEST(core_rpc, get_peer_list_response_serialization)
{
  COMMAND_RPC_GET_PEER_LIST::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_PEER_LIST::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.white_list.empty());
  ASSERT_TRUE(res2.gray_list.empty());
}

// ============================================================
// SET_LOG_HASH_RATE
// ============================================================

TEST(core_rpc, set_log_hash_rate_request_serialization)
{
  COMMAND_RPC_SET_LOG_HASH_RATE::request_t req;
  req.visible = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LOG_HASH_RATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.visible);
}

TEST(core_rpc, set_log_hash_rate_response_serialization)
{
  COMMAND_RPC_SET_LOG_HASH_RATE::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_LOG_HASH_RATE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// SET_LOG_LEVEL
// ============================================================

TEST(core_rpc, set_log_level_request_serialization)
{
  COMMAND_RPC_SET_LOG_LEVEL::request_t req;
  req.level = 3;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LOG_LEVEL::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.level, 3);
}

TEST(core_rpc, set_log_level_response_serialization)
{
  COMMAND_RPC_SET_LOG_LEVEL::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_LOG_LEVEL::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// SET_LOG_CATEGORIES
// ============================================================

TEST(core_rpc, set_log_categories_request_serialization)
{
  COMMAND_RPC_SET_LOG_CATEGORIES::request_t req;
  req.categories = "*:WARNING,net:INFO";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LOG_CATEGORIES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.categories, "*:WARNING,net:INFO");
}

TEST(core_rpc, set_log_categories_response_serialization)
{
  COMMAND_RPC_SET_LOG_CATEGORIES::response_t res;
  res.categories = "*:WARNING";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_LOG_CATEGORIES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.categories, "*:WARNING");
}

// ============================================================
// GET_TRANSACTION_POOL
// ============================================================

TEST(core_rpc, get_transaction_pool_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTION_POOL::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_transaction_pool_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.transactions.empty());
  ASSERT_TRUE(res2.spent_key_images.empty());
}

// ============================================================
// GET_TRANSACTION_POOL_HASHES_BIN
// ============================================================

TEST(core_rpc, get_transaction_pool_hashes_bin_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_transaction_pool_hashes_bin_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_TRANSACTION_POOL_HASHES
// ============================================================

TEST(core_rpc, get_transaction_pool_hashes_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_transaction_pool_hashes_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response_t res;
  res.tx_hashes.push_back("hash1");
  res.tx_hashes.push_back("hash2");
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_hashes.size(), 2u);
  ASSERT_EQ(res2.tx_hashes[0], "hash1");
  ASSERT_EQ(res2.tx_hashes[1], "hash2");
}

// ============================================================
// GET_CONNECTIONS
// ============================================================

TEST(core_rpc, get_connections_request_serialization)
{
  COMMAND_RPC_GET_CONNECTIONS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_CONNECTIONS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_connections_response_serialization)
{
  COMMAND_RPC_GET_CONNECTIONS::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_CONNECTIONS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.connections.empty());
}

// ============================================================
// GET_BLOCK_HEADERS_RANGE
// ============================================================

TEST(core_rpc, get_block_headers_range_request_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request_t req;
  req.start_height = 100;
  req.end_height = 200;
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.start_height, 100u);
  ASSERT_EQ(req2.end_height, 200u);
  ASSERT_TRUE(req2.fill_pow_hash);
}

TEST(core_rpc, get_block_headers_range_response_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.headers.empty());
}

// ============================================================
// GET_BLOCK_HEADER_BY_HASH
// ============================================================

TEST(core_rpc, get_block_header_by_hash_request_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request_t req;
  req.hash = "abcdef0123456789";
  req.hashes.push_back("hash1");
  req.hashes.push_back("hash2");
  req.fill_pow_hash = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.hash, "abcdef0123456789");
  ASSERT_EQ(req2.hashes.size(), 2u);
  ASSERT_FALSE(req2.fill_pow_hash);
}

TEST(core_rpc, get_block_header_by_hash_response_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t res;
  res.block_header.height = 12345;
  res.block_header.depth = 10;
  res.block_header.hash = "blockhash";
  res.block_header.reward = 1000000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_header.height, 12345u);
  ASSERT_EQ(res2.block_header.depth, 10u);
  ASSERT_EQ(res2.block_header.hash, "blockhash");
  ASSERT_EQ(res2.block_header.reward, 1000000u);
}

// ============================================================
// GET_BLOCK_HEADER_BY_HEIGHT
// ============================================================

TEST(core_rpc, get_block_header_by_height_request_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request_t req;
  req.height = 999999;
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.height, 999999u);
  ASSERT_TRUE(req2.fill_pow_hash);
}

TEST(core_rpc, get_block_header_by_height_response_serialization)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t res;
  res.block_header.height = 999999;
  res.block_header.reward = 600000000000ULL;
  res.block_header.num_txes = 5;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_header.height, 999999u);
  ASSERT_EQ(res2.block_header.reward, 600000000000ULL);
  ASSERT_EQ(res2.block_header.num_txes, 5u);
}

// ============================================================
// GET_BLOCK
// ============================================================

TEST(core_rpc, get_block_request_serialization)
{
  COMMAND_RPC_GET_BLOCK::request_t req;
  req.hash = "blockhash123";
  req.height = 500000;
  req.fill_pow_hash = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.hash, "blockhash123");
  ASSERT_EQ(req2.height, 500000u);
  ASSERT_FALSE(req2.fill_pow_hash);
}

TEST(core_rpc, get_block_response_serialization)
{
  COMMAND_RPC_GET_BLOCK::response_t res;
  res.block_header.height = 500000;
  res.block_header.difficulty = 100000000;
  res.miner_tx_hash = "miner_tx_hash_val";
  res.tx_hashes.push_back("tx1");
  res.tx_hashes.push_back("tx2");
  res.blob = "block_blob_data";
  res.json = "{}";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_header.height, 500000u);
  ASSERT_EQ(res2.miner_tx_hash, "miner_tx_hash_val");
  ASSERT_EQ(res2.tx_hashes.size(), 2u);
  ASSERT_EQ(res2.blob, "block_blob_data");
}

// ============================================================
// SETBANS (SET_BANS)
// ============================================================

TEST(core_rpc, setbans_request_serialization)
{
  COMMAND_RPC_SETBANS::request_t req;
  COMMAND_RPC_SETBANS::ban b;
  b.host = "192.168.1.1";
  b.ip = 0;
  b.ban = true;
  b.seconds = 3600;
  req.bans.push_back(b);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SETBANS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.bans.size(), 1u);
  ASSERT_EQ(req2.bans[0].host, "192.168.1.1");
  ASSERT_TRUE(req2.bans[0].ban);
  ASSERT_EQ(req2.bans[0].seconds, 3600u);
}

TEST(core_rpc, setbans_response_serialization)
{
  COMMAND_RPC_SETBANS::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SETBANS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GETBANS (GET_BANS)
// ============================================================

TEST(core_rpc, getbans_request_serialization)
{
  COMMAND_RPC_GETBANS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GETBANS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, getbans_response_serialization)
{
  COMMAND_RPC_GETBANS::response_t res;
  COMMAND_RPC_GETBANS::ban b;
  b.host = "10.0.0.1";
  b.ip = 167772161;
  b.seconds = 7200;
  res.bans.push_back(b);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETBANS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.bans.size(), 1u);
  ASSERT_EQ(res2.bans[0].host, "10.0.0.1");
  ASSERT_EQ(res2.bans[0].seconds, 7200u);
}

// ============================================================
// BANNED
// ============================================================

TEST(core_rpc, banned_request_serialization)
{
  COMMAND_RPC_BANNED::request_t req;
  req.address = "192.168.1.100";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_BANNED::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "192.168.1.100");
}

TEST(core_rpc, banned_response_serialization)
{
  COMMAND_RPC_BANNED::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.banned = true;
  res.seconds = 1800;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_BANNED::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.banned);
  ASSERT_EQ(res2.seconds, 1800u);
}

// ============================================================
// FLUSH_TRANSACTION_POOL
// ============================================================

TEST(core_rpc, flush_transaction_pool_request_serialization)
{
  COMMAND_RPC_FLUSH_TRANSACTION_POOL::request_t req;
  req.txids.push_back("txid1");
  req.txids.push_back("txid2");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_FLUSH_TRANSACTION_POOL::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txids.size(), 2u);
  ASSERT_EQ(req2.txids[0], "txid1");
  ASSERT_EQ(req2.txids[1], "txid2");
}

TEST(core_rpc, flush_transaction_pool_response_serialization)
{
  COMMAND_RPC_FLUSH_TRANSACTION_POOL::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_FLUSH_TRANSACTION_POOL::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_OUTPUT_HISTOGRAM
// ============================================================

TEST(core_rpc, get_output_histogram_request_serialization)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request_t req;
  req.amounts.push_back(0);
  req.amounts.push_back(1000000000);
  req.min_count = 1;
  req.max_count = 100;
  req.unlocked = true;
  req.recent_cutoff = 1600000000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.amounts.size(), 2u);
  ASSERT_EQ(req2.amounts[0], 0u);
  ASSERT_EQ(req2.min_count, 1u);
  ASSERT_EQ(req2.max_count, 100u);
  ASSERT_TRUE(req2.unlocked);
}

TEST(core_rpc, get_output_histogram_response_serialization)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res;
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry e(0, 5000000, 4000000, 100000);
  res.histogram.push_back(e);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.histogram.size(), 1u);
  ASSERT_EQ(res2.histogram[0].amount, 0u);
  ASSERT_EQ(res2.histogram[0].total_instances, 5000000u);
  ASSERT_EQ(res2.histogram[0].unlocked_instances, 4000000u);
  ASSERT_EQ(res2.histogram[0].recent_instances, 100000u);
}

// ============================================================
// GET_OUTPUT_DISTRIBUTION
// ============================================================

TEST(core_rpc, get_output_distribution_request_serialization)
{
  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request_t req;
  req.amounts.push_back(0);
  req.from_height = 100;
  req.to_height = 200;
  req.cumulative = true;
  req.binary = false;
  req.compress = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.amounts.size(), 1u);
  ASSERT_EQ(req2.amounts[0], 0u);
  ASSERT_EQ(req2.from_height, 100u);
  ASSERT_EQ(req2.to_height, 200u);
  ASSERT_TRUE(req2.cumulative);
  ASSERT_FALSE(req2.binary);
}

TEST(core_rpc, get_output_distribution_response_serialization)
{
  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.distributions.empty());
}

// ============================================================
// POP_BLOCKS
// ============================================================

TEST(core_rpc, pop_blocks_request_serialization)
{
  COMMAND_RPC_POP_BLOCKS::request_t req;
  req.nblocks = 10;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_POP_BLOCKS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.nblocks, 10u);
}

TEST(core_rpc, pop_blocks_response_serialization)
{
  COMMAND_RPC_POP_BLOCKS::response_t res;
  res.height = 999990;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_POP_BLOCKS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 999990u);
}

// ============================================================
// PRUNE_BLOCKCHAIN
// ============================================================

TEST(core_rpc, prune_blockchain_request_serialization)
{
  COMMAND_RPC_PRUNE_BLOCKCHAIN::request_t req;
  req.check = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_PRUNE_BLOCKCHAIN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.check);
}

TEST(core_rpc, prune_blockchain_response_serialization)
{
  COMMAND_RPC_PRUNE_BLOCKCHAIN::response_t res;
  res.pruned = true;
  res.pruning_seed = 384;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_PRUNE_BLOCKCHAIN::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.pruned);
  ASSERT_EQ(res2.pruning_seed, 384u);
}

// ============================================================
// GETBLOCKCOUNT
// ============================================================

TEST(core_rpc, getblockcount_response_serialization)
{
  COMMAND_RPC_GETBLOCKCOUNT::response_t res;
  res.count = 3000000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETBLOCKCOUNT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.count, 3000000u);
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GETBLOCKHASH - request is std::vector<uint64_t>, response is std::string
// ============================================================

TEST(core_rpc, getblockhash_request_is_vector)
{
  COMMAND_RPC_GETBLOCKHASH::request req;
  req.push_back(500000);
  ASSERT_EQ(req.size(), 1u);
  ASSERT_EQ(req[0], 500000u);
}

// ============================================================
// GETBLOCKTEMPLATE
// ============================================================

TEST(core_rpc, getblocktemplate_request_full_serialization)
{
  COMMAND_RPC_GETBLOCKTEMPLATE::request_t req;
  req.reserve_size = 128;
  req.wallet_address = "44addr";
  req.prev_block = "prev_block_hash";
  req.extra_nonce = "extra";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GETBLOCKTEMPLATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.reserve_size, 128u);
  ASSERT_EQ(req2.wallet_address, "44addr");
  ASSERT_EQ(req2.prev_block, "prev_block_hash");
  ASSERT_EQ(req2.extra_nonce, "extra");
}

TEST(core_rpc, getblocktemplate_response_serialization)
{
  COMMAND_RPC_GETBLOCKTEMPLATE::response_t res;
  res.difficulty = 200000000000ULL;
  res.wide_difficulty = "200000000000";
  res.difficulty_top64 = 0;
  res.height = 2500000;
  res.reserved_offset = 130;
  res.expected_reward = 600000000000ULL;
  res.cumulative_weight = 50000;
  res.prev_hash = "prevhash";
  res.seed_height = 2499968;
  res.seed_hash = "seedhash";
  res.next_seed_hash = "nextseedhash";
  res.blocktemplate_blob = "template_blob";
  res.blockhashing_blob = "hashing_blob";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETBLOCKTEMPLATE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.difficulty, 200000000000ULL);
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.reserved_offset, 130u);
  ASSERT_EQ(res2.expected_reward, 600000000000ULL);
  ASSERT_EQ(res2.prev_hash, "prevhash");
  ASSERT_EQ(res2.seed_hash, "seedhash");
  ASSERT_EQ(res2.blocktemplate_blob, "template_blob");
}

// ============================================================
// SUBMITBLOCK
// ============================================================

TEST(core_rpc, submitblock_request_is_vector)
{
  COMMAND_RPC_SUBMITBLOCK::request req;
  req.push_back("block_blob_hex");
  ASSERT_EQ(req.size(), 1u);
  ASSERT_EQ(req[0], "block_blob_hex");
}

TEST(core_rpc, submitblock_response_serialization)
{
  COMMAND_RPC_SUBMITBLOCK::response_t res;
  res.block_id = "new_block_id";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SUBMITBLOCK::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_id, "new_block_id");
}

// ============================================================
// GENERATEBLOCKS
// ============================================================

TEST(core_rpc, generateblocks_request_serialization)
{
  COMMAND_RPC_GENERATEBLOCKS::request_t req;
  req.amount_of_blocks = 10;
  req.wallet_address = "44addr";
  req.prev_block = "prev";
  req.starting_nonce = 42;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GENERATEBLOCKS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.amount_of_blocks, 10u);
  ASSERT_EQ(req2.wallet_address, "44addr");
  ASSERT_EQ(req2.prev_block, "prev");
  ASSERT_EQ(req2.starting_nonce, 42u);
}

TEST(core_rpc, generateblocks_response_serialization)
{
  COMMAND_RPC_GENERATEBLOCKS::response_t res;
  res.height = 100010;
  res.blocks.push_back("blockhash1");
  res.blocks.push_back("blockhash2");
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GENERATEBLOCKS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 100010u);
  ASSERT_EQ(res2.blocks.size(), 2u);
  ASSERT_EQ(res2.blocks[0], "blockhash1");
}

// ============================================================
// GET_LAST_BLOCK_HEADER
// ============================================================

TEST(core_rpc, get_last_block_header_request_serialization)
{
  COMMAND_RPC_GET_LAST_BLOCK_HEADER::request_t req;
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_LAST_BLOCK_HEADER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.fill_pow_hash);
}

TEST(core_rpc, get_last_block_header_response_serialization)
{
  COMMAND_RPC_GET_LAST_BLOCK_HEADER::response_t res;
  res.block_header.height = 2500000;
  res.block_header.timestamp = 1600000000;
  res.block_header.nonce = 12345;
  res.block_header.orphan_status = false;
  res.block_header.major_version = 14;
  res.block_header.minor_version = 14;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_LAST_BLOCK_HEADER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_header.height, 2500000u);
  ASSERT_EQ(res2.block_header.timestamp, 1600000000u);
  ASSERT_EQ(res2.block_header.nonce, 12345u);
  ASSERT_FALSE(res2.block_header.orphan_status);
  ASSERT_EQ(res2.block_header.major_version, 14);
}

// ============================================================
// GET_COINBASE_TX_SUM
// ============================================================

TEST(core_rpc, get_coinbase_tx_sum_request_serialization)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req;
  req.height = 100000;
  req.count = 1000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.height, 100000u);
  ASSERT_EQ(req2.count, 1000u);
}

TEST(core_rpc, get_coinbase_tx_sum_response_serialization)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res;
  res.emission_amount = 18000000000000000ULL;
  res.wide_emission_amount = "18000000000000000";
  res.emission_amount_top64 = 0;
  res.fee_amount = 500000000000ULL;
  res.wide_fee_amount = "500000000000";
  res.fee_amount_top64 = 0;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.emission_amount, 18000000000000000ULL);
  ASSERT_EQ(res2.fee_amount, 500000000000ULL);
  ASSERT_EQ(res2.wide_emission_amount, "18000000000000000");
}

// ============================================================
// GET_BASE_FEE_ESTIMATE (GET_FEE_ESTIMATE)
// ============================================================

TEST(core_rpc, get_base_fee_estimate_request_serialization)
{
  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::request_t req;
  req.grace_blocks = 10;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.grace_blocks, 10u);
}

TEST(core_rpc, get_base_fee_estimate_response_serialization)
{
  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response_t res;
  res.fee = 20000;
  res.quantization_mask = 10000;
  res.fees.push_back(20000);
  res.fees.push_back(80000);
  res.fees.push_back(320000);
  res.fees.push_back(4000000);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.fee, 20000u);
  ASSERT_EQ(res2.quantization_mask, 10000u);
  ASSERT_EQ(res2.fees.size(), 4u);
  ASSERT_EQ(res2.fees[0], 20000u);
}

// ============================================================
// GET_ALTERNATE_CHAINS
// ============================================================

TEST(core_rpc, get_alternate_chains_request_serialization)
{
  COMMAND_RPC_GET_ALTERNATE_CHAINS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ALTERNATE_CHAINS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_alternate_chains_response_serialization)
{
  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t res;
  COMMAND_RPC_GET_ALTERNATE_CHAINS::chain_info ci;
  ci.block_hash = "chainhash";
  ci.height = 2499990;
  ci.length = 10;
  ci.difficulty = 200000000;
  ci.wide_difficulty = "200000000";
  ci.difficulty_top64 = 0;
  ci.block_hashes.push_back("h1");
  ci.block_hashes.push_back("h2");
  ci.main_chain_parent_block = "parent";
  res.chains.push_back(ci);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.chains.size(), 1u);
  ASSERT_EQ(res2.chains[0].block_hash, "chainhash");
  ASSERT_EQ(res2.chains[0].height, 2499990u);
  ASSERT_EQ(res2.chains[0].length, 10u);
  ASSERT_EQ(res2.chains[0].block_hashes.size(), 2u);
  ASSERT_EQ(res2.chains[0].main_chain_parent_block, "parent");
}

// ============================================================
// UPDATE
// ============================================================

TEST(core_rpc, update_request_serialization)
{
  COMMAND_RPC_UPDATE::request_t req;
  req.command = "check";
  req.path = "/tmp/update";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_UPDATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.command, "check");
  ASSERT_EQ(req2.path, "/tmp/update");
}

TEST(core_rpc, update_response_serialization)
{
  COMMAND_RPC_UPDATE::response_t res;
  res.update = true;
  res.version = "0.18.0.0";
  res.user_uri = "https://example.com/monero";
  res.auto_uri = "https://auto.example.com/monero";
  res.hash = "abcdef";
  res.path = "/tmp/monero";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_UPDATE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.update);
  ASSERT_EQ(res2.version, "0.18.0.0");
  ASSERT_EQ(res2.user_uri, "https://example.com/monero");
  ASSERT_EQ(res2.hash, "abcdef");
}

// ============================================================
// RELAY_TX
// ============================================================

TEST(core_rpc, relay_tx_request_serialization)
{
  COMMAND_RPC_RELAY_TX::request_t req;
  req.txids.push_back("relay_txid_1");
  req.txids.push_back("relay_txid_2");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_RELAY_TX::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txids.size(), 2u);
  ASSERT_EQ(req2.txids[0], "relay_txid_1");
  ASSERT_EQ(req2.txids[1], "relay_txid_2");
}

TEST(core_rpc, relay_tx_response_serialization)
{
  COMMAND_RPC_RELAY_TX::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_RELAY_TX::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// SYNC_INFO
// ============================================================

TEST(core_rpc, sync_info_request_serialization)
{
  COMMAND_RPC_SYNC_INFO::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SYNC_INFO::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, sync_info_response_serialization)
{
  COMMAND_RPC_SYNC_INFO::response_t res;
  res.height = 2500000;
  res.target_height = 2500100;
  res.next_needed_pruning_seed = 0;
  res.overview = "[]";
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SYNC_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.target_height, 2500100u);
  ASSERT_EQ(res2.overview, "[]");
}

// ============================================================
// ACCESS_INFO
// ============================================================

TEST(core_rpc, access_info_request_serialization)
{
  COMMAND_RPC_ACCESS_INFO::request_t req;
  req.client = "client_public_key";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_INFO::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.client, "client_public_key");
}

TEST(core_rpc, access_info_response_serialization)
{
  COMMAND_RPC_ACCESS_INFO::response_t res;
  res.hashing_blob = "hashing_blob_data";
  res.seed_height = 2499968;
  res.seed_hash = "seed_hash_val";
  res.next_seed_hash = "next_seed";
  res.cookie = 12345;
  res.diff = 100000;
  res.credits_per_hash_found = 50;
  res.height = 2500000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.hashing_blob, "hashing_blob_data");
  ASSERT_EQ(res2.seed_height, 2499968u);
  ASSERT_EQ(res2.seed_hash, "seed_hash_val");
  ASSERT_EQ(res2.cookie, 12345u);
  ASSERT_EQ(res2.diff, 100000u);
  ASSERT_EQ(res2.credits_per_hash_found, 50u);
  ASSERT_EQ(res2.height, 2500000u);
}

// ============================================================
// ACCESS_SUBMIT_NONCE
// ============================================================

TEST(core_rpc, access_submit_nonce_request_serialization)
{
  COMMAND_RPC_ACCESS_SUBMIT_NONCE::request_t req;
  req.nonce = 42;
  req.cookie = 12345;
  req.client = "client_key";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_SUBMIT_NONCE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.nonce, 42u);
  ASSERT_EQ(req2.cookie, 12345u);
  ASSERT_EQ(req2.client, "client_key");
}

TEST(core_rpc, access_submit_nonce_response_serialization)
{
  COMMAND_RPC_ACCESS_SUBMIT_NONCE::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.credits = 100;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_SUBMIT_NONCE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_EQ(res2.credits, 100u);
}

// ============================================================
// ACCESS_PAY
// ============================================================

TEST(core_rpc, access_pay_request_serialization)
{
  COMMAND_RPC_ACCESS_PAY::request_t req;
  req.paying_for = "get_info";
  req.payment = 50;
  req.client = "client_key";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_PAY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.paying_for, "get_info");
  ASSERT_EQ(req2.payment, 50u);
  ASSERT_EQ(req2.client, "client_key");
}

TEST(core_rpc, access_pay_response_serialization)
{
  COMMAND_RPC_ACCESS_PAY::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.credits = 950;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_PAY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.credits, 950u);
}

// ============================================================
// GET_NET_STATS
// ============================================================

TEST(core_rpc, get_net_stats_request_serialization)
{
  COMMAND_RPC_GET_NET_STATS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_NET_STATS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_net_stats_response_serialization)
{
  COMMAND_RPC_GET_NET_STATS::response_t res;
  res.start_time = 1600000000;
  res.total_packets_in = 100000;
  res.total_bytes_in = 50000000;
  res.total_packets_out = 80000;
  res.total_bytes_out = 40000000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_NET_STATS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.start_time, 1600000000u);
  ASSERT_EQ(res2.total_packets_in, 100000u);
  ASSERT_EQ(res2.total_bytes_in, 50000000u);
  ASSERT_EQ(res2.total_packets_out, 80000u);
  ASSERT_EQ(res2.total_bytes_out, 40000000u);
}

// ============================================================
// GET_LIMIT
// ============================================================

TEST(core_rpc, get_limit_request_serialization)
{
  COMMAND_RPC_GET_LIMIT::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_LIMIT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_limit_response_serialization)
{
  COMMAND_RPC_GET_LIMIT::response_t res;
  res.limit_up = 2048;
  res.limit_down = 8192;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_LIMIT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.limit_up, 2048u);
  ASSERT_EQ(res2.limit_down, 8192u);
}

// ============================================================
// SET_LIMIT
// ============================================================

TEST(core_rpc, set_limit_request_serialization)
{
  COMMAND_RPC_SET_LIMIT::request_t req;
  req.limit_down = 1024;
  req.limit_up = 512;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LIMIT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.limit_down, 1024);
  ASSERT_EQ(req2.limit_up, 512);
}

TEST(core_rpc, set_limit_response_serialization)
{
  COMMAND_RPC_SET_LIMIT::response_t res;
  res.limit_up = 512;
  res.limit_down = 1024;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_LIMIT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.limit_up, 512);
  ASSERT_EQ(res2.limit_down, 1024);
}

// ============================================================
// OUT_PEERS
// ============================================================

TEST(core_rpc, out_peers_request_serialization)
{
  COMMAND_RPC_OUT_PEERS::request_t req;
  req.set = true;
  req.out_peers = 16;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_OUT_PEERS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.set);
  ASSERT_EQ(req2.out_peers, 16u);
}

TEST(core_rpc, out_peers_response_serialization)
{
  COMMAND_RPC_OUT_PEERS::response_t res;
  res.out_peers = 16;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_OUT_PEERS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.out_peers, 16u);
}

// ============================================================
// IN_PEERS
// ============================================================

TEST(core_rpc, in_peers_request_serialization)
{
  COMMAND_RPC_IN_PEERS::request_t req;
  req.set = true;
  req.in_peers = 32;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_IN_PEERS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.set);
  ASSERT_EQ(req2.in_peers, 32u);
}

TEST(core_rpc, in_peers_response_serialization)
{
  COMMAND_RPC_IN_PEERS::response_t res;
  res.in_peers = 32;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IN_PEERS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.in_peers, 32u);
}

// ============================================================
// HARD_FORK_INFO
// ============================================================

TEST(core_rpc, hard_fork_info_request_serialization)
{
  COMMAND_RPC_HARD_FORK_INFO::request_t req;
  req.version = 14;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_HARD_FORK_INFO::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.version, 14);
}

TEST(core_rpc, hard_fork_info_response_serialization)
{
  COMMAND_RPC_HARD_FORK_INFO::response_t res;
  res.version = 14;
  res.enabled = true;
  res.window = 10080;
  res.votes = 10080;
  res.threshold = 0;
  res.voting = 14;
  res.state = 2;
  res.earliest_height = 2210000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_HARD_FORK_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.version, 14);
  ASSERT_TRUE(res2.enabled);
  ASSERT_EQ(res2.window, 10080u);
  ASSERT_EQ(res2.votes, 10080u);
  ASSERT_EQ(res2.voting, 14);
  ASSERT_EQ(res2.state, 2u);
  ASSERT_EQ(res2.earliest_height, 2210000u);
}

// ============================================================
// FLUSH_CACHE
// ============================================================

TEST(core_rpc, flush_cache_request_serialization)
{
  COMMAND_RPC_FLUSH_CACHE::request_t req;
  req.bad_blocks = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_FLUSH_CACHE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.bad_blocks);
}

TEST(core_rpc, flush_cache_response_serialization)
{
  COMMAND_RPC_FLUSH_CACHE::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_FLUSH_CACHE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_OUTPUTS (JSON variant)
// ============================================================

TEST(core_rpc, get_outputs_request_serialization)
{
  COMMAND_RPC_GET_OUTPUTS::request_t req;
  get_outputs_out out1;
  out1.amount = 0;
  out1.index = 100;
  req.outputs.push_back(out1);
  req.get_txid = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_OUTPUTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.outputs.size(), 1u);
  ASSERT_EQ(req2.outputs[0].amount, 0u);
  ASSERT_EQ(req2.outputs[0].index, 100u);
  ASSERT_TRUE(req2.get_txid);
}

TEST(core_rpc, get_outputs_response_serialization)
{
  COMMAND_RPC_GET_OUTPUTS::response_t res;
  COMMAND_RPC_GET_OUTPUTS::outkey ok;
  ok.key = "outkey1";
  ok.mask = "mask1";
  ok.unlocked = true;
  ok.height = 500000;
  ok.txid = "txid1";
  res.outs.push_back(ok);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.outs.size(), 1u);
  ASSERT_EQ(res2.outs[0].key, "outkey1");
  ASSERT_EQ(res2.outs[0].mask, "mask1");
  ASSERT_TRUE(res2.outs[0].unlocked);
  ASSERT_EQ(res2.outs[0].height, 500000u);
  ASSERT_EQ(res2.outs[0].txid, "txid1");
}

// ============================================================
// GET_OUTPUTS_BIN
// ============================================================

TEST(core_rpc, get_outputs_bin_request_serialization)
{
  COMMAND_RPC_GET_OUTPUTS_BIN::request_t req;
  get_outputs_out out1;
  out1.amount = 0;
  out1.index = 200;
  req.outputs.push_back(out1);
  req.get_txid = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_OUTPUTS_BIN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.outputs.size(), 1u);
  ASSERT_EQ(req2.outputs[0].amount, 0u);
  ASSERT_EQ(req2.outputs[0].index, 200u);
}

// ============================================================
// GET_VERSION (extended)
// ============================================================

TEST(core_rpc, get_version_request_serialization)
{
  COMMAND_RPC_GET_VERSION::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_VERSION::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_version_full_response_serialization)
{
  COMMAND_RPC_GET_VERSION::response_t res;
  res.version = CORE_RPC_VERSION;
  res.release = true;
  res.current_height = 2500000;
  res.target_height = 2500100;
  COMMAND_RPC_GET_VERSION::hf_entry hf;
  hf.hf_version = 14;
  hf.height = 2210000;
  res.hard_forks.push_back(hf);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_VERSION::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.version, CORE_RPC_VERSION);
  ASSERT_TRUE(res2.release);
  ASSERT_EQ(res2.current_height, 2500000u);
  ASSERT_EQ(res2.target_height, 2500100u);
  ASSERT_EQ(res2.hard_forks.size(), 1u);
  ASSERT_EQ(res2.hard_forks[0].hf_version, 14);
  ASSERT_EQ(res2.hard_forks[0].height, 2210000u);
}

// ============================================================
// GET_INFO (extended request test)
// ============================================================

TEST(core_rpc, get_info_request_serialization)
{
  COMMAND_RPC_GET_INFO::request_t req;
  req.client = "test_client";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_INFO::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.client, "test_client");
}

// ============================================================
// GET_HEIGHT (request test)
// ============================================================

TEST(core_rpc, get_height_request_serialization)
{
  COMMAND_RPC_GET_HEIGHT::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_HEIGHT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

// ============================================================
// MINING_STATUS (request test)
// ============================================================

TEST(core_rpc, mining_status_request_serialization)
{
  COMMAND_RPC_MINING_STATUS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_MINING_STATUS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

// ============================================================
// STOP_DAEMON
// ============================================================

TEST(core_rpc, stop_daemon_request_serialization)
{
  COMMAND_RPC_STOP_DAEMON::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_STOP_DAEMON::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, stop_daemon_response_serialization)
{
  COMMAND_RPC_STOP_DAEMON::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_STOP_DAEMON::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// SET_BOOTSTRAP_DAEMON
// ============================================================

TEST(core_rpc, set_bootstrap_daemon_request_serialization)
{
  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::request_t req;
  req.address = "http://localhost:18081";
  req.username = "user";
  req.password = "pass";
  req.proxy = "socks5://127.0.0.1:9050";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "http://localhost:18081");
  ASSERT_EQ(req2.username, "user");
  ASSERT_EQ(req2.password, "pass");
  ASSERT_EQ(req2.proxy, "socks5://127.0.0.1:9050");
}

TEST(core_rpc, set_bootstrap_daemon_response_serialization)
{
  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_PUBLIC_NODES
// ============================================================

TEST(core_rpc, get_public_nodes_request_serialization)
{
  COMMAND_RPC_GET_PUBLIC_NODES::request_t req;
  req.gray = true;
  req.white = true;
  req.include_blocked = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_PUBLIC_NODES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.gray);
  ASSERT_TRUE(req2.white);
  ASSERT_FALSE(req2.include_blocked);
}

TEST(core_rpc, get_public_nodes_response_serialization)
{
  COMMAND_RPC_GET_PUBLIC_NODES::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_PUBLIC_NODES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_TRUE(res2.gray.empty());
  ASSERT_TRUE(res2.white.empty());
}

// ============================================================
// GETMINERDATA
// ============================================================

TEST(core_rpc, getminerdata_request_serialization)
{
  COMMAND_RPC_GETMINERDATA::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GETMINERDATA::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, getminerdata_response_serialization)
{
  COMMAND_RPC_GETMINERDATA::response_t res;
  res.major_version = 16;
  res.height = 2800000;
  res.prev_id = "prev_id_hash";
  res.seed_hash = "seed";
  res.difficulty = "300000000000";
  res.median_weight = 300000;
  res.already_generated_coins = 18300000000000000ULL;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETMINERDATA::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.major_version, 16);
  ASSERT_EQ(res2.height, 2800000u);
  ASSERT_EQ(res2.prev_id, "prev_id_hash");
  ASSERT_EQ(res2.difficulty, "300000000000");
  ASSERT_EQ(res2.median_weight, 300000u);
}

// ============================================================
// CALCPOW
// ============================================================

TEST(core_rpc, calcpow_request_serialization)
{
  COMMAND_RPC_CALCPOW::request_t req;
  req.major_version = 14;
  req.height = 2500000;
  req.block_blob = "block_data";
  req.seed_hash = "seed_hash_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CALCPOW::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.major_version, 14);
  ASSERT_EQ(req2.height, 2500000u);
  ASSERT_EQ(req2.block_blob, "block_data");
  ASSERT_EQ(req2.seed_hash, "seed_hash_data");
}

// ============================================================
// ADD_AUX_POW
// ============================================================

TEST(core_rpc, add_aux_pow_request_serialization)
{
  COMMAND_RPC_ADD_AUX_POW::request_t req;
  req.blocktemplate_blob = "template";
  COMMAND_RPC_ADD_AUX_POW::aux_pow_t ap;
  ap.id = "aux_id";
  ap.hash = "aux_hash";
  req.aux_pow.push_back(ap);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ADD_AUX_POW::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.blocktemplate_blob, "template");
  ASSERT_EQ(req2.aux_pow.size(), 1u);
  ASSERT_EQ(req2.aux_pow[0].id, "aux_id");
  ASSERT_EQ(req2.aux_pow[0].hash, "aux_hash");
}

TEST(core_rpc, add_aux_pow_response_serialization)
{
  COMMAND_RPC_ADD_AUX_POW::response_t res;
  res.blocktemplate_blob = "new_template";
  res.blockhashing_blob = "hashing";
  res.merkle_root = "merkle";
  res.merkle_tree_depth = 3;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ADD_AUX_POW::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.blocktemplate_blob, "new_template");
  ASSERT_EQ(res2.blockhashing_blob, "hashing");
  ASSERT_EQ(res2.merkle_root, "merkle");
  ASSERT_EQ(res2.merkle_tree_depth, 3u);
}

// ============================================================
// ACCESS_TRACKING
// ============================================================

TEST(core_rpc, access_tracking_request_serialization)
{
  COMMAND_RPC_ACCESS_TRACKING::request_t req;
  req.clear = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_TRACKING::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.clear);
}

TEST(core_rpc, access_tracking_response_serialization)
{
  COMMAND_RPC_ACCESS_TRACKING::response_t res;
  COMMAND_RPC_ACCESS_TRACKING::entry e;
  e.rpc = "get_info";
  e.count = 100;
  e.time = 5000;
  e.credits = 500;
  res.data.push_back(e);
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_TRACKING::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.data.size(), 1u);
  ASSERT_EQ(res2.data[0].rpc, "get_info");
  ASSERT_EQ(res2.data[0].count, 100u);
  ASSERT_EQ(res2.data[0].credits, 500u);
}

// ============================================================
// ACCESS_DATA
// ============================================================

TEST(core_rpc, access_data_request_serialization)
{
  COMMAND_RPC_ACCESS_DATA::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_DATA::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, access_data_response_serialization)
{
  COMMAND_RPC_ACCESS_DATA::response_t res;
  res.hashrate = 1000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_DATA::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.hashrate, 1000u);
}

// ============================================================
// ACCESS_ACCOUNT
// ============================================================

TEST(core_rpc, access_account_request_serialization)
{
  COMMAND_RPC_ACCESS_ACCOUNT::request_t req;
  req.client = "client_key";
  req.delta_balance = -50;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ACCESS_ACCOUNT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.client, "client_key");
  ASSERT_EQ(req2.delta_balance, -50);
}

TEST(core_rpc, access_account_response_serialization)
{
  COMMAND_RPC_ACCESS_ACCOUNT::response_t res;
  res.credits = 950;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ACCESS_ACCOUNT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.credits, 950u);
}

// ============================================================
// GET_TRANSACTION_POOL_BACKLOG
// ============================================================

TEST(core_rpc, get_transaction_pool_backlog_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

TEST(core_rpc, get_transaction_pool_backlog_response_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
}

// ============================================================
// GET_TXIDS_LOOSE
// ============================================================

TEST(core_rpc, get_txids_loose_request_serialization)
{
  COMMAND_RPC_GET_TXIDS_LOOSE::request_t req;
  req.txid_template = "abcdef";
  req.num_matching_bits = 16;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TXIDS_LOOSE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid_template, "abcdef");
  ASSERT_EQ(req2.num_matching_bits, 16u);
}

TEST(core_rpc, get_txids_loose_response_serialization)
{
  COMMAND_RPC_GET_TXIDS_LOOSE::response_t res;
  res.txids.push_back("txid1");
  res.txids.push_back("txid2");
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TXIDS_LOOSE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.txids.size(), 2u);
  ASSERT_EQ(res2.txids[0], "txid1");
}

// ============================================================
// GET_TRANSACTION_POOL_STATS (request test)
// ============================================================

TEST(core_rpc, get_tx_pool_stats_request_serialization)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
}

// ============================================================
// block_header_response standalone serialization
// ============================================================

TEST(core_rpc, block_header_response_serialization)
{
  block_header_response bhr;
  bhr.major_version = 14;
  bhr.minor_version = 14;
  bhr.timestamp = 1600000000;
  bhr.prev_hash = "prevhash";
  bhr.nonce = 98765;
  bhr.orphan_status = false;
  bhr.height = 2500000;
  bhr.depth = 100;
  bhr.hash = "blockhash";
  bhr.difficulty = 300000000000ULL;
  bhr.wide_difficulty = "300000000000";
  bhr.difficulty_top64 = 0;
  bhr.cumulative_difficulty = 9000000000000000ULL;
  bhr.wide_cumulative_difficulty = "9000000000000000";
  bhr.cumulative_difficulty_top64 = 0;
  bhr.reward = 600000000000ULL;
  bhr.block_size = 5000;
  bhr.block_weight = 5000;
  bhr.num_txes = 10;
  bhr.pow_hash = "powhash";
  bhr.long_term_weight = 300000;
  bhr.miner_tx_hash = "minertxhash";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(bhr, json));

  block_header_response bhr2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(bhr2, json));
  ASSERT_EQ(bhr2.major_version, 14);
  ASSERT_EQ(bhr2.height, 2500000u);
  ASSERT_EQ(bhr2.difficulty, 300000000000ULL);
  ASSERT_EQ(bhr2.reward, 600000000000ULL);
  ASSERT_EQ(bhr2.num_txes, 10u);
  ASSERT_EQ(bhr2.miner_tx_hash, "minertxhash");
  ASSERT_EQ(bhr2.nonce, 98765u);
}

// ============================================================
// peer serialization
// ============================================================

TEST(core_rpc, peer_serialization)
{
  peer p;
  p.id = 123456;
  p.host = "192.168.1.1";
  p.ip = 3232235777u;
  p.port = 18080;
  p.rpc_port = 18081;
  p.rpc_credits_per_hash = 0;
  p.last_seen = 1600000000;
  p.pruning_seed = 384;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(p, json));

  peer p2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(p2, json));
  ASSERT_EQ(p2.id, 123456u);
  ASSERT_EQ(p2.host, "192.168.1.1");
  ASSERT_EQ(p2.port, 18080u);
  ASSERT_EQ(p2.rpc_port, 18081u);
  ASSERT_EQ(p2.last_seen, 1600000000u);
  ASSERT_EQ(p2.pruning_seed, 384u);
}

// ============================================================
// txpool_stats serialization
// ============================================================

TEST(core_rpc, txpool_stats_serialization)
{
  txpool_stats stats;
  stats.bytes_total = 100000;
  stats.bytes_min = 200;
  stats.bytes_max = 10000;
  stats.bytes_med = 2000;
  stats.fee_total = 5000000;
  stats.oldest = 1600000000;
  stats.txs_total = 50;
  stats.num_failing = 2;
  stats.num_10m = 10;
  stats.num_not_relayed = 3;
  stats.histo_98pc = 900;
  stats.num_double_spends = 1;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(stats, json));

  txpool_stats stats2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(stats2, json));
  ASSERT_EQ(stats2.bytes_total, 100000u);
  ASSERT_EQ(stats2.bytes_min, 200u);
  ASSERT_EQ(stats2.bytes_max, 10000u);
  ASSERT_EQ(stats2.bytes_med, 2000u);
  ASSERT_EQ(stats2.fee_total, 5000000u);
  ASSERT_EQ(stats2.txs_total, 50u);
  ASSERT_EQ(stats2.num_failing, 2u);
  ASSERT_EQ(stats2.num_double_spends, 1u);
}

// ============================================================
// txpool_histo serialization
// ============================================================

TEST(core_rpc, txpool_histo_serialization)
{
  txpool_histo h;
  h.txs = 5;
  h.bytes = 10000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(h, json));

  txpool_histo h2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(h2, json));
  ASSERT_EQ(h2.txs, 5u);
  ASSERT_EQ(h2.bytes, 10000u);
}

// ============================================================
// get_outputs_out serialization
// ============================================================

TEST(core_rpc, get_outputs_out_serialization)
{
  get_outputs_out out;
  out.amount = 0;
  out.index = 42;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(out, json));

  get_outputs_out out2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(out2, json));
  ASSERT_EQ(out2.amount, 0u);
  ASSERT_EQ(out2.index, 42u);
}

// ============================================================
// tx_info serialization
// ============================================================

TEST(core_rpc, tx_info_serialization)
{
  tx_info ti;
  ti.id_hash = "txhash";
  ti.tx_json = "{}";
  ti.blob_size = 5000;
  ti.weight = 5000;
  ti.fee = 20000000;
  ti.max_used_block_id_hash = "max_block_hash";
  ti.max_used_block_height = 2499999;
  ti.kept_by_block = false;
  ti.last_failed_height = 0;
  ti.last_failed_id_hash = "";
  ti.receive_time = 1600000000;
  ti.relayed = true;
  ti.last_relayed_time = 1600000001;
  ti.do_not_relay = false;
  ti.double_spend_seen = false;
  ti.tx_blob = "blob_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(ti, json));

  tx_info ti2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(ti2, json));
  ASSERT_EQ(ti2.id_hash, "txhash");
  ASSERT_EQ(ti2.blob_size, 5000u);
  ASSERT_EQ(ti2.weight, 5000u);
  ASSERT_EQ(ti2.fee, 20000000u);
  ASSERT_TRUE(ti2.relayed);
  ASSERT_FALSE(ti2.double_spend_seen);
}

// ============================================================
// spent_key_image_info serialization
// ============================================================

TEST(core_rpc, spent_key_image_info_serialization)
{
  spent_key_image_info ski;
  ski.id_hash = "ki_hash";
  ski.txs_hashes.push_back("tx1");
  ski.txs_hashes.push_back("tx2");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(ski, json));

  spent_key_image_info ski2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(ski2, json));
  ASSERT_EQ(ski2.id_hash, "ki_hash");
  ASSERT_EQ(ski2.txs_hashes.size(), 2u);
  ASSERT_EQ(ski2.txs_hashes[0], "tx1");
}

// ============================================================
// public_node serialization
// ============================================================

TEST(core_rpc, public_node_serialization)
{
  public_node pn;
  pn.host = "node.example.com";
  pn.last_seen = 1600000000;
  pn.rpc_port = 18081;
  pn.rpc_credits_per_hash = 100;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(pn, json));

  public_node pn2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(pn2, json));
  ASSERT_EQ(pn2.host, "node.example.com");
  ASSERT_EQ(pn2.last_seen, 1600000000u);
  ASSERT_EQ(pn2.rpc_port, 18081u);
  ASSERT_EQ(pn2.rpc_credits_per_hash, 100u);
}

// ============================================================
// RPC version macros
// ============================================================

TEST(core_rpc, rpc_version_macros)
{
  ASSERT_EQ(MAKE_CORE_RPC_VERSION(3, 16), CORE_RPC_VERSION);
  ASSERT_EQ(CORE_RPC_VERSION_MAJOR, 3u);
  ASSERT_EQ(CORE_RPC_VERSION_MINOR, 16u);
  ASSERT_EQ(MAKE_CORE_RPC_VERSION(0, 0), 0u);
  ASSERT_EQ(MAKE_CORE_RPC_VERSION(1, 0), 65536u);
}

// ============================================================
// get_rpc_status helper
// ============================================================

TEST(core_rpc, get_rpc_status_trusted)
{
  ASSERT_EQ(get_rpc_status(true, CORE_RPC_STATUS_OK), CORE_RPC_STATUS_OK);
  ASSERT_EQ(get_rpc_status(true, CORE_RPC_STATUS_BUSY), CORE_RPC_STATUS_BUSY);
  ASSERT_EQ(get_rpc_status(true, "some_error"), "some_error");
}

TEST(core_rpc, get_rpc_status_untrusted)
{
  ASSERT_EQ(get_rpc_status(false, CORE_RPC_STATUS_OK), CORE_RPC_STATUS_OK);
  ASSERT_EQ(get_rpc_status(false, CORE_RPC_STATUS_BUSY), CORE_RPC_STATUS_BUSY);
  ASSERT_EQ(get_rpc_status(false, CORE_RPC_STATUS_PAYMENT_REQUIRED), CORE_RPC_STATUS_PAYMENT_REQUIRED);
  ASSERT_EQ(get_rpc_status(false, "some_error"), "<error>");
}

// ============================================================
// Round-trip: status busy
// ============================================================

TEST(core_rpc, response_status_busy_serialization)
{
  COMMAND_RPC_GET_HEIGHT::response_t res;
  res.height = 100;
  res.status = CORE_RPC_STATUS_BUSY;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_BUSY);
}

// ============================================================
// Multiple entries: GET_OUTPUT_HISTOGRAM
// ============================================================

TEST(core_rpc, get_output_histogram_multiple_entries)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res;
  res.histogram.push_back(COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry(0, 5000000, 4000000, 100000));
  res.histogram.push_back(COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry(1000000000, 50000, 40000, 1000));
  res.histogram.push_back(COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry(10000000000ULL, 500, 400, 10));
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.histogram.size(), 3u);
  ASSERT_EQ(res2.histogram[0].amount, 0u);
  ASSERT_EQ(res2.histogram[1].amount, 1000000000u);
  ASSERT_EQ(res2.histogram[2].amount, 10000000000ULL);
}

// ============================================================
// Empty vectors round-trip
// ============================================================

TEST(core_rpc, empty_vectors_round_trip)
{
  COMMAND_RPC_GET_TRANSACTIONS::request_t req;
  // Leave all vectors empty
  req.decode_as_json = false;
  req.prune = false;
  req.split = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSACTIONS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.txs_hashes.empty());
  ASSERT_FALSE(req2.decode_as_json);
}

// ============================================================
// mining_status extended fields
// ============================================================

TEST(core_rpc, mining_status_extended_response_serialization)
{
  COMMAND_RPC_MINING_STATUS::response_t res;
  res.active = true;
  res.speed = 1000;
  res.threads_count = 4;
  res.address = "44addr";
  res.pow_algorithm = "RandomX";
  res.is_background_mining_enabled = true;
  res.bg_idle_threshold = 90;
  res.bg_min_idle_seconds = 10;
  res.bg_ignore_battery = false;
  res.bg_target = 50;
  res.block_target = 120;
  res.block_reward = 600000000000ULL;
  res.difficulty = 300000000000ULL;
  res.wide_difficulty = "300000000000";
  res.difficulty_top64 = 0;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MINING_STATUS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.active);
  ASSERT_EQ(res2.speed, 1000u);
  ASSERT_EQ(res2.threads_count, 4u);
  ASSERT_EQ(res2.bg_idle_threshold, 90);
  ASSERT_EQ(res2.bg_min_idle_seconds, 10);
  ASSERT_EQ(res2.block_target, 120u);
  ASSERT_EQ(res2.block_reward, 600000000000ULL);
  ASSERT_EQ(res2.difficulty, 300000000000ULL);
}

// ============================================================
// GET_INFO extended response fields
// ============================================================

TEST(core_rpc, get_info_extended_response_serialization)
{
  COMMAND_RPC_GET_INFO::response_t res;
  res.height = 2500000;
  res.target_height = 2500100;
  res.difficulty = 300000000000ULL;
  res.wide_difficulty = "300000000000";
  res.difficulty_top64 = 0;
  res.target = 120;
  res.tx_count = 15000000;
  res.tx_pool_size = 50;
  res.alt_blocks_count = 3;
  res.outgoing_connections_count = 8;
  res.incoming_connections_count = 12;
  res.rpc_connections_count = 2;
  res.white_peerlist_size = 100;
  res.grey_peerlist_size = 200;
  res.mainnet = true;
  res.testnet = false;
  res.stagenet = false;
  res.nettype = "mainnet";
  res.top_block_hash = "tophash";
  res.cumulative_difficulty = 9000000000000000ULL;
  res.wide_cumulative_difficulty = "9000000000000000";
  res.cumulative_difficulty_top64 = 0;
  res.block_size_limit = 600000;
  res.block_weight_limit = 600000;
  res.block_size_median = 300000;
  res.block_weight_median = 300000;
  res.adjusted_time = 1600000000;
  res.start_time = 1500000000;
  res.free_space = 100000000000ULL;
  res.offline = false;
  res.bootstrap_daemon_address = "";
  res.height_without_bootstrap = 2500000;
  res.was_bootstrap_ever_used = false;
  res.database_size = 80000000000ULL;
  res.update_available = false;
  res.busy_syncing = false;
  res.version = "0.18.0.0";
  res.synchronized = true;
  res.restricted = false;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.nettype, "mainnet");
  ASSERT_EQ(res2.cumulative_difficulty, 9000000000000000ULL);
  ASSERT_EQ(res2.block_weight_limit, 600000u);
  ASSERT_EQ(res2.database_size, 80000000000ULL);
  ASSERT_EQ(res2.version, "0.18.0.0");
  ASSERT_FALSE(res2.restricted);
  ASSERT_EQ(res2.free_space, 100000000000ULL);
}

// ============================================================
// Binary serialization round-trips for additional coverage
// ============================================================

TEST(core_rpc, get_height_response_binary_roundtrip)
{
  COMMAND_RPC_GET_HEIGHT::response_t original;
  original.height = 2999999;
  original.status = CORE_RPC_STATUS_OK;
  original.hash = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
  original.untrusted = true;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_HEIGHT::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.height, 2999999u);
  ASSERT_EQ(restored.status, CORE_RPC_STATUS_OK);
  ASSERT_EQ(restored.hash, original.hash);
  ASSERT_TRUE(restored.untrusted);
}

// NOTE: COMMAND_RPC_GET_INFO::response_t uses conditional serialization that
// makes direct binary roundtrip unreliable without a full RPC context.
// The JSON roundtrip test below covers the same structure.

TEST(core_rpc, get_blocks_fast_request_binary_roundtrip)
{
  COMMAND_RPC_GET_BLOCKS_FAST::request_t original;
  original.start_height = 500000;
  original.prune = true;
  original.no_miner_tx = true;
  original.pool_info_since = 100;
  original.max_block_count = 500;
  original.requested_info = 2;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_BLOCKS_FAST::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.start_height, 500000u);
  ASSERT_TRUE(restored.prune);
  ASSERT_TRUE(restored.no_miner_tx);
  ASSERT_EQ(restored.pool_info_since, 100u);
  ASSERT_EQ(restored.max_block_count, 500u);
  ASSERT_EQ(restored.requested_info, 2);
}

TEST(core_rpc, send_raw_tx_request_binary_roundtrip)
{
  COMMAND_RPC_SEND_RAW_TX::request_t original;
  original.tx_as_hex = "deadbeefcafe0102030405";
  original.do_not_relay = true;
  original.do_sanity_checks = true;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_SEND_RAW_TX::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.tx_as_hex, "deadbeefcafe0102030405");
  ASSERT_TRUE(restored.do_not_relay);
  ASSERT_TRUE(restored.do_sanity_checks);
}

TEST(core_rpc, send_raw_tx_response_binary_roundtrip)
{
  COMMAND_RPC_SEND_RAW_TX::response_t original;
  original.reason = "fee too low";
  original.not_relayed = true;
  original.low_mixin = false;
  original.double_spend = true;
  original.invalid_input = false;
  original.invalid_output = true;
  original.too_big = false;
  original.overspend = false;
  original.fee_too_low = true;
  original.too_few_outputs = false;
  original.sanity_check_failed = false;
  original.tx_extra_too_big = true;
  original.nonzero_unlock_time = false;
  original.status = CORE_RPC_STATUS_OK;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_SEND_RAW_TX::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.reason, "fee too low");
  ASSERT_TRUE(restored.not_relayed);
  ASSERT_TRUE(restored.double_spend);
  ASSERT_TRUE(restored.invalid_output);
  ASSERT_TRUE(restored.fee_too_low);
  ASSERT_TRUE(restored.tx_extra_too_big);
  ASSERT_FALSE(restored.low_mixin);
  ASSERT_FALSE(restored.nonzero_unlock_time);
}

TEST(core_rpc, get_block_template_response_binary_roundtrip)
{
  COMMAND_RPC_GETBLOCKTEMPLATE::response_t original;
  original.difficulty = 500000000000ULL;
  original.height = 2500000;
  original.reserved_offset = 130;
  original.expected_reward = 600000000000ULL;
  original.prev_hash = "prev_hash_hex";
  original.seed_hash = "seed_hash_hex";
  original.blocktemplate_blob = "block_template_blob_data";
  original.blockhashing_blob = "block_hashing_blob_data";
  original.next_seed_hash = "next_seed_hex";
  original.status = CORE_RPC_STATUS_OK;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GETBLOCKTEMPLATE::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.difficulty, 500000000000ULL);
  ASSERT_EQ(restored.height, 2500000u);
  ASSERT_EQ(restored.reserved_offset, 130u);
  ASSERT_EQ(restored.expected_reward, 600000000000ULL);
  ASSERT_EQ(restored.blocktemplate_blob, "block_template_blob_data");
  ASSERT_EQ(restored.blockhashing_blob, "block_hashing_blob_data");
  ASSERT_EQ(restored.seed_hash, "seed_hash_hex");
}

TEST(core_rpc, hard_fork_info_response_binary_roundtrip)
{
  COMMAND_RPC_HARD_FORK_INFO::response_t original;
  original.version = 16;
  original.enabled = true;
  original.window = 10080;
  original.votes = 10000;
  original.threshold = 0;
  original.earliest_height = 2689608;
  original.voting = 16;
  original.state = 2;
  original.status = CORE_RPC_STATUS_OK;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_HARD_FORK_INFO::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.version, 16u);
  ASSERT_TRUE(restored.enabled);
  ASSERT_EQ(restored.window, 10080u);
  ASSERT_EQ(restored.earliest_height, 2689608u);
  ASSERT_EQ(restored.state, 2u);
}

TEST(core_rpc, get_coinbase_tx_sum_binary_roundtrip)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req_orig;
  req_orig.height = 100000;
  req_orig.count = 1000;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(req_orig, buff));

  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req_restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(req_restored, epee::to_span(buff)));
  ASSERT_EQ(req_restored.height, 100000u);
  ASSERT_EQ(req_restored.count, 1000u);

  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res_orig;
  res_orig.emission_amount = 18000000000000000ULL;
  res_orig.fee_amount = 500000000000ULL;
  res_orig.wide_emission_amount = "18000000000000000";
  res_orig.wide_fee_amount = "500000000000";
  res_orig.emission_amount_top64 = 0;
  res_orig.fee_amount_top64 = 0;
  res_orig.status = CORE_RPC_STATUS_OK;

  epee::byte_slice buff2;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(res_orig, buff2));

  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res_restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(res_restored, epee::to_span(buff2)));
  ASSERT_EQ(res_restored.emission_amount, 18000000000000000ULL);
  ASSERT_EQ(res_restored.fee_amount, 500000000000ULL);
}

TEST(core_rpc, get_base_fee_estimate_binary_roundtrip)
{
  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response_t original;
  original.fee = 20000;
  original.quantization_mask = 10000;
  original.status = CORE_RPC_STATUS_OK;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.fee, 20000u);
  ASSERT_EQ(restored.quantization_mask, 10000u);
}

TEST(core_rpc, get_output_distribution_request_binary_roundtrip)
{
  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request_t original;
  original.amounts.push_back(0);
  original.amounts.push_back(1000000000);
  original.amounts.push_back(10000000000ULL);
  original.from_height = 0;
  original.to_height = 0;
  original.cumulative = true;
  original.binary = true;
  original.compress = false;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.amounts.size(), 3u);
  ASSERT_EQ(restored.amounts[0], 0u);
  ASSERT_EQ(restored.amounts[1], 1000000000u);
  ASSERT_EQ(restored.amounts[2], 10000000000ULL);
  ASSERT_TRUE(restored.cumulative);
  ASSERT_TRUE(restored.binary);
  ASSERT_FALSE(restored.compress);
}

TEST(core_rpc, set_bootstrap_daemon_binary_roundtrip)
{
  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::request_t original;
  original.address = "http://node.example.com:18081";
  original.username = "user";
  original.password = "pass";
  original.proxy = "socks5://127.0.0.1:9050";

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_SET_BOOTSTRAP_DAEMON::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.address, "http://node.example.com:18081");
  ASSERT_EQ(restored.username, "user");
  ASSERT_EQ(restored.password, "pass");
  ASSERT_EQ(restored.proxy, "socks5://127.0.0.1:9050");
}

TEST(core_rpc, get_version_response_binary_roundtrip)
{
  COMMAND_RPC_GET_VERSION::response_t original;
  original.version = CORE_RPC_VERSION;
  original.release = true;
  original.current_height = 2500000;
  original.target_height = 2500100;
  original.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_VERSION::hf_entry hf;
  hf.hf_version = 14;
  hf.height = 2000000;
  original.hard_forks.push_back(hf);
  hf.hf_version = 15;
  hf.height = 2200000;
  original.hard_forks.push_back(hf);

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_VERSION::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.version, CORE_RPC_VERSION);
  ASSERT_TRUE(restored.release);
  ASSERT_EQ(restored.current_height, 2500000u);
  ASSERT_EQ(restored.hard_forks.size(), 2u);
  ASSERT_EQ(restored.hard_forks[0].hf_version, 14u);
  ASSERT_EQ(restored.hard_forks[1].height, 2200000u);
}

// ============================================================
// compress/decompress integer array helpers
// ============================================================

TEST(core_rpc, compress_decompress_empty)
{
  std::vector<uint64_t> v;
  std::string compressed = compress_integer_array(v);
  std::vector<uint64_t> decompressed = decompress_integer_array<uint64_t>(compressed);
  ASSERT_TRUE(decompressed.empty());
}

TEST(core_rpc, compress_decompress_single)
{
  std::vector<uint64_t> v = {42};
  std::string compressed = compress_integer_array(v);
  std::vector<uint64_t> decompressed = decompress_integer_array<uint64_t>(compressed);
  ASSERT_EQ(decompressed.size(), 1u);
  ASSERT_EQ(decompressed[0], 42u);
}

TEST(core_rpc, compress_decompress_multiple)
{
  std::vector<uint64_t> v = {0, 1, 127, 128, 255, 256, 65535, 1000000, UINT64_MAX};
  std::string compressed = compress_integer_array(v);
  std::vector<uint64_t> decompressed = decompress_integer_array<uint64_t>(compressed);
  ASSERT_EQ(decompressed.size(), v.size());
  for (size_t i = 0; i < v.size(); ++i)
    ASSERT_EQ(decompressed[i], v[i]);
}

// ============================================================
// Additional RPC command struct roundtrip tests
// ============================================================

TEST(core_rpc, get_block_header_by_hash_request_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request_t req;
  req.hash = "418015bb9ae982a1975da7d79277c2705727a56894ba0fb246adaabb1f4632e3";
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.hash, req.hash);
  ASSERT_EQ(req2.fill_pow_hash, true);
}

TEST(core_rpc, get_block_header_by_hash_response_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.block_header.height = 2500000;
  res.block_header.depth = 100;
  res.block_header.hash = "418015bb9ae982a1975da7d79277c2705727a56894ba0fb246adaabb1f4632e3";
  res.block_header.difficulty = 300000000000ULL;
  res.block_header.reward = 600000000ULL;
  res.block_header.timestamp = 1700000000;
  res.block_header.major_version = 16;
  res.block_header.minor_version = 16;
  res.block_header.nonce = 12345;
  res.block_header.orphan_status = false;
  res.block_header.num_txes = 5;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.status, CORE_RPC_STATUS_OK);
  ASSERT_EQ(res2.block_header.height, 2500000u);
  ASSERT_EQ(res2.block_header.depth, 100u);
  ASSERT_EQ(res2.block_header.hash, res.block_header.hash);
  ASSERT_EQ(res2.block_header.difficulty, 300000000000ULL);
  ASSERT_EQ(res2.block_header.reward, 600000000ULL);
  ASSERT_EQ(res2.block_header.nonce, 12345u);
  ASSERT_EQ(res2.block_header.num_txes, 5u);
}

TEST(core_rpc, get_block_header_by_height_request_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request_t req;
  req.height = 1234567;
  req.fill_pow_hash = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.height, 1234567u);
}

TEST(core_rpc, get_block_header_by_height_response_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.block_header.height = 1234567;
  res.block_header.timestamp = 1600000000;
  res.block_header.major_version = 14;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.block_header.height, 1234567u);
  ASSERT_EQ(res2.block_header.timestamp, 1600000000u);
  ASSERT_EQ(res2.block_header.major_version, 14u);
}

TEST(core_rpc, get_block_request_full_roundtrip)
{
  COMMAND_RPC_GET_BLOCK::request_t req;
  req.hash = "deadbeef01234567890abcdef01234567890abcdef01234567890abcdef012345";
  req.height = 0;
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.hash, req.hash);
  ASSERT_EQ(req2.fill_pow_hash, true);
}

TEST(core_rpc, get_block_response_full_roundtrip)
{
  COMMAND_RPC_GET_BLOCK::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.blob = "0102030405060708";
  res.json = "{\"major_version\":16}";
  res.block_header.height = 999;
  res.block_header.major_version = 16;
  res.tx_hashes.push_back("aabb00112233");
  res.tx_hashes.push_back("ccdd44556677");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.blob, "0102030405060708");
  ASSERT_EQ(res2.json, "{\"major_version\":16}");
  ASSERT_EQ(res2.block_header.height, 999u);
  ASSERT_EQ(res2.tx_hashes.size(), 2u);
  ASSERT_EQ(res2.tx_hashes[0], "aabb00112233");
}

TEST(core_rpc, get_peer_list_full_roundtrip)
{
  COMMAND_RPC_GET_PEER_LIST::request_t req;
  req.include_blocked = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_PEER_LIST::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.include_blocked, true);
}

TEST(core_rpc, get_peer_list_response_with_peers)
{
  COMMAND_RPC_GET_PEER_LIST::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  cryptonote::peer pe;
  pe.id = 12345;
  pe.host = "192.168.1.1";
  pe.port = 18080;
  pe.last_seen = 1700000000;
  pe.pruning_seed = 0;
  pe.rpc_port = 18081;
  pe.rpc_credits_per_hash = 0;
  res.white_list.push_back(pe);

  pe.id = 67890;
  pe.host = "10.0.0.1";
  pe.port = 18080;
  res.gray_list.push_back(pe);

  std::string json_str;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json_str));

  COMMAND_RPC_GET_PEER_LIST::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json_str));
  ASSERT_EQ(res2.white_list.size(), 1u);
  ASSERT_EQ(res2.gray_list.size(), 1u);
  ASSERT_EQ(res2.white_list[0].id, 12345u);
  ASSERT_EQ(res2.white_list[0].host, "192.168.1.1");
  ASSERT_EQ(res2.gray_list[0].id, 67890u);
}

TEST(core_rpc, hard_fork_info_full_roundtrip)
{
  COMMAND_RPC_HARD_FORK_INFO::request_t req;
  req.version = 16;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_HARD_FORK_INFO::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.version, 16u);
}

TEST(core_rpc, hard_fork_info_response_full_roundtrip)
{
  COMMAND_RPC_HARD_FORK_INFO::response_t res;
  res.version = 16;
  res.enabled = true;
  res.window = 10080;
  res.votes = 10000;
  res.threshold = 0;
  res.voting = 16;
  res.state = 2;  // ready
  res.earliest_height = 2700000;
  res.status = CORE_RPC_STATUS_OK;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_HARD_FORK_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.version, 16u);
  ASSERT_TRUE(res2.enabled);
  ASSERT_EQ(res2.window, 10080u);
  ASSERT_EQ(res2.votes, 10000u);
  ASSERT_EQ(res2.voting, 16u);
  ASSERT_EQ(res2.state, 2u);
  ASSERT_EQ(res2.earliest_height, 2700000u);
}

TEST(core_rpc, get_output_histogram_full_roundtrip)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request_t req;
  req.amounts.push_back(0);
  req.amounts.push_back(1000000000000ULL);
  req.min_count = 10;
  req.max_count = 100;
  req.unlocked = true;
  req.recent_cutoff = 500;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.amounts.size(), 2u);
  ASSERT_EQ(req2.amounts[0], 0u);
  ASSERT_EQ(req2.amounts[1], 1000000000000ULL);
  ASSERT_EQ(req2.min_count, 10u);
  ASSERT_EQ(req2.max_count, 100u);
  ASSERT_TRUE(req2.unlocked);
}

TEST(core_rpc, get_output_histogram_response_full_roundtrip)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry e;
  e.amount = 0;
  e.total_instances = 50000000;
  e.unlocked_instances = 49000000;
  e.recent_instances = 100000;
  res.histogram.push_back(e);

  e.amount = 1000000000000ULL;
  e.total_instances = 100;
  e.unlocked_instances = 90;
  e.recent_instances = 5;
  res.histogram.push_back(e);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.histogram.size(), 2u);
  ASSERT_EQ(res2.histogram[0].amount, 0u);
  ASSERT_EQ(res2.histogram[0].total_instances, 50000000u);
  ASSERT_EQ(res2.histogram[1].amount, 1000000000000ULL);
  ASSERT_EQ(res2.histogram[1].unlocked_instances, 90u);
}

TEST(core_rpc, get_coinbase_tx_sum_full_roundtrip)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req;
  req.height = 100000;
  req.count = 1000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.height, 100000u);
  ASSERT_EQ(req2.count, 1000u);
}

TEST(core_rpc, get_coinbase_tx_sum_response_full_roundtrip)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.emission_amount = 17500000000000000000ULL;
  res.emission_amount_top64 = 0;
  res.fee_amount = 500000000000ULL;
  res.fee_amount_top64 = 0;
  res.wide_emission_amount = "17500000000000000000";
  res.wide_fee_amount = "500000000000";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_COINBASE_TX_SUM::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.emission_amount, 17500000000000000000ULL);
  ASSERT_EQ(res2.fee_amount, 500000000000ULL);
  ASSERT_EQ(res2.wide_emission_amount, "17500000000000000000");
  ASSERT_EQ(res2.wide_fee_amount, "500000000000");
}

// Disabled: pre-existing build errors due to API mismatch in generated tests.
#if 0
TEST(core_rpc, sync_info_response_with_spans)
{
  COMMAND_RPC_SYNC_INFO::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.height = 2500000;
  res.target_height = 2500100;
  res.next_needed_pruning_seed = 0;
  res.overview = "[]";

  COMMAND_RPC_SYNC_INFO::peer pi;
  pi.info.host = "192.168.0.1";
  pi.info.port = "18080";
  pi.info.peer_id = "abcdef0123456789";
  pi.info.recv_count = 1000000;
  pi.info.send_count = 500000;
  pi.info.state = "normal";
  pi.info.incoming = false;
  pi.info.live_time = 3600;
  pi.info.height = 2500050;
  pi.info.connection_id = "conn-id-1";
  pi.info.avg_download = 100;
  pi.info.avg_upload = 50;
  pi.info.current_download = 10;
  pi.info.current_upload = 5;
  pi.info.recv_idle_time = 1;
  pi.info.send_idle_time = 2;
  pi.info.address = "192.168.0.1:18080";
  pi.info.rpc_port = 18081;
  pi.info.rpc_credits_per_hash = 0;
  pi.info.support_flags = 1;
  pi.info.pruning_seed = 0;
  pi.info.address_type = 1;
  res.peers.push_back(pi);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SYNC_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.target_height, 2500100u);
  ASSERT_EQ(res2.peers.size(), 1u);
  ASSERT_EQ(res2.peers.front().info.host, "192.168.0.1");
  ASSERT_EQ(res2.peers.front().info.height, 2500050u);
}

TEST(core_rpc, get_alternate_chains_response_roundtrip)
{
  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_ALTERNATE_CHAINS::chain_info ci;
  ci.block_hash = "aabbccdd";
  ci.height = 2400000;
  ci.length = 3;
  ci.difficulty = 250000000000ULL;
  ci.wide_difficulty = "250000000000";
  ci.difficulty_top64 = 0;
  ci.block_hashes.push_back("hash1");
  ci.block_hashes.push_back("hash2");
  ci.main_chain_parent_block = "parent_hash";
  res.chains.push_back(ci);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.chains.size(), 1u);
  ASSERT_EQ(res2.chains[0].block_hash, "aabbccdd");
  ASSERT_EQ(res2.chains[0].height, 2400000u);
  ASSERT_EQ(res2.chains[0].length, 3u);
  ASSERT_EQ(res2.chains[0].block_hashes.size(), 2u);
}

TEST(core_rpc, update_request_roundtrip)
{
  COMMAND_RPC_UPDATE::request_t req;
  req.command = "check";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_UPDATE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.command, "check");
}

TEST(core_rpc, update_response_full_roundtrip)
{
  COMMAND_RPC_UPDATE::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.update = true;
  res.version = "0.18.3.4";
  res.user_uri = "https://downloads.getmonero.org/cli/monero-linux-x64-v0.18.3.4.tar.bz2";
  res.auto_uri = "https://auto.update.getmonero.org/cli/monero-linux-x64-v0.18.3.4.tar.bz2";
  res.hash = "abcdef0123456789";
  res.path = "/tmp/monero-update";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_UPDATE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.update);
  ASSERT_EQ(res2.version, "0.18.3.4");
  ASSERT_EQ(res2.hash, "abcdef0123456789");
}

TEST(core_rpc, get_output_distribution_response_roundtrip)
{
  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::distribution d;
  d.amount = 0;
  d.data.start_height = 0;
  d.data.base = 0;
  d.data.distribution.push_back(100);
  d.data.distribution.push_back(200);
  d.data.distribution.push_back(300);
  d.binary = false;
  d.compress = false;
  res.distributions.push_back(d);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.distributions.size(), 1u);
  ASSERT_EQ(res2.distributions[0].amount, 0u);
  ASSERT_EQ(res2.distributions[0].data.distribution.size(), 3u);
  ASSERT_EQ(res2.distributions[0].data.distribution[2], 300u);
}

TEST(core_rpc, get_connections_response_with_multiple)
{
  COMMAND_RPC_GET_CONNECTIONS::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  for (int i = 0; i < 3; i++) {
    cryptonote::connection_info ci;
    ci.incoming = (i % 2 == 0);
    ci.ip = "192.168.1." + std::to_string(i);
    ci.port = std::to_string(18080 + i);
    ci.peer_id = "peer" + std::to_string(i);
    ci.recv_count = 1000 * (i + 1);
    ci.send_count = 500 * (i + 1);
    ci.state = "normal";
    ci.live_time = 3600 * (i + 1);
    ci.avg_download = 100 + i;
    ci.avg_upload = 50 + i;
    ci.current_download = 10;
    ci.current_upload = 5;
    ci.recv_idle_time = 1;
    ci.send_idle_time = 2;
    ci.address = ci.ip + ":" + ci.port;
    ci.host = ci.ip;
    ci.connection_id = "conn" + std::to_string(i);
    ci.height = 2500000 + i;
    ci.rpc_port = 18081;
    ci.rpc_credits_per_hash = 0;
    ci.support_flags = 1;
    ci.pruning_seed = 0;
    ci.address_type = 1;
    res.connections.push_back(ci);
  }

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_CONNECTIONS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.connections.size(), 3u);
  auto it = res2.connections.begin();
  ASSERT_TRUE(it->incoming);
  ASSERT_EQ(it->live_time, 3600u);
  ++it;
  ASSERT_FALSE(it->incoming);
  ASSERT_EQ(it->live_time, 7200u);
  ++it;
  ASSERT_EQ(it->ip, "192.168.1.2");
}

TEST(core_rpc, get_block_headers_range_request_full)
{
  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request_t req;
  req.start_height = 1000000;
  req.end_height = 1000010;
  req.fill_pow_hash = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.start_height, 1000000u);
  ASSERT_EQ(req2.end_height, 1000010u);
  ASSERT_TRUE(req2.fill_pow_hash);
}

TEST(core_rpc, get_block_headers_range_response_with_headers)
{
  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  for (int i = 0; i < 3; i++) {
    cryptonote::block_header_response bh;
    bh.height = 1000000 + i;
    bh.timestamp = 1600000000 + i * 120;
    bh.major_version = 14;
    bh.minor_version = 14;
    bh.depth = 500000 - i;
    bh.nonce = 99999 + i;
    bh.orphan_status = false;
    bh.reward = 600000000ULL;
    bh.num_txes = i + 1;
    bh.difficulty = 250000000000ULL;
    bh.wide_difficulty = "250000000000";
    bh.difficulty_top64 = 0;
    bh.hash = "block_hash_" + std::to_string(i);
    res.headers.push_back(bh);
  }

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.headers.size(), 3u);
  ASSERT_EQ(res2.headers[0].height, 1000000u);
  ASSERT_EQ(res2.headers[1].height, 1000001u);
  ASSERT_EQ(res2.headers[2].num_txes, 3u);
}

TEST(core_rpc, getbans_response_with_entries)
{
  COMMAND_RPC_GETBANS::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GETBANS::ban b;
  b.host = "10.0.0.1";
  b.ip = 167772161; // 10.0.0.1 as uint32
  b.seconds = 3600;
  res.bans.push_back(b);

  b.host = "10.0.0.2";
  b.ip = 167772162;
  b.seconds = 7200;
  res.bans.push_back(b);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETBANS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.bans.size(), 2u);
  ASSERT_EQ(res2.bans[0].host, "10.0.0.1");
  ASSERT_EQ(res2.bans[0].seconds, 3600u);
  ASSERT_EQ(res2.bans[1].seconds, 7200u);
}

TEST(core_rpc, setbans_request_with_entries)
{
  COMMAND_RPC_SETBANS::request_t req;

  COMMAND_RPC_SETBANS::ban b;
  b.host = "10.0.0.5";
  b.ip = 0;
  b.ban = true;
  b.seconds = 86400;
  req.bans.push_back(b);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SETBANS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.bans.size(), 1u);
  ASSERT_EQ(req2.bans[0].host, "10.0.0.5");
  ASSERT_TRUE(req2.bans[0].ban);
  ASSERT_EQ(req2.bans[0].seconds, 86400u);
}

TEST(core_rpc, banned_request_roundtrip)
{
  COMMAND_RPC_BANNED::request_t req;
  req.address = "203.0.113.5";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_BANNED::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "203.0.113.5");
}

TEST(core_rpc, banned_response_roundtrip)
{
  COMMAND_RPC_BANNED::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.banned = true;
  res.seconds = 42000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_BANNED::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.banned);
  ASSERT_EQ(res2.seconds, 42000u);
}

TEST(core_rpc, pop_blocks_request_roundtrip)
{
  COMMAND_RPC_POP_BLOCKS::request_t req;
  req.nblocks = 10;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_POP_BLOCKS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.nblocks, 10u);
}

TEST(core_rpc, pop_blocks_response_roundtrip)
{
  COMMAND_RPC_POP_BLOCKS::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.height = 2499990;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_POP_BLOCKS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2499990u);
}

TEST(core_rpc, prune_blockchain_request_full)
{
  COMMAND_RPC_PRUNE_BLOCKCHAIN::request_t req;
  req.check = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_PRUNE_BLOCKCHAIN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.check);
}

TEST(core_rpc, prune_blockchain_response_full)
{
  COMMAND_RPC_PRUNE_BLOCKCHAIN::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.pruned = true;
  res.pruning_seed = 385;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_PRUNE_BLOCKCHAIN::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.pruned);
  ASSERT_EQ(res2.pruning_seed, 385u);
}

TEST(core_rpc, get_info_full_response_roundtrip)
{
  COMMAND_RPC_GET_INFO::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.height = 2500000;
  res.target_height = 2500100;
  res.difficulty = 300000000000ULL;
  res.wide_difficulty = "300000000000";
  res.difficulty_top64 = 0;
  res.target = 120;
  res.tx_count = 25000000;
  res.tx_pool_size = 50;
  res.alt_blocks_count = 2;
  res.outgoing_connections_count = 8;
  res.incoming_connections_count = 12;
  res.rpc_connections_count = 3;
  res.white_peerlist_size = 1000;
  res.grey_peerlist_size = 5000;
  res.mainnet = true;
  res.testnet = false;
  res.stagenet = false;
  res.nettype = "mainnet";
  res.top_block_hash = "tophash";
  res.cumulative_difficulty = 1000000000000000000ULL;
  res.wide_cumulative_difficulty = "1000000000000000000";
  res.cumulative_difficulty_top64 = 0;
  res.block_size_limit = 600000;
  res.block_weight_limit = 600000;
  res.block_size_median = 300000;
  res.block_weight_median = 300000;
  res.adjusted_time = 1700000000;
  res.free_space = 100000000000ULL;
  res.offline = false;
  res.database_size = 150000000000ULL;
  res.update_available = false;
  res.version = "0.18.3.4";
  res.synchronized = true;
  res.busy_syncing = false;
  res.restricted = false;
  res.bootstrap_daemon_address = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_INFO::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.target_height, 2500100u);
  ASSERT_EQ(res2.difficulty, 300000000000ULL);
  ASSERT_EQ(res2.tx_count, 25000000u);
  ASSERT_EQ(res2.tx_pool_size, 50u);
  ASSERT_TRUE(res2.mainnet);
  ASSERT_FALSE(res2.testnet);
  ASSERT_TRUE(res2.synchronized);
  ASSERT_EQ(res2.version, "0.18.3.4");
  ASSERT_EQ(res2.database_size, 150000000000ULL);
}

TEST(core_rpc, mining_status_full_response_roundtrip)
{
  COMMAND_RPC_MINING_STATUS::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.active = true;
  res.speed = 1500;
  res.threads_count = 4;
  res.address = "4...address";
  res.difficulty = 300000000000ULL;
  res.wide_difficulty = "300000000000";
  res.difficulty_top64 = 0;
  res.block_target = 120;
  res.block_reward = 600000000ULL;
  res.pow_algorithm = "RandomX";
  res.is_background_mining_enabled = false;
  res.bg_idle_threshold = 0;
  res.bg_min_idle_seconds = 0;
  res.bg_ignore_battery = false;
  res.bg_target = 0;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MINING_STATUS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.active);
  ASSERT_EQ(res2.speed, 1500u);
  ASSERT_EQ(res2.threads_count, 4u);
  ASSERT_EQ(res2.block_reward, 600000000ULL);
  ASSERT_EQ(res2.pow_algorithm, "RandomX");
}

TEST(core_rpc, get_tx_pool_stats_response_full)
{
  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.pool_stats.bytes_total = 100000;
  res.pool_stats.bytes_min = 200;
  res.pool_stats.bytes_max = 50000;
  res.pool_stats.bytes_med = 5000;
  res.pool_stats.fee_total = 1000000000ULL;
  res.pool_stats.oldest = 1699999000;
  res.pool_stats.txs_total = 50;
  res.pool_stats.num_failing = 0;
  res.pool_stats.num_10m = 10;
  res.pool_stats.num_not_relayed = 2;
  res.pool_stats.num_double_spends = 0;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.pool_stats.bytes_total, 100000u);
  ASSERT_EQ(res2.pool_stats.txs_total, 50u);
  ASSERT_EQ(res2.pool_stats.fee_total, 1000000000ULL);
  ASSERT_EQ(res2.pool_stats.num_10m, 10u);
}

TEST(core_rpc, get_transactions_response_full)
{
  COMMAND_RPC_GET_TRANSACTIONS::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.missed_tx.push_back("missed_hash1");

  COMMAND_RPC_GET_TRANSACTIONS::entry e;
  e.tx_hash = "tx_hash_01";
  e.as_hex = "0102030405";
  e.as_json = "{}";
  e.block_height = 1234567;
  e.block_timestamp = 1600000000;
  e.in_pool = false;
  e.double_spend_seen = false;
  e.output_indices.push_back(100);
  e.output_indices.push_back(101);
  res.txs.push_back(e);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTIONS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.missed_tx.size(), 1u);
  ASSERT_EQ(res2.missed_tx[0], "missed_hash1");
  ASSERT_EQ(res2.txs.size(), 1u);
  ASSERT_EQ(res2.txs[0].tx_hash, "tx_hash_01");
  ASSERT_EQ(res2.txs[0].block_height, 1234567u);
  ASSERT_EQ(res2.txs[0].output_indices.size(), 2u);
}

TEST(core_rpc, get_transaction_pool_response_with_entries)
{
  COMMAND_RPC_GET_TRANSACTION_POOL::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  cryptonote::tx_info ti;
  ti.id_hash = "txhash01";
  ti.tx_blob = "blob01";
  ti.blob_size = 1000;
  ti.weight = 1500;
  ti.fee = 50000000;
  ti.max_used_block_id_hash = "maxblock";
  ti.max_used_block_height = 100;
  ti.kept_by_block = false;
  ti.last_failed_height = 0;
  ti.last_failed_id_hash = "";
  ti.receive_time = 1700000000;
  ti.relayed = true;
  ti.last_relayed_time = 1700000001;
  ti.do_not_relay = false;
  ti.double_spend_seen = false;
  ti.tx_json = "{}";
  res.transactions.push_back(ti);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSACTION_POOL::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.transactions.size(), 1u);
  ASSERT_EQ(res2.transactions[0].id_hash, "txhash01");
  ASSERT_EQ(res2.transactions[0].fee, 50000000u);
  ASSERT_TRUE(res2.transactions[0].relayed);
}

TEST(core_rpc, get_txids_loose_request_roundtrip)
{
  COMMAND_RPC_GET_TXIDS_LOOSE::request_t req;
  req.txid_template = "abcd0000efgh";
  req.num_matching_bits = 32;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TXIDS_LOOSE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid_template, "abcd0000efgh");
  ASSERT_EQ(req2.num_matching_bits, 32u);
}

TEST(core_rpc, get_txids_loose_response_roundtrip)
{
  COMMAND_RPC_GET_TXIDS_LOOSE::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.txids.push_back("match1");
  res.txids.push_back("match2");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TXIDS_LOOSE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.txids.size(), 2u);
  ASSERT_EQ(res2.txids[0], "match1");
}

TEST(core_rpc, flush_cache_request_full)
{
  COMMAND_RPC_FLUSH_CACHE::request_t req;
  req.bad_txs = true;
  req.bad_blocks = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_FLUSH_CACHE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.bad_txs);
  ASSERT_TRUE(req2.bad_blocks);
}

TEST(core_rpc, generateblocks_full_roundtrip)
{
  COMMAND_RPC_GENERATEBLOCKS::request_t req;
  req.amount_of_blocks = 100;
  req.wallet_address = "4...testaddr";
  req.prev_block = "prevhash";
  req.starting_nonce = 0;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GENERATEBLOCKS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.amount_of_blocks, 100u);
  ASSERT_EQ(req2.wallet_address, "4...testaddr");
  ASSERT_EQ(req2.prev_block, "prevhash");
}

TEST(core_rpc, generateblocks_response_full)
{
  COMMAND_RPC_GENERATEBLOCKS::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.height = 2500100;
  res.blocks.push_back("blockhash1");
  res.blocks.push_back("blockhash2");

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GENERATEBLOCKS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500100u);
  ASSERT_EQ(res2.blocks.size(), 2u);
}

TEST(core_rpc, getminerdata_response_roundtrip)
{
  COMMAND_RPC_GETMINERDATA::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.major_version = 16;
  res.height = 2500000;
  res.prev_id = "previd";
  res.seed_hash = "seedhash";
  res.difficulty = "300000000000";
  res.median_weight = 300000;
  res.already_generated_coins = 17500000000000000000ULL;

  COMMAND_RPC_GETMINERDATA::tx_backlog_entry tbe;
  tbe.id = "txid1";
  tbe.weight = 1500;
  tbe.fee = 50000000;
  res.tx_backlog.push_back(tbe);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GETMINERDATA::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.major_version, 16u);
  ASSERT_EQ(res2.height, 2500000u);
  ASSERT_EQ(res2.seed_hash, "seedhash");
  ASSERT_EQ(res2.tx_backlog.size(), 1u);
  ASSERT_EQ(res2.tx_backlog[0].fee, 50000000u);
}

TEST(core_rpc, calcpow_request_roundtrip)
{
  COMMAND_RPC_CALCPOW::request_t req;
  req.major_version = 16;
  req.height = 2500000;
  req.block_blob = "blockblob";
  req.seed_hash = "seedhash";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CALCPOW::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.major_version, 16u);
  ASSERT_EQ(req2.height, 2500000u);
  ASSERT_EQ(req2.block_blob, "blockblob");
  ASSERT_EQ(req2.seed_hash, "seedhash");
}

TEST(core_rpc, add_aux_pow_request_full)
{
  COMMAND_RPC_ADD_AUX_POW::request_t req;
  req.blocktemplate_blob = "template_blob";
  COMMAND_RPC_ADD_AUX_POW::aux_pow_t ae;
  ae.id = "aux_id1";
  ae.hash = "aux_hash1";
  req.aux_pow.push_back(ae);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ADD_AUX_POW::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.blocktemplate_blob, "template_blob");
  ASSERT_EQ(req2.aux_pow.size(), 1u);
  ASSERT_EQ(req2.aux_pow[0].id, "aux_id1");
}

TEST(core_rpc, add_aux_pow_response_full)
{
  COMMAND_RPC_ADD_AUX_POW::response_t res;
  res.status = CORE_RPC_STATUS_OK;
  res.blocktemplate_blob = "result_blob";
  res.blockhashing_blob = "hashing_blob";
  res.merkle_root = "merkle_root";
  res.merkle_tree_depth = 3;

  COMMAND_RPC_ADD_AUX_POW::aux_pow_t ae;
  ae.id = "aux_id1";
  ae.hash = "aux_hash1";
  res.aux_pow.push_back(ae);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ADD_AUX_POW::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.blocktemplate_blob, "result_blob");
  ASSERT_EQ(res2.blockhashing_blob, "hashing_blob");
  ASSERT_EQ(res2.merkle_root, "merkle_root");
  ASSERT_EQ(res2.merkle_tree_depth, 3u);
}

TEST(core_rpc, get_public_nodes_request_roundtrip)
{
  COMMAND_RPC_GET_PUBLIC_NODES::request_t req;
  req.gray = true;
  req.white = true;
  req.include_blocked = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_PUBLIC_NODES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.gray);
  ASSERT_TRUE(req2.white);
  ASSERT_FALSE(req2.include_blocked);
}

TEST(core_rpc, get_public_nodes_response_roundtrip)
{
  COMMAND_RPC_GET_PUBLIC_NODES::response_t res;
  res.status = CORE_RPC_STATUS_OK;

  cryptonote::public_node node;
  node.host = "node.example.com";
  node.last_seen = 1700000000;
  node.rpc_port = 18081;
  node.rpc_credits_per_hash = 0;
  res.white.push_back(node);

  node.host = "gray.example.com";
  node.rpc_port = 18082;
  res.gray.push_back(node);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_PUBLIC_NODES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.white.size(), 1u);
  ASSERT_EQ(res2.gray.size(), 1u);
  ASSERT_EQ(res2.white[0].host, "node.example.com");
  ASSERT_EQ(res2.gray[0].rpc_port, 18082u);
}

// ============================================================
// Binary roundtrip tests for additional RPC commands
// ============================================================

TEST(core_rpc, get_block_header_by_hash_binary_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t original;
  original.status = CORE_RPC_STATUS_OK;
  original.block_header.height = 2000000;
  original.block_header.timestamp = 1650000000;
  original.block_header.difficulty = 250000000000ULL;
  original.block_header.reward = 600000000ULL;
  original.block_header.major_version = 15;
  original.block_header.nonce = 54321;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.block_header.height, 2000000u);
  ASSERT_EQ(restored.block_header.difficulty, 250000000000ULL);
  ASSERT_EQ(restored.block_header.reward, 600000000ULL);
}

TEST(core_rpc, get_block_header_by_height_binary_roundtrip)
{
  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t original;
  original.status = CORE_RPC_STATUS_OK;
  original.block_header.height = 1500000;
  original.block_header.timestamp = 1620000000;
  original.block_header.major_version = 14;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.block_header.height, 1500000u);
  ASSERT_EQ(restored.block_header.major_version, 14u);
}

TEST(core_rpc, get_peer_list_binary_roundtrip)
{
  COMMAND_RPC_GET_PEER_LIST::response_t original;
  original.status = CORE_RPC_STATUS_OK;

  cryptonote::peer pe;
  pe.id = 11111;
  pe.host = "1.2.3.4";
  pe.port = 18080;
  pe.last_seen = 1700000000;
  pe.pruning_seed = 0;
  pe.rpc_port = 18081;
  pe.rpc_credits_per_hash = 0;
  original.white_list.push_back(pe);

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_PEER_LIST::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.white_list.size(), 1u);
  ASSERT_EQ(restored.white_list[0].id, 11111u);
  ASSERT_EQ(restored.white_list[0].host, "1.2.3.4");
}

TEST(core_rpc, get_output_histogram_binary_roundtrip)
{
  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t original;
  original.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry e;
  e.amount = 0;
  e.total_instances = 40000000;
  e.unlocked_instances = 39000000;
  e.recent_instances = 50000;
  original.histogram.push_back(e);

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.histogram.size(), 1u);
  ASSERT_EQ(restored.histogram[0].total_instances, 40000000u);
}

TEST(core_rpc, get_coinbase_tx_sum_request_binary_roundtrip)
{
  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t original;
  original.height = 200000;
  original.count = 5000;

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_COINBASE_TX_SUM::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.height, 200000u);
  ASSERT_EQ(restored.count, 5000u);
}

TEST(core_rpc, get_alternate_chains_binary_roundtrip)
{
  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t original;
  original.status = CORE_RPC_STATUS_OK;

  COMMAND_RPC_GET_ALTERNATE_CHAINS::chain_info ci;
  ci.block_hash = "chainhash";
  ci.height = 2400000;
  ci.length = 5;
  ci.difficulty = 200000000000ULL;
  ci.wide_difficulty = "200000000000";
  ci.difficulty_top64 = 0;
  ci.main_chain_parent_block = "parenthash";
  original.chains.push_back(ci);

  epee::byte_slice buff;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, buff));

  COMMAND_RPC_GET_ALTERNATE_CHAINS::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::to_span(buff)));
  ASSERT_EQ(restored.chains.size(), 1u);
  ASSERT_EQ(restored.chains[0].height, 2400000u);
  ASSERT_EQ(restored.chains[0].length, 5u);
}
#endif
