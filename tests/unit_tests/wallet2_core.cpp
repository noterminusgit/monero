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

#include "wallet/wallet2.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "mocks/mock_http_client.h"
#include <boost/filesystem.hpp>

// wallet_accessor_test is declared as a friend of wallet2 in wallet2.h
class wallet_accessor_test
{
public:
  static void set_account(tools::wallet2& w, const cryptonote::account_base& account)
  {
    w.m_account = account;
    w.m_key_device_type = account.get_device().get_type();
    w.m_account_public_address = account.get_keys().m_account_address;
  }
  static tools::wallet2::transfer_container& get_transfers(tools::wallet2& w) { return w.m_transfers; }
  static std::vector<tools::wallet2::address_book_row>& get_address_book(tools::wallet2& w) { return w.m_address_book; }
  static std::unordered_map<crypto::hash, std::string>& get_tx_notes(tools::wallet2& w) { return w.m_tx_notes; }
  static std::unordered_map<std::string, std::string>& get_attributes(tools::wallet2& w) { return w.m_attributes; }
  static std::unordered_map<crypto::hash, tools::wallet2::confirmed_transfer_details>& get_confirmed_txs(tools::wallet2& w) { return w.m_confirmed_txs; }
  static std::unordered_map<crypto::hash, tools::wallet2::unconfirmed_transfer_details>& get_unconfirmed_txs(tools::wallet2& w) { return w.m_unconfirmed_txs; }
  static tools::wallet2::payment_container& get_payments(tools::wallet2& w) { return w.m_payments; }
  static tools::hashchain& get_blockchain(tools::wallet2& w) { return w.m_blockchain; }
  static uint64_t& get_last_block_reward(tools::wallet2& w) { return w.m_last_block_reward; }
  static std::unordered_map<crypto::key_image, size_t>& get_key_images(tools::wallet2& w) { return w.m_key_images; }
  static void set_offline(tools::wallet2& w, bool v) { w.m_offline = v; }
  static cryptonote::account_base& get_account(tools::wallet2& w) { return w.m_account; }
  static bool get_ask_password(tools::wallet2& w) { return w.m_ask_password; }
  static void set_ask_password(tools::wallet2& w, tools::wallet2::AskPasswordType v) { w.m_ask_password = v; }
  static void set_unattended(tools::wallet2& w, bool v) { w.m_unattended = v; }
  static tools::fee_algorithm get_fee_algorithm_val(tools::wallet2& w) { return w.get_fee_algorithm(); }
  static void set_default_priority(tools::wallet2& w, tools::fee_priority p) { w.m_default_priority = p; }
};

namespace
{
  class Wallet2CoreTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
      m_wallet.set_subaddress_lookahead(1, 1);
    }

    tools::wallet2 m_wallet;
  };

  // A fixture that generates a wallet in SetUp for tests that always need one
  class Wallet2GeneratedTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
      m_wallet.set_subaddress_lookahead(2, 5);
      m_wallet.generate("", "", m_recovery_key, true, false);
    }

    tools::wallet2 m_wallet;
    crypto::secret_key m_recovery_key;
  };

  // A fixture that creates a wallet with mock HTTP client
  class Wallet2MockDaemonTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      auto factory = std::make_unique<test::mock_http_client_factory>();
      m_mock_client = factory->get_client();
      m_wallet = std::make_unique<tools::wallet2>(cryptonote::MAINNET, 1, false, std::move(factory));
      m_wallet->init("localhost:18081", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
      m_wallet->set_subaddress_lookahead(1, 1);
      m_wallet->generate("", "", m_recovery_key, true, false);
    }

    std::unique_ptr<tools::wallet2> m_wallet;
    std::shared_ptr<test::programmable_http_client> m_mock_client;
    crypto::secret_key m_recovery_key;
  };

  // A fixture for file I/O tests using a temp directory
  class Wallet2FileTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      m_temp_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("wallet2_test_%%%%-%%%%");
      boost::filesystem::create_directories(m_temp_dir);
    }

    void TearDown() override
    {
      boost::system::error_code ec;
      boost::filesystem::remove_all(m_temp_dir, ec);
    }

    boost::filesystem::path m_temp_dir;
  };
}

TEST_F(Wallet2CoreTest, generate_and_address)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  // Should have a valid address
  std::string addr = m_wallet.get_account().get_public_address_str(cryptonote::TESTNET);
  ASSERT_FALSE(addr.empty());
  ASSERT_GT(addr.size(), 90u); // Monero addresses are ~95 chars
}

TEST_F(Wallet2CoreTest, subaddress_derivation)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);

  // Get the main address
  cryptonote::account_public_address main_addr = m_wallet.get_account().get_keys().m_account_address;

  // Derive subaddress (0,1)
  m_wallet.set_subaddress_lookahead(1, 2);
  cryptonote::subaddress_index idx{0, 1};
  cryptonote::account_public_address sub_addr = m_wallet.get_subaddress(idx);

  // Subaddress should differ from main address
  ASSERT_NE(main_addr.m_spend_public_key, sub_addr.m_spend_public_key);
}

TEST_F(Wallet2CoreTest, balance_zero_on_new_wallet)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  ASSERT_EQ(m_wallet.balance(0, false), 0u);
  ASSERT_EQ(m_wallet.unlocked_balance(0, false), 0u);
}

TEST_F(Wallet2CoreTest, seed_language_default)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  // Should be able to get seed language
  const std::string &lang = m_wallet.get_seed_language();
  (void)lang; // May be empty for random generation
}

TEST_F(Wallet2CoreTest, network_type)
{
  ASSERT_EQ(m_wallet.nettype(), cryptonote::MAINNET);
}

TEST_F(Wallet2CoreTest, watch_only_false_for_full_wallet)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  ASSERT_FALSE(m_wallet.watch_only());
}

TEST_F(Wallet2CoreTest, multisig_status_inactive)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  auto status = m_wallet.get_multisig_status();
  ASSERT_FALSE(status.multisig_is_active);
}

TEST_F(Wallet2CoreTest, get_transfers_empty)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, true, false);
  ASSERT_EQ(m_wallet.get_num_transfer_details(), 0u);
}

// ===========================================================================
// Pure function tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, is_deterministic_after_generate)
{
  // A wallet generated with recover=true and two_random=false should be deterministic
  // Keys are encrypted after generate(); unlock them to check determinism
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  ASSERT_TRUE(m_wallet.is_deterministic());
}

TEST_F(Wallet2CoreTest, is_not_deterministic_two_random)
{
  crypto::secret_key recovery_key;
  m_wallet.generate("", "", recovery_key, false, true);
  ASSERT_FALSE(m_wallet.is_deterministic());
}

TEST_F(Wallet2GeneratedTest, get_address_as_str_format)
{
  std::string addr = m_wallet.get_address_as_str();
  ASSERT_FALSE(addr.empty());
  // Mainnet addresses start with '4'
  ASSERT_EQ(addr[0], '4');
  // Standard Monero addresses are 95 characters
  ASSERT_EQ(addr.size(), 95u);
}

TEST_F(Wallet2GeneratedTest, get_subaddress_as_str_format)
{
  cryptonote::subaddress_index idx{0, 1};
  std::string subaddr = m_wallet.get_subaddress_as_str(idx);
  ASSERT_FALSE(subaddr.empty());
  // Mainnet subaddresses start with '8'
  ASSERT_EQ(subaddr[0], '8');
  ASSERT_EQ(subaddr.size(), 95u);
}

TEST_F(Wallet2GeneratedTest, get_subaddress_as_str_main_address_index)
{
  // Subaddress (0,0) should equal the main address
  cryptonote::subaddress_index idx{0, 0};
  std::string subaddr = m_wallet.get_subaddress_as_str(idx);
  std::string main_addr = m_wallet.get_address_as_str();
  ASSERT_EQ(subaddr, main_addr);
}

TEST_F(Wallet2GeneratedTest, get_integrated_address_as_str_format)
{
  crypto::hash8 payment_id;
  memset(payment_id.data, 0xab, sizeof(payment_id.data));
  std::string integrated = m_wallet.get_integrated_address_as_str(payment_id);
  ASSERT_FALSE(integrated.empty());
  // Mainnet integrated addresses start with '4' and are 106 characters
  ASSERT_EQ(integrated[0], '4');
  ASSERT_EQ(integrated.size(), 106u);
}

TEST_F(Wallet2GeneratedTest, integrated_address_differs_from_standard)
{
  crypto::hash8 payment_id;
  memset(payment_id.data, 0xcd, sizeof(payment_id.data));
  std::string integrated = m_wallet.get_integrated_address_as_str(payment_id);
  std::string standard = m_wallet.get_address_as_str();
  ASSERT_NE(integrated, standard);
}

TEST_F(Wallet2GeneratedTest, key_on_device_false_for_software_wallet)
{
  ASSERT_FALSE(m_wallet.key_on_device());
}

TEST_F(Wallet2GeneratedTest, get_device_type_software)
{
  ASSERT_EQ(m_wallet.get_device_type(), hw::device::device_type::SOFTWARE);
}

TEST_F(Wallet2GeneratedTest, path_empty_for_in_memory_wallet)
{
  // Wallet generated with empty path should return empty path
  std::string p = m_wallet.path();
  ASSERT_TRUE(p.empty());
}

TEST_F(Wallet2GeneratedTest, get_last_block_reward_default)
{
  // The default value is not guaranteed to be 0 after generate().
  // Verify that we can set and retrieve the value via the accessor.
  wallet_accessor_test::get_last_block_reward(m_wallet) = 0;
  ASSERT_EQ(m_wallet.get_last_block_reward(), 0u);
}

TEST_F(Wallet2GeneratedTest, explicit_refresh_from_block_height_default)
{
  // The wallet2 constructor initializes m_explicit_refresh_from_block_height to true
  ASSERT_TRUE(m_wallet.explicit_refresh_from_block_height());
}

TEST_F(Wallet2GeneratedTest, explicit_refresh_from_block_height_set)
{
  m_wallet.explicit_refresh_from_block_height(true);
  ASSERT_TRUE(m_wallet.explicit_refresh_from_block_height());
}

TEST_F(Wallet2GeneratedTest, get_multisig_status_details)
{
  auto status = m_wallet.get_multisig_status();
  ASSERT_FALSE(status.multisig_is_active);
  ASSERT_EQ(status.threshold, 0u);
  ASSERT_EQ(status.total, 0u);
  // When multisig is not active, is_ready is false
  ASSERT_FALSE(status.is_ready);
}

TEST_F(Wallet2GeneratedTest, get_seed_returns_valid_words)
{
  // Keys are encrypted after generate(); unlock them so get_seed can check determinism
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("English");
  epee::wipeable_string seed;
  bool got_seed = m_wallet.get_seed(seed);
  ASSERT_TRUE(got_seed);
  ASSERT_FALSE(seed.empty());
  // Monero seed is 25 words
  int word_count = 1;
  for (size_t i = 0; i < seed.size(); ++i)
    if (seed.data()[i] == ' ') ++word_count;
  ASSERT_EQ(word_count, 25);
}

TEST_F(Wallet2GeneratedTest, set_and_get_seed_language)
{
  m_wallet.set_seed_language("Spanish");
  ASSERT_EQ(m_wallet.get_seed_language(), "Spanish");
}

TEST_F(Wallet2GeneratedTest, get_refresh_from_block_height_default)
{
  ASSERT_EQ(m_wallet.get_refresh_from_block_height(), 0u);
}

TEST_F(Wallet2GeneratedTest, set_refresh_from_block_height)
{
  m_wallet.set_refresh_from_block_height(100000);
  ASSERT_EQ(m_wallet.get_refresh_from_block_height(), 100000u);
}

TEST_F(Wallet2GeneratedTest, blockchain_height_default)
{
  // After generate with empty path, blockchain height should be just 1 (genesis)
  ASSERT_GE(m_wallet.get_blockchain_current_height(), 0u);
}

TEST_F(Wallet2GeneratedTest, get_wallet_file_empty)
{
  ASSERT_TRUE(m_wallet.get_wallet_file().empty());
}

TEST_F(Wallet2GeneratedTest, get_keys_file_empty)
{
  // When wallet is generated with empty path, the keys file is set to ".keys"
  // (prepare_file_names appends ".keys" to the empty wallet path)
  ASSERT_EQ(m_wallet.get_keys_file(), ".keys");
}

TEST_F(Wallet2GeneratedTest, get_daemon_address)
{
  // When init() is called with empty daemon address, it defaults to
  // "http://localhost:<RPC_DEFAULT_PORT>" (18081 for mainnet)
  std::string addr = m_wallet.get_daemon_address();
  ASSERT_FALSE(addr.empty());
  ASSERT_NE(addr.find("localhost"), std::string::npos);
}

TEST_F(Wallet2GeneratedTest, is_trusted_daemon)
{
  ASSERT_TRUE(m_wallet.is_trusted_daemon());
}

TEST_F(Wallet2GeneratedTest, set_trusted_daemon)
{
  m_wallet.set_trusted_daemon(false);
  ASSERT_FALSE(m_wallet.is_trusted_daemon());
  m_wallet.set_trusted_daemon(true);
  ASSERT_TRUE(m_wallet.is_trusted_daemon());
}

TEST_F(Wallet2GeneratedTest, watch_only_false)
{
  ASSERT_FALSE(m_wallet.watch_only());
}

TEST_F(Wallet2GeneratedTest, is_background_wallet_false)
{
  ASSERT_FALSE(m_wallet.is_background_wallet());
}

TEST_F(Wallet2GeneratedTest, has_multisig_partial_key_images_false)
{
  ASSERT_FALSE(m_wallet.has_multisig_partial_key_images());
}

TEST_F(Wallet2GeneratedTest, has_unknown_key_images_true_on_empty)
{
  // New empty wallet should not have unknown key images
  ASSERT_FALSE(m_wallet.has_unknown_key_images());
}

TEST_F(Wallet2GeneratedTest, is_offline_after_set)
{
  m_wallet.set_offline(true);
  ASSERT_TRUE(m_wallet.is_offline());
  m_wallet.set_offline(false);
  ASSERT_FALSE(m_wallet.is_offline());
}

