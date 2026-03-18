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

// --- GET_ADDRESS response ---
TEST(wallet_rpc, get_address_response_serialization)
{
  COMMAND_RPC_GET_ADDRESS::response_t res;
  res.address = "4...testaddr";
  COMMAND_RPC_GET_ADDRESS::address_info ai;
  ai.address = "4...sub";
  ai.label = "lbl";
  ai.address_index = 1;
  ai.used = true;
  res.addresses.push_back(ai);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());

  COMMAND_RPC_GET_ADDRESS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...testaddr");
  ASSERT_EQ(res2.addresses.size(), 1u);
  ASSERT_EQ(res2.addresses[0].label, "lbl");
  ASSERT_EQ(res2.addresses[0].address_index, 1u);
  ASSERT_TRUE(res2.addresses[0].used);
}

// --- GET_ADDRESS_INDEX ---
TEST(wallet_rpc, get_address_index_request_serialization)
{
  COMMAND_RPC_GET_ADDRESS_INDEX::request_t req;
  req.address = "4...addr";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ADDRESS_INDEX::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...addr");
}

TEST(wallet_rpc, get_address_index_response_serialization)
{
  COMMAND_RPC_GET_ADDRESS_INDEX::response_t res;
  res.index.major = 1;
  res.index.minor = 5;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ADDRESS_INDEX::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.index.major, 1u);
  ASSERT_EQ(res2.index.minor, 5u);
}

// --- CREATE_ADDRESS response ---
TEST(wallet_rpc, create_address_response_serialization)
{
  COMMAND_RPC_CREATE_ADDRESS::response_t res;
  res.address = "4...new";
  res.address_index = 7;
  res.addresses = {"4...a", "4...b"};
  res.address_indices = {7, 8};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CREATE_ADDRESS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...new");
  ASSERT_EQ(res2.address_index, 7u);
  ASSERT_EQ(res2.addresses.size(), 2u);
  ASSERT_EQ(res2.address_indices.size(), 2u);
}

// --- LABEL_ADDRESS ---
TEST(wallet_rpc, label_address_request_serialization)
{
  COMMAND_RPC_LABEL_ADDRESS::request_t req;
  req.index.major = 0;
  req.index.minor = 3;
  req.label = "my_label";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_LABEL_ADDRESS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.index.major, 0u);
  ASSERT_EQ(req2.index.minor, 3u);
  ASSERT_EQ(req2.label, "my_label");
}

TEST(wallet_rpc, label_address_response_serialization)
{
  COMMAND_RPC_LABEL_ADDRESS::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_ACCOUNTS ---
TEST(wallet_rpc, get_accounts_request_serialization)
{
  COMMAND_RPC_GET_ACCOUNTS::request_t req;
  req.tag = "savings";
  req.strict_balances = true;
  req.regexp = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ACCOUNTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tag, "savings");
  ASSERT_TRUE(req2.strict_balances);
  ASSERT_FALSE(req2.regexp);
}

TEST(wallet_rpc, get_accounts_response_serialization)
{
  COMMAND_RPC_GET_ACCOUNTS::response_t res;
  res.total_balance = 5000000000000ULL;
  res.total_unlocked_balance = 3000000000000ULL;
  COMMAND_RPC_GET_ACCOUNTS::subaddress_account_info info;
  info.account_index = 0;
  info.base_address = "4...base";
  info.balance = 5000000000000ULL;
  info.unlocked_balance = 3000000000000ULL;
  info.label = "Primary";
  info.tag = "savings";
  res.subaddress_accounts.push_back(info);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ACCOUNTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.total_balance, 5000000000000ULL);
  ASSERT_EQ(res2.subaddress_accounts.size(), 1u);
  ASSERT_EQ(res2.subaddress_accounts[0].label, "Primary");
}

// --- CREATE_ACCOUNT ---
TEST(wallet_rpc, create_account_request_serialization)
{
  COMMAND_RPC_CREATE_ACCOUNT::request_t req;
  req.label = "new_account";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CREATE_ACCOUNT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.label, "new_account");
}

TEST(wallet_rpc, create_account_response_serialization)
{
  COMMAND_RPC_CREATE_ACCOUNT::response_t res;
  res.account_index = 2;
  res.address = "4...acct";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CREATE_ACCOUNT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.account_index, 2u);
  ASSERT_EQ(res2.address, "4...acct");
}

// --- LABEL_ACCOUNT ---
TEST(wallet_rpc, label_account_request_serialization)
{
  COMMAND_RPC_LABEL_ACCOUNT::request_t req;
  req.account_index = 1;
  req.label = "savings";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_LABEL_ACCOUNT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.account_index, 1u);
  ASSERT_EQ(req2.label, "savings");
}

TEST(wallet_rpc, label_account_response_serialization)
{
  COMMAND_RPC_LABEL_ACCOUNT::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_ACCOUNT_TAGS ---
TEST(wallet_rpc, get_account_tags_request_serialization)
{
  COMMAND_RPC_GET_ACCOUNT_TAGS::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, get_account_tags_response_serialization)
{
  COMMAND_RPC_GET_ACCOUNT_TAGS::response_t res;
  COMMAND_RPC_GET_ACCOUNT_TAGS::account_tag_info tag;
  tag.tag = "mining";
  tag.label = "Mining Accounts";
  tag.accounts = {0, 1};
  res.account_tags.push_back(tag);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ACCOUNT_TAGS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.account_tags.size(), 1u);
  ASSERT_EQ(res2.account_tags[0].tag, "mining");
  ASSERT_EQ(res2.account_tags[0].accounts.size(), 2u);
}

// --- TAG_ACCOUNTS ---
TEST(wallet_rpc, tag_accounts_request_serialization)
{
  COMMAND_RPC_TAG_ACCOUNTS::request_t req;
  req.tag = "mining";
  req.accounts = {0, 2, 3};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_TAG_ACCOUNTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tag, "mining");
  ASSERT_EQ(req2.accounts.size(), 3u);
}

TEST(wallet_rpc, tag_accounts_response_serialization)
{
  COMMAND_RPC_TAG_ACCOUNTS::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- UNTAG_ACCOUNTS ---
TEST(wallet_rpc, untag_accounts_request_serialization)
{
  COMMAND_RPC_UNTAG_ACCOUNTS::request_t req;
  req.accounts = {1, 4};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_UNTAG_ACCOUNTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.accounts.size(), 2u);
}

TEST(wallet_rpc, untag_accounts_response_serialization)
{
  COMMAND_RPC_UNTAG_ACCOUNTS::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- SET_ACCOUNT_TAG_DESCRIPTION ---
TEST(wallet_rpc, set_account_tag_description_request_serialization)
{
  COMMAND_RPC_SET_ACCOUNT_TAG_DESCRIPTION::request_t req;
  req.tag = "mining";
  req.description = "Accounts for mining";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_ACCOUNT_TAG_DESCRIPTION::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tag, "mining");
  ASSERT_EQ(req2.description, "Accounts for mining");
}

