// Copyright (c) 2016-2024, The Monero Project
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

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "byte_stream.h"
#include "rpc/daemon_messages.h"
#include "rpc/daemon_rpc_version.h"
#include "rpc/message.h"
#include "serialization/json_object.h"

namespace
{
  // Helper: serialize a message to JSON string
  std::string msg_to_json(const cryptonote::rpc::Message& msg)
  {
    epee::byte_stream buffer;
    rapidjson::Writer<epee::byte_stream> dest{buffer};
    msg.toJson(dest);
    return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  }

  // Helper: parse a JSON string into a rapidjson::Document
  rapidjson::Document parse_json(const std::string& json)
  {
    rapidjson::Document doc;
    doc.Parse(json.c_str());
    return doc;
  }
}

// ============================================================
// GetHeight message tests
// ============================================================

TEST(daemon_messages, GetHeight_Request_roundtrip)
{
  cryptonote::rpc::GetHeight::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetHeight::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, GetHeight_Response_roundtrip)
{
  cryptonote::rpc::GetHeight::Response original;
  original.height = 2500000;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetHeight::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.height, 2500000u);
}

// ============================================================
// GetBlocksFast message tests
// ============================================================

TEST(daemon_messages, GetBlocksFast_Request_roundtrip)
{
  cryptonote::rpc::GetBlocksFast::Request original;
  original.start_height = 100000;
  original.prune = true;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlocksFast::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.start_height, 100000u);
  EXPECT_TRUE(restored.prune);
}

TEST(daemon_messages, GetBlocksFast_Response_roundtrip)
{
  cryptonote::rpc::GetBlocksFast::Response original;
  original.start_height = 100000;
  original.current_height = 200000;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlocksFast::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.start_height, 100000u);
  EXPECT_EQ(restored.current_height, 200000u);
}

// ============================================================
// GetHashesFast message tests
// ============================================================

TEST(daemon_messages, GetHashesFast_Request_roundtrip)
{
  cryptonote::rpc::GetHashesFast::Request original;
  original.start_height = 500;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetHashesFast::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.start_height, 500u);
}

TEST(daemon_messages, GetHashesFast_Response_roundtrip)
{
  cryptonote::rpc::GetHashesFast::Response original;
  original.start_height = 500;
  original.current_height = 1000;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetHashesFast::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.start_height, 500u);
  EXPECT_EQ(restored.current_height, 1000u);
}

// ============================================================
// GetBlockHash message tests
// ============================================================

TEST(daemon_messages, GetBlockHash_Request_roundtrip)
{
  cryptonote::rpc::GetBlockHash::Request original;
  original.height = 999999;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlockHash::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.height, 999999u);
}

TEST(daemon_messages, GetBlockHash_Response_roundtrip)
{
  cryptonote::rpc::GetBlockHash::Response original;
  // hash is crypto::hash - leave as default (all zeros)

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlockHash::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.hash, original.hash);
}

// ============================================================
// GetBlockHeaderByHeight message tests
// ============================================================

TEST(daemon_messages, GetBlockHeaderByHeight_Request_roundtrip)
{
  cryptonote::rpc::GetBlockHeaderByHeight::Request original;
  original.height = 42;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlockHeaderByHeight::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.height, 42u);
}

// ============================================================
// GetBlockHeadersByHeight message tests
// ============================================================

TEST(daemon_messages, GetBlockHeadersByHeight_Request_roundtrip)
{
  cryptonote::rpc::GetBlockHeadersByHeight::Request original;
  original.heights.push_back(100);
  original.heights.push_back(200);
  original.heights.push_back(300);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlockHeadersByHeight::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.heights.size(), 3u);
  EXPECT_EQ(restored.heights[0], 100u);
  EXPECT_EQ(restored.heights[1], 200u);
  EXPECT_EQ(restored.heights[2], 300u);
}

// ============================================================
// GetBlockHeaderByHash message tests
// ============================================================

TEST(daemon_messages, GetBlockHeaderByHash_Request_roundtrip)
{
  cryptonote::rpc::GetBlockHeaderByHash::Request original;
  // hash is crypto::hash - leave as default

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetBlockHeaderByHash::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.hash, original.hash);
}

// ============================================================
// SetLogLevel message tests
// ============================================================

TEST(daemon_messages, SetLogLevel_Request_roundtrip)
{
  cryptonote::rpc::SetLogLevel::Request original;
  original.level = 3;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SetLogLevel::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.level, 3);
}