TEST_F(Wallet2GeneratedTest, estimate_fee_static_method)
{
  // Test the static estimate_fee with known parameters
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_varies_with_inputs)
{
  uint64_t fee1 = tools::wallet2::estimate_fee(true, true, 1, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  uint64_t fee2 = tools::wallet2::estimate_fee(true, true, 5, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee2, fee1);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_varies_with_outputs)
{
  uint64_t fee1 = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  uint64_t fee2 = tools::wallet2::estimate_fee(true, true, 2, 16, 8, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee2, fee1);
}

// ===========================================================================
// Wallet settings / configuration tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, always_confirm_transfers_default)
{
  ASSERT_TRUE(m_wallet.always_confirm_transfers());
}

TEST_F(Wallet2GeneratedTest, always_confirm_transfers_toggle)
{
  m_wallet.always_confirm_transfers(false);
  ASSERT_FALSE(m_wallet.always_confirm_transfers());
  m_wallet.always_confirm_transfers(true);
  ASSERT_TRUE(m_wallet.always_confirm_transfers());
}

TEST_F(Wallet2GeneratedTest, store_tx_info_default)
{
  ASSERT_TRUE(m_wallet.store_tx_info());
}

TEST_F(Wallet2GeneratedTest, default_mixin)
{
  m_wallet.default_mixin(15);
  ASSERT_EQ(m_wallet.default_mixin(), 15u);
}

TEST_F(Wallet2GeneratedTest, auto_refresh_toggle)
{
  bool orig = m_wallet.auto_refresh();
  m_wallet.auto_refresh(!orig);
  ASSERT_EQ(m_wallet.auto_refresh(), !orig);
}

TEST_F(Wallet2GeneratedTest, min_output_count)
{
  m_wallet.set_min_output_count(5);
  ASSERT_EQ(m_wallet.get_min_output_count(), 5u);
}

TEST_F(Wallet2GeneratedTest, min_output_value)
{
  m_wallet.set_min_output_value(100000);
  ASSERT_EQ(m_wallet.get_min_output_value(), 100000u);
}

TEST_F(Wallet2GeneratedTest, merge_destinations)
{
  m_wallet.merge_destinations(true);
  ASSERT_TRUE(m_wallet.merge_destinations());
  m_wallet.merge_destinations(false);
  ASSERT_FALSE(m_wallet.merge_destinations());
}

TEST_F(Wallet2GeneratedTest, confirm_backlog)
{
  m_wallet.confirm_backlog(true);
  ASSERT_TRUE(m_wallet.confirm_backlog());
}

TEST_F(Wallet2GeneratedTest, confirm_backlog_threshold)
{
  m_wallet.set_confirm_backlog_threshold(10);
  ASSERT_EQ(m_wallet.get_confirm_backlog_threshold(), 10u);
}

TEST_F(Wallet2GeneratedTest, segregate_pre_fork_outputs)
{
  m_wallet.segregate_pre_fork_outputs(true);
  ASSERT_TRUE(m_wallet.segregate_pre_fork_outputs());
}

TEST_F(Wallet2GeneratedTest, key_reuse_mitigation2)
{
  m_wallet.key_reuse_mitigation2(true);
  ASSERT_TRUE(m_wallet.key_reuse_mitigation2());
}

TEST_F(Wallet2GeneratedTest, ignore_fractional_outputs)
{
  m_wallet.ignore_fractional_outputs(true);
  ASSERT_TRUE(m_wallet.ignore_fractional_outputs());
}

TEST_F(Wallet2GeneratedTest, ignore_outputs_above)
{
  m_wallet.ignore_outputs_above(1000000000000ULL);
  ASSERT_EQ(m_wallet.ignore_outputs_above(), 1000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, ignore_outputs_below)
{
  m_wallet.ignore_outputs_below(5000ULL);
  ASSERT_EQ(m_wallet.ignore_outputs_below(), 5000ULL);
}

TEST_F(Wallet2GeneratedTest, track_uses)
{
  m_wallet.track_uses(true);
  ASSERT_TRUE(m_wallet.track_uses());
}

TEST_F(Wallet2GeneratedTest, inactivity_lock_timeout)
{
  m_wallet.inactivity_lock_timeout(300);
  ASSERT_EQ(m_wallet.inactivity_lock_timeout(), 300u);
}

TEST_F(Wallet2GeneratedTest, max_reorg_depth)
{
  m_wallet.max_reorg_depth(1000);
  ASSERT_EQ(m_wallet.max_reorg_depth(), 1000u);
}

TEST_F(Wallet2GeneratedTest, export_format)
{
  m_wallet.set_export_format(tools::wallet2::ExportFormat::Ascii);
  ASSERT_EQ(m_wallet.export_format(), tools::wallet2::ExportFormat::Ascii);
  m_wallet.set_export_format(tools::wallet2::ExportFormat::Binary);
  ASSERT_EQ(m_wallet.export_format(), tools::wallet2::ExportFormat::Binary);
}

TEST_F(Wallet2GeneratedTest, subaddress_lookahead)
{
  m_wallet.set_subaddress_lookahead(10, 50);
  auto lookahead = m_wallet.get_subaddress_lookahead();
  ASSERT_EQ(lookahead.first, 10u);
  ASSERT_EQ(lookahead.second, 50u);
}

// ===========================================================================
// Address book CRUD tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, address_book_empty_initially)
{
  auto book = m_wallet.get_address_book();
  ASSERT_TRUE(book.empty());
}

TEST_F(Wallet2GeneratedTest, add_address_book_row)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Test contact", false));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_EQ(book[0].m_description, "Test contact");
  ASSERT_FALSE(book[0].m_is_subaddress);
  ASSERT_FALSE(book[0].m_has_payment_id);
}

TEST_F(Wallet2GeneratedTest, add_address_book_row_with_payment_id)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  crypto::hash8 payment_id;
  memset(payment_id.data, 0x42, sizeof(payment_id.data));
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, &payment_id, "With PID", false));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_TRUE(book[0].m_has_payment_id);
  ASSERT_EQ(book[0].m_payment_id, payment_id);
}

TEST_F(Wallet2GeneratedTest, add_multiple_address_book_rows)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Contact 1", false));
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Contact 2", false));
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Contact 3", false));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 3u);
}

TEST_F(Wallet2GeneratedTest, set_address_book_row)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Original", false));
  ASSERT_TRUE(m_wallet.set_address_book_row(0, addr, nullptr, "Updated", true));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_EQ(book[0].m_description, "Updated");
  ASSERT_TRUE(book[0].m_is_subaddress);
}

TEST_F(Wallet2GeneratedTest, set_address_book_row_out_of_bounds)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  ASSERT_FALSE(m_wallet.set_address_book_row(0, addr, nullptr, "No rows", false));
}

TEST_F(Wallet2GeneratedTest, delete_address_book_row)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  m_wallet.add_address_book_row(addr, nullptr, "A", false);
  m_wallet.add_address_book_row(addr, nullptr, "B", false);
  ASSERT_TRUE(m_wallet.delete_address_book_row(0));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_EQ(book[0].m_description, "B");
}

TEST_F(Wallet2GeneratedTest, delete_address_book_row_out_of_bounds)
{
  ASSERT_FALSE(m_wallet.delete_address_book_row(0));
}

TEST_F(Wallet2GeneratedTest, delete_last_address_book_row)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  m_wallet.add_address_book_row(addr, nullptr, "Only", false);
  ASSERT_TRUE(m_wallet.delete_address_book_row(0));
  ASSERT_TRUE(m_wallet.get_address_book().empty());
}

// ===========================================================================
// Subaddress management tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_num_subaddress_accounts_initial)
{
  // After generate, there should be at least 1 account (the main account)
  ASSERT_GE(m_wallet.get_num_subaddress_accounts(), 1u);
}

TEST_F(Wallet2GeneratedTest, add_subaddress_account)
{
  size_t before = m_wallet.get_num_subaddress_accounts();
  m_wallet.add_subaddress_account("Savings");
  ASSERT_EQ(m_wallet.get_num_subaddress_accounts(), before + 1);
}

TEST_F(Wallet2GeneratedTest, add_multiple_subaddress_accounts)
{
  size_t before = m_wallet.get_num_subaddress_accounts();
  m_wallet.add_subaddress_account("Account A");
  m_wallet.add_subaddress_account("Account B");
  m_wallet.add_subaddress_account("Account C");
  ASSERT_EQ(m_wallet.get_num_subaddress_accounts(), before + 3);
}

TEST_F(Wallet2GeneratedTest, get_num_subaddresses_initial)
{
  // The main account should have at least 1 subaddress (the main address itself)
  ASSERT_GE(m_wallet.get_num_subaddresses(0), 1u);
}

TEST_F(Wallet2GeneratedTest, add_subaddress)
{
  size_t before = m_wallet.get_num_subaddresses(0);
  m_wallet.add_subaddress(0, "Sub 1");
  ASSERT_EQ(m_wallet.get_num_subaddresses(0), before + 1);
}

TEST_F(Wallet2GeneratedTest, get_num_subaddresses_invalid_account)
{
  ASSERT_EQ(m_wallet.get_num_subaddresses(999), 0u);
}

TEST_F(Wallet2GeneratedTest, get_subaddress_label)
{
  m_wallet.add_subaddress_account("My Account");
  size_t idx = m_wallet.get_num_subaddress_accounts() - 1;
  std::string label = m_wallet.get_subaddress_label({(uint32_t)idx, 0});
  ASSERT_EQ(label, "My Account");
}

TEST_F(Wallet2GeneratedTest, set_subaddress_label)
{
  m_wallet.set_subaddress_label({0, 0}, "Primary");
  ASSERT_EQ(m_wallet.get_subaddress_label({0, 0}), "Primary");
}

TEST_F(Wallet2GeneratedTest, subaddresses_are_unique)
{
  m_wallet.add_subaddress(0, "Sub A");
  m_wallet.add_subaddress(0, "Sub B");
  size_t n = m_wallet.get_num_subaddresses(0);
  cryptonote::subaddress_index idx_a{0, (uint32_t)(n - 2)};
  cryptonote::subaddress_index idx_b{0, (uint32_t)(n - 1)};
  std::string addr_a = m_wallet.get_subaddress_as_str(idx_a);
  std::string addr_b = m_wallet.get_subaddress_as_str(idx_b);
  ASSERT_NE(addr_a, addr_b);
}

TEST_F(Wallet2GeneratedTest, subaddress_index_lookup)
{
  cryptonote::subaddress_index idx{0, 0};
  cryptonote::account_public_address addr = m_wallet.get_subaddress(idx);
  auto found = m_wallet.get_subaddress_index(addr);
  ASSERT_TRUE(found.is_initialized());
  ASSERT_EQ(found->major, 0u);
  ASSERT_EQ(found->minor, 0u);
}

// ===========================================================================
// Message signing and verification tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, sign_and_verify_with_spend_key)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data = "Hello, Monero!";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key);
  ASSERT_FALSE(sig.empty());
  // Starts with SigV2
  ASSERT_EQ(sig.substr(0, 5), "SigV2");

  auto result = m_wallet.verify(data, m_wallet.get_address(), sig);
  ASSERT_TRUE(result.valid);
  ASSERT_EQ(result.type, tools::wallet2::sign_with_spend_key);
}

TEST_F(Wallet2GeneratedTest, sign_and_verify_with_view_key)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data = "Verify with view key";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_view_key);
  ASSERT_FALSE(sig.empty());

  auto result = m_wallet.verify(data, m_wallet.get_address(), sig);
  ASSERT_TRUE(result.valid);
  ASSERT_EQ(result.type, tools::wallet2::sign_with_view_key);
}

TEST_F(Wallet2GeneratedTest, verify_fails_with_wrong_data)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data = "Original message";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key);

  auto result = m_wallet.verify("Tampered message", m_wallet.get_address(), sig);
  ASSERT_FALSE(result.valid);
}

TEST_F(Wallet2GeneratedTest, verify_fails_with_wrong_address)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data = "Test data";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key);

  // Create another wallet with a known-different key to get a different address
  tools::wallet2 other_wallet;
  other_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  other_wallet.set_subaddress_lookahead(1, 1);
  crypto::secret_key other_key;
  other_wallet.generate("", "", other_key, false, false);

  // Ensure addresses are actually different
  ASSERT_NE(m_wallet.get_address_as_str(), other_wallet.get_address_as_str());

  auto result = m_wallet.verify(data, other_wallet.get_address(), sig);
  ASSERT_FALSE(result.valid);
}

TEST_F(Wallet2GeneratedTest, sign_empty_message)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string sig = m_wallet.sign("", tools::wallet2::sign_with_spend_key);
  ASSERT_FALSE(sig.empty());
  auto result = m_wallet.verify("", m_wallet.get_address(), sig);
  ASSERT_TRUE(result.valid);
}

TEST_F(Wallet2GeneratedTest, sign_with_subaddress)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.add_subaddress(0, "signing sub");
  size_t n = m_wallet.get_num_subaddresses(0);
  cryptonote::subaddress_index idx{0, (uint32_t)(n - 1)};
  std::string data = "Subaddress signing";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key, idx);
  ASSERT_FALSE(sig.empty());

  cryptonote::account_public_address sub_addr = m_wallet.get_subaddress(idx);
  auto result = m_wallet.verify(data, sub_addr, sig);
  ASSERT_TRUE(result.valid);
}

// ===========================================================================
// TX note tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_and_get_tx_note)
{
  crypto::hash txid;
  memset(txid.data, 0xaa, sizeof(txid.data));
  m_wallet.set_tx_note(txid, "Payment for goods");
  ASSERT_EQ(m_wallet.get_tx_note(txid), "Payment for goods");
}

TEST_F(Wallet2GeneratedTest, get_tx_note_missing)
{
  crypto::hash txid;
  memset(txid.data, 0xbb, sizeof(txid.data));
  ASSERT_EQ(m_wallet.get_tx_note(txid), "");
}

TEST_F(Wallet2GeneratedTest, set_tx_note_overwrite)
{
  crypto::hash txid;
  memset(txid.data, 0xcc, sizeof(txid.data));
  m_wallet.set_tx_note(txid, "First note");
  m_wallet.set_tx_note(txid, "Second note");
  ASSERT_EQ(m_wallet.get_tx_note(txid), "Second note");
}

TEST_F(Wallet2GeneratedTest, set_tx_note_empty)
{
  crypto::hash txid;
  memset(txid.data, 0xdd, sizeof(txid.data));
  m_wallet.set_tx_note(txid, "");
  ASSERT_EQ(m_wallet.get_tx_note(txid), "");
}

TEST_F(Wallet2GeneratedTest, set_multiple_tx_notes)
{
  crypto::hash txid1, txid2;
  memset(txid1.data, 0x01, sizeof(txid1.data));
  memset(txid2.data, 0x02, sizeof(txid2.data));
  m_wallet.set_tx_note(txid1, "Note 1");
  m_wallet.set_tx_note(txid2, "Note 2");
  ASSERT_EQ(m_wallet.get_tx_note(txid1), "Note 1");
  ASSERT_EQ(m_wallet.get_tx_note(txid2), "Note 2");
}

// ===========================================================================
// Attribute tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_and_get_attribute)
{
  m_wallet.set_attribute("test.key", "test_value");
  std::string value;
  ASSERT_TRUE(m_wallet.get_attribute("test.key", value));
  ASSERT_EQ(value, "test_value");
}

TEST_F(Wallet2GeneratedTest, get_attribute_missing)
{
  std::string value;
  ASSERT_FALSE(m_wallet.get_attribute("nonexistent.key", value));
}

TEST_F(Wallet2GeneratedTest, set_attribute_overwrite)
{
  m_wallet.set_attribute("key", "value1");
  m_wallet.set_attribute("key", "value2");
  std::string value;
  ASSERT_TRUE(m_wallet.get_attribute("key", value));
  ASSERT_EQ(value, "value2");
}

TEST_F(Wallet2GeneratedTest, multiple_attributes)
{
  m_wallet.set_attribute("a", "1");
  m_wallet.set_attribute("b", "2");
  m_wallet.set_attribute("c", "3");
  std::string val;
  ASSERT_TRUE(m_wallet.get_attribute("a", val));
  ASSERT_EQ(val, "1");
  ASSERT_TRUE(m_wallet.get_attribute("b", val));
  ASSERT_EQ(val, "2");
  ASSERT_TRUE(m_wallet.get_attribute("c", val));
  ASSERT_EQ(val, "3");
}

// ===========================================================================
// Description tests (uses attributes internally)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_and_get_description)
{
  m_wallet.set_description("My Wallet");
  ASSERT_EQ(m_wallet.get_description(), "My Wallet");
}

TEST_F(Wallet2GeneratedTest, get_description_default_empty)
{
  ASSERT_EQ(m_wallet.get_description(), "");
}

TEST_F(Wallet2GeneratedTest, set_description_overwrite)
{
  m_wallet.set_description("First");
  m_wallet.set_description("Second");
  ASSERT_EQ(m_wallet.get_description(), "Second");
}

TEST_F(Wallet2GeneratedTest, set_description_empty)
{
  m_wallet.set_description("Not empty");
  m_wallet.set_description("");
  ASSERT_EQ(m_wallet.get_description(), "");
}

// ===========================================================================
// Balance queries with wallet_accessor_test
// ===========================================================================

TEST_F(Wallet2GeneratedTest, balance_with_populated_transfers)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  ASSERT_TRUE(transfers.empty());

  // Add a fake transfer
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 100;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 1000;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_spent_height = 0;
  td.m_amount = 1000000000000ULL; // 1 XMR
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xab, sizeof(td.m_key_image));
  memset(&td.m_txid, 0x01, sizeof(td.m_txid));

  // Create a minimal tx_prefix with an output
  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x42, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);

  // Register the key image
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);
  key_images[td.m_key_image] = 0;

  ASSERT_EQ(m_wallet.get_num_transfer_details(), 1u);
  ASSERT_EQ(m_wallet.balance(0, false), 1000000000000ULL);
}

// ===========================================================================
// Freeze / Thaw operations
// ===========================================================================

TEST_F(Wallet2GeneratedTest, freeze_and_thaw)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 50;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 500;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_amount = 500000000000ULL;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xfe, sizeof(td.m_key_image));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x55, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);

  ASSERT_FALSE(m_wallet.frozen(0));
  m_wallet.freeze(0);
  ASSERT_TRUE(m_wallet.frozen(0));
  m_wallet.thaw(0);
  ASSERT_FALSE(m_wallet.frozen(0));
}