TEST(wallet_rpc, set_account_tag_description_response_serialization)
{
  COMMAND_RPC_SET_ACCOUNT_TAG_DESCRIPTION::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_HEIGHT request ---
TEST(wallet_rpc, get_height_request_serialization)
{
  COMMAND_RPC_GET_HEIGHT::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

// --- TRANSFER response ---
TEST(wallet_rpc, transfer_response_serialization)
{
  COMMAND_RPC_TRANSFER::response_t res;
  res.tx_hash = "abc123";
  res.tx_key = "key456";
  res.amount = 1000000000000ULL;
  res.fee = 10000000ULL;
  res.weight = 1234;
  res.tx_blob = "blob";
  res.tx_metadata = "meta";
  res.multisig_txset = "";
  res.unsigned_txset = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_TRANSFER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_hash, "abc123");
  ASSERT_EQ(res2.tx_key, "key456");
  ASSERT_EQ(res2.amount, 1000000000000ULL);
  ASSERT_EQ(res2.fee, 10000000ULL);
  ASSERT_EQ(res2.weight, 1234u);
}

// --- TRANSFER_SPLIT ---
TEST(wallet_rpc, transfer_split_request_serialization)
{
  COMMAND_RPC_TRANSFER_SPLIT::request_t req;
  req.account_index = 0;
  req.priority = 2;
  req.ring_size = 16;
  req.unlock_time = 0;
  req.get_tx_keys = true;
  req.do_not_relay = false;
  req.get_tx_hex = true;
  req.get_tx_metadata = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_TRANSFER_SPLIT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.priority, 2u);
  ASSERT_EQ(req2.ring_size, 16u);
  ASSERT_TRUE(req2.get_tx_keys);
  ASSERT_TRUE(req2.get_tx_hex);
}

TEST(wallet_rpc, transfer_split_response_serialization)
{
  COMMAND_RPC_TRANSFER_SPLIT::response_t res;
  res.tx_hash_list = {"hash1", "hash2"};
  res.tx_key_list = {"key1", "key2"};
  res.amount_list = {100, 200};
  res.fee_list = {10, 20};
  res.weight_list = {500, 600};
  res.multisig_txset = "";
  res.unsigned_txset = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_TRANSFER_SPLIT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 2);
  ASSERT_EQ(std::distance(res2.fee_list.begin(), res2.fee_list.end()), 2);
}

// --- DESCRIBE_TRANSFER ---
TEST(wallet_rpc, describe_transfer_request_serialization)
{
  COMMAND_RPC_DESCRIBE_TRANSFER::request_t req;
  req.unsigned_txset = "unsigned_hex";
  req.multisig_txset = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_DESCRIBE_TRANSFER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.unsigned_txset, "unsigned_hex");
}

TEST(wallet_rpc, describe_transfer_response_serialization)
{
  COMMAND_RPC_DESCRIBE_TRANSFER::response_t res;
  COMMAND_RPC_DESCRIBE_TRANSFER::transfer_description td;
  td.amount_in = 2000000000000ULL;
  td.amount_out = 1999990000000ULL;
  td.ring_size = 16;
  td.unlock_time = 0;
  td.fee = 10000000ULL;
  td.dummy_outputs = 0;
  td.extra = "";
  td.payment_id = "";
  td.change_amount = 999990000000ULL;
  td.change_address = "4...change";
  res.desc.push_back(td);
  res.summary.amount_in = 2000000000000ULL;
  res.summary.amount_out = 1999990000000ULL;
  res.summary.fee = 10000000ULL;
  res.summary.change_amount = 999990000000ULL;
  res.summary.change_address = "4...change";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_DESCRIBE_TRANSFER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.desc.begin(), res2.desc.end()), 1);
  ASSERT_EQ(res2.summary.fee, 10000000ULL);
}

// --- SIGN_TRANSFER ---
TEST(wallet_rpc, sign_transfer_request_serialization)
{
  COMMAND_RPC_SIGN_TRANSFER::request_t req;
  req.unsigned_txset = "unsigned_data";
  req.export_raw = true;
  req.get_tx_keys = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SIGN_TRANSFER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.unsigned_txset, "unsigned_data");
  ASSERT_TRUE(req2.export_raw);
  ASSERT_TRUE(req2.get_tx_keys);
}

TEST(wallet_rpc, sign_transfer_response_serialization)
{
  COMMAND_RPC_SIGN_TRANSFER::response_t res;
  res.signed_txset = "signed_hex";
  res.tx_hash_list = {"h1"};
  res.tx_raw_list = {"r1"};
  res.tx_key_list = {"k1"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SIGN_TRANSFER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.signed_txset, "signed_hex");
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 1);
}

// --- SUBMIT_TRANSFER ---
TEST(wallet_rpc, submit_transfer_request_serialization)
{
  COMMAND_RPC_SUBMIT_TRANSFER::request_t req;
  req.tx_data_hex = "tx_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SUBMIT_TRANSFER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tx_data_hex, "tx_data");
}

TEST(wallet_rpc, submit_transfer_response_serialization)
{
  COMMAND_RPC_SUBMIT_TRANSFER::response_t res;
  res.tx_hash_list = {"hash1", "hash2"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SUBMIT_TRANSFER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 2);
}

// --- SWEEP_DUST ---
TEST(wallet_rpc, sweep_dust_request_serialization)
{
  COMMAND_RPC_SWEEP_DUST::request_t req;
  req.get_tx_keys = true;
  req.do_not_relay = false;
  req.get_tx_hex = true;
  req.get_tx_metadata = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SWEEP_DUST::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.get_tx_keys);
  ASSERT_TRUE(req2.get_tx_hex);
  ASSERT_FALSE(req2.do_not_relay);
}

TEST(wallet_rpc, sweep_dust_response_serialization)
{
  COMMAND_RPC_SWEEP_DUST::response_t res;
  res.tx_hash_list = {"dusthash"};
  res.fee_list = {5000};
  res.multisig_txset = "";
  res.unsigned_txset = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SWEEP_DUST::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 1);
}

// --- SWEEP_ALL ---
TEST(wallet_rpc, sweep_all_request_serialization)
{
  COMMAND_RPC_SWEEP_ALL::request_t req;
  req.address = "4...dest";
  req.account_index = 0;
  req.priority = 1;
  req.ring_size = 16;
  req.outputs = 1;
  req.unlock_time = 0;
  req.get_tx_keys = true;
  req.below_amount = 0;
  req.do_not_relay = false;
  req.get_tx_hex = false;
  req.get_tx_metadata = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SWEEP_ALL::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...dest");
  ASSERT_EQ(req2.priority, 1u);
  ASSERT_EQ(req2.ring_size, 16u);
}

TEST(wallet_rpc, sweep_all_response_serialization)
{
  COMMAND_RPC_SWEEP_ALL::response_t res;
  res.tx_hash_list = {"sa_hash"};
  res.fee_list = {20000};
  res.amount_list = {999000};
  res.multisig_txset = "";
  res.unsigned_txset = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SWEEP_ALL::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 1);
}

// --- SWEEP_SINGLE ---
TEST(wallet_rpc, sweep_single_request_serialization)
{
  COMMAND_RPC_SWEEP_SINGLE::request_t req;
  req.address = "4...single";
  req.priority = 2;
  req.ring_size = 16;
  req.outputs = 1;
  req.unlock_time = 0;
  req.get_tx_key = true;
  req.key_image = "ki_hex";
  req.do_not_relay = false;
  req.get_tx_hex = true;
  req.get_tx_metadata = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SWEEP_SINGLE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...single");
  ASSERT_EQ(req2.key_image, "ki_hex");
  ASSERT_TRUE(req2.get_tx_key);
}

TEST(wallet_rpc, sweep_single_response_serialization)
{
  COMMAND_RPC_SWEEP_SINGLE::response_t res;
  res.tx_hash = "single_hash";
  res.tx_key = "single_key";
  res.amount = 500000;
  res.fee = 1000;
  res.weight = 800;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SWEEP_SINGLE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_hash, "single_hash");
  ASSERT_EQ(res2.amount, 500000u);
}

