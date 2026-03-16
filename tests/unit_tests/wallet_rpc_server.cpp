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

#include "wallet/wallet_rpc_server_commands_defs.h"
#include "wallet/wallet_rpc_server_error_codes.h"
#include "serialization/keyvalue_serialization.h"
#include "storages/portable_storage_template_helper.h"

using namespace tools::wallet_rpc;

TEST(wallet_rpc, version_constants)
{
  ASSERT_GT(WALLET_RPC_VERSION_MAJOR, 0);
  ASSERT_GE(WALLET_RPC_VERSION_MINOR, 0);
  ASSERT_EQ(WALLET_RPC_VERSION, MAKE_WALLET_RPC_VERSION(WALLET_RPC_VERSION_MAJOR, WALLET_RPC_VERSION_MINOR));
}

TEST(wallet_rpc, error_codes_are_negative)
{
  ASSERT_LT(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_WRONG_ADDRESS, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_NOT_ENOUGH_MONEY, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_ZERO_DESTINATION, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_NOT_OPEN, 0);
  ASSERT_LT(WALLET_RPC_ERROR_CODE_NO_DAEMON_CONNECTION, 0);
}

TEST(wallet_rpc, error_codes_unique)
{
  // Spot check that error codes don't collide
  ASSERT_NE(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, WALLET_RPC_ERROR_CODE_WRONG_ADDRESS);
  ASSERT_NE(WALLET_RPC_ERROR_CODE_NOT_ENOUGH_MONEY, WALLET_RPC_ERROR_CODE_TX_NOT_POSSIBLE);
  ASSERT_NE(WALLET_RPC_ERROR_CODE_WRONG_TXID, WALLET_RPC_ERROR_CODE_WRONG_SIGNATURE);
  ASSERT_NE(WALLET_RPC_ERROR_CODE_ALREADY_MULTISIG, WALLET_RPC_ERROR_CODE_NOT_MULTISIG);
}

TEST(wallet_rpc, get_balance_request_serialization)
{
  COMMAND_RPC_GET_BALANCE::request_t req;
  req.account_index = 0;
  req.all_accounts = false;
  req.strict = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());

  COMMAND_RPC_GET_BALANCE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.account_index, 0u);
}

TEST(wallet_rpc, get_balance_response_serialization)
{
  COMMAND_RPC_GET_BALANCE::response_t res;
  res.balance = 1000000000000ULL;
  res.unlocked_balance = 500000000000ULL;
  res.multisig_import_needed = false;
  res.blocks_to_unlock = 10;
  res.time_to_unlock = 1200;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());

  COMMAND_RPC_GET_BALANCE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.balance, 1000000000000ULL);
  ASSERT_EQ(res2.unlocked_balance, 500000000000ULL);
}

TEST(wallet_rpc, get_address_request_serialization)
{
  COMMAND_RPC_GET_ADDRESS::request_t req;
  req.account_index = 1;
  req.address_index = {0, 1, 2};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ADDRESS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.account_index, 1u);
  ASSERT_EQ(req2.address_index.size(), 3u);
}

TEST(wallet_rpc, get_height_response_serialization)
{
  COMMAND_RPC_GET_HEIGHT::response_t res;
  res.height = 2500000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_HEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 2500000u);
}

TEST(wallet_rpc, make_integrated_address_request)
{
  COMMAND_RPC_MAKE_INTEGRATED_ADDRESS::request_t req;
  req.standard_address = "";
  req.payment_id = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, transfer_request_serialization)
{
  COMMAND_RPC_TRANSFER::request_t req;
  req.priority = 1;
  req.mixin = 15;
  req.ring_size = 16;
  req.unlock_time = 0;
  req.do_not_relay = false;
  req.get_tx_hex = false;
  req.get_tx_metadata = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_TRANSFER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.ring_size, 16u);
  ASSERT_EQ(req2.priority, 1u);
}

TEST(wallet_rpc, status_constants)
{
  ASSERT_STREQ(WALLET_RPC_STATUS_OK, "OK");
  ASSERT_STREQ(WALLET_RPC_STATUS_BUSY, "BUSY");
}

TEST(wallet_rpc, get_version_response_serialization)
{
  COMMAND_RPC_GET_VERSION::response_t res;
  res.version = WALLET_RPC_VERSION;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_VERSION::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.version, WALLET_RPC_VERSION);
}

TEST(wallet_rpc, create_address_request_serialization)
{
  COMMAND_RPC_CREATE_ADDRESS::request_t req;
  req.account_index = 0;
  req.label = "test_label";
  req.count = 3;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CREATE_ADDRESS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.account_index, 0u);
  ASSERT_EQ(req2.label, "test_label");
  ASSERT_EQ(req2.count, 3u);
}