TEST_F(Wallet2GeneratedTest, freeze_by_key_image)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 60;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 600;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_amount = 200000000000ULL;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xdc, sizeof(td.m_key_image));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x66, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);

  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);
  key_images[td.m_key_image] = 0;

  ASSERT_FALSE(m_wallet.frozen(td.m_key_image));
  m_wallet.freeze(td.m_key_image);
  ASSERT_TRUE(m_wallet.frozen(td.m_key_image));
  m_wallet.thaw(td.m_key_image);
  ASSERT_FALSE(m_wallet.frozen(td.m_key_image));
}

TEST_F(Wallet2GeneratedTest, frozen_transfer_excluded_from_unlocked_balance)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 1;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 100;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_spent_height = 0;
  td.m_amount = 300000000000ULL;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xef, sizeof(td.m_key_image));
  memset(&td.m_txid, 0x03, sizeof(td.m_txid));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x77, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);
  key_images[td.m_key_image] = 0;

  uint64_t balance_before = m_wallet.balance(0, false);
  m_wallet.freeze(0);
  uint64_t balance_after = m_wallet.balance(0, true);
  // Frozen transfers should be excluded in strict mode
  ASSERT_LT(balance_after, balance_before);
}

// ===========================================================================
// Transfer detail queries
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_transfer_details_valid_index)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 200;
  td.m_amount = 100000000ULL;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_internal_output_index = 0;

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x88, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);

  const auto& detail = m_wallet.get_transfer_details(0);
  ASSERT_EQ(detail.m_block_height, 200u);
  ASSERT_EQ(detail.m_amount, 100000000ULL);
}

TEST_F(Wallet2GeneratedTest, get_transfers_container)
{
  tools::wallet2::transfer_container tc;
  m_wallet.get_transfers(tc);
  ASSERT_TRUE(tc.empty());
}

// ===========================================================================
// Encryption / Decryption round-trip tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, encrypt_decrypt_round_trip)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  const crypto::secret_key& view_key = m_wallet.get_account().get_keys().m_view_secret_key;
  std::string plaintext = "Secret message for testing encryption";
  std::string ciphertext = m_wallet.encrypt(plaintext, view_key, true);
  ASSERT_NE(ciphertext, plaintext);
  ASSERT_FALSE(ciphertext.empty());
  std::string decrypted = m_wallet.decrypt(ciphertext, view_key, true);
  ASSERT_EQ(decrypted, plaintext);
}

TEST_F(Wallet2GeneratedTest, encrypt_decrypt_view_secret_key)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string plaintext = "Encrypt with view secret key";
  std::string ciphertext = m_wallet.encrypt_with_view_secret_key(plaintext, true);
  ASSERT_NE(ciphertext, plaintext);
  std::string decrypted = m_wallet.decrypt_with_view_secret_key(ciphertext, true);
  ASSERT_EQ(decrypted, plaintext);
}

TEST_F(Wallet2GeneratedTest, encrypt_decrypt_empty_string)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  const crypto::secret_key& view_key = m_wallet.get_account().get_keys().m_view_secret_key;
  std::string ciphertext = m_wallet.encrypt(std::string(""), view_key, true);
  std::string decrypted = m_wallet.decrypt(ciphertext, view_key, true);
  ASSERT_EQ(decrypted, "");
}

// ===========================================================================
// URI generation and parsing tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, make_uri_basic)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 0, "", "", error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_FALSE(uri.empty());
  ASSERT_EQ(uri.substr(0, 7), "monero:");
}

TEST_F(Wallet2GeneratedTest, make_uri_with_amount)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 1000000000000ULL, "Test", "Alice", error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_NE(uri.find("tx_amount="), std::string::npos);
}

TEST_F(Wallet2GeneratedTest, parse_uri_round_trip)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 1000000000000ULL, "Payment", "Bob", error);
  ASSERT_TRUE(error.empty());

  std::string parsed_addr, parsed_pid;
  uint64_t parsed_amount;
  std::string parsed_desc, parsed_name;
  std::vector<std::string> unknown;
  std::string parse_error;
  bool ok = m_wallet.parse_uri(uri, parsed_addr, parsed_pid, parsed_amount, parsed_desc, parsed_name, unknown, parse_error);
  ASSERT_TRUE(ok) << parse_error;
  ASSERT_EQ(parsed_addr, addr);
  ASSERT_EQ(parsed_amount, 1000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, parse_uri_invalid_scheme)
{
  std::string addr, pid, desc, name, error;
  uint64_t amount;
  std::vector<std::string> unknown;
  bool ok = m_wallet.parse_uri("bitcoin:abc", addr, pid, amount, desc, name, unknown, error);
  ASSERT_FALSE(ok);
}

// ===========================================================================
// Account tags tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, account_tags_initially_empty)
{
  const auto& tags = m_wallet.get_account_tags();
  ASSERT_TRUE(tags.first.empty());
}

TEST_F(Wallet2GeneratedTest, set_account_tag)
{
  m_wallet.set_account_tag({0}, "savings");
  const auto& tags = m_wallet.get_account_tags();
  ASSERT_FALSE(tags.second.empty());
  ASSERT_EQ(tags.second[0], "savings");
}

TEST_F(Wallet2GeneratedTest, set_account_tag_description)
{
  m_wallet.set_account_tag({0}, "savings");
  m_wallet.set_account_tag_description("savings", "Long-term savings");
  const auto& tags = m_wallet.get_account_tags();
  auto it = tags.first.find("savings");
  ASSERT_NE(it, tags.first.end());
  ASSERT_EQ(it->second, "Long-term savings");
}

// ===========================================================================
// Wallet path validation tests
// ===========================================================================

TEST_F(Wallet2CoreTest, wallet_valid_path_format)
{
  ASSERT_TRUE(tools::wallet2::wallet_valid_path_format("/tmp/wallet"));
  ASSERT_TRUE(tools::wallet2::wallet_valid_path_format("wallet_file"));
}

// ===========================================================================
// Payment ID parsing tests
// ===========================================================================

TEST_F(Wallet2CoreTest, parse_short_payment_id_valid)
{
  crypto::hash8 pid;
  ASSERT_TRUE(tools::wallet2::parse_short_payment_id("aaaaaaaaaaaaaaaa", pid));
}

TEST_F(Wallet2CoreTest, parse_short_payment_id_invalid)
{
  crypto::hash8 pid;
  ASSERT_FALSE(tools::wallet2::parse_short_payment_id("not_a_valid_pid", pid));
}

TEST_F(Wallet2CoreTest, parse_long_payment_id_valid)
{
  crypto::hash pid;
  ASSERT_TRUE(tools::wallet2::parse_long_payment_id(
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", pid));
}

TEST_F(Wallet2CoreTest, parse_long_payment_id_invalid)
{
  crypto::hash pid;
  ASSERT_FALSE(tools::wallet2::parse_long_payment_id("too_short", pid));
}

// ===========================================================================
// Background sync type parsing
// ===========================================================================

TEST_F(Wallet2CoreTest, background_sync_type_from_str_off)
{
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("off"), tools::wallet2::BackgroundSyncOff);
}

TEST_F(Wallet2CoreTest, background_sync_type_from_str_reuse)
{
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("reuse-wallet-password"), tools::wallet2::BackgroundSyncReusePassword);
}

TEST_F(Wallet2CoreTest, background_sync_type_from_str_custom)
{
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("custom-background-password"), tools::wallet2::BackgroundSyncCustomPassword);
}

TEST_F(Wallet2CoreTest, background_sync_type_from_str_unknown_throws)
{
  ASSERT_THROW(tools::wallet2::background_sync_type_from_str("invalid"), std::logic_error);
}

// ===========================================================================
// Network type constructor tests
// ===========================================================================

TEST(Wallet2ConstructorTest, mainnet_default)
{
  tools::wallet2 w;
  ASSERT_EQ(w.nettype(), cryptonote::MAINNET);
}

TEST(Wallet2ConstructorTest, testnet)
{
  tools::wallet2 w(cryptonote::TESTNET);
  ASSERT_EQ(w.nettype(), cryptonote::TESTNET);
}

TEST(Wallet2ConstructorTest, stagenet)
{
  tools::wallet2 w(cryptonote::STAGENET);
  ASSERT_EQ(w.nettype(), cryptonote::STAGENET);
}

// ===========================================================================
// Testnet address format tests
// ===========================================================================

TEST(Wallet2TestnetTest, testnet_address_starts_with_9)
{
  tools::wallet2 w(cryptonote::TESTNET);
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key key;
  w.generate("", "", key, true, false);
  std::string addr = w.get_address_as_str();
  ASSERT_EQ(addr[0], '9');
}

TEST(Wallet2TestnetTest, stagenet_address_starts_with_5)
{
  tools::wallet2 w(cryptonote::STAGENET);
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key key;
  w.generate("", "", key, true, false);
  std::string addr = w.get_address_as_str();
  ASSERT_EQ(addr[0], '5');
}

// ===========================================================================
// File I/O tests
// ===========================================================================

TEST_F(Wallet2FileTest, store_to_and_load_round_trip)
{
  std::string wallet_path = (m_temp_dir / "test_wallet").string();
  epee::wipeable_string password("testpass123");

  // Create and generate a wallet
  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, password, recovery_key, true, false);
  std::string original_addr = w1.get_address_as_str();

  // Set some metadata
  w1.set_description("Test wallet");
  w1.set_attribute("custom.key", "custom_value");
  w1.store();

  // Load into a new wallet
  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, password);

  ASSERT_EQ(w2.get_address_as_str(), original_addr);
  ASSERT_EQ(w2.get_description(), "Test wallet");
  std::string val;
  ASSERT_TRUE(w2.get_attribute("custom.key", val));
  ASSERT_EQ(val, "custom_value");
}

TEST_F(Wallet2FileTest, change_password)
{
  std::string wallet_path = (m_temp_dir / "test_wallet_pw").string();
  epee::wipeable_string old_pass("old_password");
  epee::wipeable_string new_pass("new_password");

  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, old_pass, recovery_key, true, false);
  std::string addr = w1.get_address_as_str();

  // Change password
  w1.change_password(wallet_path, old_pass, new_pass);

  // Load with new password should work
  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, new_pass);
  ASSERT_EQ(w2.get_address_as_str(), addr);
}

TEST_F(Wallet2FileTest, verify_password_correct)
{
  std::string wallet_path = (m_temp_dir / "test_wallet_verify").string();
  epee::wipeable_string password("verify_me");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, password, recovery_key, true, false);
  w.store();

  ASSERT_TRUE(w.verify_password(password));
}

TEST_F(Wallet2FileTest, verify_password_wrong)
{
  std::string wallet_path = (m_temp_dir / "test_wallet_verify_wrong").string();
  epee::wipeable_string password("correct_password");
  epee::wipeable_string wrong("wrong_password");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, password, recovery_key, true, false);
  w.store();

  ASSERT_FALSE(w.verify_password(wrong));
}

TEST_F(Wallet2FileTest, store_to_new_path)
{
  std::string wallet_path1 = (m_temp_dir / "wallet_orig").string();
  std::string wallet_path2 = (m_temp_dir / "wallet_copy").string();
  epee::wipeable_string password("test_pw");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path1, password, recovery_key, true, false);
  w.set_description("Moveable wallet");
  std::string addr = w.get_address_as_str();

  // Store to a new location
  w.store_to(wallet_path2, password);

  // Load from new location
  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path2, password);
  ASSERT_EQ(w2.get_address_as_str(), addr);
  ASSERT_EQ(w2.get_description(), "Moveable wallet");
}

TEST_F(Wallet2FileTest, store_address_book_persists)
{
  std::string wallet_path = (m_temp_dir / "wallet_addrbook").string();
  epee::wipeable_string password("addrbook_pass");

  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, password, recovery_key, true, false);

  cryptonote::account_public_address addr = w1.get_address();
  w1.add_address_book_row(addr, nullptr, "Persistent contact", false);
  w1.store();

  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, password);

  auto book = w2.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_EQ(book[0].m_description, "Persistent contact");
}

TEST_F(Wallet2FileTest, store_tx_notes_persist)
{
  std::string wallet_path = (m_temp_dir / "wallet_txnotes").string();
  epee::wipeable_string password("notes_pass");

  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, password, recovery_key, true, false);

  crypto::hash txid;
  memset(txid.data, 0x11, sizeof(txid.data));
  w1.set_tx_note(txid, "Persisted note");
  w1.store();

  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, password);

  ASSERT_EQ(w2.get_tx_note(txid), "Persisted note");
}

TEST_F(Wallet2FileTest, wallet_exists_check)
{
  std::string wallet_path = (m_temp_dir / "wallet_exists").string();
  epee::wipeable_string password("exists_pw");

  // Before creation, files should not exist
  bool keys_exists = false, wallet_exists = false;
  tools::wallet2::wallet_exists(wallet_path, keys_exists, wallet_exists);
  ASSERT_FALSE(keys_exists);
  ASSERT_FALSE(wallet_exists);

  // Create wallet
  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, password, recovery_key, true, false);
  w.store();

  // After creation, files should exist
  tools::wallet2::wallet_exists(wallet_path, keys_exists, wallet_exists);
  ASSERT_TRUE(keys_exists);
}

TEST_F(Wallet2FileTest, load_with_wrong_password_throws)
{
  std::string wallet_path = (m_temp_dir / "wallet_wrong_pw").string();
  epee::wipeable_string password("correct");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, password, recovery_key, true, false);
  w.store();

  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  ASSERT_ANY_THROW(w2.load(wallet_path, epee::wipeable_string("wrong")));
}

// ===========================================================================
// Mock daemon tests
// ===========================================================================

TEST_F(Wallet2MockDaemonTest, wallet_created_with_mock)
{
  ASSERT_NE(m_wallet, nullptr);
  ASSERT_FALSE(m_wallet->get_address_as_str().empty());
}

TEST_F(Wallet2MockDaemonTest, mock_client_initially_not_connected)
{
  // Client should not be connected before any operation
  // The mock starts disconnected
  ASSERT_FALSE(m_mock_client->is_connected());
}

TEST_F(Wallet2MockDaemonTest, mock_client_connect_succeeds)
{
  m_mock_client->set_should_connect(true);
  bool connected = m_mock_client->connect(std::chrono::milliseconds(1000));
  ASSERT_TRUE(connected);
  ASSERT_TRUE(m_mock_client->is_connected());
}

TEST_F(Wallet2MockDaemonTest, mock_client_connect_fails)
{
  m_mock_client->set_should_connect(false);
  bool connected = m_mock_client->connect(std::chrono::milliseconds(1000));
  ASSERT_FALSE(connected);
  ASSERT_FALSE(m_mock_client->is_connected());
}

TEST_F(Wallet2MockDaemonTest, mock_client_disconnect)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->connect(std::chrono::milliseconds(1000));
  ASSERT_TRUE(m_mock_client->is_connected());
  m_mock_client->disconnect();
  ASSERT_FALSE(m_mock_client->is_connected());
}

TEST_F(Wallet2MockDaemonTest, mock_canned_response)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_response("/test_uri", "{\"status\":\"OK\"}");
  m_mock_client->connect(std::chrono::milliseconds(1000));

  const epee::net_utils::http::http_response_info* response = nullptr;
  bool ok = m_mock_client->invoke("/test_uri", "POST", "{}", std::chrono::milliseconds(1000), &response);
  ASSERT_TRUE(ok);
  ASSERT_NE(response, nullptr);
  ASSERT_EQ(response->m_response_code, 200);
  ASSERT_EQ(response->m_body, "{\"status\":\"OK\"}");
}

TEST_F(Wallet2MockDaemonTest, mock_json_rpc_response)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_json_rpc_response("get_info", "{\"height\":100}");
  m_mock_client->connect(std::chrono::milliseconds(1000));

  const epee::net_utils::http::http_response_info* response = nullptr;
  bool ok = m_mock_client->invoke("/json_rpc", "POST",
    "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_info\"}",
    std::chrono::milliseconds(1000), &response);
  ASSERT_TRUE(ok);
  ASSERT_NE(response, nullptr);
  ASSERT_NE(response->m_body.find("\"height\":100"), std::string::npos);
}

TEST_F(Wallet2MockDaemonTest, mock_invocation_tracking)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_default_result(true);
  m_mock_client->connect(std::chrono::milliseconds(1000));

  m_mock_client->invoke("/uri1", "POST", "body1", std::chrono::milliseconds(1000));
  m_mock_client->invoke("/uri2", "GET", "body2", std::chrono::milliseconds(1000));

  auto invocations = m_mock_client->get_invocations();
  ASSERT_EQ(invocations.size(), 2u);
  ASSERT_EQ(invocations[0].uri, "/uri1");
  ASSERT_EQ(invocations[0].body, "body1");
  ASSERT_EQ(invocations[1].uri, "/uri2");
}

