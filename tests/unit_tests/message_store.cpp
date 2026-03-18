// Copyright (c) 2018-2024, The Monero Project
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

#include "wallet/message_store.h"
#include "net/abstract_http_client.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/crypto.h"
#include "string_tools.h"
#include <boost/filesystem.hpp>

namespace
{
  class null_http_client : public epee::net_utils::http::abstract_http_client
  {
  public:
    void set_server(std::string host, std::string port, boost::optional<epee::net_utils::http::login> user, epee::net_utils::ssl_options_t ssl_options = epee::net_utils::ssl_support_t::e_ssl_support_autodetect) override {}
    void set_auto_connect(bool auto_connect) override {}
    bool connect(std::chrono::milliseconds timeout) override { return false; }
    bool disconnect() override { return true; }
    bool is_connected(bool *ssl = NULL) override { return false; }
    bool invoke(const boost::string_ref uri, const boost::string_ref method, const boost::string_ref body, std::chrono::milliseconds timeout, const epee::net_utils::http::http_response_info** ppresponse_info = NULL, const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override { return false; }
    bool invoke_get(const boost::string_ref uri, std::chrono::milliseconds timeout, const std::string& body = std::string(), const epee::net_utils::http::http_response_info** ppresponse_info = NULL, const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override { return false; }
    uint64_t get_bytes_sent() const override { return 0; }
    uint64_t get_bytes_received() const override { return 0; }
  };

  mms::multisig_wallet_state make_wallet_state(const std::string &mms_file = "")
  {
    mms::multisig_wallet_state state;
    cryptonote::account_base acct;
    acct.generate();
    state.address = acct.get_keys().m_account_address;
    state.nettype = cryptonote::TESTNET;
    state.view_secret_key = acct.get_keys().m_view_secret_key;
    state.multisig = false;
    state.multisig_is_ready = false;
    state.multisig_kex_is_done = false;
    state.has_multisig_partial_key_images = false;
    state.multisig_rounds_passed = 0;
    state.num_transfer_details = 0;
    state.mms_file = mms_file;
    return state;
  }

  class MessageStoreTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_temp_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("mms-test-%%%%-%%%%");
      boost::filesystem::create_directories(m_temp_dir);
      m_state = make_wallet_state((m_temp_dir / "test.mms").string());
      m_store.reset(new mms::message_store(std::make_unique<null_http_client>()));
      m_store->init(m_state, "me", "BM-me-address", 3, 2);
    }

    void TearDown() override
    {
      boost::filesystem::remove_all(m_temp_dir);
    }

    boost::filesystem::path m_temp_dir;
    mms::multisig_wallet_state m_state;
    std::unique_ptr<mms::message_store> m_store;
  };
}

// ---- Static string conversion tests ----

TEST(message_store, message_type_to_string)
{
  const char *s;
  s = mms::message_store::message_type_to_string(mms::message_type::key_set);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::additional_key_set);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::multisig_sync_data);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::partially_signed_tx);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::fully_signed_tx);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::note);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::signer_config);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::auto_config_data);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, message_direction_to_string)
{
  const char *s;
  s = mms::message_store::message_direction_to_string(mms::message_direction::in);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_direction_to_string(mms::message_direction::out);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, message_state_to_string)
{
  const char *s;
  s = mms::message_store::message_state_to_string(mms::message_state::ready_to_send);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::sent);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::waiting);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::processed);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::cancelled);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, get_sanitized_text_normal)
{
  std::string result = mms::message_store::get_sanitized_text("hello world", 100);
  ASSERT_EQ(result, "hello world");
}

TEST(message_store, get_sanitized_text_truncated)
{
  std::string result = mms::message_store::get_sanitized_text("hello world", 5);
  ASSERT_EQ(result.size(), 5u);
}

TEST(message_store, get_sanitized_text_empty)
{
  std::string result = mms::message_store::get_sanitized_text("", 100);
  ASSERT_TRUE(result.empty());
}