TEST(daemon_messages, SetLogLevel_Response_roundtrip)
{
  cryptonote::rpc::SetLogLevel::Response original;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SetLogLevel::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// HardForkInfo message tests
// ============================================================

TEST(daemon_messages, HardForkInfo_Request_roundtrip)
{
  cryptonote::rpc::HardForkInfo::Request original;
  original.version = 14;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::HardForkInfo::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.version, 14u);
}

// ============================================================
// GetOutputHistogram message tests
// ============================================================

TEST(daemon_messages, GetOutputHistogram_Request_roundtrip)
{
  cryptonote::rpc::GetOutputHistogram::Request original;
  original.amounts.push_back(0);
  original.amounts.push_back(1000000000);
  original.min_count = 10;
  original.max_count = 100;
  original.unlocked = true;
  original.recent_cutoff = 50;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetOutputHistogram::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.amounts.size(), 2u);
  EXPECT_EQ(restored.amounts[0], 0u);
  EXPECT_EQ(restored.amounts[1], 1000000000u);
  EXPECT_EQ(restored.min_count, 10u);
  EXPECT_EQ(restored.max_count, 100u);
  EXPECT_TRUE(restored.unlocked);
  EXPECT_EQ(restored.recent_cutoff, 50u);
}

// ============================================================
// GetFeeEstimate message tests
// ============================================================

TEST(daemon_messages, GetFeeEstimate_Request_roundtrip)
{
  cryptonote::rpc::GetFeeEstimate::Request original;
  original.num_grace_blocks = 10;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetFeeEstimate::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.num_grace_blocks, 10u);
}

TEST(daemon_messages, GetFeeEstimate_Response_roundtrip)
{
  cryptonote::rpc::GetFeeEstimate::Response original;
  original.estimated_base_fee = 20000;
  original.fee_mask = 10000;
  original.size_scale = 1;
  original.hard_fork_version = 14;
  original.fees.push_back(20000);
  original.fees.push_back(80000);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetFeeEstimate::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.estimated_base_fee, 20000u);
  EXPECT_EQ(restored.fee_mask, 10000u);
  EXPECT_EQ(restored.size_scale, 1u);
  EXPECT_EQ(restored.hard_fork_version, 14u);
  ASSERT_EQ(restored.fees.size(), 2u);
  EXPECT_EQ(restored.fees[0], 20000u);
  EXPECT_EQ(restored.fees[1], 80000u);
}

// ============================================================
// GetRPCVersion message tests
// ============================================================

TEST(daemon_messages, GetRPCVersion_Request_roundtrip)
{
  cryptonote::rpc::GetRPCVersion::Request original;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetRPCVersion::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, GetRPCVersion_Response_roundtrip)
{
  cryptonote::rpc::GetRPCVersion::Response original;
  original.version = 42;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetRPCVersion::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.version, 42u);
}

// ============================================================
// GetOutputDistribution message tests
// ============================================================

TEST(daemon_messages, GetOutputDistribution_Request_roundtrip)
{
  cryptonote::rpc::GetOutputDistribution::Request original;
  original.amounts.push_back(0);
  original.from_height = 2000000;
  original.to_height = 2500000;
  original.cumulative = true;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetOutputDistribution::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.amounts.size(), 1u);
  EXPECT_EQ(restored.amounts[0], 0u);
  EXPECT_EQ(restored.from_height, 2000000u);
  EXPECT_EQ(restored.to_height, 2500000u);
  EXPECT_TRUE(restored.cumulative);
}

// ============================================================
// SendRawTx message tests
// ============================================================

TEST(daemon_messages, SendRawTx_Request_relay_field)
{
  cryptonote::rpc::SendRawTx::Request original;
  original.relay = false;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SendRawTx::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_FALSE(restored.relay);
}

TEST(daemon_messages, SendRawTx_Response_roundtrip)
{
  cryptonote::rpc::SendRawTx::Response original;
  original.relayed = true;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SendRawTx::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_TRUE(restored.relayed);
}

// ============================================================
// SendRawTxHex message tests
// ============================================================

TEST(daemon_messages, SendRawTxHex_Request_roundtrip)
{
  cryptonote::rpc::SendRawTxHex::Request original;
  original.tx_as_hex = "deadbeef0102";
  original.relay = true;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SendRawTxHex::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.tx_as_hex, "deadbeef0102");
  EXPECT_TRUE(restored.relay);
}

// ============================================================
// StartMining message tests
// ============================================================