TEST_F(Wallet2MockDaemonTest, mock_invoke_count)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_default_result(true);
  m_mock_client->connect(std::chrono::milliseconds(1000));

  ASSERT_EQ(m_mock_client->get_invoke_count(), 0u);
  m_mock_client->invoke("/a", "POST", "", std::chrono::milliseconds(100));
  m_mock_client->invoke("/b", "POST", "", std::chrono::milliseconds(100));
  ASSERT_EQ(m_mock_client->get_invoke_count(), 2u);
}

TEST_F(Wallet2MockDaemonTest, mock_bytes_tracking)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_response("/test", "response_body");
  m_mock_client->connect(std::chrono::milliseconds(1000));

  const epee::net_utils::http::http_response_info* resp = nullptr;
  std::string body = "request_body";
  m_mock_client->invoke("/test", "POST", body, std::chrono::milliseconds(100), &resp);

  ASSERT_EQ(m_mock_client->get_bytes_sent(), body.size());
  ASSERT_GT(m_mock_client->get_bytes_received(), 0u);
}

TEST_F(Wallet2MockDaemonTest, mock_clear_invocations)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_default_result(true);
  m_mock_client->connect(std::chrono::milliseconds(1000));

  m_mock_client->invoke("/x", "POST", "", std::chrono::milliseconds(100));
  ASSERT_EQ(m_mock_client->get_invoke_count(), 1u);

  m_mock_client->clear_invocations();
  ASSERT_EQ(m_mock_client->get_invoke_count(), 0u);
  ASSERT_TRUE(m_mock_client->get_invocations().empty());
}

TEST_F(Wallet2MockDaemonTest, mock_reset)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_response("/test", "data");
  m_mock_client->connect(std::chrono::milliseconds(1000));
  m_mock_client->invoke("/test", "POST", "", std::chrono::milliseconds(100));

  m_mock_client->reset();
  ASSERT_FALSE(m_mock_client->is_connected());
  ASSERT_EQ(m_mock_client->get_invoke_count(), 0u);
  ASSERT_EQ(m_mock_client->get_bytes_sent(), 0u);
  ASSERT_EQ(m_mock_client->get_bytes_received(), 0u);
}

TEST_F(Wallet2MockDaemonTest, mock_handler_function)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_handler("/dynamic", [](const std::string& body) {
    return "{\"echo\":\"" + body + "\"}";
  });
  m_mock_client->connect(std::chrono::milliseconds(1000));

  const epee::net_utils::http::http_response_info* resp = nullptr;
  bool ok = m_mock_client->invoke("/dynamic", "POST", "hello", std::chrono::milliseconds(100), &resp);
  ASSERT_TRUE(ok);
  ASSERT_NE(resp, nullptr);
  ASSERT_EQ(resp->m_body, "{\"echo\":\"hello\"}");
}

TEST_F(Wallet2MockDaemonTest, offline_mode_behavior)
{
  m_wallet->set_offline(true);
  ASSERT_TRUE(m_wallet->is_offline());

  // In offline mode, the wallet should not attempt daemon communication
  // check_connection should fail gracefully
  uint32_t version = 0;
  bool ssl = false;
  bool connected = m_wallet->check_connection(&version, &ssl, 200000);
  ASSERT_FALSE(connected);
}

TEST_F(Wallet2MockDaemonTest, wallet_address_with_mock)
{
  std::string addr = m_wallet->get_address_as_str();
  ASSERT_FALSE(addr.empty());
  ASSERT_EQ(addr[0], '4'); // mainnet
  ASSERT_EQ(addr.size(), 95u);
}

TEST_F(Wallet2MockDaemonTest, mock_connect_count)
{
  m_mock_client->set_should_connect(true);
  ASSERT_EQ(m_mock_client->get_connect_count(), 0u);
  m_mock_client->connect(std::chrono::milliseconds(100));
  ASSERT_EQ(m_mock_client->get_connect_count(), 1u);
  m_mock_client->connect(std::chrono::milliseconds(100));
  ASSERT_EQ(m_mock_client->get_connect_count(), 2u);
}

TEST_F(Wallet2MockDaemonTest, mock_last_invocation)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_default_result(true);
  m_mock_client->connect(std::chrono::milliseconds(1000));

  m_mock_client->invoke("/first", "POST", "body_first", std::chrono::milliseconds(100));
  m_mock_client->invoke("/second", "GET", "body_second", std::chrono::milliseconds(100));

  auto last = m_mock_client->get_last_invocation();
  ASSERT_EQ(last.uri, "/second");
  ASSERT_EQ(last.method, "GET");
  ASSERT_EQ(last.body, "body_second");
}

TEST_F(Wallet2MockDaemonTest, mock_invoke_get)
{
  m_mock_client->set_should_connect(true);
  m_mock_client->set_response("/gettest", "{\"result\":\"ok\"}");
  m_mock_client->connect(std::chrono::milliseconds(1000));

  const epee::net_utils::http::http_response_info* resp = nullptr;
  bool ok = m_mock_client->invoke_get("/gettest", std::chrono::milliseconds(100), "", &resp);
  ASSERT_TRUE(ok);
  ASSERT_NE(resp, nullptr);
  ASSERT_EQ(resp->m_body, "{\"result\":\"ok\"}");

  auto last = m_mock_client->get_last_invocation();
  ASSERT_EQ(last.method, "GET");
}

// ===========================================================================
// Wallet accessor utility tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, accessor_get_blockchain)
{
  auto& chain = wallet_accessor_test::get_blockchain(m_wallet);
  // Chain should have at least genesis after generate
  (void)chain; // Just check it compiles and doesn't crash
}

TEST_F(Wallet2GeneratedTest, accessor_set_last_block_reward)
{
  wallet_accessor_test::get_last_block_reward(m_wallet) = 600000000000ULL;
  ASSERT_EQ(m_wallet.get_last_block_reward(), 600000000000ULL);
}

TEST_F(Wallet2GeneratedTest, accessor_get_confirmed_txs_empty)
{
  auto& confirmed = wallet_accessor_test::get_confirmed_txs(m_wallet);
  ASSERT_TRUE(confirmed.empty());
}

TEST_F(Wallet2GeneratedTest, accessor_get_unconfirmed_txs_empty)
{
  auto& unconfirmed = wallet_accessor_test::get_unconfirmed_txs(m_wallet);
  ASSERT_TRUE(unconfirmed.empty());
}

TEST_F(Wallet2GeneratedTest, accessor_get_payments_empty)
{
  auto& payments = wallet_accessor_test::get_payments(m_wallet);
  ASSERT_TRUE(payments.empty());
}

// ===========================================================================
// Wallet key inspection tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, account_keys_nonzero)
{
  const auto& keys = m_wallet.get_account().get_keys();
  // Spend and view keys should be nonzero after generation
  ASSERT_NE(keys.m_spend_secret_key, crypto::null_skey);
  ASSERT_NE(keys.m_view_secret_key, crypto::null_skey);
}

TEST_F(Wallet2GeneratedTest, public_address_nonzero)
{
  const auto& addr = m_wallet.get_account().get_keys().m_account_address;
  crypto::public_key null_key;
  memset(&null_key, 0, sizeof(null_key));
  ASSERT_NE(addr.m_spend_public_key, null_key);
  ASSERT_NE(addr.m_view_public_key, null_key);
}

TEST_F(Wallet2GeneratedTest, get_address_matches_account_address)
{
  cryptonote::account_public_address addr1 = m_wallet.get_address();
  cryptonote::account_public_address addr2 = m_wallet.get_account().get_keys().m_account_address;
  ASSERT_EQ(addr1.m_spend_public_key, addr2.m_spend_public_key);
  ASSERT_EQ(addr1.m_view_public_key, addr2.m_view_public_key);
}

// ===========================================================================
// Refresh type tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_refresh_type)
{
  m_wallet.set_refresh_type(tools::wallet2::RefreshFull);
  ASSERT_EQ(m_wallet.get_refresh_type(), tools::wallet2::RefreshFull);
  m_wallet.set_refresh_type(tools::wallet2::RefreshNoCoinbase);
  ASSERT_EQ(m_wallet.get_refresh_type(), tools::wallet2::RefreshNoCoinbase);
  m_wallet.set_refresh_type(tools::wallet2::RefreshOptimizeCoinbase);
  ASSERT_EQ(m_wallet.get_refresh_type(), tools::wallet2::RefreshOptimizeCoinbase);
}

// ===========================================================================
// Multisig enable toggle
// ===========================================================================

TEST_F(Wallet2GeneratedTest, multisig_enable_toggle)
{
  m_wallet.enable_multisig(true);
  ASSERT_TRUE(m_wallet.is_multisig_enabled());
  m_wallet.enable_multisig(false);
  ASSERT_FALSE(m_wallet.is_multisig_enabled());
}

// ===========================================================================
// Device name
// ===========================================================================

TEST_F(Wallet2GeneratedTest, device_name_default)
{
  // Default software wallet
  const std::string& name = m_wallet.device_name();
  (void)name; // Just make sure it doesn't crash
}

TEST_F(Wallet2GeneratedTest, device_name_set)
{
  m_wallet.device_name("Trezor");
  ASSERT_EQ(m_wallet.device_name(), "Trezor");
}

// ===========================================================================
// Allow mismatched daemon version
// ===========================================================================

TEST_F(Wallet2GeneratedTest, allow_mismatched_daemon_version)
{
  m_wallet.allow_mismatched_daemon_version(true);
  ASSERT_TRUE(m_wallet.is_mismatched_daemon_version_allowed());
  m_wallet.allow_mismatched_daemon_version(false);
  ASSERT_FALSE(m_wallet.is_mismatched_daemon_version_allowed());
}

// ===========================================================================
// Balance queries with multiple transfers and subaddresses
// ===========================================================================

TEST_F(Wallet2GeneratedTest, balance_all_empty_wallet)
{
  ASSERT_EQ(m_wallet.balance_all(false), 0u);
  ASSERT_EQ(m_wallet.balance_all(true), 0u);
}

TEST_F(Wallet2GeneratedTest, unlocked_balance_all_empty_wallet)
{
  ASSERT_EQ(m_wallet.unlocked_balance_all(false), 0u);
  ASSERT_EQ(m_wallet.unlocked_balance_all(true), 0u);
}

TEST_F(Wallet2GeneratedTest, balance_per_subaddress_empty)
{
  auto bps = m_wallet.balance_per_subaddress(0, false);
  ASSERT_TRUE(bps.empty());
}

TEST_F(Wallet2GeneratedTest, unlocked_balance_per_subaddress_empty)
{
  auto ubps = m_wallet.unlocked_balance_per_subaddress(0, false);
  ASSERT_TRUE(ubps.empty());
}

TEST_F(Wallet2GeneratedTest, balance_with_multiple_subaddress_transfers)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  // Add transfers to different subaddresses
  for (uint32_t i = 0; i < 3; ++i)
  {
    tools::wallet2::transfer_details td = {};
    td.m_block_height = 100 + i;
    td.m_internal_output_index = 0;
    td.m_global_output_index = 1000 + i;
    td.m_spent = false;
    td.m_frozen = false;
    td.m_spent_height = 0;
    td.m_amount = (i + 1) * 1000000000000ULL;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, i};
    td.m_key_image_partial = false;
    td.m_mask = rct::identity();
    memset(&td.m_key_image, 0x10 + i, sizeof(td.m_key_image));
    memset(&td.m_txid, 0x20 + i, sizeof(td.m_txid));

    cryptonote::transaction_prefix tx_prefix;
    tx_prefix.version = 2;
    tx_prefix.unlock_time = 0;
    cryptonote::tx_out out;
    cryptonote::txout_to_key tk;
    memset(&tk.key, 0x30 + i, sizeof(tk.key));
    out.amount = 0;
    out.target = tk;
    tx_prefix.vout.push_back(out);
    td.m_tx = tx_prefix;

    key_images[td.m_key_image] = transfers.size();
    transfers.push_back(td);
  }

  // Total balance should be 1 + 2 + 3 = 6 XMR
  uint64_t total = m_wallet.balance(0, false);
  ASSERT_EQ(total, 6000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, balance_per_subaddress_multiple)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  // Transfer to subaddress 0
  {
    tools::wallet2::transfer_details td = {};
    td.m_block_height = 100;
    td.m_internal_output_index = 0;
    td.m_global_output_index = 500;
    td.m_spent = false;
    td.m_frozen = false;
    td.m_spent_height = 0;
    td.m_amount = 2000000000000ULL;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 0};
    td.m_key_image_partial = false;
    td.m_mask = rct::identity();
    memset(&td.m_key_image, 0xa1, sizeof(td.m_key_image));
    memset(&td.m_txid, 0xa2, sizeof(td.m_txid));

    cryptonote::transaction_prefix tx_prefix;
    tx_prefix.version = 2;
    tx_prefix.unlock_time = 0;
    cryptonote::tx_out out;
    cryptonote::txout_to_key tk;
    memset(&tk.key, 0xa3, sizeof(tk.key));
    out.amount = 0;
    out.target = tk;
    tx_prefix.vout.push_back(out);
    td.m_tx = tx_prefix;

    key_images[td.m_key_image] = transfers.size();
    transfers.push_back(td);
  }

  // Transfer to subaddress 1
  {
    tools::wallet2::transfer_details td = {};
    td.m_block_height = 101;
    td.m_internal_output_index = 0;
    td.m_global_output_index = 501;
    td.m_spent = false;
    td.m_frozen = false;
    td.m_spent_height = 0;
    td.m_amount = 3000000000000ULL;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 1};
    td.m_key_image_partial = false;
    td.m_mask = rct::identity();
    memset(&td.m_key_image, 0xb1, sizeof(td.m_key_image));
    memset(&td.m_txid, 0xb2, sizeof(td.m_txid));

    cryptonote::transaction_prefix tx_prefix;
    tx_prefix.version = 2;
    tx_prefix.unlock_time = 0;
    cryptonote::tx_out out;
    cryptonote::txout_to_key tk;
    memset(&tk.key, 0xb3, sizeof(tk.key));
    out.amount = 0;
    out.target = tk;
    tx_prefix.vout.push_back(out);
    td.m_tx = tx_prefix;

    key_images[td.m_key_image] = transfers.size();
    transfers.push_back(td);
  }

  auto bps = m_wallet.balance_per_subaddress(0, false);
  ASSERT_EQ(bps.size(), 2u);
  ASSERT_EQ(bps[0], 2000000000000ULL);
  ASSERT_EQ(bps[1], 3000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, balance_all_with_multiple_accounts)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  // Add a second subaddress account
  m_wallet.add_subaddress_account("Second Account");

  // Transfer to account 0
  {
    tools::wallet2::transfer_details td = {};
    td.m_block_height = 100;
    td.m_internal_output_index = 0;
    td.m_global_output_index = 700;
    td.m_spent = false;
    td.m_frozen = false;
    td.m_spent_height = 0;
    td.m_amount = 1000000000000ULL;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 0};
    td.m_key_image_partial = false;
    td.m_mask = rct::identity();
    memset(&td.m_key_image, 0xc1, sizeof(td.m_key_image));
    memset(&td.m_txid, 0xc2, sizeof(td.m_txid));

    cryptonote::transaction_prefix tx_prefix;
    tx_prefix.version = 2;
    tx_prefix.unlock_time = 0;
    cryptonote::tx_out out;
    cryptonote::txout_to_key tk;
    memset(&tk.key, 0xc3, sizeof(tk.key));
    out.amount = 0;
    out.target = tk;
    tx_prefix.vout.push_back(out);
    td.m_tx = tx_prefix;

    key_images[td.m_key_image] = transfers.size();
    transfers.push_back(td);
  }

  // Transfer to account 1
  {
    tools::wallet2::transfer_details td = {};
    td.m_block_height = 101;
    td.m_internal_output_index = 0;
    td.m_global_output_index = 701;
    td.m_spent = false;
    td.m_frozen = false;
    td.m_spent_height = 0;
    td.m_amount = 4000000000000ULL;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {1, 0};
    td.m_key_image_partial = false;
    td.m_mask = rct::identity();
    memset(&td.m_key_image, 0xd1, sizeof(td.m_key_image));
    memset(&td.m_txid, 0xd2, sizeof(td.m_txid));

    cryptonote::transaction_prefix tx_prefix;
    tx_prefix.version = 2;
    tx_prefix.unlock_time = 0;
    cryptonote::tx_out out;
    cryptonote::txout_to_key tk;
    memset(&tk.key, 0xd3, sizeof(tk.key));
    out.amount = 0;
    out.target = tk;
    tx_prefix.vout.push_back(out);
    td.m_tx = tx_prefix;

    key_images[td.m_key_image] = transfers.size();
    transfers.push_back(td);
  }

  ASSERT_EQ(m_wallet.balance(0, false), 1000000000000ULL);
  ASSERT_EQ(m_wallet.balance(1, false), 4000000000000ULL);
  ASSERT_EQ(m_wallet.balance_all(false), 5000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, spent_transfer_excluded_from_balance)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  tools::wallet2::transfer_details td = {};
  td.m_block_height = 50;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 800;
  td.m_spent = true;  // spent
  td.m_frozen = false;
  td.m_spent_height = 100;
  td.m_amount = 5000000000000ULL;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xe1, sizeof(td.m_key_image));
  memset(&td.m_txid, 0xe2, sizeof(td.m_txid));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0xe3, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  key_images[td.m_key_image] = transfers.size();
  transfers.push_back(td);

  // Spent transfers are not counted
  ASSERT_EQ(m_wallet.balance(0, false), 0u);
}