// --- RELAY_TX ---
TEST(wallet_rpc, relay_tx_request_serialization)
{
  COMMAND_RPC_RELAY_TX::request_t req;
  req.hex = "tx_hex_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_RELAY_TX::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.hex, "tx_hex_data");
}

TEST(wallet_rpc, relay_tx_response_serialization)
{
  COMMAND_RPC_RELAY_TX::response_t res;
  res.tx_hash = "relayed_hash";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_RELAY_TX::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_hash, "relayed_hash");
}

// --- STORE ---
TEST(wallet_rpc, store_request_serialization)
{
  COMMAND_RPC_STORE::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, store_response_serialization)
{
  COMMAND_RPC_STORE::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_PAYMENTS ---
TEST(wallet_rpc, get_payments_request_serialization)
{
  COMMAND_RPC_GET_PAYMENTS::request_t req;
  req.payment_id = "abc123def456";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_PAYMENTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.payment_id, "abc123def456");
}

TEST(wallet_rpc, get_payments_response_serialization)
{
  COMMAND_RPC_GET_PAYMENTS::response_t res;
  payment_details pd;
  pd.payment_id = "pay1";
  pd.tx_hash = "txh";
  pd.amount = 100000;
  pd.block_height = 1000;
  pd.unlock_time = 0;
  pd.locked = false;
  pd.subaddr_index.major = 0;
  pd.subaddr_index.minor = 0;
  pd.address = "4...pay";
  res.payments.push_back(pd);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_PAYMENTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.payments.begin(), res2.payments.end()), 1);
}

// --- GET_BULK_PAYMENTS ---
TEST(wallet_rpc, get_bulk_payments_request_serialization)
{
  COMMAND_RPC_GET_BULK_PAYMENTS::request_t req;
  req.payment_ids = {"id1", "id2"};
  req.min_block_height = 500000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_BULK_PAYMENTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.payment_ids.size(), 2u);
  ASSERT_EQ(req2.min_block_height, 500000u);
}

TEST(wallet_rpc, get_bulk_payments_response_serialization)
{
  COMMAND_RPC_GET_BULK_PAYMENTS::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_BULK_PAYMENTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.payments.begin(), res2.payments.end()), 0);
}

// --- INCOMING_TRANSFERS ---
TEST(wallet_rpc, incoming_transfers_request_serialization)
{
  COMMAND_RPC_INCOMING_TRANSFERS::request_t req;
  req.transfer_type = "all";
  req.account_index = 0;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_INCOMING_TRANSFERS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.transfer_type, "all");
  ASSERT_EQ(req2.account_index, 0u);
}

TEST(wallet_rpc, incoming_transfers_response_serialization)
{
  COMMAND_RPC_INCOMING_TRANSFERS::response_t res;
  transfer_details td;
  td.amount = 999;
  td.spent = false;
  td.global_index = 12345;
  td.tx_hash = "txh";
  td.subaddr_index.major = 0;
  td.subaddr_index.minor = 1;
  td.key_image = "ki";
  td.pubkey = "pk";
  td.block_height = 100;
  td.frozen = false;
  td.unlocked = true;
  res.transfers.push_back(td);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_INCOMING_TRANSFERS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.transfers.begin(), res2.transfers.end()), 1);
}

// --- QUERY_KEY ---
TEST(wallet_rpc, query_key_request_serialization)
{
  COMMAND_RPC_QUERY_KEY::request_t req;
  req.key_type = "view_key";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_QUERY_KEY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key_type, "view_key");
}

TEST(wallet_rpc, query_key_response_serialization)
{
  COMMAND_RPC_QUERY_KEY::response_t res;
  res.key = "deadbeef01234567";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_QUERY_KEY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.key, "deadbeef01234567");
}

// --- MAKE_INTEGRATED_ADDRESS response ---
TEST(wallet_rpc, make_integrated_address_response_serialization)
{
  COMMAND_RPC_MAKE_INTEGRATED_ADDRESS::response_t res;
  res.integrated_address = "4...integrated";
  res.payment_id = "0123456789abcdef";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MAKE_INTEGRATED_ADDRESS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.integrated_address, "4...integrated");
  ASSERT_EQ(res2.payment_id, "0123456789abcdef");
}

// --- SPLIT_INTEGRATED_ADDRESS ---
TEST(wallet_rpc, split_integrated_address_request_serialization)
{
  COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::request_t req;
  req.integrated_address = "4...integ";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.integrated_address, "4...integ");
}

TEST(wallet_rpc, split_integrated_address_response_serialization)
{
  COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::response_t res;
  res.standard_address = "4...std";
  res.payment_id = "abcdef";
  res.is_subaddress = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.standard_address, "4...std");
  ASSERT_EQ(res2.payment_id, "abcdef");
  ASSERT_FALSE(res2.is_subaddress);
}

// --- STOP_WALLET ---
TEST(wallet_rpc, stop_wallet_request_serialization)
{
  COMMAND_RPC_STOP_WALLET::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, stop_wallet_response_serialization)
{
  COMMAND_RPC_STOP_WALLET::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- RESCAN_BLOCKCHAIN ---
TEST(wallet_rpc, rescan_blockchain_request_serialization)
{
  COMMAND_RPC_RESCAN_BLOCKCHAIN::request_t req;
  req.hard = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_RESCAN_BLOCKCHAIN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.hard);
}

TEST(wallet_rpc, rescan_blockchain_response_serialization)
{
  COMMAND_RPC_RESCAN_BLOCKCHAIN::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- SET_TX_NOTES ---
TEST(wallet_rpc, set_tx_notes_request_serialization)
{
  COMMAND_RPC_SET_TX_NOTES::request_t req;
  req.txids = {"txid1", "txid2"};
  req.notes = {"note1", "note2"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_TX_NOTES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(std::distance(req2.txids.begin(), req2.txids.end()), 2);
  ASSERT_EQ(std::distance(req2.notes.begin(), req2.notes.end()), 2);
}

TEST(wallet_rpc, set_tx_notes_response_serialization)
{
  COMMAND_RPC_SET_TX_NOTES::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_TX_NOTES ---
TEST(wallet_rpc, get_tx_notes_request_serialization)
{
  COMMAND_RPC_GET_TX_NOTES::request_t req;
  req.txids = {"txid1"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TX_NOTES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(std::distance(req2.txids.begin(), req2.txids.end()), 1);
}

TEST(wallet_rpc, get_tx_notes_response_serialization)
{
  COMMAND_RPC_GET_TX_NOTES::response_t res;
  res.notes = {"note_a", "note_b"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TX_NOTES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.notes.begin(), res2.notes.end()), 2);
}

// --- SET_ATTRIBUTE ---
TEST(wallet_rpc, set_attribute_request_serialization)
{
  COMMAND_RPC_SET_ATTRIBUTE::request_t req;
  req.key = "mykey";
  req.value = "myvalue";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_ATTRIBUTE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key, "mykey");
  ASSERT_EQ(req2.value, "myvalue");
}

TEST(wallet_rpc, set_attribute_response_serialization)
{
  COMMAND_RPC_SET_ATTRIBUTE::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_ATTRIBUTE ---
TEST(wallet_rpc, get_attribute_request_serialization)
{
  COMMAND_RPC_GET_ATTRIBUTE::request_t req;
  req.key = "mykey";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ATTRIBUTE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key, "mykey");
}

TEST(wallet_rpc, get_attribute_response_serialization)
{
  COMMAND_RPC_GET_ATTRIBUTE::response_t res;
  res.value = "stored_val";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ATTRIBUTE::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.value, "stored_val");
}

// --- GET_TX_KEY ---
TEST(wallet_rpc, get_tx_key_request_serialization)
{
  COMMAND_RPC_GET_TX_KEY::request_t req;
  req.txid = "txid_abc";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TX_KEY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid_abc");
}

TEST(wallet_rpc, get_tx_key_response_serialization)
{
  COMMAND_RPC_GET_TX_KEY::response_t res;
  res.tx_key = "secret_key_hex";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TX_KEY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_key, "secret_key_hex");
}