TEST(daemon_messages, StartMining_Request_roundtrip)
{
  cryptonote::rpc::StartMining::Request original;
  original.miner_address = "44GBHzv6ZyQdJkjqZje6KLZ3xSyN1hBSFAnLP6EAqJtCRVzMzZmeXTC2AHKDS9aEDTRKmo6a6o9r9j86pYfhCWDkKjbtcns";
  original.threads_count = 4;
  original.do_background_mining = true;
  original.ignore_battery = false;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::StartMining::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_FALSE(restored.miner_address.empty());
  EXPECT_EQ(restored.threads_count, 4u);
  EXPECT_TRUE(restored.do_background_mining);
  EXPECT_FALSE(restored.ignore_battery);
}

TEST(daemon_messages, StartMining_Response_roundtrip)
{
  cryptonote::rpc::StartMining::Response original;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::StartMining::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// StopMining / SaveBC empty message tests
// ============================================================

TEST(daemon_messages, StopMining_Request_roundtrip)
{
  cryptonote::rpc::StopMining::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::StopMining::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, StopMining_Response_roundtrip)
{
  cryptonote::rpc::StopMining::Response original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::StopMining::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, SaveBC_Request_roundtrip)
{
  cryptonote::rpc::SaveBC::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SaveBC::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, SaveBC_Response_roundtrip)
{
  cryptonote::rpc::SaveBC::Response original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::SaveBC::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// MiningStatus message tests
// ============================================================

TEST(daemon_messages, MiningStatus_Request_roundtrip)
{
  cryptonote::rpc::MiningStatus::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::MiningStatus::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

TEST(daemon_messages, MiningStatus_Response_roundtrip)
{
  cryptonote::rpc::MiningStatus::Response original;
  original.active = true;
  original.speed = 1500;
  original.threads_count = 8;
  original.address = "some_address";
  original.is_background_mining_enabled = true;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::MiningStatus::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_TRUE(restored.active);
  EXPECT_EQ(restored.speed, 1500u);
  EXPECT_EQ(restored.threads_count, 8u);
  EXPECT_EQ(restored.address, "some_address");
  EXPECT_TRUE(restored.is_background_mining_enabled);
}

// ============================================================
// GetLastBlockHeader message tests
// ============================================================

TEST(daemon_messages, GetLastBlockHeader_Request_roundtrip)
{
  cryptonote::rpc::GetLastBlockHeader::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetLastBlockHeader::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// GetPeerList message tests
// ============================================================

TEST(daemon_messages, GetPeerList_Request_roundtrip)
{
  cryptonote::rpc::GetPeerList::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetPeerList::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// GetTransactionPool message tests
// ============================================================

TEST(daemon_messages, GetTransactionPool_Request_roundtrip)
{
  cryptonote::rpc::GetTransactionPool::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetTransactionPool::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// GetInfo message tests
// ============================================================

TEST(daemon_messages, GetInfo_Request_roundtrip)
{
  cryptonote::rpc::GetInfo::Request original;
  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetInfo::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
}

// ============================================================
// Message base class tests
// ============================================================

TEST(daemon_messages, Message_status_constants)
{
  EXPECT_STREQ(cryptonote::rpc::Message::STATUS_OK, "OK");
  EXPECT_STREQ(cryptonote::rpc::Message::STATUS_RETRY, "Retry");
  EXPECT_STREQ(cryptonote::rpc::Message::STATUS_FAILED, "Failed");
  EXPECT_STREQ(cryptonote::rpc::Message::STATUS_BAD_REQUEST, "Invalid request type");
  EXPECT_STREQ(cryptonote::rpc::Message::STATUS_BAD_JSON, "Malformed json");
}

TEST(daemon_messages, Message_default_status_is_ok)
{
  cryptonote::rpc::GetHeight::Request msg;
  EXPECT_EQ(msg.status, cryptonote::rpc::Message::STATUS_OK);
}

// ============================================================
// FullMessage tests
// ============================================================

TEST(daemon_messages, FullMessage_getRequest)
{
  cryptonote::rpc::GetHeight::Request req;
  epee::byte_slice result = cryptonote::rpc::FullMessage::getRequest("get_height", req, 1);
  ASSERT_GT(result.size(), 0u);

  // Parse the result and verify it's valid JSON-RPC
  std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
  rapidjson::Document doc;
  doc.Parse(json_str.c_str());
  ASSERT_FALSE(doc.HasParseError());
  ASSERT_TRUE(doc.HasMember("jsonrpc"));
  ASSERT_TRUE(doc.HasMember("method"));
  ASSERT_TRUE(doc.HasMember("params"));
  ASSERT_TRUE(doc.HasMember("id"));
  EXPECT_STREQ(doc["method"].GetString(), "get_height");
  EXPECT_EQ(doc["id"].GetUint(), 1u);
}

TEST(daemon_messages, FullMessage_parse_request)
{
  // Build a valid JSON-RPC request
  std::string json = R"({"jsonrpc":"2.0","id":5,"method":"get_height","params":{"rpc_version":0}})";
  cryptonote::rpc::FullMessage parsed{std::string(json), true};
  EXPECT_EQ(parsed.getRequestType(), "get_height");
  EXPECT_EQ(parsed.getID().GetInt(), 5);
}

TEST(daemon_messages, FullMessage_invalid_json_throws)
{
  EXPECT_THROW(
    (cryptonote::rpc::FullMessage{"not json at all", true}),
    cryptonote::json::PARSE_FAIL
  );
}

TEST(daemon_messages, FullMessage_missing_method_throws)
{
  EXPECT_THROW(
    (cryptonote::rpc::FullMessage{R"({"jsonrpc":"2.0","id":1,"params":{}})", true}),
    cryptonote::json::MISSING_KEY
  );
}

TEST(daemon_messages, FullMessage_missing_jsonrpc_throws)
{
  EXPECT_THROW(
    (cryptonote::rpc::FullMessage{R"({"id":1,"method":"test","params":{}})", true}),
    cryptonote::json::MISSING_KEY
  );
}

TEST(daemon_messages, BAD_REQUEST_produces_error_response)
{
  epee::byte_slice result = cryptonote::rpc::BAD_REQUEST("invalid_method");
  ASSERT_GT(result.size(), 0u);

  std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
  rapidjson::Document doc;
  doc.Parse(json_str.c_str());
  ASSERT_FALSE(doc.HasParseError());
  ASSERT_TRUE(doc.HasMember("error"));
}

TEST(daemon_messages, BAD_JSON_produces_error_response)
{
  epee::byte_slice result = cryptonote::rpc::BAD_JSON("parse error at offset 5");
  ASSERT_GT(result.size(), 0u);

  std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
  rapidjson::Document doc;
  doc.Parse(json_str.c_str());
  ASSERT_FALSE(doc.HasParseError());
  ASSERT_TRUE(doc.HasMember("error"));
}

// ============================================================
// fromJson with wrong type should throw
// ============================================================

TEST(daemon_messages, GetHeight_Response_fromJson_wrong_type_throws)
{
  rapidjson::Document doc;
  doc.SetArray();
  cryptonote::rpc::GetHeight::Response resp;
  EXPECT_THROW(resp.fromJson(doc), cryptonote::json::WRONG_TYPE);
}

TEST(daemon_messages, GetBlocksFast_Request_fromJson_wrong_type_throws)
{
  rapidjson::Document doc;
  doc.SetString("not_object", doc.GetAllocator());
  cryptonote::rpc::GetBlocksFast::Request req;
  EXPECT_THROW(req.fromJson(doc), cryptonote::json::WRONG_TYPE);
}

TEST(daemon_messages, MiningStatus_Response_fromJson_wrong_type_throws)
{
  rapidjson::Document doc;
  doc.SetInt(42);
  cryptonote::rpc::MiningStatus::Response resp;
  EXPECT_THROW(resp.fromJson(doc), cryptonote::json::WRONG_TYPE);
}

// ============================================================
// GetTransactions message tests
// ============================================================

TEST(daemon_messages, GetTransactions_Request_roundtrip)
{
  cryptonote::rpc::GetTransactions::Request original;
  crypto::hash h1, h2;
  memset(&h1, 0x11, sizeof(h1));
  memset(&h2, 0x22, sizeof(h2));
  original.tx_hashes.push_back(h1);
  original.tx_hashes.push_back(h2);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetTransactions::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.tx_hashes.size(), 2u);
  EXPECT_EQ(restored.tx_hashes[0], h1);
  EXPECT_EQ(restored.tx_hashes[1], h2);
}

TEST(daemon_messages, GetTransactions_Request_empty)
{
  cryptonote::rpc::GetTransactions::Request original;
  // empty tx_hashes

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetTransactions::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_TRUE(restored.tx_hashes.empty());
}

// ============================================================
// KeyImagesSpent message tests
// ============================================================

TEST(daemon_messages, KeyImagesSpent_Request_roundtrip)
{
  cryptonote::rpc::KeyImagesSpent::Request original;
  crypto::key_image ki;
  memset(&ki, 0xAB, sizeof(ki));
  original.key_images.push_back(ki);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::KeyImagesSpent::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.key_images.size(), 1u);
  EXPECT_EQ(restored.key_images[0], ki);
}

TEST(daemon_messages, KeyImagesSpent_Response_roundtrip)
{
  cryptonote::rpc::KeyImagesSpent::Response original;
  original.spent_status.push_back(cryptonote::rpc::KeyImagesSpent::UNSPENT);
  original.spent_status.push_back(cryptonote::rpc::KeyImagesSpent::SPENT_IN_BLOCKCHAIN);
  original.spent_status.push_back(cryptonote::rpc::KeyImagesSpent::SPENT_IN_POOL);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::KeyImagesSpent::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.spent_status.size(), 3u);
  EXPECT_EQ(restored.spent_status[0], 0u);
  EXPECT_EQ(restored.spent_status[1], 1u);
  EXPECT_EQ(restored.spent_status[2], 2u);
}

// ============================================================
// GetTxGlobalOutputIndices message tests
// ============================================================

TEST(daemon_messages, GetTxGlobalOutputIndices_Request_roundtrip)
{
  cryptonote::rpc::GetTxGlobalOutputIndices::Request original;
  memset(&original.tx_hash, 0xDD, sizeof(original.tx_hash));

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetTxGlobalOutputIndices::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.tx_hash, original.tx_hash);
}

TEST(daemon_messages, GetTxGlobalOutputIndices_Response_roundtrip)
{
  cryptonote::rpc::GetTxGlobalOutputIndices::Response original;
  original.output_indices.push_back(0);
  original.output_indices.push_back(42);
  original.output_indices.push_back(999);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetTxGlobalOutputIndices::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.output_indices.size(), 3u);
  EXPECT_EQ(restored.output_indices[0], 0u);
  EXPECT_EQ(restored.output_indices[1], 42u);
  EXPECT_EQ(restored.output_indices[2], 999u);
}

// ============================================================
// GetOutputKeys message tests
// ============================================================

TEST(daemon_messages, GetOutputKeys_Request_roundtrip)
{
  cryptonote::rpc::GetOutputKeys::Request original;
  cryptonote::rpc::output_amount_and_index oai;
  oai.amount = 0;
  oai.index = 12345;
  original.outputs.push_back(oai);

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetOutputKeys::Request restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  ASSERT_EQ(restored.outputs.size(), 1u);
  EXPECT_EQ(restored.outputs[0].amount, 0u);
  EXPECT_EQ(restored.outputs[0].index, 12345u);
}

// ============================================================
// FullMessage getResponse test
// ============================================================

TEST(daemon_messages, FullMessage_getResponse)
{
  cryptonote::rpc::GetHeight::Response resp;
  resp.height = 1234567;

  // Create a valid JSON ID value
  rapidjson::Document doc;
  doc.SetInt(99);

  epee::byte_slice result = cryptonote::rpc::FullMessage::getResponse(resp, doc);
  ASSERT_GT(result.size(), 0u);

  std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
  rapidjson::Document parsed;
  parsed.Parse(json_str.c_str());
  ASSERT_FALSE(parsed.HasParseError());
  ASSERT_TRUE(parsed.HasMember("jsonrpc"));
  ASSERT_TRUE(parsed.HasMember("id"));
  ASSERT_TRUE(parsed.HasMember("result"));
}

// ============================================================
// FullMessage parse response
// ============================================================

TEST(daemon_messages, FullMessage_parse_response)
{
  std::string json = R"({"jsonrpc":"2.0","id":10,"result":{"height":500000,"status":"OK","rpc_version":0}})";
  cryptonote::rpc::FullMessage parsed{std::string(json), false};
  const rapidjson::Value& msg = parsed.getMessage();
  ASSERT_TRUE(msg.HasMember("height"));
  EXPECT_EQ(msg["height"].GetUint64(), 500000u);
}

// ============================================================
// Message base class fields
// ============================================================

TEST(daemon_messages, Message_toJson_writes_rpc_version)
{
  // The base Message::toJson always writes rpc_version as DAEMON_RPC_VERSION_ZMQ
  cryptonote::rpc::GetHeight::Request original;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());
  ASSERT_TRUE(doc.HasMember("rpc_version"));
  EXPECT_EQ(doc["rpc_version"].GetUint(), cryptonote::rpc::DAEMON_RPC_VERSION_ZMQ);
}