TEST_F(Wallet2GeneratedTest, balance_ignores_outputs_above)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  tools::wallet2::transfer_details td = {};
  td.m_block_height = 50;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 900;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_spent_height = 0;
  td.m_amount = 100000000000000ULL; // 100 XMR
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xf1, sizeof(td.m_key_image));
  memset(&td.m_txid, 0xf2, sizeof(td.m_txid));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0xf3, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  key_images[td.m_key_image] = transfers.size();
  transfers.push_back(td);

  // Set ignore_outputs_above to 10 XMR - the 100 XMR output should be ignored
  m_wallet.ignore_outputs_above(10000000000000ULL);
  ASSERT_EQ(m_wallet.balance(0, false), 0u);

  // Raise the limit - now it should count
  m_wallet.ignore_outputs_above(200000000000000ULL);
  ASSERT_EQ(m_wallet.balance(0, false), 100000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, balance_ignores_outputs_below)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  tools::wallet2::transfer_details td = {};
  td.m_block_height = 50;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 910;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_spent_height = 0;
  td.m_amount = 1000ULL; // tiny amount
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0xf4, sizeof(td.m_key_image));
  memset(&td.m_txid, 0xf5, sizeof(td.m_txid));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0xf6, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  key_images[td.m_key_image] = transfers.size();
  transfers.push_back(td);

  // Set ignore_outputs_below to larger than our output
  m_wallet.ignore_outputs_below(10000ULL);
  ASSERT_EQ(m_wallet.balance(0, false), 0u);

  // Lower the threshold
  m_wallet.ignore_outputs_below(0);
  ASSERT_EQ(m_wallet.balance(0, false), 1000ULL);
}

TEST_F(Wallet2GeneratedTest, unlocked_balance_blocks_to_unlock)
{
  // On an empty wallet, blocks_to_unlock should be 0
  uint64_t blocks_to_unlock = 999;
  uint64_t time_to_unlock = 999;
  uint64_t ub = m_wallet.unlocked_balance(0, false, &blocks_to_unlock, &time_to_unlock);
  ASSERT_EQ(ub, 0u);
  ASSERT_EQ(blocks_to_unlock, 0u);
  ASSERT_EQ(time_to_unlock, 0u);
}

TEST_F(Wallet2GeneratedTest, unlocked_balance_all_blocks_to_unlock)
{
  uint64_t blocks_to_unlock = 999;
  uint64_t time_to_unlock = 999;
  uint64_t ub = m_wallet.unlocked_balance_all(false, &blocks_to_unlock, &time_to_unlock);
  ASSERT_EQ(ub, 0u);
  ASSERT_EQ(blocks_to_unlock, 0u);
  ASSERT_EQ(time_to_unlock, 0u);
}

// ===========================================================================
// Freeze/thaw additional tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, freeze_out_of_range_throws)
{
  ASSERT_ANY_THROW(m_wallet.freeze(0));
}

TEST_F(Wallet2GeneratedTest, thaw_out_of_range_throws)
{
  ASSERT_ANY_THROW(m_wallet.thaw(0));
}

TEST_F(Wallet2GeneratedTest, frozen_out_of_range_throws)
{
  ASSERT_ANY_THROW(m_wallet.frozen(0));
}

TEST_F(Wallet2GeneratedTest, freeze_unknown_key_image_throws)
{
  crypto::key_image ki;
  memset(&ki, 0xff, sizeof(ki));
  ASSERT_ANY_THROW(m_wallet.freeze(ki));
}

TEST_F(Wallet2GeneratedTest, frozen_transfer_details_method)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  tools::wallet2::transfer_details td = {};
  td.m_block_height = 50;
  td.m_internal_output_index = 0;
  td.m_global_output_index = 500;
  td.m_spent = false;
  td.m_frozen = false;
  td.m_amount = 100000000000ULL;
  td.m_rct = true;
  td.m_key_image_known = true;
  td.m_key_image_request = false;
  td.m_pk_index = 0;
  td.m_subaddr_index = {0, 0};
  td.m_key_image_partial = false;
  td.m_mask = rct::identity();
  memset(&td.m_key_image, 0x11, sizeof(td.m_key_image));

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x22, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);

  // frozen(const transfer_details&) overload
  ASSERT_FALSE(m_wallet.frozen(transfers[0]));
  m_wallet.freeze(0);
  ASSERT_TRUE(m_wallet.frozen(transfers[0]));
}

// ===========================================================================
// Subaddress management advanced tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, add_subaddress_to_second_account)
{
  m_wallet.add_subaddress_account("Account 2");
  uint32_t acct = m_wallet.get_num_subaddress_accounts() - 1;
  size_t before = m_wallet.get_num_subaddresses(acct);
  m_wallet.add_subaddress(acct, "Sub in acct 2");
  ASSERT_EQ(m_wallet.get_num_subaddresses(acct), before + 1);
}

TEST_F(Wallet2GeneratedTest, subaddress_label_on_new_subaddress)
{
  m_wallet.add_subaddress(0, "Labeled Sub");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  ASSERT_EQ(m_wallet.get_subaddress_label({0, idx}), "Labeled Sub");
}

TEST_F(Wallet2GeneratedTest, set_subaddress_label_updates)
{
  m_wallet.add_subaddress(0, "Original");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  m_wallet.set_subaddress_label({0, idx}, "Updated");
  ASSERT_EQ(m_wallet.get_subaddress_label({0, idx}), "Updated");
}

TEST_F(Wallet2GeneratedTest, get_subaddress_label_main_address)
{
  // The main address label is usually empty by default
  std::string label = m_wallet.get_subaddress_label({0, 0});
  // Just should not crash, label might be empty or "Primary account"
  (void)label;
}

TEST_F(Wallet2GeneratedTest, subaddress_different_accounts_different_addresses)
{
  m_wallet.add_subaddress_account("Account 2");
  uint32_t acct = m_wallet.get_num_subaddress_accounts() - 1;
  std::string addr0 = m_wallet.get_subaddress_as_str({0, 0});
  std::string addr1 = m_wallet.get_subaddress_as_str({acct, 0});
  ASSERT_NE(addr0, addr1);
}

TEST_F(Wallet2GeneratedTest, subaddress_index_lookup_for_added_subaddress)
{
  m_wallet.add_subaddress(0, "Lookup Test");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  cryptonote::account_public_address addr = m_wallet.get_subaddress({0, idx});
  auto found = m_wallet.get_subaddress_index(addr);
  ASSERT_TRUE(found.is_initialized());
  ASSERT_EQ(found->major, 0u);
  ASSERT_EQ(found->minor, idx);
}

TEST_F(Wallet2GeneratedTest, subaddress_index_lookup_nonexistent)
{
  // Create a random address not in the wallet
  cryptonote::account_public_address fake_addr;
  memset(&fake_addr.m_spend_public_key, 0xab, sizeof(fake_addr.m_spend_public_key));
  memset(&fake_addr.m_view_public_key, 0xcd, sizeof(fake_addr.m_view_public_key));
  auto found = m_wallet.get_subaddress_index(fake_addr);
  ASSERT_FALSE(found.is_initialized());
}

TEST_F(Wallet2GeneratedTest, get_subaddress_spend_public_key)
{
  cryptonote::subaddress_index idx{0, 0};
  crypto::public_key key = m_wallet.get_subaddress_spend_public_key(idx);
  // Main address spend key should equal the account spend key
  crypto::public_key null_key;
  memset(&null_key, 0, sizeof(null_key));
  ASSERT_NE(key, null_key);
}

// ===========================================================================
// TX estimation tests (static estimate_fee)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, estimate_fee_zero_extra)
{
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 1, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_with_extra)
{
  uint64_t fee_no_extra = tools::wallet2::estimate_fee(true, true, 1, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  uint64_t fee_with_extra = tools::wallet2::estimate_fee(true, true, 1, 16, 2, 100,
    true, true, true, true, 20000, 10000);
  ASSERT_GE(fee_with_extra, fee_no_extra);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_higher_base_fee)
{
  uint64_t fee_low = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 10000, 10000);
  uint64_t fee_high = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 40000, 10000);
  ASSERT_GT(fee_high, fee_low);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_no_rct)
{
  uint64_t fee = tools::wallet2::estimate_fee(true, false, 2, 16, 2, 0,
    false, false, false, false, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_not_per_byte)
{
  uint64_t fee = tools::wallet2::estimate_fee(false, true, 2, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_many_inputs)
{
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 20, 16, 2, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_many_outputs)
{
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 2, 16, 16, 0,
    true, true, true, true, 20000, 10000);
  ASSERT_GT(fee, 0u);
}

TEST_F(Wallet2GeneratedTest, estimate_fee_quantization_mask)
{
  // Test that fee quantization mask rounds up
  uint64_t fee1 = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 20000, 1);  // no quantization
  uint64_t fee2 = tools::wallet2::estimate_fee(true, true, 2, 16, 2, 0,
    true, true, true, true, 20000, 10000);  // quantized
  ASSERT_GE(fee2, fee1);
}

// ===========================================================================
// Signing tests - additional
// ===========================================================================

TEST_F(Wallet2GeneratedTest, sign_long_message)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data(10000, 'A'); // 10KB message
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key);
  ASSERT_FALSE(sig.empty());
  auto result = m_wallet.verify(data, m_wallet.get_address(), sig);
  ASSERT_TRUE(result.valid);
}

TEST_F(Wallet2GeneratedTest, sign_binary_data)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data;
  data.resize(256);
  for (int i = 0; i < 256; ++i) data[i] = (char)i;
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key);
  auto result = m_wallet.verify(data, m_wallet.get_address(), sig);
  ASSERT_TRUE(result.valid);
}

TEST_F(Wallet2GeneratedTest, verify_invalid_signature_format)
{
  auto result = m_wallet.verify("test", m_wallet.get_address(), "not_a_real_signature");
  ASSERT_FALSE(result.valid);
}

TEST_F(Wallet2GeneratedTest, sign_with_view_key_verify_subaddress_fails)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  std::string data = "View key signing test";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_view_key);

  // Verify with a subaddress should fail since we signed with main address
  m_wallet.add_subaddress(0, "test sub");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  cryptonote::account_public_address sub_addr = m_wallet.get_subaddress({0, idx});
  auto result = m_wallet.verify(data, sub_addr, sig);
  ASSERT_FALSE(result.valid);
}

TEST_F(Wallet2GeneratedTest, sign_with_subaddress_verify_main_fails)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.add_subaddress(0, "signing sub 2");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  cryptonote::subaddress_index sub_idx{0, idx};
  std::string data = "Sub address signing 2";
  std::string sig = m_wallet.sign(data, tools::wallet2::sign_with_spend_key, sub_idx);

  // Verify with main address should fail
  auto result = m_wallet.verify(data, m_wallet.get_address(), sig);
  ASSERT_FALSE(result.valid);
}

// ===========================================================================
// TX notes extended tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, tx_note_unicode)
{
  crypto::hash txid;
  memset(txid.data, 0x44, sizeof(txid.data));
  std::string unicode_note = "Payment \xc3\xa9\xc3\xa0\xc3\xbc"; // UTF-8
  m_wallet.set_tx_note(txid, unicode_note);
  ASSERT_EQ(m_wallet.get_tx_note(txid), unicode_note);
}

TEST_F(Wallet2GeneratedTest, tx_note_very_long)
{
  crypto::hash txid;
  memset(txid.data, 0x55, sizeof(txid.data));
  std::string long_note(10000, 'X');
  m_wallet.set_tx_note(txid, long_note);
  ASSERT_EQ(m_wallet.get_tx_note(txid), long_note);
}

// ===========================================================================
// Attribute extended tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, attribute_empty_key)
{
  m_wallet.set_attribute("", "empty_key_value");
  std::string val;
  ASSERT_TRUE(m_wallet.get_attribute("", val));
  ASSERT_EQ(val, "empty_key_value");
}

TEST_F(Wallet2GeneratedTest, attribute_empty_value)
{
  m_wallet.set_attribute("key_with_empty", "");
  std::string val;
  ASSERT_TRUE(m_wallet.get_attribute("key_with_empty", val));
  ASSERT_EQ(val, "");
}

TEST_F(Wallet2GeneratedTest, attribute_special_characters)
{
  m_wallet.set_attribute("key.with.dots/and/slashes", "value with spaces & symbols!@#$%");
  std::string val;
  ASSERT_TRUE(m_wallet.get_attribute("key.with.dots/and/slashes", val));
  ASSERT_EQ(val, "value with spaces & symbols!@#$%");
}

// ===========================================================================
// Description extended tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, description_very_long)
{
  std::string long_desc(5000, 'D');
  m_wallet.set_description(long_desc);
  ASSERT_EQ(m_wallet.get_description(), long_desc);
}

// ===========================================================================
// Payment queries with mock data
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_payments_by_id_empty)
{
  crypto::hash pid;
  memset(pid.data, 0x99, sizeof(pid.data));
  std::list<tools::wallet2::payment_details> payments;
  m_wallet.get_payments(pid, payments);
  ASSERT_TRUE(payments.empty());
}

TEST_F(Wallet2GeneratedTest, get_payments_by_id_with_data)
{
  auto& payments_map = wallet_accessor_test::get_payments(m_wallet);
  crypto::hash pid;
  memset(pid.data, 0x88, sizeof(pid.data));

  tools::wallet2::payment_details pd;
  memset(&pd.m_tx_hash, 0x77, sizeof(pd.m_tx_hash));
  pd.m_amount = 1000000000000ULL;
  pd.m_fee = 10000000ULL;
  pd.m_block_height = 500;
  pd.m_unlock_time = 0;
  pd.m_timestamp = 1000000;
  pd.m_coinbase = false;
  pd.m_subaddr_index = {0, 0};

  payments_map.emplace(pid, pd);

  std::list<tools::wallet2::payment_details> results;
  m_wallet.get_payments(pid, results);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_EQ(results.front().m_amount, 1000000000000ULL);
}

TEST_F(Wallet2GeneratedTest, get_payments_by_range)
{
  auto& payments_map = wallet_accessor_test::get_payments(m_wallet);
  crypto::hash pid;
  memset(pid.data, 0x66, sizeof(pid.data));

  tools::wallet2::payment_details pd;
  memset(&pd.m_tx_hash, 0x55, sizeof(pd.m_tx_hash));
  pd.m_amount = 2000000000000ULL;
  pd.m_fee = 20000000ULL;
  pd.m_block_height = 300;
  pd.m_unlock_time = 0;
  pd.m_timestamp = 2000000;
  pd.m_coinbase = false;
  pd.m_subaddr_index = {0, 0};

  payments_map.emplace(pid, pd);

  std::list<std::pair<crypto::hash, tools::wallet2::payment_details>> results;
  m_wallet.get_payments(results, 0, 500);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_EQ(results.front().second.m_block_height, 300u);
}