// --- CHECK_TX_KEY ---
TEST(wallet_rpc, check_tx_key_request_serialization)
{
  COMMAND_RPC_CHECK_TX_KEY::request_t req;
  req.txid = "txid1";
  req.tx_key = "key1";
  req.address = "4...addr";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CHECK_TX_KEY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid1");
  ASSERT_EQ(req2.tx_key, "key1");
  ASSERT_EQ(req2.address, "4...addr");
}

TEST(wallet_rpc, check_tx_key_response_serialization)
{
  COMMAND_RPC_CHECK_TX_KEY::response_t res;
  res.received = 1000000;
  res.in_pool = false;
  res.confirmations = 50;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CHECK_TX_KEY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.received, 1000000u);
  ASSERT_FALSE(res2.in_pool);
  ASSERT_EQ(res2.confirmations, 50u);
}

// --- GET_TX_PROOF ---
TEST(wallet_rpc, get_tx_proof_request_serialization)
{
  COMMAND_RPC_GET_TX_PROOF::request_t req;
  req.txid = "txid";
  req.address = "4...addr";
  req.message = "proof message";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TX_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid");
  ASSERT_EQ(req2.message, "proof message");
}

TEST(wallet_rpc, get_tx_proof_response_serialization)
{
  COMMAND_RPC_GET_TX_PROOF::response_t res;
  res.signature = "proof_sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TX_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.signature, "proof_sig");
}

// --- CHECK_TX_PROOF ---
TEST(wallet_rpc, check_tx_proof_request_serialization)
{
  COMMAND_RPC_CHECK_TX_PROOF::request_t req;
  req.txid = "txid";
  req.address = "4...addr";
  req.message = "msg";
  req.signature = "sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CHECK_TX_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid");
  ASSERT_EQ(req2.signature, "sig");
}

TEST(wallet_rpc, check_tx_proof_response_serialization)
{
  COMMAND_RPC_CHECK_TX_PROOF::response_t res;
  res.good = true;
  res.received = 500000;
  res.in_pool = false;
  res.confirmations = 100;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CHECK_TX_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.good);
  ASSERT_EQ(res2.received, 500000u);
  ASSERT_EQ(res2.confirmations, 100u);
}

// --- GET_SPEND_PROOF ---
TEST(wallet_rpc, get_spend_proof_request_serialization)
{
  COMMAND_RPC_GET_SPEND_PROOF::request_t req;
  req.txid = "txid_spend";
  req.message = "spend msg";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_SPEND_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid_spend");
  ASSERT_EQ(req2.message, "spend msg");
}

TEST(wallet_rpc, get_spend_proof_response_serialization)
{
  COMMAND_RPC_GET_SPEND_PROOF::response_t res;
  res.signature = "spend_sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_SPEND_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.signature, "spend_sig");
}

// --- CHECK_SPEND_PROOF ---
TEST(wallet_rpc, check_spend_proof_request_serialization)
{
  COMMAND_RPC_CHECK_SPEND_PROOF::request_t req;
  req.txid = "txid";
  req.message = "msg";
  req.signature = "sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CHECK_SPEND_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid");
  ASSERT_EQ(req2.signature, "sig");
}

TEST(wallet_rpc, check_spend_proof_response_serialization)
{
  COMMAND_RPC_CHECK_SPEND_PROOF::response_t res;
  res.good = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CHECK_SPEND_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.good);
}

// --- GET_RESERVE_PROOF ---
TEST(wallet_rpc, get_reserve_proof_request_serialization)
{
  COMMAND_RPC_GET_RESERVE_PROOF::request_t req;
  req.all = false;
  req.account_index = 0;
  req.amount = 1000000;
  req.message = "reserve msg";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_RESERVE_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_FALSE(req2.all);
  ASSERT_EQ(req2.amount, 1000000u);
  ASSERT_EQ(req2.message, "reserve msg");
}

TEST(wallet_rpc, get_reserve_proof_response_serialization)
{
  COMMAND_RPC_GET_RESERVE_PROOF::response_t res;
  res.signature = "reserve_sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_RESERVE_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.signature, "reserve_sig");
}

// --- CHECK_RESERVE_PROOF ---
TEST(wallet_rpc, check_reserve_proof_request_serialization)
{
  COMMAND_RPC_CHECK_RESERVE_PROOF::request_t req;
  req.address = "4...addr";
  req.message = "msg";
  req.signature = "sig";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CHECK_RESERVE_PROOF::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...addr");
  ASSERT_EQ(req2.signature, "sig");
}

TEST(wallet_rpc, check_reserve_proof_response_serialization)
{
  COMMAND_RPC_CHECK_RESERVE_PROOF::response_t res;
  res.good = true;
  res.total = 5000000;
  res.spent = 1000000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_CHECK_RESERVE_PROOF::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.good);
  ASSERT_EQ(res2.total, 5000000u);
  ASSERT_EQ(res2.spent, 1000000u);
}

// --- GET_TRANSFERS ---
TEST(wallet_rpc, get_transfers_request_serialization)
{
  COMMAND_RPC_GET_TRANSFERS::request_t req;
  req.in = true;
  req.out = true;
  req.pending = false;
  req.failed = false;
  req.pool = false;
  req.filter_by_height = true;
  req.min_height = 100000;
  req.max_height = 200000;
  req.account_index = 0;
  req.all_accounts = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSFERS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.in);
  ASSERT_TRUE(req2.out);
  ASSERT_TRUE(req2.filter_by_height);
  ASSERT_EQ(req2.min_height, 100000u);
  ASSERT_EQ(req2.max_height, 200000u);
}

TEST(wallet_rpc, get_transfers_response_serialization)
{
  COMMAND_RPC_GET_TRANSFERS::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSFERS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.in.begin(), res2.in.end()), 0);
  ASSERT_EQ(std::distance(res2.out.begin(), res2.out.end()), 0);
}

// --- GET_TRANSFER_BY_TXID ---
TEST(wallet_rpc, get_transfer_by_txid_request_serialization)
{
  COMMAND_RPC_GET_TRANSFER_BY_TXID::request_t req;
  req.txid = "txid_lookup";
  req.account_index = 1;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_TRANSFER_BY_TXID::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.txid, "txid_lookup");
  ASSERT_EQ(req2.account_index, 1u);
}

TEST(wallet_rpc, get_transfer_by_txid_response_serialization)
{
  COMMAND_RPC_GET_TRANSFER_BY_TXID::response_t res;
  res.transfer.txid = "found_txid";
  res.transfer.amount = 999;
  res.transfer.fee = 10;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_TRANSFER_BY_TXID::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.transfer.txid, "found_txid");
  ASSERT_EQ(res2.transfer.amount, 999u);
}

// --- SIGN ---
TEST(wallet_rpc, sign_request_serialization)
{
  COMMAND_RPC_SIGN::request_t req;
  req.data = "data_to_sign";
  req.account_index = 0;
  req.address_index = 1;
  req.signature_type = "spend";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SIGN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.data, "data_to_sign");
  ASSERT_EQ(req2.address_index, 1u);
  ASSERT_EQ(req2.signature_type, "spend");
}