TEST(daemon_messages, GetHeight_Response_fromJson_reads_rpc_version)
{
  // GetHeight::Response::fromJson calls the base class fromJson,
  // which reads the rpc_version field
  cryptonote::rpc::GetHeight::Response original;
  original.height = 100;

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());

  cryptonote::rpc::GetHeight::Response restored;
  EXPECT_NO_THROW(restored.fromJson(doc));
  EXPECT_EQ(restored.height, 100u);
}

TEST(daemon_messages, Message_status_not_serialized_by_toJson)
{
  // The status and error_details fields are not included in toJson output
  cryptonote::rpc::GetHeight::Request original;
  original.status = cryptonote::rpc::Message::STATUS_FAILED;
  original.error_details = "something went wrong";

  std::string json = msg_to_json(original);
  auto doc = parse_json(json);
  ASSERT_FALSE(doc.HasParseError());
  // Verify status and error_details are not in the JSON
  EXPECT_FALSE(doc.HasMember("status"));
  EXPECT_FALSE(doc.HasMember("error_details"));
}

// ============================================================
// BAD_REQUEST with ID
// ============================================================

TEST(daemon_messages, BAD_REQUEST_with_id)
{
  rapidjson::Document doc;
  doc.SetInt(77);

  epee::byte_slice result = cryptonote::rpc::BAD_REQUEST("invalid", doc);
  ASSERT_GT(result.size(), 0u);

  std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
  rapidjson::Document parsed;
  parsed.Parse(json_str.c_str());
  ASSERT_FALSE(parsed.HasParseError());
  ASSERT_TRUE(parsed.HasMember("error"));
  ASSERT_TRUE(parsed.HasMember("id"));
  EXPECT_EQ(parsed["id"].GetInt(), 77);
}