TEST(message_store, get_sanitized_text_nonprintable)
{
  std::string input = "hello\x01\x02world";
  std::string result = mms::message_store::get_sanitized_text(input, 100);
  ASSERT_NE(result.find("hello"), std::string::npos);
  ASSERT_NE(result.find("world"), std::string::npos);
}

TEST(message_store, get_sanitized_text_zero_length)
{
  std::string result = mms::message_store::get_sanitized_text("anything", 0);
  ASSERT_TRUE(result.empty());
}

TEST(message_store, get_sanitized_text_html_stripped)
{
  std::string input = "before<script>alert(1)</script>after";
  std::string result = mms::message_store::get_sanitized_text(input, 200);
  // < and > should be replaced
  EXPECT_EQ(result.find('<'), std::string::npos);
  EXPECT_EQ(result.find('>'), std::string::npos);
}

// ---- Constructor and init ----

TEST_F(MessageStoreTest, ConstructorDefaults)
{
  mms::message_store store(std::make_unique<null_http_client>());
  EXPECT_FALSE(store.get_active());
  EXPECT_FALSE(store.get_auto_send());
  EXPECT_EQ(store.get_num_authorized_signers(), 0u);
  EXPECT_EQ(store.get_num_required_signers(), 0u);
}

TEST_F(MessageStoreTest, InitSetsProperties)
{
  EXPECT_TRUE(m_store->get_active());
  EXPECT_EQ(m_store->get_num_authorized_signers(), 3u);
  EXPECT_EQ(m_store->get_num_required_signers(), 2u);
}

TEST_F(MessageStoreTest, InitCreatesSigners)
{
  auto &signers = m_store->get_all_signers();
  ASSERT_EQ(signers.size(), 3u);
  EXPECT_TRUE(signers[0].me);
  EXPECT_FALSE(signers[1].me);
  EXPECT_FALSE(signers[2].me);
}

TEST_F(MessageStoreTest, InitSetsFirstSigner)
{
  const auto &signer0 = m_store->get_signer(0);
  EXPECT_EQ(signer0.label, "me");
  EXPECT_EQ(signer0.transport_address, "BM-me-address");
  EXPECT_TRUE(signer0.monero_address_known);
  EXPECT_EQ(signer0.monero_address, m_state.address);
}

TEST_F(MessageStoreTest, SetActive)
{
  m_store->set_active(false);
  EXPECT_FALSE(m_store->get_active());
  m_store->set_active(true);
  EXPECT_TRUE(m_store->get_active());
}

TEST_F(MessageStoreTest, SetAutoSend)
{
  m_store->set_auto_send(true);
  EXPECT_TRUE(m_store->get_auto_send());
  m_store->set_auto_send(false);
  EXPECT_FALSE(m_store->get_auto_send());
}

// ---- Signer management ----

TEST_F(MessageStoreTest, SetAndGetSigner)
{
  cryptonote::account_base acct;
  acct.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct.get_keys().m_account_address);
  const auto &s = m_store->get_signer(1);
  EXPECT_EQ(s.label, "alice");
  EXPECT_EQ(s.transport_address, "BM-alice");
  EXPECT_TRUE(s.monero_address_known);
}

TEST_F(MessageStoreTest, SetSignerPartialUpdate)
{
  m_store->set_signer(m_state, 1, std::string("bob"), boost::none, boost::none);
  const auto &s = m_store->get_signer(1);
  EXPECT_EQ(s.label, "bob");
  EXPECT_TRUE(s.transport_address.empty());
  EXPECT_FALSE(s.monero_address_known);
}

TEST_F(MessageStoreTest, GetSignerInvalidIndexThrows)
{
  EXPECT_ANY_THROW(m_store->get_signer(99));
}

TEST_F(MessageStoreTest, GetSignerIndexByMoneroAddress)
{
  uint32_t idx = UINT32_MAX;
  bool found = m_store->get_signer_index_by_monero_address(m_state.address, idx);
  EXPECT_TRUE(found);
  EXPECT_EQ(idx, 0u);
}