TEST(wallet_rpc, sign_response_serialization)
{
  COMMAND_RPC_SIGN::response_t res;
  res.signature = "SigV1xxx";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SIGN::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.signature, "SigV1xxx");
}

// --- VERIFY ---
TEST(wallet_rpc, verify_request_serialization)
{
  COMMAND_RPC_VERIFY::request_t req;
  req.data = "data";
  req.address = "4...addr";
  req.signature = "SigV1xxx";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_VERIFY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.data, "data");
  ASSERT_EQ(req2.address, "4...addr");
  ASSERT_EQ(req2.signature, "SigV1xxx");
}

TEST(wallet_rpc, verify_response_serialization)
{
  COMMAND_RPC_VERIFY::response_t res;
  res.good = true;
  res.version = 2;
  res.old = false;
  res.signature_type = "spend";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_VERIFY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.good);
  ASSERT_EQ(res2.version, 2u);
  ASSERT_FALSE(res2.old);
  ASSERT_EQ(res2.signature_type, "spend");
}

// --- EXPORT_OUTPUTS ---
TEST(wallet_rpc, export_outputs_request_serialization)
{
  COMMAND_RPC_EXPORT_OUTPUTS::request_t req;
  req.all = true;
  req.start = 0;
  req.count = 100;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_EXPORT_OUTPUTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.all);
  ASSERT_EQ(req2.start, 0u);
  ASSERT_EQ(req2.count, 100u);
}

TEST(wallet_rpc, export_outputs_response_serialization)
{
  COMMAND_RPC_EXPORT_OUTPUTS::response_t res;
  res.outputs_data_hex = "deadbeef";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_EXPORT_OUTPUTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.outputs_data_hex, "deadbeef");
}

// --- IMPORT_OUTPUTS ---
TEST(wallet_rpc, import_outputs_request_serialization)
{
  COMMAND_RPC_IMPORT_OUTPUTS::request_t req;
  req.outputs_data_hex = "cafebabe";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_IMPORT_OUTPUTS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.outputs_data_hex, "cafebabe");
}

TEST(wallet_rpc, import_outputs_response_serialization)
{
  COMMAND_RPC_IMPORT_OUTPUTS::response_t res;
  res.num_imported = 42;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IMPORT_OUTPUTS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.num_imported, 42u);
}

// --- EXPORT_KEY_IMAGES ---
TEST(wallet_rpc, export_key_images_request_serialization)
{
  COMMAND_RPC_EXPORT_KEY_IMAGES::request_t req;
  req.all = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_EXPORT_KEY_IMAGES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.all);
}

TEST(wallet_rpc, export_key_images_response_serialization)
{
  COMMAND_RPC_EXPORT_KEY_IMAGES::response_t res;
  res.offset = 5;
  COMMAND_RPC_EXPORT_KEY_IMAGES::signed_key_image ski;
  ski.key_image = "ki_hex";
  ski.signature = "sig_hex";
  res.signed_key_images.push_back(ski);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_EXPORT_KEY_IMAGES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.offset, 5u);
  ASSERT_EQ(res2.signed_key_images.size(), 1u);
  ASSERT_EQ(res2.signed_key_images[0].key_image, "ki_hex");
}

// --- IMPORT_KEY_IMAGES ---
TEST(wallet_rpc, import_key_images_request_serialization)
{
  COMMAND_RPC_IMPORT_KEY_IMAGES::request_t req;
  req.offset = 0;
  COMMAND_RPC_IMPORT_KEY_IMAGES::signed_key_image ski;
  ski.key_image = "ki";
  ski.signature = "sig";
  req.signed_key_images.push_back(ski);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_IMPORT_KEY_IMAGES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.offset, 0u);
  ASSERT_EQ(req2.signed_key_images.size(), 1u);
}

TEST(wallet_rpc, import_key_images_response_serialization)
{
  COMMAND_RPC_IMPORT_KEY_IMAGES::response_t res;
  res.height = 300000;
  res.spent = 1000;
  res.unspent = 9000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IMPORT_KEY_IMAGES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.height, 300000u);
  ASSERT_EQ(res2.spent, 1000u);
  ASSERT_EQ(res2.unspent, 9000u);
}

// --- MAKE_URI ---
TEST(wallet_rpc, make_uri_request_serialization)
{
  COMMAND_RPC_MAKE_URI::request_t req;
  req.address = "4...addr";
  req.payment_id = "pid";
  req.amount = 1000000;
  req.tx_description = "test payment";
  req.recipient_name = "Alice";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_MAKE_URI::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...addr");
  ASSERT_EQ(req2.amount, 1000000u);
  ASSERT_EQ(req2.recipient_name, "Alice");
}

TEST(wallet_rpc, make_uri_response_serialization)
{
  COMMAND_RPC_MAKE_URI::response_t res;
  res.uri = "monero:4...addr?amount=1000000";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MAKE_URI::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.uri, "monero:4...addr?amount=1000000");
}

// --- PARSE_URI ---
TEST(wallet_rpc, parse_uri_request_serialization)
{
  COMMAND_RPC_PARSE_URI::request_t req;
  req.uri = "monero:4...addr?amount=1000000";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_PARSE_URI::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.uri, "monero:4...addr?amount=1000000");
}

TEST(wallet_rpc, parse_uri_response_serialization)
{
  COMMAND_RPC_PARSE_URI::response_t res;
  res.uri.address = "4...addr";
  res.uri.amount = 1000000;
  res.uri.recipient_name = "Bob";
  res.unknown_parameters = {"custom=1"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_PARSE_URI::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.uri.address, "4...addr");
  ASSERT_EQ(res2.uri.amount, 1000000u);
  ASSERT_EQ(res2.unknown_parameters.size(), 1u);
}

// --- GET_ADDRESS_BOOK_ENTRY ---
TEST(wallet_rpc, get_address_book_entry_request_serialization)
{
  COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::request_t req;
  req.entries = {0, 1, 2};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(std::distance(req2.entries.begin(), req2.entries.end()), 3);
}

TEST(wallet_rpc, get_address_book_entry_response_serialization)
{
  COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::response_t res;
  COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::entry e;
  e.index = 0;
  e.address = "4...book";
  e.description = "Friend";
  res.entries.push_back(e);

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.entries.size(), 1u);
  ASSERT_EQ(res2.entries[0].description, "Friend");
}

// --- ADD_ADDRESS_BOOK_ENTRY ---
TEST(wallet_rpc, add_address_book_entry_request_serialization)
{
  COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::request_t req;
  req.address = "4...new_contact";
  req.description = "New contact";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...new_contact");
  ASSERT_EQ(req2.description, "New contact");
}

TEST(wallet_rpc, add_address_book_entry_response_serialization)
{
  COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::response_t res;
  res.index = 3;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.index, 3u);
}

// --- EDIT_ADDRESS_BOOK_ENTRY ---
TEST(wallet_rpc, edit_address_book_entry_request_serialization)
{
  COMMAND_RPC_EDIT_ADDRESS_BOOK_ENTRY::request_t req;
  req.index = 1;
  req.set_address = true;
  req.address = "4...edited";
  req.set_description = true;
  req.description = "Updated";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_EDIT_ADDRESS_BOOK_ENTRY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.index, 1u);
  ASSERT_TRUE(req2.set_address);
  ASSERT_EQ(req2.address, "4...edited");
  ASSERT_TRUE(req2.set_description);
  ASSERT_EQ(req2.description, "Updated");
}