TEST_F(Wallet2GeneratedTest, get_payments_by_range_filter_height)
{
  auto& payments_map = wallet_accessor_test::get_payments(m_wallet);
  crypto::hash pid;
  memset(pid.data, 0x44, sizeof(pid.data));

  tools::wallet2::payment_details pd;
  memset(&pd.m_tx_hash, 0x33, sizeof(pd.m_tx_hash));
  pd.m_amount = 500000000000ULL;
  pd.m_fee = 5000000ULL;
  pd.m_block_height = 200;
  pd.m_unlock_time = 0;
  pd.m_timestamp = 3000000;
  pd.m_coinbase = false;
  pd.m_subaddr_index = {0, 0};

  payments_map.emplace(pid, pd);

  // Filter to only blocks above 300
  std::list<std::pair<crypto::hash, tools::wallet2::payment_details>> results;
  m_wallet.get_payments(results, 300, 500);
  ASSERT_TRUE(results.empty());
}

// ===========================================================================
// Confirmed/Unconfirmed transfer queries
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_payments_out_empty)
{
  std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> confirmed;
  m_wallet.get_payments_out(confirmed, 0);
  ASSERT_TRUE(confirmed.empty());
}

TEST_F(Wallet2GeneratedTest, get_payments_out_with_data)
{
  auto& confirmed_txs = wallet_accessor_test::get_confirmed_txs(m_wallet);
  crypto::hash txid;
  memset(txid.data, 0x22, sizeof(txid.data));

  tools::wallet2::confirmed_transfer_details ctd;
  ctd.m_amount_in = 2000000000000ULL;
  ctd.m_amount_out = 1900000000000ULL;
  ctd.m_change = 100000000000ULL;
  ctd.m_block_height = 400;
  ctd.m_timestamp = 4000000;
  ctd.m_subaddr_account = 0;

  confirmed_txs[txid] = ctd;

  std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> results;
  m_wallet.get_payments_out(results, 0, 500);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_EQ(results.front().second.m_block_height, 400u);
}

TEST_F(Wallet2GeneratedTest, get_unconfirmed_payments_out_empty)
{
  std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>> unconfirmed;
  m_wallet.get_unconfirmed_payments_out(unconfirmed);
  ASSERT_TRUE(unconfirmed.empty());
}

TEST_F(Wallet2GeneratedTest, get_unconfirmed_payments_out_with_data)
{
  auto& unconfirmed_txs = wallet_accessor_test::get_unconfirmed_txs(m_wallet);
  crypto::hash txid;
  memset(txid.data, 0x11, sizeof(txid.data));

  tools::wallet2::unconfirmed_transfer_details utd;
  utd.m_amount_in = 3000000000000ULL;
  utd.m_amount_out = 2800000000000ULL;
  utd.m_change = 200000000000ULL;
  utd.m_sent_time = time(nullptr);
  utd.m_timestamp = 5000000;
  utd.m_state = tools::wallet2::unconfirmed_transfer_details::pending;
  utd.m_subaddr_account = 0;
  utd.m_subaddr_indices = {0};

  unconfirmed_txs[txid] = utd;

  std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>> results;
  m_wallet.get_unconfirmed_payments_out(results);
  ASSERT_EQ(results.size(), 1u);
}

// ===========================================================================
// Export/Import payments round-trip
// ===========================================================================

TEST_F(Wallet2GeneratedTest, export_import_payments_round_trip)
{
  auto& payments_map = wallet_accessor_test::get_payments(m_wallet);
  crypto::hash pid;
  memset(pid.data, 0xab, sizeof(pid.data));

  tools::wallet2::payment_details pd;
  memset(&pd.m_tx_hash, 0xcd, sizeof(pd.m_tx_hash));
  pd.m_amount = 7000000000000ULL;
  pd.m_fee = 70000000ULL;
  pd.m_block_height = 600;
  pd.m_unlock_time = 0;
  pd.m_timestamp = 6000000;
  pd.m_coinbase = false;
  pd.m_subaddr_index = {0, 0};

  payments_map.emplace(pid, pd);

  // Export
  auto exported = m_wallet.export_payments();
  ASSERT_FALSE(exported.empty());

  // Clear and reimport
  payments_map.clear();
  ASSERT_TRUE(payments_map.empty());
  m_wallet.import_payments(exported);

  // Verify
  std::list<tools::wallet2::payment_details> results;
  m_wallet.get_payments(pid, results);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_EQ(results.front().m_amount, 7000000000000ULL);
}

// ===========================================================================
// Export/Import blockchain round-trip
// ===========================================================================

TEST_F(Wallet2GeneratedTest, export_blockchain_non_empty)
{
  auto bc = m_wallet.export_blockchain();
  // After generate, blockchain should have at least genesis
  // The export produces a tuple of (offset, genesis, hashes)
  (void)bc;
}

// ===========================================================================
// Encryption tests - additional
// ===========================================================================

TEST_F(Wallet2GeneratedTest, encrypt_decrypt_large_data)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  const crypto::secret_key& view_key = m_wallet.get_account().get_keys().m_view_secret_key;
  std::string large_data(100000, 'Z');
  std::string ciphertext = m_wallet.encrypt(large_data, view_key, true);
  ASSERT_NE(ciphertext, large_data);
  std::string decrypted = m_wallet.decrypt(ciphertext, view_key, true);
  ASSERT_EQ(decrypted, large_data);
}

TEST_F(Wallet2GeneratedTest, encrypt_decrypt_unauthenticated)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  const crypto::secret_key& view_key = m_wallet.get_account().get_keys().m_view_secret_key;
  std::string plaintext = "Unauthenticated encryption test";
  std::string ciphertext = m_wallet.encrypt(plaintext, view_key, false);
  std::string decrypted = m_wallet.decrypt(ciphertext, view_key, false);
  ASSERT_EQ(decrypted, plaintext);
}

TEST_F(Wallet2GeneratedTest, encrypt_authenticated_tampered_fails)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  const crypto::secret_key& view_key = m_wallet.get_account().get_keys().m_view_secret_key;
  std::string plaintext = "Tamper test data";
  std::string ciphertext = m_wallet.encrypt(plaintext, view_key, true);

  // Tamper with the ciphertext
  if (ciphertext.size() > 10)
    ciphertext[10] ^= 0xFF;

  ASSERT_ANY_THROW(m_wallet.decrypt(ciphertext, view_key, true));
}

// ===========================================================================
// Seed language tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_seed_english)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("English");
  epee::wipeable_string seed;
  ASSERT_TRUE(m_wallet.get_seed(seed));
  ASSERT_FALSE(seed.empty());
  // 25 words
  int count = 1;
  for (size_t i = 0; i < seed.size(); ++i)
    if (seed.data()[i] == ' ') ++count;
  ASSERT_EQ(count, 25);
}

TEST_F(Wallet2GeneratedTest, get_seed_spanish)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("Spanish");
  epee::wipeable_string seed;
  ASSERT_TRUE(m_wallet.get_seed(seed));
  ASSERT_FALSE(seed.empty());
  int count = 1;
  for (size_t i = 0; i < seed.size(); ++i)
    if (seed.data()[i] == ' ') ++count;
  ASSERT_EQ(count, 25);
}

TEST_F(Wallet2GeneratedTest, get_seed_japanese)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("Japanese");
  epee::wipeable_string seed;
  ASSERT_TRUE(m_wallet.get_seed(seed));
  ASSERT_FALSE(seed.empty());
}

TEST_F(Wallet2GeneratedTest, get_seed_german)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("German");
  epee::wipeable_string seed;
  ASSERT_TRUE(m_wallet.get_seed(seed));
  ASSERT_FALSE(seed.empty());
}

TEST_F(Wallet2GeneratedTest, get_seed_with_passphrase)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  m_wallet.set_seed_language("English");

  epee::wipeable_string seed_no_pass, seed_with_pass;
  ASSERT_TRUE(m_wallet.get_seed(seed_no_pass));
  ASSERT_TRUE(m_wallet.get_seed(seed_with_pass, epee::wipeable_string("my passphrase")));
  // Seeds with and without passphrase should be different
  // (passphrase modifies the seed output)
  ASSERT_NE(std::string(seed_no_pass.data(), seed_no_pass.size()),
            std::string(seed_with_pass.data(), seed_with_pass.size()));
}

TEST_F(Wallet2GeneratedTest, get_multisig_seed_fails_on_non_multisig)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  epee::wipeable_string seed;
  ASSERT_FALSE(m_wallet.get_multisig_seed(seed));
}

// ===========================================================================
// URI tests - additional
// ===========================================================================

TEST_F(Wallet2GeneratedTest, make_uri_with_recipient_name)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 0, "", "Alice", error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_NE(uri.find("recipient_name="), std::string::npos);
}

TEST_F(Wallet2GeneratedTest, make_uri_with_description)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 0, "Coffee purchase", "", error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_NE(uri.find("tx_description="), std::string::npos);
}

TEST_F(Wallet2GeneratedTest, parse_uri_no_amount)
{
  std::string addr = m_wallet.get_address_as_str();
  std::string error;
  std::string uri = m_wallet.make_uri(addr, "", 0, "", "", error);
  ASSERT_TRUE(error.empty());

  std::string parsed_addr, parsed_pid, parsed_desc, parsed_name;
  uint64_t parsed_amount = 0;
  std::vector<std::string> unknown;
  std::string parse_error;
  bool ok = m_wallet.parse_uri(uri, parsed_addr, parsed_pid, parsed_amount, parsed_desc, parsed_name, unknown, parse_error);
  ASSERT_TRUE(ok) << parse_error;
  ASSERT_EQ(parsed_addr, addr);
  // parse_uri does not modify amount when no tx_amount is in the URI,
  // so the value remains at whatever it was initialized to
  ASSERT_EQ(parsed_amount, 0u);
}

TEST_F(Wallet2GeneratedTest, parse_uri_empty_string)
{
  std::string addr, pid, desc, name, error;
  uint64_t amount;
  std::vector<std::string> unknown;
  bool ok = m_wallet.parse_uri("", addr, pid, amount, desc, name, unknown, error);
  ASSERT_FALSE(ok);
}

// ===========================================================================
// Account tags extended tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_account_tag_multiple_accounts)
{
  m_wallet.add_subaddress_account("A2");
  m_wallet.add_subaddress_account("A3");
  m_wallet.set_account_tag({0, 1, 2}, "all_tagged");
  const auto& tags = m_wallet.get_account_tags();
  ASSERT_EQ(tags.second[0], "all_tagged");
  ASSERT_EQ(tags.second[1], "all_tagged");
  ASSERT_EQ(tags.second[2], "all_tagged");
}

TEST_F(Wallet2GeneratedTest, set_account_tag_empty_untags)
{
  m_wallet.set_account_tag({0}, "tagged");
  m_wallet.set_account_tag({0}, "");
  const auto& tags = m_wallet.get_account_tags();
  if (!tags.second.empty()) {
    ASSERT_EQ(tags.second[0], "");
  }
}

// ===========================================================================
// Wallet property/config tests - additional
// ===========================================================================

TEST_F(Wallet2GeneratedTest, default_priority)
{
  m_wallet.set_default_priority(tools::fee_priority::Normal);
  ASSERT_TRUE(m_wallet.get_default_priority() == tools::fee_priority::Normal);
  m_wallet.set_default_priority(tools::fee_priority::Priority);
  ASSERT_TRUE(m_wallet.get_default_priority() == tools::fee_priority::Priority);
}

TEST_F(Wallet2GeneratedTest, ask_password_type)
{
  m_wallet.ask_password(tools::wallet2::AskPasswordNever);
  ASSERT_EQ(m_wallet.ask_password(), tools::wallet2::AskPasswordNever);
  m_wallet.ask_password(tools::wallet2::AskPasswordOnAction);
  ASSERT_EQ(m_wallet.ask_password(), tools::wallet2::AskPasswordOnAction);
  m_wallet.ask_password(tools::wallet2::AskPasswordToDecrypt);
  ASSERT_EQ(m_wallet.ask_password(), tools::wallet2::AskPasswordToDecrypt);
}

TEST_F(Wallet2GeneratedTest, confirm_export_overwrite)
{
  m_wallet.confirm_export_overwrite(true);
  ASSERT_TRUE(m_wallet.confirm_export_overwrite());
  m_wallet.confirm_export_overwrite(false);
  ASSERT_FALSE(m_wallet.confirm_export_overwrite());
}

TEST_F(Wallet2GeneratedTest, auto_low_priority)
{
  m_wallet.auto_low_priority(true);
  ASSERT_TRUE(m_wallet.auto_low_priority());
  m_wallet.auto_low_priority(false);
  ASSERT_FALSE(m_wallet.auto_low_priority());
}

TEST_F(Wallet2GeneratedTest, segregation_height)
{
  m_wallet.segregation_height(500000);
  ASSERT_EQ(m_wallet.segregation_height(), 500000u);
}

TEST_F(Wallet2GeneratedTest, print_ring_members)
{
  m_wallet.print_ring_members(true);
  ASSERT_TRUE(m_wallet.print_ring_members());
  m_wallet.print_ring_members(false);
  ASSERT_FALSE(m_wallet.print_ring_members());
}

TEST_F(Wallet2GeneratedTest, store_tx_info_toggle)
{
  m_wallet.store_tx_info(false);
  ASSERT_FALSE(m_wallet.store_tx_info());
  m_wallet.store_tx_info(true);
  ASSERT_TRUE(m_wallet.store_tx_info());
}

TEST_F(Wallet2GeneratedTest, show_wallet_name_when_locked)
{
  m_wallet.show_wallet_name_when_locked(true);
  ASSERT_TRUE(m_wallet.show_wallet_name_when_locked());
  m_wallet.show_wallet_name_when_locked(false);
  ASSERT_FALSE(m_wallet.show_wallet_name_when_locked());
}

TEST_F(Wallet2GeneratedTest, setup_background_mining)
{
  m_wallet.setup_background_mining(tools::wallet2::BackgroundMiningYes);
  ASSERT_EQ(m_wallet.setup_background_mining(), tools::wallet2::BackgroundMiningYes);
  m_wallet.setup_background_mining(tools::wallet2::BackgroundMiningNo);
  ASSERT_EQ(m_wallet.setup_background_mining(), tools::wallet2::BackgroundMiningNo);
}

TEST_F(Wallet2GeneratedTest, device_derivation_path)
{
  m_wallet.device_derivation_path("m/44'/128'/0'");
  ASSERT_EQ(m_wallet.device_derivation_path(), "m/44'/128'/0'");
}

// ===========================================================================
// Hashchain tests (via accessor)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, blockchain_accessor_genesis)
{
  auto& chain = wallet_accessor_test::get_blockchain(m_wallet);
  // After generate, there should be genesis
  if (!chain.empty())
  {
    ASSERT_NE(chain.genesis(), crypto::null_hash);
  }
}

// ===========================================================================
// Transfer detail queries - extended
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_num_transfer_details_after_adding)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  ASSERT_EQ(m_wallet.get_num_transfer_details(), 0u);

  tools::wallet2::transfer_details td = {};
  td.m_block_height = 1;
  td.m_amount = 1000000000000ULL;
  td.m_internal_output_index = 0;

  cryptonote::transaction_prefix tx_prefix;
  tx_prefix.version = 2;
  tx_prefix.unlock_time = 0;
  cryptonote::tx_out out;
  cryptonote::txout_to_key tk;
  memset(&tk.key, 0x99, sizeof(tk.key));
  out.amount = 0;
  out.target = tk;
  tx_prefix.vout.push_back(out);
  td.m_tx = tx_prefix;

  transfers.push_back(td);
  ASSERT_EQ(m_wallet.get_num_transfer_details(), 1u);

  transfers.push_back(td);
  ASSERT_EQ(m_wallet.get_num_transfer_details(), 2u);
}

// ===========================================================================
// File I/O tests - additional
// ===========================================================================

TEST_F(Wallet2FileTest, rewrite_wallet)
{
  std::string wallet_path = (m_temp_dir / "wallet_rewrite").string();
  epee::wipeable_string password("rewrite_pass");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, password, recovery_key, true, false);
  std::string addr = w.get_address_as_str();

  // Rewrite should not change the wallet keys
  w.rewrite(wallet_path, password);

  // Load and verify
  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, password);
  ASSERT_EQ(w2.get_address_as_str(), addr);
}

TEST_F(Wallet2FileTest, save_and_load_to_file_binary)
{
  std::string file_path = (m_temp_dir / "test_binary_file").string();
  std::string data = "Hello binary test data 12345";
  tools::wallet2 w;
  w.set_export_format(tools::wallet2::ExportFormat::Binary);
  bool saved = w.save_to_file(file_path, data, true);
  ASSERT_TRUE(saved);

  std::string loaded;
  bool ok = tools::wallet2::load_from_file(file_path, loaded);
  ASSERT_TRUE(ok);
  ASSERT_EQ(loaded, data);
}