// ============================================================
// KeyImagesSpent status enum values
// ============================================================

TEST(daemon_messages, KeyImagesSpent_status_enum_values)
{
  EXPECT_EQ(cryptonote::rpc::KeyImagesSpent::UNSPENT, 0);
  EXPECT_EQ(cryptonote::rpc::KeyImagesSpent::SPENT_IN_BLOCKCHAIN, 1);
  EXPECT_EQ(cryptonote::rpc::KeyImagesSpent::SPENT_IN_POOL, 2);
}

// ============================================================
// Multiple FullMessage requests with different IDs
// ============================================================

TEST(daemon_messages, FullMessage_different_ids)
{
  for (unsigned id = 0; id < 5; ++id)
  {
    cryptonote::rpc::GetHeight::Request req;
    epee::byte_slice result = cryptonote::rpc::FullMessage::getRequest("get_height", req, id);

    std::string json_str(reinterpret_cast<const char*>(result.data()), result.size());
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());
    ASSERT_FALSE(doc.HasParseError());
    EXPECT_EQ(doc["id"].GetUint(), id);
  }
}

// ============================================================
// fromJson with missing fields should not crash
// ============================================================

TEST(daemon_messages, GetFeeEstimate_Response_fromJson_missing_fields_throws)
{
  // An empty JSON object -- required fields are missing, should throw
  rapidjson::Document doc;
  doc.SetObject();

  cryptonote::rpc::GetFeeEstimate::Response resp;
  EXPECT_THROW(resp.fromJson(doc), cryptonote::json::MISSING_KEY);
}

TEST(daemon_messages, GetOutputDistribution_Request_fromJson_missing_fields_throws)
{
  // An empty JSON object -- required fields are missing, should throw
  rapidjson::Document doc;
  doc.SetObject();

  cryptonote::rpc::GetOutputDistribution::Request req;
  EXPECT_THROW(req.fromJson(doc), cryptonote::json::MISSING_KEY);
}

TEST(daemon_messages, HardForkInfo_Request_fromJson_wrong_type_throws)
{
  rapidjson::Document doc;
  doc.SetNull();
  cryptonote::rpc::HardForkInfo::Request req;
  EXPECT_THROW(req.fromJson(doc), cryptonote::json::WRONG_TYPE);
}

TEST(daemon_messages, SetLogLevel_Request_fromJson_wrong_type_throws)
{
  rapidjson::Document doc;
  doc.SetBool(true);
  cryptonote::rpc::SetLogLevel::Request req;
  EXPECT_THROW(req.fromJson(doc), cryptonote::json::WRONG_TYPE);
}