TEST(wallet_rpc, edit_address_book_entry_response_serialization)
{
  COMMAND_RPC_EDIT_ADDRESS_BOOK_ENTRY::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- DELETE_ADDRESS_BOOK_ENTRY ---
TEST(wallet_rpc, delete_address_book_entry_request_serialization)
{
  COMMAND_RPC_DELETE_ADDRESS_BOOK_ENTRY::request_t req;
  req.index = 2;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_DELETE_ADDRESS_BOOK_ENTRY::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.index, 2u);
}

TEST(wallet_rpc, delete_address_book_entry_response_serialization)
{
  COMMAND_RPC_DELETE_ADDRESS_BOOK_ENTRY::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- REFRESH ---
TEST(wallet_rpc, refresh_request_serialization)
{
  COMMAND_RPC_REFRESH::request_t req;
  req.start_height = 250000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_REFRESH::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.start_height, 250000u);
}

TEST(wallet_rpc, refresh_response_serialization)
{
  COMMAND_RPC_REFRESH::response_t res;
  res.blocks_fetched = 100;
  res.received_money = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_REFRESH::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.blocks_fetched, 100u);
  ASSERT_TRUE(res2.received_money);
}

// --- AUTO_REFRESH ---
TEST(wallet_rpc, auto_refresh_request_serialization)
{
  COMMAND_RPC_AUTO_REFRESH::request_t req;
  req.enable = true;
  req.period = 30;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_AUTO_REFRESH::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.enable);
  ASSERT_EQ(req2.period, 30u);
}

TEST(wallet_rpc, auto_refresh_response_serialization)
{
  COMMAND_RPC_AUTO_REFRESH::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- RESCAN_SPENT ---
TEST(wallet_rpc, rescan_spent_request_serialization)
{
  COMMAND_RPC_RESCAN_SPENT::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, rescan_spent_response_serialization)
{
  COMMAND_RPC_RESCAN_SPENT::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- START_MINING ---
TEST(wallet_rpc, start_mining_request_serialization)
{
  COMMAND_RPC_START_MINING::request_t req;
  req.threads_count = 4;
  req.do_background_mining = true;
  req.ignore_battery = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_START_MINING::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.threads_count, 4u);
  ASSERT_TRUE(req2.do_background_mining);
  ASSERT_FALSE(req2.ignore_battery);
}

TEST(wallet_rpc, start_mining_response_serialization)
{
  COMMAND_RPC_START_MINING::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- STOP_MINING ---
TEST(wallet_rpc, stop_mining_request_serialization)
{
  COMMAND_RPC_STOP_MINING::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, stop_mining_response_serialization)
{
  COMMAND_RPC_STOP_MINING::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_LANGUAGES ---
TEST(wallet_rpc, get_languages_request_serialization)
{
  COMMAND_RPC_GET_LANGUAGES::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, get_languages_response_serialization)
{
  COMMAND_RPC_GET_LANGUAGES::response_t res;
  res.languages = {"English", "Spanish"};
  res.languages_local = {"English", "Espanol"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_LANGUAGES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.languages.size(), 2u);
  ASSERT_EQ(res2.languages_local.size(), 2u);
}

// --- CREATE_WALLET ---
TEST(wallet_rpc, create_wallet_request_serialization)
{
  COMMAND_RPC_CREATE_WALLET::request_t req;
  req.filename = "testwallet";
  req.password = "pass123";
  req.language = "English";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CREATE_WALLET::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.filename, "testwallet");
  ASSERT_EQ(req2.password, "pass123");
  ASSERT_EQ(req2.language, "English");
}

TEST(wallet_rpc, create_wallet_response_serialization)
{
  COMMAND_RPC_CREATE_WALLET::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- OPEN_WALLET ---
TEST(wallet_rpc, open_wallet_request_serialization)
{
  COMMAND_RPC_OPEN_WALLET::request_t req;
  req.filename = "mywallet";
  req.password = "secret";
  req.autosave_current = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_OPEN_WALLET::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.filename, "mywallet");
  ASSERT_EQ(req2.password, "secret");
  ASSERT_TRUE(req2.autosave_current);
}

TEST(wallet_rpc, open_wallet_response_serialization)
{
  COMMAND_RPC_OPEN_WALLET::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- CLOSE_WALLET ---
TEST(wallet_rpc, close_wallet_request_serialization)
{
  COMMAND_RPC_CLOSE_WALLET::request_t req;
  req.autosave_current = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CLOSE_WALLET::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_FALSE(req2.autosave_current);
}

TEST(wallet_rpc, close_wallet_response_serialization)
{
  COMMAND_RPC_CLOSE_WALLET::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- CHANGE_WALLET_PASSWORD ---
TEST(wallet_rpc, change_wallet_password_request_serialization)
{
  COMMAND_RPC_CHANGE_WALLET_PASSWORD::request_t req;
  req.old_password = "oldpass";
  req.new_password = "newpass";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_CHANGE_WALLET_PASSWORD::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.old_password, "oldpass");
  ASSERT_EQ(req2.new_password, "newpass");
}

TEST(wallet_rpc, change_wallet_password_response_serialization)
{
  COMMAND_RPC_CHANGE_WALLET_PASSWORD::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GENERATE_FROM_KEYS ---
TEST(wallet_rpc, generate_from_keys_request_serialization)
{
  COMMAND_RPC_GENERATE_FROM_KEYS::request req;
  req.restore_height = 100000;
  req.filename = "restored";
  req.address = "4...addr";
  req.spendkey = "spend_hex";
  req.viewkey = "view_hex";
  req.password = "pw";
  req.autosave_current = false;
  req.language = "English";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GENERATE_FROM_KEYS::request req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.restore_height, 100000u);
  ASSERT_EQ(req2.filename, "restored");
  ASSERT_EQ(req2.spendkey, "spend_hex");
}

TEST(wallet_rpc, generate_from_keys_response_serialization)
{
  COMMAND_RPC_GENERATE_FROM_KEYS::response res;
  res.address = "4...generated";
  res.info = "Wallet generated";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GENERATE_FROM_KEYS::response res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...generated");
  ASSERT_EQ(res2.info, "Wallet generated");
}

// --- RESTORE_DETERMINISTIC_WALLET ---
TEST(wallet_rpc, restore_deterministic_wallet_request_serialization)
{
  COMMAND_RPC_RESTORE_DETERMINISTIC_WALLET::request_t req;
  req.restore_height = 50000;
  req.filename = "restored_det";
  req.seed = "word1 word2 word3";
  req.seed_offset = "";
  req.password = "pw";
  req.language = "English";
  req.autosave_current = true;
  req.enable_multisig_experimental = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_RESTORE_DETERMINISTIC_WALLET::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.restore_height, 50000u);
  ASSERT_EQ(req2.seed, "word1 word2 word3");
  ASSERT_EQ(req2.filename, "restored_det");
}

TEST(wallet_rpc, restore_deterministic_wallet_response_serialization)
{
  COMMAND_RPC_RESTORE_DETERMINISTIC_WALLET::response_t res;
  res.address = "4...restored";
  res.seed = "word1 word2 word3";
  res.info = "Wallet restored";
  res.was_deprecated = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_RESTORE_DETERMINISTIC_WALLET::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...restored");
  ASSERT_EQ(res2.seed, "word1 word2 word3");
  ASSERT_FALSE(res2.was_deprecated);
}

// --- IS_MULTISIG ---
TEST(wallet_rpc, is_multisig_request_serialization)
{
  COMMAND_RPC_IS_MULTISIG::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, is_multisig_response_serialization)
{
  COMMAND_RPC_IS_MULTISIG::response_t res;
  res.multisig = true;
  res.kex_is_done = true;
  res.ready = true;
  res.threshold = 2;
  res.total = 3;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IS_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.multisig);
  ASSERT_TRUE(res2.kex_is_done);
  ASSERT_TRUE(res2.ready);
  ASSERT_EQ(res2.threshold, 2u);
  ASSERT_EQ(res2.total, 3u);
}

// --- PREPARE_MULTISIG ---
TEST(wallet_rpc, prepare_multisig_request_serialization)
{
  COMMAND_RPC_PREPARE_MULTISIG::request_t req;
  req.enable_multisig_experimental = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_PREPARE_MULTISIG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_TRUE(req2.enable_multisig_experimental);
}

TEST(wallet_rpc, prepare_multisig_response_serialization)
{
  COMMAND_RPC_PREPARE_MULTISIG::response_t res;
  res.multisig_info = "MultisigV1abc";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_PREPARE_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.multisig_info, "MultisigV1abc");
}