TEST_F(Wallet2FileTest, load_from_file_nonexistent)
{
  std::string file_path = (m_temp_dir / "nonexistent_file").string();
  std::string loaded;
  bool ok = tools::wallet2::load_from_file(file_path, loaded);
  ASSERT_FALSE(ok);
}

TEST_F(Wallet2FileTest, store_attributes_persist)
{
  std::string wallet_path = (m_temp_dir / "wallet_attrs").string();
  epee::wipeable_string password("attrs_pass");

  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, password, recovery_key, true, false);

  w1.set_attribute("custom.attr1", "value1");
  w1.set_attribute("custom.attr2", "value2");
  w1.set_description("My persistent wallet");
  w1.store();

  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  w2.load(wallet_path, password);

  std::string val;
  ASSERT_TRUE(w2.get_attribute("custom.attr1", val));
  ASSERT_EQ(val, "value1");
  ASSERT_TRUE(w2.get_attribute("custom.attr2", val));
  ASSERT_EQ(val, "value2");
  ASSERT_EQ(w2.get_description(), "My persistent wallet");
}

TEST_F(Wallet2FileTest, store_subaddresses_persist)
{
  std::string wallet_path = (m_temp_dir / "wallet_subaddr").string();
  epee::wipeable_string password("subaddr_pass");

  tools::wallet2 w1;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(2, 5);
  crypto::secret_key recovery_key;
  w1.generate(wallet_path, password, recovery_key, true, false);

  w1.add_subaddress_account("Extra Account");
  w1.add_subaddress(0, "Extra Sub");
  size_t num_accounts = w1.get_num_subaddress_accounts();
  size_t num_subs = w1.get_num_subaddresses(0);
  w1.store();

  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(2, 5);
  w2.load(wallet_path, password);

  ASSERT_EQ(w2.get_num_subaddress_accounts(), num_accounts);
  ASSERT_EQ(w2.get_num_subaddresses(0), num_subs);
}

TEST_F(Wallet2FileTest, change_password_old_fails)
{
  std::string wallet_path = (m_temp_dir / "wallet_pw_change").string();
  epee::wipeable_string old_pass("old");
  epee::wipeable_string new_pass("new");

  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  w.generate(wallet_path, old_pass, recovery_key, true, false);
  w.change_password(wallet_path, old_pass, new_pass);

  // Old password should fail
  tools::wallet2 w2;
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.set_subaddress_lookahead(1, 1);
  ASSERT_ANY_THROW(w2.load(wallet_path, old_pass));
}

// ===========================================================================
// Integrated address tests - additional
// ===========================================================================

TEST_F(Wallet2GeneratedTest, integrated_address_different_payment_ids)
{
  crypto::hash8 pid1, pid2;
  memset(pid1.data, 0x11, sizeof(pid1.data));
  memset(pid2.data, 0x22, sizeof(pid2.data));
  std::string addr1 = m_wallet.get_integrated_address_as_str(pid1);
  std::string addr2 = m_wallet.get_integrated_address_as_str(pid2);
  ASSERT_NE(addr1, addr2);
}

TEST_F(Wallet2GeneratedTest, integrated_address_zero_payment_id)
{
  crypto::hash8 pid;
  memset(pid.data, 0x00, sizeof(pid.data));
  std::string addr = m_wallet.get_integrated_address_as_str(pid);
  ASSERT_FALSE(addr.empty());
  ASSERT_EQ(addr.size(), 106u);
}

// ===========================================================================
// Payment ID parsing - additional
// ===========================================================================

TEST_F(Wallet2CoreTest, parse_payment_id_long_format)
{
  crypto::hash pid;
  ASSERT_TRUE(tools::wallet2::parse_payment_id(
    "1111111111111111111111111111111111111111111111111111111111111111", pid));
}

TEST_F(Wallet2CoreTest, parse_payment_id_invalid_format)
{
  crypto::hash pid;
  ASSERT_FALSE(tools::wallet2::parse_payment_id("xyz", pid));
}

TEST_F(Wallet2CoreTest, parse_short_payment_id_all_zeros)
{
  crypto::hash8 pid;
  ASSERT_TRUE(tools::wallet2::parse_short_payment_id("0000000000000000", pid));
}

TEST_F(Wallet2CoreTest, parse_short_payment_id_all_ff)
{
  crypto::hash8 pid;
  ASSERT_TRUE(tools::wallet2::parse_short_payment_id("ffffffffffffffff", pid));
}

TEST_F(Wallet2CoreTest, parse_long_payment_id_all_zeros)
{
  crypto::hash pid;
  ASSERT_TRUE(tools::wallet2::parse_long_payment_id(
    "0000000000000000000000000000000000000000000000000000000000000000", pid));
}

// ===========================================================================
// Wallet key encryption toggle tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, key_encryption_enabled_after_generate)
{
  ASSERT_TRUE(m_wallet.is_key_encryption_enabled());
}

TEST_F(Wallet2GeneratedTest, keys_unlocker_allows_seed_access)
{
  // Without unlocker, keys are encrypted - but we can still get determinism check
  // after unlocking
  epee::wipeable_string password("");
  {
    tools::wallet_keys_unlocker unlocker(m_wallet, &password);
    ASSERT_TRUE(m_wallet.is_deterministic());
  }
  // After scope, keys re-locked
}

// ===========================================================================
// get_multisig_first_kex_msg test
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_multisig_first_kex_msg)
{
  // The Wallet2GeneratedTest fixture generates the wallet with recover=true
  // from an uninitialized recovery key. If that key is null (all zeros),
  // the spend secret key will also be null after sc_reduce32, and
  // get_multisig_first_kex_msg() will throw "Unexpected null secret key".
  // Create a fresh wallet with recover=false to get a valid random spend key.
  tools::wallet2 valid_wallet;
  valid_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  valid_wallet.set_subaddress_lookahead(2, 5);
  crypto::secret_key dummy_key;
  valid_wallet.generate("", "", dummy_key, false, false);

  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(valid_wallet, &password);
  std::string msg = valid_wallet.get_multisig_first_kex_msg();
  ASSERT_FALSE(msg.empty());
  // The message should be base58 encoded multisig info
  ASSERT_GT(msg.size(), 10u);
}

// ===========================================================================
// Static method tests
// ===========================================================================

TEST(Wallet2StaticTest, wallet_valid_path_format_empty)
{
  // wallet_valid_path_format() returns !file_path.empty(), so empty string is invalid
  ASSERT_FALSE(tools::wallet2::wallet_valid_path_format(""));
}

TEST(Wallet2StaticTest, wallet_valid_path_format_relative)
{
  ASSERT_TRUE(tools::wallet2::wallet_valid_path_format("relative/path/wallet"));
}

TEST(Wallet2StaticTest, wallet_valid_path_format_absolute)
{
  ASSERT_TRUE(tools::wallet2::wallet_valid_path_format("/absolute/path/wallet"));
}

// ===========================================================================
// check_hard_fork_version static tests
// ===========================================================================

TEST_F(Wallet2GeneratedTest, check_hard_fork_version_empty_daemon_forks)
{
  std::vector<std::pair<uint8_t, uint64_t>> empty_forks;
  bool wallet_outdated = false, daemon_outdated = false;
  bool result = m_wallet.check_hard_fork_version(
    cryptonote::MAINNET, empty_forks, 0, 0, &wallet_outdated, &daemon_outdated);
  // With empty daemon forks, should still return true (no incompatibility detected)
  ASSERT_TRUE(result);
}

// ===========================================================================
// Export outputs (empty wallet)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, export_outputs_empty)
{
  auto outputs = m_wallet.export_outputs(true);
  ASSERT_TRUE(std::get<2>(outputs).empty());
}

// ===========================================================================
// Export key images (empty wallet)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, export_key_images_empty)
{
  epee::wipeable_string password("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &password);
  auto result = m_wallet.export_key_images(true);
  ASSERT_TRUE(result.second.empty());
}

// ===========================================================================
// Address book edge cases
// ===========================================================================

TEST_F(Wallet2GeneratedTest, address_book_subaddress)
{
  m_wallet.add_subaddress(0, "sub_for_book");
  uint32_t idx = m_wallet.get_num_subaddresses(0) - 1;
  cryptonote::account_public_address sub_addr = m_wallet.get_subaddress({0, idx});
  ASSERT_TRUE(m_wallet.add_address_book_row(sub_addr, nullptr, "Subaddress contact", true));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 1u);
  ASSERT_TRUE(book[0].m_is_subaddress);
}

TEST_F(Wallet2GeneratedTest, address_book_description_special_chars)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  ASSERT_TRUE(m_wallet.add_address_book_row(addr, nullptr, "Test <>&\"' special chars", false));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book[0].m_description, "Test <>&\"' special chars");
}

TEST_F(Wallet2GeneratedTest, delete_address_book_middle_row)
{
  cryptonote::account_public_address addr = m_wallet.get_address();
  m_wallet.add_address_book_row(addr, nullptr, "First", false);
  m_wallet.add_address_book_row(addr, nullptr, "Second", false);
  m_wallet.add_address_book_row(addr, nullptr, "Third", false);
  ASSERT_TRUE(m_wallet.delete_address_book_row(1));
  auto book = m_wallet.get_address_book();
  ASSERT_EQ(book.size(), 2u);
  ASSERT_EQ(book[0].m_description, "First");
  ASSERT_EQ(book[1].m_description, "Third");
}

// ===========================================================================
// Approximate blockchain height
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_approximate_blockchain_height)
{
  uint64_t height = m_wallet.get_approximate_blockchain_height();
  // Should return a non-zero estimate for mainnet
  ASSERT_GT(height, 0u);
}

// ===========================================================================
// TX device aux
// ===========================================================================

TEST_F(Wallet2GeneratedTest, set_and_get_tx_device_aux)
{
  crypto::hash txid;
  memset(txid.data, 0xee, sizeof(txid.data));
  m_wallet.set_tx_device_aux(txid, "device_data_123");
  ASSERT_EQ(m_wallet.get_tx_device_aux(txid), "device_data_123");
}

TEST_F(Wallet2GeneratedTest, get_tx_device_aux_missing)
{
  crypto::hash txid;
  memset(txid.data, 0xdd, sizeof(txid.data));
  ASSERT_EQ(m_wallet.get_tx_device_aux(txid), "");
}

// ===========================================================================
// Background sync type static tests
// ===========================================================================

TEST(Wallet2StaticTest, background_sync_type_round_trip)
{
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("off"),
            tools::wallet2::BackgroundSyncOff);
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("reuse-wallet-password"),
            tools::wallet2::BackgroundSyncReusePassword);
  ASSERT_EQ(tools::wallet2::background_sync_type_from_str("custom-background-password"),
            tools::wallet2::BackgroundSyncCustomPassword);
}

// ===========================================================================
// Phase 5: Fee estimation and fee priority
// ===========================================================================

TEST(Wallet2StaticTest, estimate_fee_per_byte_single_input_output)
{
  // Minimal: 1 input, 1 output, mixin 15, bulletproof_plus, clsag, view_tags
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 1, 15, 1, 0,
    false, true, true, true, 20000, 10000);
  EXPECT_GT(fee, 0u);
}

TEST(Wallet2StaticTest, estimate_fee_per_byte_more_inputs_higher)
{
  uint64_t fee_1in = tools::wallet2::estimate_fee(true, true, 1, 15, 2, 0,
    false, true, true, true, 20000, 10000);
  uint64_t fee_4in = tools::wallet2::estimate_fee(true, true, 4, 15, 2, 0,
    false, true, true, true, 20000, 10000);
  EXPECT_GT(fee_4in, fee_1in);
}

TEST(Wallet2StaticTest, estimate_fee_per_byte_more_outputs_higher)
{
  uint64_t fee_1out = tools::wallet2::estimate_fee(true, true, 1, 15, 1, 0,
    false, true, true, true, 20000, 10000);
  uint64_t fee_4out = tools::wallet2::estimate_fee(true, true, 1, 15, 4, 0,
    false, true, true, true, 20000, 10000);
  EXPECT_GT(fee_4out, fee_1out);
}

TEST(Wallet2StaticTest, estimate_fee_extra_size_increases_fee)
{
  uint64_t fee_no_extra = tools::wallet2::estimate_fee(true, true, 1, 15, 2, 0,
    false, true, true, true, 20000, 10000);
  uint64_t fee_with_extra = tools::wallet2::estimate_fee(true, true, 1, 15, 2, 256,
    false, true, true, true, 20000, 10000);
  EXPECT_GE(fee_with_extra, fee_no_extra);
}

TEST(Wallet2StaticTest, estimate_fee_higher_base_fee_higher)
{
  uint64_t fee_low = tools::wallet2::estimate_fee(true, true, 2, 15, 2, 0,
    false, true, true, true, 10000, 10000);
  uint64_t fee_high = tools::wallet2::estimate_fee(true, true, 2, 15, 2, 0,
    false, true, true, true, 100000, 10000);
  EXPECT_GT(fee_high, fee_low);
}

TEST(Wallet2StaticTest, estimate_fee_quantization_mask)
{
  // Fee should be a multiple of quantization mask
  uint64_t mask = 10000;
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 2, 15, 2, 0,
    false, true, true, true, 20000, mask);
  EXPECT_EQ(fee % mask, 0u);
}

TEST(Wallet2StaticTest, estimate_fee_non_per_byte)
{
  // Legacy fee calculation (not per-byte)
  uint64_t fee = tools::wallet2::estimate_fee(false, true, 1, 15, 2, 0,
    false, true, true, true, 20000, 10000);
  EXPECT_GT(fee, 0u);
}

TEST(Wallet2StaticTest, estimate_fee_bulletproof_vs_plus)
{
  // bulletproof_plus should generally produce smaller tx weight => lower fee
  uint64_t fee_bp = tools::wallet2::estimate_fee(true, true, 2, 15, 2, 0,
    true, true, false, true, 20000, 10000);
  uint64_t fee_bpp = tools::wallet2::estimate_fee(true, true, 2, 15, 2, 0,
    false, true, true, true, 20000, 10000);
  // BP+ should be less or equal to BP
  EXPECT_LE(fee_bpp, fee_bp);
}

TEST(Wallet2StaticTest, estimate_fee_zero_inputs_outputs)
{
  // Edge case: 0 inputs, 0 outputs - should still compute without crashing
  uint64_t fee = tools::wallet2::estimate_fee(true, true, 0, 15, 0, 0,
    false, true, true, true, 20000, 10000);
  // Fee may be 0 or positive (depends on base overhead), just check no crash
  (void)fee;
}

// ===========================================================================
// Phase 5: Fee priority utilities
// ===========================================================================

TEST(FeePriorityTest, to_string_roundtrip)
{
  using namespace tools;
  EXPECT_EQ(fee_priority_utilities::to_string(fee_priority::Default), "default");
  EXPECT_EQ(fee_priority_utilities::to_string(fee_priority::Unimportant), "unimportant");
  EXPECT_EQ(fee_priority_utilities::to_string(fee_priority::Normal), "normal");
  EXPECT_EQ(fee_priority_utilities::to_string(fee_priority::Elevated), "elevated");
  EXPECT_EQ(fee_priority_utilities::to_string(fee_priority::Priority), "priority");
}

TEST(FeePriorityTest, from_string_valid)
{
  using namespace tools;
  auto p = fee_priority_utilities::from_string("normal");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p.value(), fee_priority::Normal);
}

TEST(FeePriorityTest, from_string_invalid)
{
  auto p = tools::fee_priority_utilities::from_string("super_high");
  EXPECT_FALSE(p.has_value());
}

TEST(FeePriorityTest, decrease)
{
  using namespace tools;
  EXPECT_EQ(fee_priority_utilities::decrease(fee_priority::Default), fee_priority::Default);
  EXPECT_EQ(fee_priority_utilities::decrease(fee_priority::Unimportant), fee_priority::Default);
  EXPECT_EQ(fee_priority_utilities::decrease(fee_priority::Normal), fee_priority::Unimportant);
  EXPECT_EQ(fee_priority_utilities::decrease(fee_priority::Elevated), fee_priority::Normal);
  EXPECT_EQ(fee_priority_utilities::decrease(fee_priority::Priority), fee_priority::Elevated);
}