TEST_F(MessageStoreTest, GetSignerIndexByMoneroAddressNotFound)
{
  cryptonote::account_base acct;
  acct.generate();
  uint32_t idx = UINT32_MAX;
  EXPECT_FALSE(m_store->get_signer_index_by_monero_address(acct.get_keys().m_account_address, idx));
}

TEST_F(MessageStoreTest, GetSignerIndexByLabel)
{
  uint32_t idx = UINT32_MAX;
  EXPECT_TRUE(m_store->get_signer_index_by_label("me", idx));
  EXPECT_EQ(idx, 0u);
}

TEST_F(MessageStoreTest, GetSignerIndexByLabelNotFound)
{
  uint32_t idx = UINT32_MAX;
  EXPECT_FALSE(m_store->get_signer_index_by_label("nonexistent", idx));
}

// ---- Signer config complete ----

TEST_F(MessageStoreTest, SignerConfigNotComplete)
{
  EXPECT_FALSE(m_store->signer_config_complete());
}

TEST_F(MessageStoreTest, SignerConfigComplete)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  EXPECT_TRUE(m_store->signer_config_complete());
}

TEST_F(MessageStoreTest, SignerLabelsNotComplete)
{
  EXPECT_FALSE(m_store->signer_labels_complete());
}

TEST_F(MessageStoreTest, SignerLabelsComplete)
{
  m_store->set_signer(m_state, 1, std::string("alice"), boost::none, boost::none);
  m_store->set_signer(m_state, 2, std::string("bob"), boost::none, boost::none);
  EXPECT_TRUE(m_store->signer_labels_complete());
}

// ---- Signer config serialization round-trip ----

TEST_F(MessageStoreTest, GetAndUnpackSignerConfig)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);

  std::string config;
  m_store->get_signer_config(config);
  EXPECT_FALSE(config.empty());

  std::vector<mms::authorized_signer> unpacked;
  m_store->unpack_signer_config(m_state, config, unpacked);
  ASSERT_EQ(unpacked.size(), 3u);
  EXPECT_EQ(unpacked[0].label, "me");
  EXPECT_EQ(unpacked[1].label, "alice");
  EXPECT_EQ(unpacked[2].label, "bob");
}

// ---- Message operations ----

TEST_F(MessageStoreTest, AddAndGetMessage)
{
  size_t idx = m_store->add_message(m_state, 1, mms::message_type::key_set, mms::message_direction::out, "test content");
  EXPECT_EQ(idx, 0u);
  auto &msgs = m_store->get_all_messages();
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].type, mms::message_type::key_set);
  EXPECT_EQ(msgs[0].direction, mms::message_direction::out);
  EXPECT_EQ(msgs[0].content, "test content");
  EXPECT_EQ(msgs[0].state, mms::message_state::ready_to_send);
}

TEST_F(MessageStoreTest, AddIncomingMessageStateIsWaiting)
{
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::in, "hello");
  EXPECT_EQ(m_store->get_all_messages()[0].state, mms::message_state::waiting);
}

TEST_F(MessageStoreTest, AddAdditionalKeySetSetsRound)
{
  m_state.multisig_rounds_passed = 5;
  m_store->add_message(m_state, 1, mms::message_type::additional_key_set, mms::message_direction::out, "data");
  EXPECT_EQ(m_store->get_all_messages()[0].round, 5u);
}

TEST_F(MessageStoreTest, GetMessageByIdFound)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "msg1");
  mms::message m;
  EXPECT_TRUE(m_store->get_message_by_id(1, m));
  EXPECT_EQ(m.content, "msg1");
}

TEST_F(MessageStoreTest, GetMessageByIdNotFound)
{
  mms::message m;
  EXPECT_FALSE(m_store->get_message_by_id(999, m));
}

TEST_F(MessageStoreTest, GetMessageByIdThrowsForInvalid)
{
  EXPECT_ANY_THROW(m_store->get_message_by_id(999));
}

TEST_F(MessageStoreTest, DeleteMessage)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "msg1");
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::in, "msg2");
  ASSERT_EQ(m_store->get_all_messages().size(), 2u);
  m_store->delete_message(1);
  ASSERT_EQ(m_store->get_all_messages().size(), 1u);
  mms::message m;
  EXPECT_FALSE(m_store->get_message_by_id(1, m));
  EXPECT_TRUE(m_store->get_message_by_id(2, m));
}