// --- MAKE_MULTISIG ---
TEST(wallet_rpc, make_multisig_request_serialization)
{
  COMMAND_RPC_MAKE_MULTISIG::request_t req;
  req.multisig_info = {"info1", "info2"};
  req.threshold = 2;
  req.password = "pw";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_MAKE_MULTISIG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.multisig_info.size(), 2u);
  ASSERT_EQ(req2.threshold, 2u);
  ASSERT_EQ(req2.password, "pw");
}

TEST(wallet_rpc, make_multisig_response_serialization)
{
  COMMAND_RPC_MAKE_MULTISIG::response_t res;
  res.address = "4...multisig";
  res.multisig_info = "extra_info";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_MAKE_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...multisig");
  ASSERT_EQ(res2.multisig_info, "extra_info");
}

// --- EXPORT_MULTISIG ---
TEST(wallet_rpc, export_multisig_request_serialization)
{
  COMMAND_RPC_EXPORT_MULTISIG::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, export_multisig_response_serialization)
{
  COMMAND_RPC_EXPORT_MULTISIG::response_t res;
  res.info = "multisig_export_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_EXPORT_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.info, "multisig_export_data");
}

// --- IMPORT_MULTISIG ---
TEST(wallet_rpc, import_multisig_request_serialization)
{
  COMMAND_RPC_IMPORT_MULTISIG::request_t req;
  req.info = {"info_a", "info_b"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_IMPORT_MULTISIG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.info.size(), 2u);
}

TEST(wallet_rpc, import_multisig_response_serialization)
{
  COMMAND_RPC_IMPORT_MULTISIG::response_t res;
  res.n_outputs = 15;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_IMPORT_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.n_outputs, 15u);
}

// --- FINALIZE_MULTISIG ---
TEST(wallet_rpc, finalize_multisig_request_serialization)
{
  COMMAND_RPC_FINALIZE_MULTISIG::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, finalize_multisig_response_serialization)
{
  COMMAND_RPC_FINALIZE_MULTISIG::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- EXCHANGE_MULTISIG_KEYS ---
TEST(wallet_rpc, exchange_multisig_keys_request_serialization)
{
  COMMAND_RPC_EXCHANGE_MULTISIG_KEYS::request_t req;
  req.password = "pass";
  req.multisig_info = {"round2_a", "round2_b"};
  req.force_update_use_with_caution = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_EXCHANGE_MULTISIG_KEYS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.password, "pass");
  ASSERT_EQ(req2.multisig_info.size(), 2u);
  ASSERT_FALSE(req2.force_update_use_with_caution);
}

TEST(wallet_rpc, exchange_multisig_keys_response_serialization)
{
  COMMAND_RPC_EXCHANGE_MULTISIG_KEYS::response_t res;
  res.address = "4...ms_addr";
  res.multisig_info = "next_round";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_EXCHANGE_MULTISIG_KEYS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.address, "4...ms_addr");
  ASSERT_EQ(res2.multisig_info, "next_round");
}

// --- SIGN_MULTISIG ---
TEST(wallet_rpc, sign_multisig_request_serialization)
{
  COMMAND_RPC_SIGN_MULTISIG::request_t req;
  req.tx_data_hex = "ms_tx_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SIGN_MULTISIG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tx_data_hex, "ms_tx_data");
}

TEST(wallet_rpc, sign_multisig_response_serialization)
{
  COMMAND_RPC_SIGN_MULTISIG::response_t res;
  res.tx_data_hex = "signed_ms_tx";
  res.tx_hash_list = {"ms_hash1"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SIGN_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.tx_data_hex, "signed_ms_tx");
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 1);
}

// --- SUBMIT_MULTISIG ---
TEST(wallet_rpc, submit_multisig_request_serialization)
{
  COMMAND_RPC_SUBMIT_MULTISIG::request_t req;
  req.tx_data_hex = "final_ms_tx";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SUBMIT_MULTISIG::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.tx_data_hex, "final_ms_tx");
}

TEST(wallet_rpc, submit_multisig_response_serialization)
{
  COMMAND_RPC_SUBMIT_MULTISIG::response_t res;
  res.tx_hash_list = {"submitted_hash"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SUBMIT_MULTISIG::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(std::distance(res2.tx_hash_list.begin(), res2.tx_hash_list.end()), 1);
}

// --- VALIDATE_ADDRESS ---
TEST(wallet_rpc, validate_address_request_serialization)
{
  COMMAND_RPC_VALIDATE_ADDRESS::request_t req;
  req.address = "4...validate";
  req.any_net_type = true;
  req.allow_openalias = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_VALIDATE_ADDRESS::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "4...validate");
  ASSERT_TRUE(req2.any_net_type);
  ASSERT_FALSE(req2.allow_openalias);
}

TEST(wallet_rpc, validate_address_response_serialization)
{
  COMMAND_RPC_VALIDATE_ADDRESS::response_t res;
  res.valid = true;
  res.integrated = false;
  res.subaddress = false;
  res.nettype = "mainnet";
  res.openalias_address = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_VALIDATE_ADDRESS::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.valid);
  ASSERT_FALSE(res2.integrated);
  ASSERT_FALSE(res2.subaddress);
  ASSERT_EQ(res2.nettype, "mainnet");
}

// --- SET_DAEMON ---
TEST(wallet_rpc, set_daemon_request_serialization)
{
  COMMAND_RPC_SET_DAEMON::request_t req;
  req.address = "http://localhost:18081";
  req.username = "user";
  req.password = "pass";
  req.trusted = true;
  req.ssl_support = "autodetect";
  req.ssl_private_key_path = "";
  req.ssl_certificate_path = "";
  req.ssl_ca_file = "";
  req.ssl_allow_any_cert = false;
  req.proxy = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_DAEMON::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.address, "http://localhost:18081");
  ASSERT_EQ(req2.username, "user");
  ASSERT_TRUE(req2.trusted);
}

TEST(wallet_rpc, set_daemon_response_serialization)
{
  COMMAND_RPC_SET_DAEMON::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- SET_LOG_LEVEL ---
TEST(wallet_rpc, set_log_level_request_serialization)
{
  COMMAND_RPC_SET_LOG_LEVEL::request_t req;
  req.level = 2;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LOG_LEVEL::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.level, 2);
}

