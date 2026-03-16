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

TEST(core_rpc, get_block_hash_request_serialization)
{
  COMMAND_RPC_GETBLOCKHASH::request_t req;
  req.push_back(100);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
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