TEST_F(MessageStoreTest, DeleteAllMessages)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "a");
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::in, "b");
  m_store->delete_all_messages();
  EXPECT_EQ(m_store->get_all_messages().size(), 0u);
}

TEST_F(MessageStoreTest, MessageIdsIncrement)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "a");
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::in, "b");
  auto &msgs = m_store->get_all_messages();
  EXPECT_EQ(msgs[0].id, 1u);
  EXPECT_EQ(msgs[1].id, 2u);
}

// ---- Set message processed/sent ----

TEST_F(MessageStoreTest, SetMessageProcessedWaiting)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "data");
  m_store->set_message_processed_or_sent(1);
  EXPECT_EQ(m_store->get_all_messages()[0].state, mms::message_state::processed);
}

TEST_F(MessageStoreTest, SetMessageSentReadyToSend)
{
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::out, "data");
  m_store->set_message_processed_or_sent(1);
  EXPECT_EQ(m_store->get_all_messages()[0].state, mms::message_state::sent);
}

TEST_F(MessageStoreTest, SetMessagesProcessedBatch)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "a");
  m_store->add_message(m_state, 1, mms::message_type::note, mms::message_direction::in, "b");
  mms::processing_data pd;
  pd.processing = mms::message_processing::process_sync_data;
  pd.message_ids.push_back(1);
  pd.message_ids.push_back(2);
  m_store->set_messages_processed(pd);
  EXPECT_EQ(m_store->get_all_messages()[0].state, mms::message_state::processed);
  EXPECT_EQ(m_store->get_all_messages()[1].state, mms::message_state::processed);
}

// ---- process_wallet_created_data ----

TEST_F(MessageStoreTest, ProcessWalletCreatedDataKeySet)
{
  m_store->process_wallet_created_data(m_state, mms::message_type::key_set, "my-key-set");
  auto &msgs = m_store->get_all_messages();
  ASSERT_EQ(msgs.size(), 2u); // one for each non-me signer
  EXPECT_EQ(msgs[0].signer_index, 1u);
  EXPECT_EQ(msgs[1].signer_index, 2u);
  EXPECT_EQ(msgs[0].direction, mms::message_direction::out);
}

TEST_F(MessageStoreTest, ProcessWalletCreatedDataFullySignedTx)
{
  m_store->process_wallet_created_data(m_state, mms::message_type::fully_signed_tx, "full-tx");
  auto &msgs = m_store->get_all_messages();
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].type, mms::message_type::fully_signed_tx);
  EXPECT_EQ(msgs[0].signer_index, 0u);
}

TEST_F(MessageStoreTest, ProcessWalletCreatedDataPartiallySigned1of3)
{
  mms::message_store store(std::make_unique<null_http_client>());
  auto state = make_wallet_state();
  store.init(state, "me", "BM-me", 3, 1);
  store.process_wallet_created_data(state, mms::message_type::partially_signed_tx, "tx");
  EXPECT_EQ(store.get_all_messages()[0].type, mms::message_type::fully_signed_tx);
}

TEST_F(MessageStoreTest, ProcessWalletCreatedDataIllegalType)
{
  EXPECT_ANY_THROW(m_store->process_wallet_created_data(m_state, mms::message_type::note, "data"));
}

// ---- Auto-config token ----

TEST_F(MessageStoreTest, AutoConfigTokenRoundTrip)
{
  // Create a valid auto-config token using the same algorithm as create_auto_config_token()
  // (We avoid calling start_auto_config because it tries to connect to Bitmessage.)
  unsigned char random[AUTO_CONFIG_TOKEN_BYTES];
  crypto::rand(AUTO_CONFIG_TOKEN_BYTES, random);
  std::string token_bytes;
  token_bytes.append((char *)random, AUTO_CONFIG_TOKEN_BYTES);
  const crypto::hash &hash = crypto::cn_fast_hash(token_bytes.data(), token_bytes.size());
  token_bytes += hash.data[0];
  std::string token = std::string(AUTO_CONFIG_TOKEN_PREFIX) + epee::string_tools::buff_to_hex_nodelimer(token_bytes);

  std::string adjusted;
  EXPECT_TRUE(m_store->check_auto_config_token(token, adjusted));
  EXPECT_EQ(adjusted, token);
}