TEST(wallet_rpc, set_log_level_response_serialization)
{
  COMMAND_RPC_SET_LOG_LEVEL::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- SET_LOG_CATEGORIES ---
TEST(wallet_rpc, set_log_categories_request_serialization)
{
  COMMAND_RPC_SET_LOG_CATEGORIES::request_t req;
  req.categories = "wallet.rpc:INFO";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_LOG_CATEGORIES::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.categories, "wallet.rpc:INFO");
}

TEST(wallet_rpc, set_log_categories_response_serialization)
{
  COMMAND_RPC_SET_LOG_CATEGORIES::response_t res;
  res.categories = "wallet.rpc:INFO";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_SET_LOG_CATEGORIES::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.categories, "wallet.rpc:INFO");
}

// --- ESTIMATE_TX_SIZE_AND_WEIGHT ---
TEST(wallet_rpc, estimate_tx_size_and_weight_request_serialization)
{
  COMMAND_RPC_ESTIMATE_TX_SIZE_AND_WEIGHT::request_t req;
  req.n_inputs = 2;
  req.n_outputs = 2;
  req.ring_size = 16;
  req.rct = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_ESTIMATE_TX_SIZE_AND_WEIGHT::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.n_inputs, 2u);
  ASSERT_EQ(req2.n_outputs, 2u);
  ASSERT_EQ(req2.ring_size, 16u);
  ASSERT_TRUE(req2.rct);
}

TEST(wallet_rpc, estimate_tx_size_and_weight_response_serialization)
{
  COMMAND_RPC_ESTIMATE_TX_SIZE_AND_WEIGHT::response_t res;
  res.size = 5000;
  res.weight = 7000;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_ESTIMATE_TX_SIZE_AND_WEIGHT::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.size, 5000u);
  ASSERT_EQ(res2.weight, 7000u);
}

// --- FREEZE ---
TEST(wallet_rpc, freeze_request_serialization)
{
  COMMAND_RPC_FREEZE::request_t req;
  req.key_image = "ki_to_freeze";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_FREEZE::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key_image, "ki_to_freeze");
}

TEST(wallet_rpc, freeze_response_serialization)
{
  COMMAND_RPC_FREEZE::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- THAW ---
TEST(wallet_rpc, thaw_request_serialization)
{
  COMMAND_RPC_THAW::request_t req;
  req.key_image = "ki_to_thaw";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_THAW::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key_image, "ki_to_thaw");
}

TEST(wallet_rpc, thaw_response_serialization)
{
  COMMAND_RPC_THAW::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- FROZEN ---
TEST(wallet_rpc, frozen_request_serialization)
{
  COMMAND_RPC_FROZEN::request_t req;
  req.key_image = "ki_check";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_FROZEN::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.key_image, "ki_check");
}

TEST(wallet_rpc, frozen_response_serialization)
{
  COMMAND_RPC_FROZEN::response_t res;
  res.frozen = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_FROZEN::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_TRUE(res2.frozen);
}

// --- SCAN_TX ---
TEST(wallet_rpc, scan_tx_request_serialization)
{
  COMMAND_RPC_SCAN_TX::request_t req;
  req.txids = {"txid_scan1", "txid_scan2"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SCAN_TX::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(std::distance(req2.txids.begin(), req2.txids.end()), 2);
}

TEST(wallet_rpc, scan_tx_response_serialization)
{
  COMMAND_RPC_SCAN_TX::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_VERSION request ---
TEST(wallet_rpc, get_version_request_serialization)
{
  COMMAND_RPC_GET_VERSION::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

// --- SET_SUBADDR_LOOKAHEAD ---
TEST(wallet_rpc, set_subaddr_lookahead_request_serialization)
{
  COMMAND_RPC_SET_SUBADDR_LOOKAHEAD::request_t req;
  req.password = "pw";
  req.major_idx = 50;
  req.minor_idx = 200;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SET_SUBADDR_LOOKAHEAD::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.password, "pw");
  ASSERT_EQ(req2.major_idx, 50u);
  ASSERT_EQ(req2.minor_idx, 200u);
}

TEST(wallet_rpc, set_subaddr_lookahead_response_serialization)
{
  COMMAND_RPC_SET_SUBADDR_LOOKAHEAD::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_MULTISIG_KEY_EXCHANGE_BOOSTER ---
TEST(wallet_rpc, get_multisig_key_exchange_booster_request_serialization)
{
  COMMAND_RPC_GET_MULTISIG_KEY_EXCHANGE_BOOSTER::request_t req;
  req.password = "pw";
  req.multisig_info = {"info1"};
  req.threshold = 2;
  req.num_signers = 3;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_GET_MULTISIG_KEY_EXCHANGE_BOOSTER::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.password, "pw");
  ASSERT_EQ(req2.threshold, 2u);
  ASSERT_EQ(req2.num_signers, 3u);
}

TEST(wallet_rpc, get_multisig_key_exchange_booster_response_serialization)
{
  COMMAND_RPC_GET_MULTISIG_KEY_EXCHANGE_BOOSTER::response_t res;
  res.multisig_info = "booster_data";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_MULTISIG_KEY_EXCHANGE_BOOSTER::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.multisig_info, "booster_data");
}

// --- SETUP_BACKGROUND_SYNC ---
TEST(wallet_rpc, setup_background_sync_request_serialization)
{
  COMMAND_RPC_SETUP_BACKGROUND_SYNC::request_t req;
  req.background_sync_type = "custom-password";
  req.wallet_password = "wallet_pw";
  req.background_cache_password = "cache_pw";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_SETUP_BACKGROUND_SYNC::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.background_sync_type, "custom-password");
  ASSERT_EQ(req2.wallet_password, "wallet_pw");
}

TEST(wallet_rpc, setup_background_sync_response_serialization)
{
  COMMAND_RPC_SETUP_BACKGROUND_SYNC::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- START_BACKGROUND_SYNC ---
TEST(wallet_rpc, start_background_sync_request_serialization)
{
  COMMAND_RPC_START_BACKGROUND_SYNC::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, start_background_sync_response_serialization)
{
  COMMAND_RPC_START_BACKGROUND_SYNC::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- STOP_BACKGROUND_SYNC ---
TEST(wallet_rpc, stop_background_sync_request_serialization)
{
  COMMAND_RPC_STOP_BACKGROUND_SYNC::request_t req;
  req.wallet_password = "pw";
  req.seed = "seed words";
  req.seed_offset = "offset";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));

  COMMAND_RPC_STOP_BACKGROUND_SYNC::request_t req2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(req2, json));
  ASSERT_EQ(req2.wallet_password, "pw");
  ASSERT_EQ(req2.seed, "seed words");
}

TEST(wallet_rpc, stop_background_sync_response_serialization)
{
  COMMAND_RPC_STOP_BACKGROUND_SYNC::response_t res;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));
  ASSERT_FALSE(json.empty());
}

// --- GET_DEFAULT_FEE_PRIORITY ---
TEST(wallet_rpc, get_default_fee_priority_request_serialization)
{
  COMMAND_RPC_GET_DEFAULT_FEE_PRIORITY::request_t req;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(req, json));
  ASSERT_FALSE(json.empty());
}

TEST(wallet_rpc, get_default_fee_priority_response_serialization)
{
  COMMAND_RPC_GET_DEFAULT_FEE_PRIORITY::response_t res;
  res.priority = 2;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(res, json));

  COMMAND_RPC_GET_DEFAULT_FEE_PRIORITY::response_t res2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(res2, json));
  ASSERT_EQ(res2.priority, 2u);
}