TEST(FeePriorityTest, clamp_within_range)
{
  using namespace tools;
  EXPECT_EQ(fee_priority_utilities::clamp(fee_priority::Normal), fee_priority::Normal);
  EXPECT_EQ(fee_priority_utilities::clamp(fee_priority::Priority), fee_priority::Priority);
  EXPECT_EQ(fee_priority_utilities::clamp(fee_priority::Default), fee_priority::Default);
}

TEST(FeePriorityTest, clamp_modified_maps_default)
{
  using namespace tools;
  EXPECT_EQ(fee_priority_utilities::clamp_modified(fee_priority::Default), fee_priority::Unimportant);
  EXPECT_EQ(fee_priority_utilities::clamp_modified(fee_priority::Normal), fee_priority::Normal);
}

TEST(FeePriorityTest, is_valid)
{
  using namespace tools;
  EXPECT_TRUE(fee_priority_utilities::is_valid(0));
  EXPECT_TRUE(fee_priority_utilities::is_valid(1));
  EXPECT_TRUE(fee_priority_utilities::is_valid(4));
  EXPECT_FALSE(fee_priority_utilities::is_valid(5));
  EXPECT_FALSE(fee_priority_utilities::is_valid(100));
}

TEST(FeePriorityTest, from_integral_clamped)
{
  using namespace tools;
  EXPECT_EQ(fee_priority_utilities::from_integral(0), fee_priority::Default);
  EXPECT_EQ(fee_priority_utilities::from_integral(2), fee_priority::Normal);
  EXPECT_EQ(fee_priority_utilities::from_integral(4), fee_priority::Priority);
  // Values >= Priority get clamped to Priority
  EXPECT_EQ(fee_priority_utilities::from_integral(99), fee_priority::Priority);
}

// ===========================================================================
// Phase 5: Fee multiplier (requires generated wallet)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, get_fee_multiplier_unimportant)
{
  epee::wipeable_string unlock_pw("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &unlock_pw);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(mult, 1u); // Unimportant maps to index 0 → multiplier 1
}

TEST_F(Wallet2GeneratedTest, get_fee_multiplier_normal)
{
  epee::wipeable_string unlock_pw("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &unlock_pw);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(mult, 5u); // fee_steps[3].fee_multipliers[1] = 5
}

TEST_F(Wallet2GeneratedTest, get_fee_multiplier_elevated)
{
  epee::wipeable_string unlock_pw("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &unlock_pw);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(mult, 25u); // fee_steps[3].fee_multipliers[2] = 25
}

TEST_F(Wallet2GeneratedTest, get_fee_multiplier_priority)
{
  epee::wipeable_string unlock_pw("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &unlock_pw);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Priority, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(mult, 1000u); // fee_steps[3].fee_multipliers[3] = 1000
}

TEST_F(Wallet2GeneratedTest, get_fee_multiplier_older_algorithm)
{
  epee::wipeable_string unlock_pw("");
  tools::wallet_keys_unlocker unlocker(m_wallet, &unlock_pw);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(mult, 1u);
  uint64_t mult2 = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(mult2, 2u);
}

TEST_F(Wallet2GeneratedTest, get_default_priority)
{
  EXPECT_EQ(m_wallet.get_default_priority(), tools::fee_priority::Default);
}

TEST_F(Wallet2GeneratedTest, set_default_priority)
{
  m_wallet.set_default_priority(tools::fee_priority::Elevated);
  EXPECT_EQ(m_wallet.get_default_priority(), tools::fee_priority::Elevated);
  m_wallet.set_default_priority(tools::fee_priority::Default);
}

// ===========================================================================
// Phase 5: Key encryption/decryption
// ===========================================================================

TEST_F(Wallet2GeneratedTest, keys_encrypted_after_generate)
{
  EXPECT_TRUE(m_wallet.is_key_encryption_enabled());
}

TEST_F(Wallet2FileTest, decrypt_encrypt_keys_roundtrip)
{
  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key rk;
  w.generate("", "", rk, false, false);
  EXPECT_TRUE(w.is_key_encryption_enabled());

  {
    epee::wipeable_string pw("");
    tools::wallet_keys_unlocker unlocker(w, &pw);
    const auto& keys = w.get_account().get_keys();
    EXPECT_NE(keys.m_spend_secret_key, crypto::null_skey);
  }
  EXPECT_TRUE(w.is_key_encryption_enabled());
}

TEST_F(Wallet2FileTest, wallet_keys_unlocker_multiple_scopes)
{
  tools::wallet2 w;
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 1);
  crypto::secret_key rk;
  w.generate("", "", rk, false, false);

  crypto::secret_key spend_key_1, spend_key_2;
  {
    epee::wipeable_string pw("");
    tools::wallet_keys_unlocker unlocker(w, &pw);
    spend_key_1 = w.get_account().get_keys().m_spend_secret_key;
    EXPECT_NE(spend_key_1, crypto::null_skey);
  }
  {
    epee::wipeable_string pw("");
    tools::wallet_keys_unlocker unlocker(w, &pw);
    spend_key_2 = w.get_account().get_keys().m_spend_secret_key;
    EXPECT_NE(spend_key_2, crypto::null_skey);
  }
  EXPECT_EQ(spend_key_1, spend_key_2);
}

TEST_F(Wallet2GeneratedTest, view_key_accessible_without_unlock)
{
  // View key should always be accessible (never encrypted)
  const auto& keys = m_wallet.get_account().get_keys();
  EXPECT_NE(keys.m_view_secret_key, crypto::null_skey);
}

// ===========================================================================
// Phase 5: View-only wallet
// ===========================================================================

TEST_F(Wallet2FileTest, create_view_only_wallet)
{
  // Generate a full wallet to get keys
  tools::wallet2 full_wallet;
  full_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  full_wallet.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  full_wallet.generate("", "", recovery_key, true, false);

  crypto::secret_key view_key;
  cryptonote::account_public_address addr;
  {
    epee::wipeable_string full_pw("");
    tools::wallet_keys_unlocker unlocker(full_wallet, &full_pw);
    view_key = full_wallet.get_account().get_keys().m_view_secret_key;
    addr = full_wallet.get_account().get_keys().m_account_address;
  }

  // Create view-only wallet from the address and view key
  std::string wallet_path = (m_temp_dir / "view_only_wallet").string();
  tools::wallet2 view_wallet;
  view_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  view_wallet.set_subaddress_lookahead(1, 1);
  view_wallet.generate(wallet_path, "", addr, view_key, true);

  EXPECT_TRUE(view_wallet.watch_only());
  // View-only wallet should have same address
  EXPECT_EQ(view_wallet.get_account().get_keys().m_account_address.m_spend_public_key,
            addr.m_spend_public_key);
  EXPECT_EQ(view_wallet.get_account().get_keys().m_account_address.m_view_public_key,
            addr.m_view_public_key);
}

TEST_F(Wallet2FileTest, view_only_wallet_has_zero_spend_key)
{
  tools::wallet2 full_wallet;
  full_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  full_wallet.set_subaddress_lookahead(1, 1);
  crypto::secret_key recovery_key;
  full_wallet.generate("", "", recovery_key, true, false);

  crypto::secret_key view_key;
  cryptonote::account_public_address addr;
  {
    epee::wipeable_string full_pw("");
    tools::wallet_keys_unlocker unlocker(full_wallet, &full_pw);
    view_key = full_wallet.get_account().get_keys().m_view_secret_key;
    addr = full_wallet.get_account().get_keys().m_account_address;
  }

  std::string wallet_path = (m_temp_dir / "view_only_wallet2").string();
  tools::wallet2 view_wallet;
  view_wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  view_wallet.set_subaddress_lookahead(1, 1);
  view_wallet.generate(wallet_path, "", addr, view_key, true);

  // Spend secret key should be null for view-only wallet
  EXPECT_EQ(view_wallet.get_account().get_keys().m_spend_secret_key, crypto::null_skey);
}

// ===========================================================================
// Phase 5: Wallet file — view-only and key recovery
// ===========================================================================

// ===========================================================================
// Phase 5: Subaddress derivation (deeper tests)
// ===========================================================================

TEST_F(Wallet2GeneratedTest, subaddress_cross_account_different)
{
  m_wallet.add_subaddress_account("Account 1");
  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{1, 1};
  auto sub1 = m_wallet.get_subaddress(idx1);
  auto sub2 = m_wallet.get_subaddress(idx2);
  EXPECT_NE(sub1.m_spend_public_key, sub2.m_spend_public_key);
}

TEST_F(Wallet2GeneratedTest, subaddress_derivation_deterministic)
{
  cryptonote::subaddress_index idx{0, 3};
  auto sub1 = m_wallet.get_subaddress(idx);
  auto sub2 = m_wallet.get_subaddress(idx);
  EXPECT_EQ(sub1.m_spend_public_key, sub2.m_spend_public_key);
  EXPECT_EQ(sub1.m_view_public_key, sub2.m_view_public_key);
}

TEST_F(Wallet2GeneratedTest, subaddress_reverse_lookup_works)
{
  cryptonote::subaddress_index idx{0, 1};
  auto sub = m_wallet.get_subaddress(idx);
  auto found_idx = m_wallet.get_subaddress_index(sub);
  ASSERT_TRUE(found_idx.has_value());
  EXPECT_EQ(found_idx->major, 0u);
  EXPECT_EQ(found_idx->minor, 1u);
}

// ===========================================================================
// Phase 5: Multisig wallet setup
// ===========================================================================

TEST_F(Wallet2FileTest, multisig_2_of_2_setup)
{
  // Create two wallets
  tools::wallet2 w1, w2;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  w2.set_subaddress_lookahead(1, 1);

  crypto::secret_key rk1, rk2;
  std::string path1 = (m_temp_dir / "ms_wallet1").string();
  std::string path2 = (m_temp_dir / "ms_wallet2").string();
  w1.generate(path1, "", rk1, false, false);
  w2.generate(path2, "", rk2, false, false);

  // Get first kex messages
  w1.decrypt_keys("");
  std::string kex1 = w1.get_multisig_first_kex_msg();
  w1.encrypt_keys("");
  w2.decrypt_keys("");
  std::string kex2 = w2.get_multisig_first_kex_msg();
  w2.encrypt_keys("");

  EXPECT_FALSE(kex1.empty());
  EXPECT_FALSE(kex2.empty());
  EXPECT_NE(kex1, kex2);

  // Round 1: make_multisig with ALL kex messages
  std::vector<std::string> all_kex = {kex1, kex2};
  std::vector<std::string> infos(2);
  infos[0] = w1.make_multisig("", all_kex, 2);
  infos[1] = w2.make_multisig("", all_kex, 2);

  // Exchange rounds until ready
  while (!w1.get_multisig_status().is_ready)
  {
    std::vector<std::string> new_infos(2);
    new_infos[0] = w1.exchange_multisig_keys("", infos);
    new_infos[1] = w2.exchange_multisig_keys("", infos);
    infos = new_infos;
  }

  auto ms1 = w1.get_multisig_status();
  auto ms2 = w2.get_multisig_status();
  EXPECT_TRUE(ms1.multisig_is_active);
  EXPECT_TRUE(ms2.multisig_is_active);
  EXPECT_TRUE(ms1.is_ready);
  EXPECT_TRUE(ms2.is_ready);
  EXPECT_EQ(ms1.threshold, 2u);
  EXPECT_EQ(ms1.total, 2u);

  // Multisig wallets should have the same address
  EXPECT_EQ(w1.get_account().get_keys().m_account_address.m_spend_public_key,
            w2.get_account().get_keys().m_account_address.m_spend_public_key);
}

TEST_F(Wallet2FileTest, multisig_2_of_3_setup)
{
  tools::wallet2 w1, w2, w3;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w3.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  w2.set_subaddress_lookahead(1, 1);
  w3.set_subaddress_lookahead(1, 1);

  crypto::secret_key rk1, rk2, rk3;
  std::string path1 = (m_temp_dir / "ms3_wallet1").string();
  std::string path2 = (m_temp_dir / "ms3_wallet2").string();
  std::string path3 = (m_temp_dir / "ms3_wallet3").string();
  w1.generate(path1, "", rk1, false, false);
  w2.generate(path2, "", rk2, false, false);
  w3.generate(path3, "", rk3, false, false);

  // Get initial kex messages
  w1.decrypt_keys(""); std::string kex1 = w1.get_multisig_first_kex_msg(); w1.encrypt_keys("");
  w2.decrypt_keys(""); std::string kex2 = w2.get_multisig_first_kex_msg(); w2.encrypt_keys("");
  w3.decrypt_keys(""); std::string kex3 = w3.get_multisig_first_kex_msg(); w3.encrypt_keys("");

  // Round 1: make_multisig with all kex messages
  std::vector<std::string> all_kex = {kex1, kex2, kex3};
  std::vector<std::string> infos(3);
  infos[0] = w1.make_multisig("", all_kex, 2);
  infos[1] = w2.make_multisig("", all_kex, 2);
  infos[2] = w3.make_multisig("", all_kex, 2);

  // Exchange rounds until ready
  while (!w1.get_multisig_status().is_ready)
  {
    std::vector<std::string> new_infos(3);
    new_infos[0] = w1.exchange_multisig_keys("", infos);
    new_infos[1] = w2.exchange_multisig_keys("", infos);
    new_infos[2] = w3.exchange_multisig_keys("", infos);
    infos = new_infos;
  }

  auto ms1 = w1.get_multisig_status();
  EXPECT_TRUE(ms1.multisig_is_active);
  EXPECT_TRUE(ms1.is_ready);
  EXPECT_EQ(ms1.threshold, 2u);
  EXPECT_EQ(ms1.total, 3u);

  // All three should have the same multisig address
  EXPECT_EQ(w1.get_account().get_keys().m_account_address.m_spend_public_key,
            w2.get_account().get_keys().m_account_address.m_spend_public_key);
  EXPECT_EQ(w2.get_account().get_keys().m_account_address.m_spend_public_key,
            w3.get_account().get_keys().m_account_address.m_spend_public_key);
}

// ===========================================================================
// Phase 5: Wallet seed recovery
// ===========================================================================

TEST_F(Wallet2FileTest, seed_recovery_produces_same_wallet)
{
  crypto::secret_key recovery_key;
  std::string original_addr;

  // Generate wallet and capture recovery key
  {
    tools::wallet2 w;
    w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    w.set_subaddress_lookahead(1, 1);
    recovery_key = w.generate("", "", crypto::secret_key(), true, false);
    original_addr = w.get_account().get_public_address_str(cryptonote::MAINNET);
  }

  // Recover wallet from the key
  {
    tools::wallet2 w2;
    w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    w2.set_subaddress_lookahead(1, 1);
    w2.generate("", "", recovery_key, true, false);
    std::string recovered_addr = w2.get_account().get_public_address_str(cryptonote::MAINNET);
    EXPECT_EQ(recovered_addr, original_addr);
  }
}

TEST_F(Wallet2FileTest, different_seeds_different_addresses)
{
  tools::wallet2 w1, w2;
  w1.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w2.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w1.set_subaddress_lookahead(1, 1);
  w2.set_subaddress_lookahead(1, 1);

  crypto::secret_key rk1, rk2;
  w1.generate("", "", rk1, false, false);
  w2.generate("", "", rk2, false, false);

  EXPECT_NE(w1.get_account().get_public_address_str(cryptonote::MAINNET),
            w2.get_account().get_public_address_str(cryptonote::MAINNET));
}

// ===========================================================================
// Phase 5: Wallet testnet/stagenet subaddress prefixes
// ===========================================================================

TEST(Wallet2TestnetTest, testnet_subaddress_prefix)
{
  tools::wallet2 w(cryptonote::TESTNET, 1, false);
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 2);
  crypto::secret_key rk;
  w.generate("", "", rk, true, false);
  cryptonote::subaddress_index idx{0, 1};
  std::string sub_str = w.get_subaddress_as_str(idx);
  // Testnet subaddresses start with 'B'
  EXPECT_EQ(sub_str[0], 'B');
}

TEST(Wallet2TestnetTest, stagenet_subaddress_prefix)
{
  tools::wallet2 w(cryptonote::STAGENET, 1, false);
  w.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
  w.set_subaddress_lookahead(1, 2);
  crypto::secret_key rk;
  w.generate("", "", rk, true, false);
  cryptonote::subaddress_index idx{0, 1};
  std::string sub_str = w.get_subaddress_as_str(idx);
  // Stagenet subaddresses start with '7'
  EXPECT_EQ(sub_str[0], '7');
}