TEST_F(MessageStoreTest, AutoConfigTokenInvalid)
{
  std::string adjusted;
  EXPECT_FALSE(m_store->check_auto_config_token("invalid", adjusted));
  EXPECT_FALSE(m_store->check_auto_config_token("", adjusted));
}

TEST_F(MessageStoreTest, StopAutoConfig)
{
  // Manually simulate an auto-config-in-progress state on all signers
  // (We avoid calling start_auto_config because it tries to connect to Bitmessage.)
  // stop_auto_config iterates all signers and clears auto_config fields.
  // It also calls delete_transport_address for non-empty transport addresses,
  // which would throw with our null HTTP client, so we leave transport addresses empty.
  auto &signers = const_cast<std::vector<mms::authorized_signer>&>(m_store->get_all_signers());
  for (auto &s : signers)
  {
    s.auto_config_token = "mms_fake_token";
    s.auto_config_running = true;
    // Leave auto_config_transport_address empty so stop_auto_config won't try to call the transporter
  }
  EXPECT_TRUE(m_store->get_signer(1).auto_config_running);
  m_store->stop_auto_config();
  EXPECT_FALSE(m_store->get_signer(1).auto_config_running);
  EXPECT_TRUE(m_store->get_signer(1).auto_config_token.empty());
}

// ---- Config checksum ----

TEST_F(MessageStoreTest, GetConfigChecksum)
{
  std::string checksum = m_store->get_config_checksum();
  EXPECT_FALSE(checksum.empty());
  EXPECT_EQ(checksum.size(), 8u);
}

TEST_F(MessageStoreTest, GetConfigChecksumDeterministic)
{
  EXPECT_EQ(m_store->get_config_checksum(), m_store->get_config_checksum());
}

TEST_F(MessageStoreTest, GetConfigChecksumChangesWithSigners)
{
  std::string c1 = m_store->get_config_checksum();
  cryptonote::account_base acct;
  acct.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct.get_keys().m_account_address);
  EXPECT_NE(c1, m_store->get_config_checksum());
}

// ---- signer_to_string ----

TEST_F(MessageStoreTest, SignerToString)
{
  std::string s = m_store->signer_to_string(m_store->get_signer(0), 100);
  EXPECT_NE(s.find("me"), std::string::npos);
}

TEST_F(MessageStoreTest, SignerToStringTruncatesLabel)
{
  m_store->set_signer(m_state, 1, std::string("a_very_long_label_that_exceeds"), boost::none, boost::none);
  std::string s = m_store->signer_to_string(m_store->get_signer(1), 10);
  EXPECT_LE(s.size(), 10u);
}

TEST_F(MessageStoreTest, SignerToStringWithTransportAddress)
{
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-12345"), boost::none);
  std::string s = m_store->signer_to_string(m_store->get_signer(1), 100);
  EXPECT_NE(s.find("alice"), std::string::npos);
  EXPECT_NE(s.find("BM-12345"), std::string::npos);
}

// ---- File I/O ----

TEST_F(MessageStoreTest, WriteAndReadFromFile)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "saved message");
  std::string filepath = (m_temp_dir / "test_rw.mms").string();
  m_store->write_to_file(m_state, filepath);

  mms::message_store store2(std::make_unique<null_http_client>());
  store2.init(m_state, "me", "BM-me-address", 3, 2);
  store2.delete_all_messages();
  store2.read_from_file(m_state, filepath);
  EXPECT_GE(store2.get_all_messages().size(), 1u);
}

TEST_F(MessageStoreTest, ReadFromNonExistentFile)
{
  mms::message_store store(std::make_unique<null_http_client>());
  auto state = make_wallet_state();
  store.init(state, "me", "BM-me", 2, 2);
  EXPECT_NO_THROW(store.read_from_file(state, "/tmp/nonexistent_mms_file_xyz.mms"));
}

// ---- get_processable_messages ----

TEST_F(MessageStoreTest, GetProcessableMessagesIncompleteConfig)
{
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_FALSE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_FALSE(wait_reason.empty());
}

TEST_F(MessageStoreTest, GetProcessableMessagesSignerConfig)
{
  m_store->add_message(m_state, 0, mms::message_type::signer_config, mms::message_direction::in, "config");
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  ASSERT_EQ(data_list.size(), 1u);
  EXPECT_EQ(data_list[0].processing, mms::message_processing::process_signer_config);
}

TEST_F(MessageStoreTest, GetProcessableMessagesPrepareMultisig)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_EQ(data_list[0].processing, mms::message_processing::prepare_multisig);
}

TEST_F(MessageStoreTest, GetProcessableMessagesMakeMultisig)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  m_store->add_message(m_state, 1, mms::message_type::key_set, mms::message_direction::out, "my-keys");
  m_store->add_message(m_state, 1, mms::message_type::key_set, mms::message_direction::in, "alice-keys");
  m_store->add_message(m_state, 2, mms::message_type::key_set, mms::message_direction::in, "bob-keys");
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_EQ(data_list[0].processing, mms::message_processing::make_multisig);
}

TEST_F(MessageStoreTest, GetProcessableMessagesFullySignedTx)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  m_state.multisig = true;
  m_state.multisig_is_ready = true;
  m_state.multisig_kex_is_done = true;
  m_store->add_message(m_state, 1, mms::message_type::fully_signed_tx, mms::message_direction::in, "full-tx");
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_EQ(data_list[0].processing, mms::message_processing::submit_tx);
}

TEST_F(MessageStoreTest, GetProcessableMessagesSyncNeeded)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  m_state.multisig = true;
  m_state.multisig_is_ready = true;
  m_state.multisig_kex_is_done = true;
  m_state.has_multisig_partial_key_images = true;
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_EQ(data_list[0].processing, mms::message_processing::create_sync_data);
}

TEST_F(MessageStoreTest, GetProcessableMessagesExchangeMultisigKeys)
{
  cryptonote::account_base acct1, acct2;
  acct1.generate();
  acct2.generate();
  m_store->set_signer(m_state, 1, std::string("alice"), std::string("BM-alice"), acct1.get_keys().m_account_address);
  m_store->set_signer(m_state, 2, std::string("bob"), std::string("BM-bob"), acct2.get_keys().m_account_address);
  m_state.multisig = true;
  m_state.multisig_is_ready = false;
  m_state.multisig_rounds_passed = 1;
  m_store->add_message(m_state, 1, mms::message_type::additional_key_set, mms::message_direction::in, "ak1");
  m_store->add_message(m_state, 2, mms::message_type::additional_key_set, mms::message_direction::in, "ak2");
  std::vector<mms::processing_data> data_list;
  std::string wait_reason;
  EXPECT_TRUE(m_store->get_processable_messages(m_state, false, data_list, wait_reason));
  EXPECT_EQ(data_list[0].processing, mms::message_processing::exchange_multisig_keys);
}

// ---- Set options ----

TEST_F(MessageStoreTest, SetOptionsString)
{
  EXPECT_NO_THROW(m_store->set_options("http://localhost:8442/", epee::wipeable_string("user:pass")));
}

// ---- Re-init clears state ----

TEST_F(MessageStoreTest, ReInitClearsMessages)
{
  m_store->add_message(m_state, 0, mms::message_type::note, mms::message_direction::in, "msg");
  ASSERT_EQ(m_store->get_all_messages().size(), 1u);
  m_store->init(m_state, "me2", "BM-me2", 2, 2);
  EXPECT_EQ(m_store->get_all_messages().size(), 0u);
  EXPECT_EQ(m_store->get_num_authorized_signers(), 2u);
}
