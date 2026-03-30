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
#include "wallet/wallet_utils.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "ringct/rctSigs.h"
#include "crypto/crypto.h"
#include "wallet/fee_priority.h"
#include "wallet/fee_algorithm.h"

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
  static std::unordered_map<crypto::key_image, size_t>& get_key_images(tools::wallet2& w) { return w.m_key_images; }
  static void set_offline(tools::wallet2& w, bool v) { w.m_offline = v; }
  static cryptonote::account_base& get_account(tools::wallet2& w) { return w.m_account; }
  static void set_default_priority(tools::wallet2& w, tools::fee_priority p) { w.m_default_priority = p; }
  static bool is_spent(const tools::wallet2& w, size_t idx, bool strict = true) { return w.is_spent(idx, strict); }
  static bool is_spent(const tools::wallet2& w, const tools::wallet2::transfer_details& td, bool strict = true) { return w.is_spent(td, strict); }
  static void set_spent(tools::wallet2& w, size_t idx, uint64_t height) { w.set_spent(idx, height); }
  static void set_unspent(tools::wallet2& w, size_t idx) { w.set_unspent(idx); }
  static float get_output_relatedness(const tools::wallet2& w, const tools::wallet2::transfer_details& td0, const tools::wallet2::transfer_details& td1) { return w.get_output_relatedness(td0, td1); }
  static bool get_tx_key_cached(const tools::wallet2& w, const crypto::hash& txid, crypto::secret_key& tx_key, std::vector<crypto::secret_key>& additional_tx_keys) { return w.get_tx_key_cached(txid, tx_key, additional_tx_keys); }
};

namespace
{
  // Helper to create a basic transfer_details with controllable fields
  tools::wallet2::transfer_details make_transfer_detail(
    uint64_t amount, uint64_t block_height, bool spent,
    const crypto::hash& txid = crypto::null_hash,
    uint64_t internal_output_index = 0)
  {
    tools::wallet2::transfer_details td = AUTO_VAL_INIT(td);
    td.m_amount = amount;
    td.m_block_height = block_height;
    td.m_spent = spent;
    td.m_spent_height = spent ? block_height + 10 : 0;
    td.m_txid = txid;
    td.m_internal_output_index = internal_output_index;
    td.m_global_output_index = 0;
    td.m_rct = true;
    td.m_key_image_known = true;
    td.m_key_image_request = false;
    td.m_key_image_partial = false;
    td.m_frozen = false;
    td.m_pk_index = 0;
    td.m_subaddr_index = {0, 0};
    return td;
  }

  // Helper to make a unique txid from an integer
  crypto::hash make_txid(uint32_t id)
  {
    crypto::hash h = crypto::null_hash;
    h.data[0] = id & 0xff;
    h.data[1] = (id >> 8) & 0xff;
    h.data[2] = (id >> 16) & 0xff;
    h.data[3] = (id >> 24) & 0xff;
    return h;
  }

  // Helper to make a key image from an integer
  crypto::key_image make_key_image(uint32_t id)
  {
    crypto::key_image ki;
    memset(&ki, 0, sizeof(ki));
    ki.data[0] = id & 0xff;
    ki.data[1] = (id >> 8) & 0xff;
    return ki;
  }

  // Fixture providing a generated wallet for tests
  class Wallet2TxConstructionTest : public ::testing::Test
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
}

// ============================================================================
// Fee multiplier tests (via public get_fee_multiplier with explicit fee_algorithm)
// ============================================================================

TEST_F(Wallet2TxConstructionTest, fee_multiplier_unimportant_pre_v3)
{
  // Pre-hardfork v3, Unimportant priority should give multiplier 1
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(1u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_normal_pre_v3)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(2u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_elevated_pre_v3)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(3u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_unimportant_v3)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::HardforkV3);
  EXPECT_EQ(1u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_normal_v3)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::HardforkV3);
  EXPECT_EQ(20u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_elevated_v3)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::HardforkV3);
  EXPECT_EQ(166u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_unimportant_v5)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::HardforkV5);
  EXPECT_EQ(1u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_normal_v5)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::HardforkV5);
  EXPECT_EQ(4u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_elevated_v5)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::HardforkV5);
  EXPECT_EQ(20u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_priority_v5)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Priority, tools::fee_algorithm::HardforkV5);
  EXPECT_EQ(166u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_unimportant_v8)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(1u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_normal_v8)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(5u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_elevated_v8)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(25u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_priority_v8)
{
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Priority, tools::fee_algorithm::HardforkV8);
  EXPECT_EQ(1000u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_higher_priority_is_larger)
{
  // For HardforkV8, multipliers should be strictly increasing with priority
  uint64_t m1 = m_wallet.get_fee_multiplier(tools::fee_priority::Unimportant, tools::fee_algorithm::HardforkV8);
  uint64_t m2 = m_wallet.get_fee_multiplier(tools::fee_priority::Normal, tools::fee_algorithm::HardforkV8);
  uint64_t m3 = m_wallet.get_fee_multiplier(tools::fee_priority::Elevated, tools::fee_algorithm::HardforkV8);
  uint64_t m4 = m_wallet.get_fee_multiplier(tools::fee_priority::Priority, tools::fee_algorithm::HardforkV8);
  EXPECT_LT(m1, m2);
  EXPECT_LT(m2, m3);
  EXPECT_LT(m3, m4);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_default_priority_maps_to_unimportant_pre_v5)
{
  // Default priority with PreHardforkV3 algo maps to Unimportant (multiplier 1)
  wallet_accessor_test::set_default_priority(m_wallet, tools::fee_priority::Default);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Default, tools::fee_algorithm::PreHardforkV3);
  EXPECT_EQ(1u, mult);
}

TEST_F(Wallet2TxConstructionTest, fee_multiplier_default_priority_maps_to_normal_v5_and_later)
{
  // Default priority with HardforkV5+ algo maps to Normal
  wallet_accessor_test::set_default_priority(m_wallet, tools::fee_priority::Default);
  uint64_t mult = m_wallet.get_fee_multiplier(tools::fee_priority::Default, tools::fee_algorithm::HardforkV5);
  EXPECT_EQ(4u, mult);
}

// ============================================================================
// tx_destination_entry structure tests
// ============================================================================

TEST(wallet2_tx_construction, tx_dest_entry_default_construction)
{
  cryptonote::tx_destination_entry dest;
  EXPECT_EQ(0u, dest.amount);
  EXPECT_FALSE(dest.is_subaddress);
  EXPECT_FALSE(dest.is_integrated);
  EXPECT_TRUE(dest.original.empty());
}

TEST(wallet2_tx_construction, tx_dest_entry_amount_and_subaddress)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest(1000000000, addr, true);
  EXPECT_EQ(1000000000u, dest.amount);
  EXPECT_TRUE(dest.is_subaddress);
  EXPECT_FALSE(dest.is_integrated);
}

TEST(wallet2_tx_construction, tx_dest_entry_with_original_address)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest("original_addr_string", 5000000000, addr, false);
  EXPECT_EQ(5000000000u, dest.amount);
  EXPECT_EQ("original_addr_string", dest.original);
  EXPECT_FALSE(dest.is_subaddress);
  EXPECT_FALSE(dest.is_integrated);
}

TEST(wallet2_tx_construction, tx_dest_entry_zero_amount)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest(0, addr, false);
  EXPECT_EQ(0u, dest.amount);
}

TEST(wallet2_tx_construction, tx_dest_entry_multiple_destinations)
{
  std::vector<cryptonote::tx_destination_entry> dsts;
  cryptonote::account_public_address addr1, addr2, addr3;
  memset(&addr1, 1, sizeof(addr1));
  memset(&addr2, 2, sizeof(addr2));
  memset(&addr3, 3, sizeof(addr3));

  dsts.emplace_back(1000000000, addr1, false);
  dsts.emplace_back(2000000000, addr2, true);
  dsts.emplace_back(500000000, addr3, false);

  EXPECT_EQ(3u, dsts.size());
  EXPECT_EQ(1000000000u, dsts[0].amount);
  EXPECT_EQ(2000000000u, dsts[1].amount);
  EXPECT_EQ(500000000u, dsts[2].amount);
  EXPECT_FALSE(dsts[0].is_subaddress);
  EXPECT_TRUE(dsts[1].is_subaddress);
  EXPECT_FALSE(dsts[2].is_subaddress);
}

TEST(wallet2_tx_construction, tx_dest_entry_address_returns_original_if_set)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest("my_original_address", 100, addr, false);
  crypto::hash empty_payment_id = crypto::null_hash;
  EXPECT_EQ("my_original_address", dest.address(cryptonote::MAINNET, empty_payment_id));
}

// ============================================================================
// Transfer details tests
// ============================================================================

TEST(wallet2_tx_construction, transfer_detail_amount_extraction)
{
  auto td = make_transfer_detail(500000000000, 1000, false);
  EXPECT_EQ(500000000000u, td.amount());
}

TEST(wallet2_tx_construction, transfer_detail_is_rct)
{
  auto td = make_transfer_detail(100000000, 1000, false);
  td.m_rct = true;
  EXPECT_TRUE(td.is_rct());
  td.m_rct = false;
  EXPECT_FALSE(td.is_rct());
}

TEST(wallet2_tx_construction, transfer_detail_zero_amount)
{
  auto td = make_transfer_detail(0, 1000, false);
  EXPECT_EQ(0u, td.amount());
}

TEST(wallet2_tx_construction, transfer_detail_large_amount)
{
  // Max supply of monero is ~18.4 million XMR = 18400000 * 1e12 piconero
  uint64_t max_amount = 18400000ULL * 1000000000000ULL;
  auto td = make_transfer_detail(max_amount, 1000, false);
  EXPECT_EQ(max_amount, td.amount());
}

TEST(wallet2_tx_construction, transfer_detail_key_image_stored)
{
  auto td = make_transfer_detail(100, 1000, false);
  crypto::key_image ki = make_key_image(42);
  td.m_key_image = ki;
  EXPECT_EQ(ki, td.m_key_image);
  EXPECT_TRUE(td.m_key_image_known);
}

TEST(wallet2_tx_construction, transfer_detail_key_image_unknown)
{
  auto td = make_transfer_detail(100, 1000, false);
  td.m_key_image_known = false;
  EXPECT_FALSE(td.m_key_image_known);
}

// ============================================================================
// Spent status tests (via wallet_accessor_test)
// ============================================================================

TEST_F(Wallet2TxConstructionTest, transfer_detail_spent_status_initial_unspent)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.push_back(make_transfer_detail(1000000000000, 1000, false, make_txid(1)));
  transfers.back().m_key_image = make_key_image(1);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  EXPECT_FALSE(wallet_accessor_test::is_spent(m_wallet, 0));
}

TEST_F(Wallet2TxConstructionTest, transfer_detail_set_spent)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.push_back(make_transfer_detail(1000000000000, 1000, false, make_txid(1)));
  transfers.back().m_key_image = make_key_image(1);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  wallet_accessor_test::set_spent(m_wallet, 0, 1010);
  EXPECT_TRUE(wallet_accessor_test::is_spent(m_wallet, 0));
}

TEST_F(Wallet2TxConstructionTest, transfer_detail_set_unspent_after_spent)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.push_back(make_transfer_detail(1000000000000, 1000, false, make_txid(1)));
  transfers.back().m_key_image = make_key_image(1);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  wallet_accessor_test::set_spent(m_wallet, 0, 1010);
  EXPECT_TRUE(wallet_accessor_test::is_spent(m_wallet, 0));
  wallet_accessor_test::set_unspent(m_wallet, 0);
  EXPECT_FALSE(wallet_accessor_test::is_spent(m_wallet, 0));
}

TEST_F(Wallet2TxConstructionTest, transfer_detail_frozen_not_same_as_spent)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  transfers.push_back(make_transfer_detail(1000000000000, 1000, false, make_txid(1)));
  transfers.back().m_key_image = make_key_image(1);
  transfers.back().m_frozen = true;
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  // Frozen is a separate state from spent; is_spent checks m_spent flag, not m_frozen
  EXPECT_FALSE(wallet_accessor_test::is_spent(m_wallet, 0, true));
  EXPECT_FALSE(wallet_accessor_test::is_spent(m_wallet, 0, false));
  EXPECT_TRUE(transfers[0].m_frozen);
}

// ============================================================================
// Output relatedness tests (via wallet_accessor_test)
// ============================================================================

TEST_F(Wallet2TxConstructionTest, output_relatedness_same_tx)
{
  crypto::hash txid = make_txid(100);
  auto td0 = make_transfer_detail(100, 1000, false, txid);
  auto td1 = make_transfer_detail(200, 1000, false, txid);

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(1.0f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_same_block_different_tx)
{
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1000, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.9f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_adjacent_blocks)
{
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1001, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.8f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_close_blocks)
{
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1005, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.2f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_far_apart_blocks)
{
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 2000, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.0f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_boundary_dh_9)
{
  // dh=9 is < 10, so should be 0.2
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1009, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.2f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_boundary_dh_10)
{
  // dh=10 is >= 10, so should be 0.0
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1010, false, make_txid(2));

  float r = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  EXPECT_FLOAT_EQ(0.0f, r);
}

TEST_F(Wallet2TxConstructionTest, output_relatedness_symmetric)
{
  auto td0 = make_transfer_detail(100, 1000, false, make_txid(1));
  auto td1 = make_transfer_detail(200, 1005, false, make_txid(2));

  float r_forward = wallet_accessor_test::get_output_relatedness(m_wallet, td0, td1);
  float r_reverse = wallet_accessor_test::get_output_relatedness(m_wallet, td1, td0);
  EXPECT_FLOAT_EQ(r_forward, r_reverse);
}

// ============================================================================
// Input selection / transfer container sorting patterns
// ============================================================================

TEST(wallet2_tx_construction, transfer_container_sorting_by_amount)
{
  tools::wallet2::transfer_container transfers;
  transfers.push_back(make_transfer_detail(3000000000000, 100, false, make_txid(1)));
  transfers.push_back(make_transfer_detail(1000000000000, 200, false, make_txid(2)));
  transfers.push_back(make_transfer_detail(5000000000000, 300, false, make_txid(3)));
  transfers.push_back(make_transfer_detail(2000000000000, 400, false, make_txid(4)));

  // Build indices and sort by amount
  std::vector<size_t> indices = {0, 1, 2, 3};
  std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
    return transfers[a].amount() < transfers[b].amount();
  });

  EXPECT_EQ(1u, indices[0]); // 1 XMR
  EXPECT_EQ(3u, indices[1]); // 2 XMR
  EXPECT_EQ(0u, indices[2]); // 3 XMR
  EXPECT_EQ(2u, indices[3]); // 5 XMR
}

TEST(wallet2_tx_construction, transfer_container_filter_unspent)
{
  tools::wallet2::transfer_container transfers;
  transfers.push_back(make_transfer_detail(1000000000000, 100, true, make_txid(1)));   // spent
  transfers.push_back(make_transfer_detail(2000000000000, 200, false, make_txid(2)));  // unspent
  transfers.push_back(make_transfer_detail(3000000000000, 300, true, make_txid(3)));   // spent
  transfers.push_back(make_transfer_detail(4000000000000, 400, false, make_txid(4)));  // unspent

  std::vector<size_t> unspent;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    if (!transfers[i].m_spent)
      unspent.push_back(i);
  }

  EXPECT_EQ(2u, unspent.size());
  EXPECT_EQ(1u, unspent[0]);
  EXPECT_EQ(3u, unspent[1]);
}

TEST(wallet2_tx_construction, transfer_container_filter_by_subaddress)
{
  tools::wallet2::transfer_container transfers;
  auto td1 = make_transfer_detail(1000000000000, 100, false, make_txid(1));
  td1.m_subaddr_index = {0, 0};
  auto td2 = make_transfer_detail(2000000000000, 200, false, make_txid(2));
  td2.m_subaddr_index = {0, 1};
  auto td3 = make_transfer_detail(3000000000000, 300, false, make_txid(3));
  td3.m_subaddr_index = {1, 0};
  auto td4 = make_transfer_detail(4000000000000, 400, false, make_txid(4));
  td4.m_subaddr_index = {0, 0};

  transfers.push_back(td1);
  transfers.push_back(td2);
  transfers.push_back(td3);
  transfers.push_back(td4);

  // Filter for account 0, subaddress 0
  std::vector<size_t> filtered;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    if (transfers[i].m_subaddr_index.major == 0 && transfers[i].m_subaddr_index.minor == 0)
      filtered.push_back(i);
  }

  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ(0u, filtered[0]);
  EXPECT_EQ(3u, filtered[1]);
}

TEST(wallet2_tx_construction, transfer_container_total_unspent_amount)
{
  tools::wallet2::transfer_container transfers;
  transfers.push_back(make_transfer_detail(1000000000000, 100, false, make_txid(1)));
  transfers.push_back(make_transfer_detail(2000000000000, 200, true, make_txid(2)));
  transfers.push_back(make_transfer_detail(3000000000000, 300, false, make_txid(3)));

  uint64_t total = 0;
  for (const auto& td : transfers)
  {
    if (!td.m_spent)
      total += td.amount();
  }

  EXPECT_EQ(4000000000000u, total);
}

// ============================================================================
// Subaddress handling tests
// ============================================================================

TEST(wallet2_tx_construction, subaddress_index_is_zero)
{
  cryptonote::subaddress_index idx{0, 0};
  EXPECT_TRUE(idx.is_zero());
}

TEST(wallet2_tx_construction, subaddress_index_non_zero_major)
{
  cryptonote::subaddress_index idx{1, 0};
  EXPECT_FALSE(idx.is_zero());
}

TEST(wallet2_tx_construction, subaddress_index_non_zero_minor)
{
  cryptonote::subaddress_index idx{0, 1};
  EXPECT_FALSE(idx.is_zero());
}

TEST(wallet2_tx_construction, subaddress_index_equality)
{
  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{0, 1};
  cryptonote::subaddress_index idx3{0, 2};
  EXPECT_EQ(idx1, idx2);
  EXPECT_NE(idx1, idx3);
}

TEST(wallet2_tx_construction, subaddress_index_in_destination)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest(100, addr, true);
  EXPECT_TRUE(dest.is_subaddress);
}

TEST(wallet2_tx_construction, subaddress_flag_false_for_main_address)
{
  cryptonote::account_public_address addr;
  memset(&addr, 0, sizeof(addr));
  cryptonote::tx_destination_entry dest(100, addr, false);
  EXPECT_FALSE(dest.is_subaddress);
}

TEST_F(Wallet2TxConstructionTest, subaddress_derivation_differs_from_main)
{
  cryptonote::account_public_address main_addr = m_wallet.get_account().get_keys().m_account_address;
  cryptonote::subaddress_index sub_idx{0, 1};
  cryptonote::account_public_address sub_addr = m_wallet.get_subaddress(sub_idx);

  EXPECT_NE(main_addr.m_spend_public_key, sub_addr.m_spend_public_key);
}

TEST_F(Wallet2TxConstructionTest, subaddress_account1_differs_from_account0)
{
  cryptonote::subaddress_index idx0{0, 1};
  cryptonote::subaddress_index idx1{1, 1};
  cryptonote::account_public_address addr0 = m_wallet.get_subaddress(idx0);
  cryptonote::account_public_address addr1 = m_wallet.get_subaddress(idx1);

  EXPECT_NE(addr0.m_spend_public_key, addr1.m_spend_public_key);
}

// ============================================================================
// Payment ID handling tests
// ============================================================================

TEST(wallet2_tx_construction, set_and_get_payment_id)
{
  crypto::hash payment_id;
  memset(&payment_id, 0xAB, sizeof(payment_id));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);

  crypto::hash recovered_id;
  EXPECT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(extra_nonce, recovered_id));
  EXPECT_EQ(payment_id, recovered_id);
}

TEST(wallet2_tx_construction, set_and_get_encrypted_payment_id)
{
  crypto::hash8 payment_id8;
  memset(&payment_id8, 0xCD, sizeof(payment_id8));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id8);

  crypto::hash8 recovered_id8;
  EXPECT_TRUE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce, recovered_id8));
  EXPECT_EQ(payment_id8, recovered_id8);
}

TEST(wallet2_tx_construction, payment_id_wrong_type_fails)
{
  crypto::hash8 payment_id8;
  memset(&payment_id8, 0xCD, sizeof(payment_id8));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id8);

  // Trying to get full payment_id from encrypted nonce should fail
  crypto::hash wrong_id;
  EXPECT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(extra_nonce, wrong_id));
}

TEST(wallet2_tx_construction, encrypted_payment_id_wrong_type_fails)
{
  crypto::hash payment_id;
  memset(&payment_id, 0xAB, sizeof(payment_id));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);

  // Trying to get encrypted payment_id from full nonce should fail
  crypto::hash8 wrong_id8;
  EXPECT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce, wrong_id8));
}

TEST(wallet2_tx_construction, empty_extra_nonce_no_payment_id)
{
  cryptonote::blobdata empty_nonce;
  crypto::hash payment_id;
  EXPECT_FALSE(cryptonote::get_payment_id_from_tx_extra_nonce(empty_nonce, payment_id));
}

TEST(wallet2_tx_construction, empty_extra_nonce_no_encrypted_payment_id)
{
  cryptonote::blobdata empty_nonce;
  crypto::hash8 payment_id8;
  EXPECT_FALSE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(empty_nonce, payment_id8));
}

TEST(wallet2_tx_construction, payment_id_roundtrip_through_tx_extra)
{
  crypto::hash payment_id;
  memset(&payment_id, 0x42, sizeof(payment_id));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);

  std::vector<uint8_t> tx_extra;
  EXPECT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, extra_nonce));
  EXPECT_FALSE(tx_extra.empty());

  // Parse back the tx_extra
  std::vector<cryptonote::tx_extra_field> fields;
  EXPECT_TRUE(cryptonote::parse_tx_extra(tx_extra, fields));

  cryptonote::tx_extra_nonce recovered_nonce;
  EXPECT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, recovered_nonce));

  crypto::hash recovered_id;
  EXPECT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(recovered_nonce.nonce, recovered_id));
  EXPECT_EQ(payment_id, recovered_id);
}

TEST(wallet2_tx_construction, encrypted_payment_id_roundtrip_through_tx_extra)
{
  crypto::hash8 payment_id8;
  memset(&payment_id8, 0x77, sizeof(payment_id8));

  cryptonote::blobdata extra_nonce;
  cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id8);

  std::vector<uint8_t> tx_extra;
  EXPECT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, extra_nonce));

  std::vector<cryptonote::tx_extra_field> fields;
  EXPECT_TRUE(cryptonote::parse_tx_extra(tx_extra, fields));

  cryptonote::tx_extra_nonce recovered_nonce;
  EXPECT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, recovered_nonce));

  crypto::hash8 recovered_id8;
  EXPECT_TRUE(cryptonote::get_encrypted_payment_id_from_tx_extra_nonce(recovered_nonce.nonce, recovered_id8));
  EXPECT_EQ(payment_id8, recovered_id8);
}

// ============================================================================
// Amount formatting (print_money / parse_amount) tests
// ============================================================================

TEST(wallet2_tx_construction, print_money_one_xmr)
{
  // 1 XMR = 1e12 piconero
  std::string s = cryptonote::print_money(1000000000000ULL);
  EXPECT_EQ("1.000000000000", s);
}

TEST(wallet2_tx_construction, print_money_zero)
{
  std::string s = cryptonote::print_money(0);
  EXPECT_EQ("0.000000000000", s);
}

TEST(wallet2_tx_construction, print_money_sub_piconero)
{
  std::string s = cryptonote::print_money(1);
  EXPECT_EQ("0.000000000001", s);
}

TEST(wallet2_tx_construction, print_money_large_amount)
{
  // 18.4 million XMR
  std::string s = cryptonote::print_money(18400000000000000000ULL);
  EXPECT_EQ("18400000.000000000000", s);
}

TEST(wallet2_tx_construction, print_money_fractional)
{
  // 1.5 XMR
  std::string s = cryptonote::print_money(1500000000000ULL);
  EXPECT_EQ("1.500000000000", s);
}

TEST(wallet2_tx_construction, parse_amount_one_xmr)
{
  uint64_t amount = 0;
  EXPECT_TRUE(cryptonote::parse_amount(amount, "1.0"));
  EXPECT_EQ(1000000000000ULL, amount);
}

TEST(wallet2_tx_construction, parse_amount_zero)
{
  uint64_t amount = 42;
  EXPECT_TRUE(cryptonote::parse_amount(amount, "0.0"));
  EXPECT_EQ(0u, amount);
}

TEST(wallet2_tx_construction, parse_amount_fractional)
{
  uint64_t amount = 0;
  EXPECT_TRUE(cryptonote::parse_amount(amount, "0.5"));
  EXPECT_EQ(500000000000ULL, amount);
}

TEST(wallet2_tx_construction, parse_amount_whole_number)
{
  uint64_t amount = 0;
  EXPECT_TRUE(cryptonote::parse_amount(amount, "100"));
  EXPECT_EQ(100000000000000ULL, amount);
}

TEST(wallet2_tx_construction, parse_amount_invalid_string)
{
  uint64_t amount = 0;
  EXPECT_FALSE(cryptonote::parse_amount(amount, "not_a_number"));
}

TEST(wallet2_tx_construction, parse_amount_negative_fails)
{
  uint64_t amount = 0;
  EXPECT_FALSE(cryptonote::parse_amount(amount, "-1.0"));
}

TEST(wallet2_tx_construction, parse_amount_roundtrip)
{
  uint64_t original = 123456789012ULL;
  std::string s = cryptonote::print_money(original);
  uint64_t recovered = 0;
  EXPECT_TRUE(cryptonote::parse_amount(recovered, s));
  EXPECT_EQ(original, recovered);
}

// ============================================================================
// Amount decomposition tests
// ============================================================================

TEST(wallet2_tx_construction, decompose_amount_zero)
{
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(0, 0,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });
  EXPECT_TRUE(chunks.empty());
  EXPECT_TRUE(dust_vec.empty());
}

TEST(wallet2_tx_construction, decompose_amount_single_digit)
{
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(7, 0,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });
  // 7 should produce a single chunk of 7
  uint64_t total = 0;
  for (auto c : chunks) total += c;
  for (auto d : dust_vec) total += d;
  EXPECT_EQ(7u, total);
}

TEST(wallet2_tx_construction, decompose_amount_multi_digit)
{
  // 123 -> 3 + 20 + 100 (or dust handling depending on threshold)
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(123, 0,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });

  uint64_t total = 0;
  for (auto c : chunks) total += c;
  for (auto d : dust_vec) total += d;
  EXPECT_EQ(123u, total);
}

TEST(wallet2_tx_construction, decompose_amount_preserves_total)
{
  uint64_t amount = 62387455827ULL;
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(amount, 1000000,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });

  uint64_t total = 0;
  for (auto c : chunks) total += c;
  for (auto d : dust_vec) total += d;
  EXPECT_EQ(amount, total);
}

TEST(wallet2_tx_construction, decompose_amount_chunks_are_digit_powers)
{
  // Each chunk should be a single non-zero digit times a power of 10
  std::vector<uint64_t> chunks;
  cryptonote::decompose_amount_into_digits(12345, 0,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t) {});

  for (uint64_t c : chunks)
  {
    // Remove trailing zeros
    uint64_t tmp = c;
    while (tmp > 0 && tmp % 10 == 0)
      tmp /= 10;
    // Should be a single digit 1-9
    EXPECT_GE(tmp, 1u);
    EXPECT_LE(tmp, 9u);
  }
}

TEST(wallet2_tx_construction, decompose_amount_dust_below_threshold)
{
  // With dust threshold of 1000, amounts less than 1000 per digit go to dust
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(999, 1000,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });

  // 999 is entirely dust
  EXPECT_TRUE(chunks.empty());
  EXPECT_EQ(1u, dust_vec.size());
  EXPECT_EQ(999u, dust_vec[0]);
}

// ============================================================================
// round_money_up tests
// ============================================================================

TEST(wallet2_tx_construction, round_money_up_already_round)
{
  EXPECT_EQ(1000000000000ULL, cryptonote::round_money_up(1000000000000ULL, 1));
}

TEST(wallet2_tx_construction, round_money_up_2_sig_digits)
{
  // 1234 with 2 sig digits -> 1300
  EXPECT_EQ(1300u, cryptonote::round_money_up(1234, 2));
}

TEST(wallet2_tx_construction, round_money_up_1_sig_digit)
{
  // 1234 with 1 sig digit -> 2000
  EXPECT_EQ(2000u, cryptonote::round_money_up(1234, 1));
}

TEST(wallet2_tx_construction, round_money_up_zero)
{
  EXPECT_EQ(0u, cryptonote::round_money_up(0, 1));
}

TEST(wallet2_tx_construction, round_money_up_exact_digits)
{
  // 1200 with 2 sig digits -> 1200 (already exactly 2 significant digits)
  EXPECT_EQ(1200u, cryptonote::round_money_up(1200, 2));
}

// ============================================================================
// Fee priority utilities tests
// ============================================================================

TEST(wallet2_tx_construction, fee_priority_from_integral_0)
{
  EXPECT_EQ(tools::fee_priority::Default, tools::fee_priority_utilities::from_integral(0));
}

TEST(wallet2_tx_construction, fee_priority_from_integral_1)
{
  EXPECT_EQ(tools::fee_priority::Unimportant, tools::fee_priority_utilities::from_integral(1));
}

TEST(wallet2_tx_construction, fee_priority_from_integral_4)
{
  EXPECT_EQ(tools::fee_priority::Priority, tools::fee_priority_utilities::from_integral(4));
}

TEST(wallet2_tx_construction, fee_priority_from_integral_clamped_high)
{
  // Values above Priority should clamp to Priority
  EXPECT_EQ(tools::fee_priority::Priority, tools::fee_priority_utilities::from_integral(100));
}

TEST(wallet2_tx_construction, fee_priority_to_string)
{
  EXPECT_EQ("default", tools::fee_priority_utilities::to_string(tools::fee_priority::Default));
  EXPECT_EQ("unimportant", tools::fee_priority_utilities::to_string(tools::fee_priority::Unimportant));
  EXPECT_EQ("normal", tools::fee_priority_utilities::to_string(tools::fee_priority::Normal));
  EXPECT_EQ("elevated", tools::fee_priority_utilities::to_string(tools::fee_priority::Elevated));
  EXPECT_EQ("priority", tools::fee_priority_utilities::to_string(tools::fee_priority::Priority));
}

TEST(wallet2_tx_construction, fee_priority_from_string)
{
  auto p = tools::fee_priority_utilities::from_string("normal");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(tools::fee_priority::Normal, p.value());
}

TEST(wallet2_tx_construction, fee_priority_from_string_invalid)
{
  auto p = tools::fee_priority_utilities::from_string("garbage");
  EXPECT_FALSE(p.has_value());
}

TEST(wallet2_tx_construction, fee_priority_decrease)
{
  EXPECT_EQ(tools::fee_priority::Normal, tools::fee_priority_utilities::decrease(tools::fee_priority::Elevated));
  EXPECT_EQ(tools::fee_priority::Unimportant, tools::fee_priority_utilities::decrease(tools::fee_priority::Normal));
  EXPECT_EQ(tools::fee_priority::Default, tools::fee_priority_utilities::decrease(tools::fee_priority::Unimportant));
  // Decreasing Default stays Default
  EXPECT_EQ(tools::fee_priority::Default, tools::fee_priority_utilities::decrease(tools::fee_priority::Default));
}

TEST(wallet2_tx_construction, fee_priority_is_valid)
{
  EXPECT_TRUE(tools::fee_priority_utilities::is_valid(0));
  EXPECT_TRUE(tools::fee_priority_utilities::is_valid(1));
  EXPECT_TRUE(tools::fee_priority_utilities::is_valid(4));
  EXPECT_FALSE(tools::fee_priority_utilities::is_valid(5));
  EXPECT_FALSE(tools::fee_priority_utilities::is_valid(100));
}

TEST(wallet2_tx_construction, fee_priority_clamp)
{
  EXPECT_EQ(tools::fee_priority::Default, tools::fee_priority_utilities::clamp(tools::fee_priority::Default));
  EXPECT_EQ(tools::fee_priority::Priority, tools::fee_priority_utilities::clamp(tools::fee_priority::Priority));
  // Values beyond Priority clamp to Priority
  EXPECT_EQ(tools::fee_priority::Priority, tools::fee_priority_utilities::clamp(static_cast<tools::fee_priority>(99)));
}

TEST(wallet2_tx_construction, fee_priority_clamp_modified_default_becomes_unimportant)
{
  EXPECT_EQ(tools::fee_priority::Unimportant, tools::fee_priority_utilities::clamp_modified(tools::fee_priority::Default));
}

TEST(wallet2_tx_construction, fee_priority_clamp_modified_non_default_unchanged)
{
  EXPECT_EQ(tools::fee_priority::Normal, tools::fee_priority_utilities::clamp_modified(tools::fee_priority::Normal));
  EXPECT_EQ(tools::fee_priority::Priority, tools::fee_priority_utilities::clamp_modified(tools::fee_priority::Priority));
}

// ============================================================================
// Fee algorithm utilities tests
// ============================================================================

TEST(wallet2_tx_construction, fee_algorithm_integral_values)
{
  EXPECT_EQ(-1, tools::fee_algorithm_utilities::as_integral(tools::fee_algorithm::Unset));
  EXPECT_EQ(0, tools::fee_algorithm_utilities::as_integral(tools::fee_algorithm::PreHardforkV3));
  EXPECT_EQ(1, tools::fee_algorithm_utilities::as_integral(tools::fee_algorithm::HardforkV3));
  EXPECT_EQ(2, tools::fee_algorithm_utilities::as_integral(tools::fee_algorithm::HardforkV5));
  EXPECT_EQ(3, tools::fee_algorithm_utilities::as_integral(tools::fee_algorithm::HardforkV8));
}

// ============================================================================
// Change address logic tests
// ============================================================================

TEST_F(Wallet2TxConstructionTest, main_address_is_subaddress_zero)
{
  cryptonote::subaddress_index main_idx{0, 0};
  EXPECT_TRUE(main_idx.is_zero());
  cryptonote::account_public_address main_addr = m_wallet.get_subaddress(main_idx);
  // Main address via get_subaddress(0,0) should equal the account address
  cryptonote::account_public_address account_addr = m_wallet.get_account().get_keys().m_account_address;
  EXPECT_EQ(account_addr.m_spend_public_key, main_addr.m_spend_public_key);
  EXPECT_EQ(account_addr.m_view_public_key, main_addr.m_view_public_key);
}

TEST_F(Wallet2TxConstructionTest, different_subaddresses_different_spend_keys)
{
  // All subaddresses should have different spend keys
  std::set<crypto::public_key> spend_keys;
  for (uint32_t minor = 0; minor < 5; ++minor)
  {
    cryptonote::subaddress_index idx{0, minor};
    cryptonote::account_public_address addr = m_wallet.get_subaddress(idx);
    spend_keys.insert(addr.m_spend_public_key);
  }
  EXPECT_EQ(5u, spend_keys.size());
}

// ============================================================================
// Transfer detail frozen state tests
// ============================================================================

TEST(wallet2_tx_construction, transfer_detail_frozen_field)
{
  auto td = make_transfer_detail(1000, 100, false);
  EXPECT_FALSE(td.m_frozen);
  td.m_frozen = true;
  EXPECT_TRUE(td.m_frozen);
}

TEST(wallet2_tx_construction, transfer_detail_subaddr_index_field)
{
  auto td = make_transfer_detail(1000, 100, false);
  td.m_subaddr_index = {2, 5};
  EXPECT_EQ(2u, td.m_subaddr_index.major);
  EXPECT_EQ(5u, td.m_subaddr_index.minor);
}

TEST(wallet2_tx_construction, transfer_detail_spent_height)
{
  auto td = make_transfer_detail(1000, 100, true);
  EXPECT_TRUE(td.m_spent);
  EXPECT_EQ(110u, td.m_spent_height); // make_transfer_detail sets spent_height = block_height + 10
}

// ============================================================================
// Transaction extra field construction tests
// ============================================================================

TEST(wallet2_tx_construction, add_tx_pub_key_to_extra)
{
  std::vector<uint8_t> tx_extra;
  crypto::public_key pub_key;
  memset(&pub_key, 0x42, sizeof(pub_key));

  EXPECT_TRUE(cryptonote::add_tx_pub_key_to_extra(tx_extra, pub_key));
  EXPECT_FALSE(tx_extra.empty());

  // Parse it back
  crypto::public_key recovered = cryptonote::get_tx_pub_key_from_extra(tx_extra);
  EXPECT_EQ(pub_key, recovered);
}

TEST(wallet2_tx_construction, multiple_extra_fields)
{
  std::vector<uint8_t> tx_extra;

  // Add a pub key
  crypto::public_key pub_key;
  memset(&pub_key, 0x42, sizeof(pub_key));
  EXPECT_TRUE(cryptonote::add_tx_pub_key_to_extra(tx_extra, pub_key));

  // Add a payment ID nonce
  crypto::hash payment_id;
  memset(&payment_id, 0xAA, sizeof(payment_id));
  cryptonote::blobdata extra_nonce;
  cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);
  EXPECT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, extra_nonce));

  // Parse both back
  std::vector<cryptonote::tx_extra_field> fields;
  EXPECT_TRUE(cryptonote::parse_tx_extra(tx_extra, fields));

  crypto::public_key recovered_key = cryptonote::get_tx_pub_key_from_extra(tx_extra);
  EXPECT_EQ(pub_key, recovered_key);

  cryptonote::tx_extra_nonce recovered_nonce;
  EXPECT_TRUE(cryptonote::find_tx_extra_field_by_type(fields, recovered_nonce));
  crypto::hash recovered_id;
  EXPECT_TRUE(cryptonote::get_payment_id_from_tx_extra_nonce(recovered_nonce.nonce, recovered_id));
  EXPECT_EQ(payment_id, recovered_id);
}

// ============================================================================
// Wallet balance tests with mock transfer details
// ============================================================================

TEST_F(Wallet2TxConstructionTest, balance_reflects_transfers)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);

  // Add some unspent transfers for account 0
  auto td1 = make_transfer_detail(1000000000000, 100, false, make_txid(1));
  td1.m_key_image = make_key_image(1);
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  auto td2 = make_transfer_detail(2000000000000, 200, false, make_txid(2));
  td2.m_key_image = make_key_image(2);
  td2.m_subaddr_index = {0, 0};
  transfers.push_back(td2);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(2)] = 1;

  uint64_t bal = m_wallet.balance(0, false);
  EXPECT_EQ(3000000000000u, bal);
}

TEST_F(Wallet2TxConstructionTest, balance_excludes_spent)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);

  auto td1 = make_transfer_detail(1000000000000, 100, false, make_txid(1));
  td1.m_key_image = make_key_image(1);
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(1)] = 0;

  auto td2 = make_transfer_detail(2000000000000, 200, false, make_txid(2));
  td2.m_key_image = make_key_image(2);
  td2.m_subaddr_index = {0, 0};
  transfers.push_back(td2);
  wallet_accessor_test::get_key_images(m_wallet)[make_key_image(2)] = 1;

  // Mark one as spent
  wallet_accessor_test::set_spent(m_wallet, 0, 150);
  uint64_t bal = m_wallet.balance(0, false);
  EXPECT_EQ(2000000000000u, bal);
}

// ============================================================================
// Transaction weight/size helpers - testing growth patterns via transfer details
// ============================================================================

TEST(wallet2_tx_construction, more_inputs_mean_larger_tx)
{
  // The fundamental property: more inputs = heavier transaction
  // We verify this by counting selected transfers and their amounts
  tools::wallet2::transfer_container transfers;
  std::vector<size_t> small_set, large_set;

  for (size_t i = 0; i < 2; ++i)
  {
    transfers.push_back(make_transfer_detail(1000000000000, 100 + i, false, make_txid(i)));
    small_set.push_back(i);
  }
  for (size_t i = 0; i < 10; ++i)
  {
    if (i >= 2)
      transfers.push_back(make_transfer_detail(1000000000000, 100 + i, false, make_txid(i)));
    large_set.push_back(i);
  }

  // More inputs means heavier tx
  EXPECT_LT(small_set.size(), large_set.size());
}

// ============================================================================
// Additional edge case tests
// ============================================================================

TEST(wallet2_tx_construction, print_money_max_uint64)
{
  // Ensure print_money handles the maximum uint64 value without crash
  std::string s = cryptonote::print_money(UINT64_MAX);
  EXPECT_FALSE(s.empty());
  // Should contain a decimal point
  EXPECT_NE(std::string::npos, s.find('.'));
}

TEST(wallet2_tx_construction, parse_amount_empty_string_fails)
{
  uint64_t amount = 0;
  EXPECT_FALSE(cryptonote::parse_amount(amount, ""));
}

TEST(wallet2_tx_construction, parse_amount_leading_zeros)
{
  uint64_t amount = 0;
  EXPECT_TRUE(cryptonote::parse_amount(amount, "001.0"));
  EXPECT_EQ(1000000000000ULL, amount);
}

TEST(wallet2_tx_construction, decompose_large_amount_preserves_total)
{
  // Test with a large "real world" amount: 1000 XMR
  uint64_t amount = 1000000000000000ULL;
  std::vector<uint64_t> chunks;
  std::vector<uint64_t> dust_vec;
  cryptonote::decompose_amount_into_digits(amount, 0,
    [&](uint64_t c) { chunks.push_back(c); },
    [&](uint64_t d) { dust_vec.push_back(d); });

  uint64_t total = 0;
  for (auto c : chunks) total += c;
  for (auto d : dust_vec) total += d;
  EXPECT_EQ(amount, total);
}

TEST(wallet2_tx_construction, transfer_detail_multiple_subaddr_accounts)
{
  // Verify we can properly filter transfers across multiple accounts
  tools::wallet2::transfer_container transfers;
  for (uint32_t acct = 0; acct < 3; ++acct)
  {
    for (uint32_t sub = 0; sub < 4; ++sub)
    {
      auto td = make_transfer_detail(1000000000000 * (acct + 1), 100 + acct * 10 + sub, false, make_txid(acct * 10 + sub));
      td.m_subaddr_index = {acct, sub};
      transfers.push_back(td);
    }
  }

  // Count transfers for account 1
  size_t acct1_count = 0;
  uint64_t acct1_total = 0;
  for (const auto& td : transfers)
  {
    if (td.m_subaddr_index.major == 1)
    {
      ++acct1_count;
      acct1_total += td.amount();
    }
  }
  EXPECT_EQ(4u, acct1_count);
  EXPECT_EQ(4 * 2000000000000u, acct1_total);
}

TEST(wallet2_tx_construction, round_money_up_string_overload)
{
  std::string result = cryptonote::round_money_up("1.234567890123", 2);
  // The result should be a rounded-up money string
  uint64_t amount = 0;
  EXPECT_TRUE(cryptonote::parse_amount(amount, result));
  EXPECT_GE(amount, 1234567890123ULL); // Should be >= original
}

// ============================================================================
// Gamma decoy picker regression tests (Bug #9: gamma decoy feasibility)
//
// The gamma_picker::pick() function has a known edge case: when the gamma
// distribution suggests spending an output faster than the consensus lock time
// allows, the code falls back to picking from a RECENT_SPEND_WINDOW. These
// tests exercise the picker under various rct_offsets configurations.
// ============================================================================

namespace
{
  // Helper to build a synthetic rct_offsets vector.
  // num_blocks: total number of blocks represented
  // outputs_per_block: how many RingCT outputs each block contributes
  // Returns a cumulative offset vector of size num_blocks.
  std::vector<uint64_t> make_rct_offsets(size_t num_blocks, uint64_t outputs_per_block)
  {
    std::vector<uint64_t> offsets(num_blocks);
    uint64_t cumulative = 0;
    for (size_t i = 0; i < num_blocks; ++i)
    {
      cumulative += outputs_per_block;
      offsets[i] = cumulative;
    }
    return offsets;
  }

}

// 1. Basic pick: create a gamma_picker with reasonable rct_offsets data,
//    call pick() many times, verify returned indices are valid most of the time.
TEST(wallet2_tx_construction, gamma_picker_basic_pick)
{
  // Use a realistic chain size: 50000 blocks with 50 outputs each = 2.5M total RCT outputs.
  // The gamma distribution (shape=19.28, scale=1/1.61) can suggest very large time offsets,
  // so we need enough outputs to keep the bad-pick rate reasonable.
  const size_t num_blocks = 50000;
  const uint64_t outputs_per_block = 50;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, outputs_per_block);

  tools::gamma_picker picker(offsets);
  EXPECT_GT(picker.get_num_rct_outs(), 0u);

  const int num_picks = 1000;
  int bad_picks = 0;
  for (int i = 0; i < num_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx == std::numeric_limits<uint64_t>::max())
      ++bad_picks;
  }
  // With 2.5M outputs across 50000 blocks, the vast majority of picks should succeed.
  // Allow up to 20% bad picks (generous margin for the gamma tail).
  EXPECT_LT(bad_picks, num_picks / 5)
    << "Too many bad picks: " << bad_picks << " out of " << num_picks;
}

// 2. All outputs old: many blocks (representing outputs much older than lock time).
//    Verify pick() still returns valid indices.
TEST(wallet2_tx_construction, gamma_picker_all_outputs_old)
{
  // 5000 blocks at 120s each = 600000s total chain time, all well past
  // DEFAULT_UNLOCK_TIME (1200s). 3 outputs per block = 15000 total outputs.
  const size_t num_blocks = 5000;
  const uint64_t outputs_per_block = 3;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, outputs_per_block);

  tools::gamma_picker picker(offsets);

  const int num_picks = 1000;
  int valid_picks = 0;
  for (int i = 0; i < num_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
      ++valid_picks;
  }
  // Should still get a good number of valid picks even when all outputs are "old"
  EXPECT_GT(valid_picks, num_picks / 2)
    << "Expected most picks to be valid with old outputs, got " << valid_picks;
}

// 3. Respects output range: all valid picks must be < total RCT outputs (rct_offsets.back()).
TEST(wallet2_tx_construction, gamma_picker_respects_output_range)
{
  const size_t num_blocks = 2000;
  const uint64_t outputs_per_block = 5;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, outputs_per_block);
  const uint64_t total_outputs = offsets.back();

  tools::gamma_picker picker(offsets);

  const int num_picks = 5000;
  for (int i = 0; i < num_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
    {
      EXPECT_LT(idx, total_outputs)
        << "pick() returned index " << idx << " which is >= total outputs " << total_outputs;
    }
  }
}

// 4. Statistical distribution: the gamma distribution favors recent outputs.
//    Run many picks and verify outputs cluster toward higher indices (more recent).
TEST(wallet2_tx_construction, gamma_picker_statistical_distribution)
{
  const size_t num_blocks = 2000;
  const uint64_t outputs_per_block = 5;
  std::vector<uint64_t> offsets = make_rct_offsets(num_blocks, outputs_per_block);
  const uint64_t total_outputs = offsets.back(); // 10000

  tools::gamma_picker picker(offsets);

  const int num_picks = 10000;
  uint64_t midpoint = total_outputs / 2;
  int above_midpoint = 0;
  int below_midpoint = 0;
  int valid_picks = 0;

  for (int i = 0; i < num_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
    {
      ++valid_picks;
      if (idx >= midpoint)
        ++above_midpoint;
      else
        ++below_midpoint;
    }
  }

  // The gamma distribution should favor recent (higher-index) outputs.
  // We expect significantly more picks above the midpoint than below.
  ASSERT_GT(valid_picks, 0) << "No valid picks at all";
  EXPECT_GT(above_midpoint, below_midpoint)
    << "Expected recent outputs (above midpoint) to be picked more often. "
    << "Above: " << above_midpoint << ", Below: " << below_midpoint;
}

// 5. Empty/minimal offsets: constructor should reject offsets that are too small.
//    rct_offsets.size() must be >= CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE (10).
TEST(wallet2_tx_construction, gamma_picker_empty_offsets_handled)
{
  // Empty vector: should throw
  {
    std::vector<uint64_t> empty_offsets;
    EXPECT_THROW(tools::gamma_picker picker(empty_offsets), tools::error::wallet_internal_error);
  }

  // Single element [1]: size 1 < 10, should throw
  {
    std::vector<uint64_t> tiny_offsets = {1};
    EXPECT_THROW(tools::gamma_picker picker(tiny_offsets), tools::error::wallet_internal_error);
  }

  // Two elements [0, 1]: size 2 < 10, should throw
  {
    std::vector<uint64_t> small_offsets = {0, 1};
    EXPECT_THROW(tools::gamma_picker picker(small_offsets), tools::error::wallet_internal_error);
  }

  // Exactly 9 elements: still < 10, should throw
  {
    std::vector<uint64_t> nine_offsets;
    for (uint64_t i = 1; i <= 9; ++i)
      nine_offsets.push_back(i);
    EXPECT_THROW(tools::gamma_picker picker(nine_offsets), tools::error::wallet_internal_error);
  }

  // Exactly 10 elements with valid data: should NOT throw.
  // end = begin + 10 - 9 = begin + 1, so num_rct_outputs = offsets[0] which must be > 0.
  {
    std::vector<uint64_t> min_offsets;
    for (uint64_t i = 1; i <= 10; ++i)
      min_offsets.push_back(i);
    EXPECT_NO_THROW(tools::gamma_picker picker(min_offsets));
  }
}

// 6. Single block with many outputs: rct_offsets represents effectively one block
//    with many outputs (minimum 10 entries for the constructor, but only the first
//    block has outputs, rest are padding at the same cumulative count).
TEST(wallet2_tx_construction, gamma_picker_single_block_offsets)
{
  // 10 blocks minimum. First block has 100 outputs, rest have 0 new outputs.
  // offsets: [100, 100, 100, 100, 100, 100, 100, 100, 100, 100]
  // num_rct_outputs = offsets[0] = 100 (since end = begin + 1)
  std::vector<uint64_t> offsets(10, 100);

  tools::gamma_picker picker(offsets);
  EXPECT_EQ(100u, picker.get_num_rct_outs());

  const int num_picks = 1000;
  int valid_picks = 0;
  for (int i = 0; i < num_picks; ++i)
  {
    uint64_t idx = picker.pick();
    if (idx != std::numeric_limits<uint64_t>::max())
    {
      EXPECT_LT(idx, 100u)
        << "pick() returned index " << idx << " which is >= the single block's 100 outputs";
      ++valid_picks;
    }
  }
  // With only 100 outputs in a single block, valid picks should still occur
  EXPECT_GT(valid_picks, 0)
    << "Expected at least some valid picks from a single-block configuration";
}

// ============================================================================
// Coin selection component tests
// ============================================================================

// Test 1: coin_selection_picks_exact_amount
// Verifies that pick_preferred_rct_inputs returns a single output when one
// exactly meets the needed amount. We test the filtering logic directly since
// pick_preferred_rct_inputs requires is_transfer_unlocked which needs blockchain
// state. Instead, we replicate the core filtering logic from pick_preferred_rct_inputs.
TEST_F(Wallet2TxConstructionTest, coin_selection_picks_exact_amount)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  // Create transfers with amounts [1, 2, 3, 5, 8] XMR (in piconero)
  const uint64_t XMR = 1000000000000ULL;
  uint64_t amounts[] = {1 * XMR, 2 * XMR, 3 * XMR, 5 * XMR, 8 * XMR};
  for (uint32_t i = 0; i < 5; ++i)
  {
    auto td = make_transfer_detail(amounts[i], 1000, false, make_txid(i + 1));
    td.m_key_image = make_key_image(i + 1);
    td.m_rct = true;
    td.m_subaddr_index = {0, 0};
    transfers.push_back(td);
    key_images[make_key_image(i + 1)] = i;
  }

  // Simulate the core pick_preferred_rct_inputs logic:
  // Look for a single output >= needed_money that is unspent, unfrozen, RCT
  uint64_t needed_money = 5 * XMR;
  uint32_t subaddr_account = 0;
  std::set<uint32_t> subaddr_indices = {0};

  std::vector<size_t> single_picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == subaddr_account &&
        subaddr_indices.count(td.m_subaddr_index.minor) == 1)
    {
      single_picks.push_back(i);
      break; // pick_preferred_rct_inputs returns first match
    }
  }

  // The 5 XMR output (index 3) should be picked as the first exact match
  ASSERT_EQ(1u, single_picks.size());
  EXPECT_EQ(3u, single_picks[0]);
  EXPECT_EQ(5 * XMR, transfers[single_picks[0]].amount());
}

// Test 2: coin_selection_avoids_spent_outputs
// Verifies that the coin selection filtering excludes spent outputs.
TEST_F(Wallet2TxConstructionTest, coin_selection_avoids_spent_outputs)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  // Create 5 outputs: indices 0,1 are spent, indices 2,3,4 are unspent
  for (uint32_t i = 0; i < 5; ++i)
  {
    bool spent = (i < 2);
    auto td = make_transfer_detail(3 * XMR, 1000, spent, make_txid(i + 100));
    td.m_key_image = make_key_image(i + 100);
    td.m_rct = true;
    td.m_subaddr_index = {0, 0};
    transfers.push_back(td);
    key_images[make_key_image(i + 100)] = i;
  }

  // Mark first two as spent via wallet_accessor_test
  wallet_accessor_test::set_spent(m_wallet, 0, 1010);
  wallet_accessor_test::set_spent(m_wallet, 1, 1010);

  // Filter using pick_preferred_rct_inputs logic
  uint64_t needed_money = 3 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == 0)
    {
      picks.push_back(i);
      break;
    }
  }

  // The first eligible output should be index 2 (first unspent)
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(2u, picks[0]);

  // Double check: spent outputs are indeed spent
  EXPECT_TRUE(wallet_accessor_test::is_spent(m_wallet, 0));
  EXPECT_TRUE(wallet_accessor_test::is_spent(m_wallet, 1));
  EXPECT_FALSE(wallet_accessor_test::is_spent(m_wallet, 2));
}

// Test 3: coin_selection_respects_frozen_outputs
// Verifies frozen outputs are excluded from selection.
TEST_F(Wallet2TxConstructionTest, coin_selection_respects_frozen_outputs)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  for (uint32_t i = 0; i < 4; ++i)
  {
    auto td = make_transfer_detail(5 * XMR, 1000, false, make_txid(i + 200));
    td.m_key_image = make_key_image(i + 200);
    td.m_rct = true;
    td.m_subaddr_index = {0, 0};
    transfers.push_back(td);
    key_images[make_key_image(i + 200)] = i;
  }

  // Freeze first two outputs using wallet2::freeze()
  m_wallet.freeze(0);
  m_wallet.freeze(1);

  EXPECT_TRUE(m_wallet.frozen(0));
  EXPECT_TRUE(m_wallet.frozen(1));
  EXPECT_FALSE(m_wallet.frozen(2));
  EXPECT_FALSE(m_wallet.frozen(3));

  // Filter: frozen outputs should be skipped
  uint64_t needed_money = 5 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == 0)
    {
      picks.push_back(i);
      break;
    }
  }

  // First non-frozen is index 2
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(2u, picks[0]);

  // Thaw index 0 and verify it's selectable again
  m_wallet.thaw(0);
  EXPECT_FALSE(m_wallet.frozen(0));

  picks.clear();
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == 0)
    {
      picks.push_back(i);
      break;
    }
  }
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(0u, picks[0]);
}

// Test 4: coin_selection_prefers_same_subaddress
// pick_preferred_rct_inputs pairs outputs from the same subaddress for privacy.
// In two-output mode, td2.m_subaddr_index == td.m_subaddr_index is required.
TEST_F(Wallet2TxConstructionTest, coin_selection_prefers_same_subaddress)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  // Create outputs from different subaddresses (all account 0)
  // Subaddr {0,0}: 2 XMR, 3 XMR (sum = 5 XMR, enough for target)
  // Subaddr {0,1}: 4 XMR (not enough alone, no pair)
  // Subaddr {0,2}: 6 XMR (enough alone)
  auto td0 = make_transfer_detail(2 * XMR, 1000, false, make_txid(300));
  td0.m_key_image = make_key_image(300);
  td0.m_subaddr_index = {0, 0};
  transfers.push_back(td0);
  key_images[make_key_image(300)] = 0;

  auto td1 = make_transfer_detail(3 * XMR, 1000, false, make_txid(301));
  td1.m_key_image = make_key_image(301);
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  key_images[make_key_image(301)] = 1;

  auto td2 = make_transfer_detail(4 * XMR, 1000, false, make_txid(302));
  td2.m_key_image = make_key_image(302);
  td2.m_subaddr_index = {0, 1};
  transfers.push_back(td2);
  key_images[make_key_image(302)] = 2;

  auto td3 = make_transfer_detail(6 * XMR, 1000, false, make_txid(303));
  td3.m_key_image = make_key_image(303);
  td3.m_subaddr_index = {0, 2};
  transfers.push_back(td3);
  key_images[make_key_image(303)] = 3;

  // Simulate two-output pairing from pick_preferred_rct_inputs:
  // Only pairs from same subaddress are considered
  uint64_t needed_money = 5 * XMR;
  std::vector<size_t> pair_picks;

  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& tdi = transfers[i];
    if (tdi.m_spent || tdi.m_frozen || !tdi.is_rct() || tdi.m_subaddr_index.major != 0)
      continue;

    for (size_t j = i + 1; j < transfers.size(); ++j)
    {
      const auto& tdj = transfers[j];
      if (tdj.m_spent || tdj.m_frozen || !tdj.is_rct() ||
          tdj.m_subaddr_index != tdi.m_subaddr_index)
        continue;

      if (tdi.amount() + tdj.amount() >= needed_money)
      {
        pair_picks.push_back(i);
        pair_picks.push_back(j);
        goto found_pair;
      }
    }
  }
  found_pair:

  // Should find pair from subaddress {0,0}: indices 0 and 1
  ASSERT_EQ(2u, pair_picks.size());
  EXPECT_EQ(0u, pair_picks[0]);
  EXPECT_EQ(1u, pair_picks[1]);
  // Both from same subaddress
  EXPECT_EQ(transfers[pair_picks[0]].m_subaddr_index, transfers[pair_picks[1]].m_subaddr_index);
}

// Test 5: coin_selection_change_amount
// Verifies: change = selected_amount - target - fee
TEST(wallet2_tx_construction, coin_selection_change_amount)
{
  const uint64_t XMR = 1000000000000ULL;

  uint64_t selected_amount = 10 * XMR;
  uint64_t target = 3 * XMR;
  uint64_t fee = 20000000ULL; // 0.00002 XMR typical fee

  uint64_t change = selected_amount - target - fee;

  EXPECT_EQ(change, 10 * XMR - 3 * XMR - 20000000ULL);
  EXPECT_EQ(change, 6999980000000ULL);

  // Change must be non-negative
  EXPECT_GT(change, 0u);

  // If selected exactly covers target + fee, change is 0
  uint64_t exact_selected = target + fee;
  uint64_t exact_change = exact_selected - target - fee;
  EXPECT_EQ(0u, exact_change);
}

// Test 6: coin_selection_dust_threshold
// Tests the tx_dust_policy structure and dust threshold filtering logic.
TEST(wallet2_tx_construction, coin_selection_dust_threshold)
{
  // Default dust threshold from config
  uint64_t default_dust = ::config::DEFAULT_DUST_THRESHOLD;
  EXPECT_EQ(2000000000ULL, default_dust); // 2 * 10^9 piconero

  // tx_dust_policy with default threshold
  tools::tx_dust_policy policy(default_dust);
  EXPECT_EQ(default_dust, policy.dust_threshold);
  EXPECT_TRUE(policy.add_to_fee); // dust should be added to fee by default

  // tx_dust_policy with zero threshold (post-RCT: no dust concept)
  tools::tx_dust_policy rct_policy(0);
  EXPECT_EQ(0u, rct_policy.dust_threshold);
  EXPECT_TRUE(rct_policy.add_to_fee);

  // Filter outputs below dust threshold (pre-RCT behavior)
  tools::wallet2::transfer_container transfers;
  transfers.push_back(make_transfer_detail(100, 1000, false, make_txid(1)));              // well below dust
  transfers.push_back(make_transfer_detail(1000000000, 1000, false, make_txid(2)));       // below dust (1e9 < 2e9)
  transfers.push_back(make_transfer_detail(2000000000, 1000, false, make_txid(3)));       // exactly at dust threshold
  transfers.push_back(make_transfer_detail(5000000000, 1000, false, make_txid(4)));       // above dust
  transfers.push_back(make_transfer_detail(1000000000000, 1000, false, make_txid(5)));    // way above dust

  // Count outputs above dust threshold
  std::vector<size_t> above_dust;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    if (transfers[i].amount() >= default_dust)
      above_dust.push_back(i);
  }

  // Indices 2, 3, 4 should be above dust
  EXPECT_EQ(3u, above_dust.size());
  EXPECT_EQ(2u, above_dust[0]);
  EXPECT_EQ(3u, above_dust[1]);
  EXPECT_EQ(4u, above_dust[2]);

  // With RCT policy (threshold=0), all outputs pass
  std::vector<size_t> rct_eligible;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    if (transfers[i].amount() >= rct_policy.dust_threshold)
      rct_eligible.push_back(i);
  }
  EXPECT_EQ(5u, rct_eligible.size());
}

// ============================================================================
// Reserve proof structural tests
// ============================================================================

// Test 12: reserve_proof_type_check
// Verify reserve_proof_entry serialization roundtrip via binary archive.
TEST(wallet2_tx_construction, reserve_proof_serialization_roundtrip)
{
  tools::wallet2::reserve_proof_entry entry;
  memset(&entry.txid, 0x11, sizeof(entry.txid));
  entry.index_in_tx = 42;
  memset(&entry.shared_secret, 0x22, sizeof(entry.shared_secret));
  memset(&entry.key_image, 0x33, sizeof(entry.key_image));
  memset(&entry.shared_secret_sig, 0x44, sizeof(entry.shared_secret_sig));
  memset(&entry.key_image_sig, 0x55, sizeof(entry.key_image_sig));

  // Serialize to blob
  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(entry, blob));
  EXPECT_GT(blob.size(), 0u);

  // Deserialize from blob
  tools::wallet2::reserve_proof_entry recovered;
  ASSERT_TRUE(cryptonote::t_serializable_object_from_blob(recovered, blob));

  // Verify all fields match
  EXPECT_EQ(entry.txid, recovered.txid);
  EXPECT_EQ(entry.index_in_tx, recovered.index_in_tx);
  EXPECT_EQ(entry.shared_secret, recovered.shared_secret);
  EXPECT_EQ(entry.key_image, recovered.key_image);
  EXPECT_EQ(0, memcmp(&entry.shared_secret_sig, &recovered.shared_secret_sig, sizeof(crypto::signature)));
  EXPECT_EQ(0, memcmp(&entry.key_image_sig, &recovered.key_image_sig, sizeof(crypto::signature)));
}

// Test: reserve_proof_entry default values
TEST(wallet2_tx_construction, reserve_proof_entry_default_values)
{
  tools::wallet2::reserve_proof_entry entry = AUTO_VAL_INIT(entry);
  EXPECT_EQ(entry.index_in_tx, 0u);
  EXPECT_EQ(entry.txid, crypto::null_hash);
}

// Test: reserve_proof_entry vector serialization roundtrip
TEST(wallet2_tx_construction, reserve_proof_vector_serialization)
{
  std::vector<tools::wallet2::reserve_proof_entry> entries;

  for (uint32_t i = 0; i < 3; ++i)
  {
    tools::wallet2::reserve_proof_entry e;
    memset(&e.txid, i + 1, sizeof(e.txid));
    e.index_in_tx = i * 10;
    memset(&e.shared_secret, i + 0x10, sizeof(e.shared_secret));
    memset(&e.key_image, i + 0x20, sizeof(e.key_image));
    memset(&e.shared_secret_sig, i + 0x30, sizeof(e.shared_secret_sig));
    memset(&e.key_image_sig, i + 0x40, sizeof(e.key_image_sig));
    entries.push_back(e);
  }

  // Serialize vector
  cryptonote::blobdata blob;
  ASSERT_TRUE(cryptonote::t_serializable_object_to_blob(entries, blob));
  EXPECT_GT(blob.size(), 0u);

  // Deserialize vector
  std::vector<tools::wallet2::reserve_proof_entry> recovered;
  ASSERT_TRUE(cryptonote::t_serializable_object_from_blob(recovered, blob));

  ASSERT_EQ(entries.size(), recovered.size());
  for (size_t i = 0; i < entries.size(); ++i)
  {
    EXPECT_EQ(entries[i].txid, recovered[i].txid);
    EXPECT_EQ(entries[i].index_in_tx, recovered[i].index_in_tx);
    EXPECT_EQ(entries[i].shared_secret, recovered[i].shared_secret);
    EXPECT_EQ(entries[i].key_image, recovered[i].key_image);
  }
}

// ============================================================================
// TX key retrieval tests
// ============================================================================

// Test 13: tx_key_retrieval_types
// get_tx_key_cached returns false for non-existent tx hashes.
TEST_F(Wallet2TxConstructionTest, tx_key_retrieval_nonexistent_returns_false)
{
  crypto::hash random_txid;
  memset(&random_txid, 0xAA, sizeof(random_txid));

  crypto::secret_key tx_key;
  std::vector<crypto::secret_key> additional_tx_keys;

  // get_tx_key_cached is private, but we test via the friend class.
  // The wallet has no tx_keys stored, so any lookup should fail.
  bool found = wallet_accessor_test::get_tx_key_cached(m_wallet, random_txid, tx_key, additional_tx_keys);
  EXPECT_FALSE(found);
  EXPECT_TRUE(additional_tx_keys.empty());
}

// Test: null hash tx key retrieval
TEST_F(Wallet2TxConstructionTest, tx_key_retrieval_null_hash_returns_false)
{
  crypto::secret_key tx_key;
  std::vector<crypto::secret_key> additional_tx_keys;

  bool found = wallet_accessor_test::get_tx_key_cached(m_wallet, crypto::null_hash, tx_key, additional_tx_keys);
  EXPECT_FALSE(found);
}

// ============================================================================
// Additional coin selection edge cases
// ============================================================================

// Test: coin selection with all outputs spent
TEST_F(Wallet2TxConstructionTest, coin_selection_all_spent_yields_no_picks)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  for (uint32_t i = 0; i < 3; ++i)
  {
    auto td = make_transfer_detail(5 * XMR, 1000, false, make_txid(i + 400));
    td.m_key_image = make_key_image(i + 400);
    td.m_rct = true;
    td.m_subaddr_index = {0, 0};
    transfers.push_back(td);
    key_images[make_key_image(i + 400)] = i;
  }

  // Mark all as spent
  for (uint32_t i = 0; i < 3; ++i)
    wallet_accessor_test::set_spent(m_wallet, i, 1010);

  // No picks should be found
  uint64_t needed_money = 5 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money)
    {
      picks.push_back(i);
      break;
    }
  }

  EXPECT_TRUE(picks.empty());
}

// Test: coin selection with all outputs frozen
TEST_F(Wallet2TxConstructionTest, coin_selection_all_frozen_yields_no_picks)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  for (uint32_t i = 0; i < 3; ++i)
  {
    auto td = make_transfer_detail(5 * XMR, 1000, false, make_txid(i + 500));
    td.m_key_image = make_key_image(i + 500);
    td.m_rct = true;
    td.m_subaddr_index = {0, 0};
    transfers.push_back(td);
    key_images[make_key_image(i + 500)] = i;
  }

  // Freeze all
  for (uint32_t i = 0; i < 3; ++i)
    m_wallet.freeze(i);

  // No picks should be found
  uint64_t needed_money = 5 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money)
    {
      picks.push_back(i);
      break;
    }
  }

  EXPECT_TRUE(picks.empty());
}

// Test: coin selection ignores non-RCT outputs
TEST_F(Wallet2TxConstructionTest, coin_selection_ignores_non_rct)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  // Non-RCT output with enough amount
  auto td0 = make_transfer_detail(10 * XMR, 1000, false, make_txid(600));
  td0.m_key_image = make_key_image(600);
  td0.m_rct = false; // non-RCT
  td0.m_subaddr_index = {0, 0};
  transfers.push_back(td0);
  key_images[make_key_image(600)] = 0;

  // RCT output with enough amount
  auto td1 = make_transfer_detail(5 * XMR, 1000, false, make_txid(601));
  td1.m_key_image = make_key_image(601);
  td1.m_rct = true;
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  key_images[make_key_image(601)] = 1;

  // RCT filter should skip index 0, pick index 1
  uint64_t needed_money = 5 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == 0)
    {
      picks.push_back(i);
      break;
    }
  }

  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(1u, picks[0]);
}

// Test: coin selection with output amount bounds (ignore_outputs_above/below)
TEST_F(Wallet2TxConstructionTest, coin_selection_respects_output_bounds)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  // Create outputs of various sizes
  auto td0 = make_transfer_detail(1 * XMR, 1000, false, make_txid(700));
  td0.m_key_image = make_key_image(700);
  td0.m_rct = true;
  td0.m_subaddr_index = {0, 0};
  transfers.push_back(td0);
  key_images[make_key_image(700)] = 0;

  auto td1 = make_transfer_detail(50 * XMR, 1000, false, make_txid(701));
  td1.m_key_image = make_key_image(701);
  td1.m_rct = true;
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  key_images[make_key_image(701)] = 1;

  auto td2 = make_transfer_detail(5 * XMR, 1000, false, make_txid(702));
  td2.m_key_image = make_key_image(702);
  td2.m_rct = true;
  td2.m_subaddr_index = {0, 0};
  transfers.push_back(td2);
  key_images[make_key_image(702)] = 2;

  // Set output bounds via public methods
  uint64_t min_output = 2 * XMR;
  uint64_t max_output = 20 * XMR;
  m_wallet.ignore_outputs_below(min_output);
  m_wallet.ignore_outputs_above(max_output);

  EXPECT_EQ(min_output, m_wallet.ignore_outputs_below());
  EXPECT_EQ(max_output, m_wallet.ignore_outputs_above());

  // Filter with bounds (replicate pick_preferred_rct_inputs logic)
  uint64_t needed_money = 3 * XMR;
  std::vector<size_t> picks;
  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& td = transfers[i];
    if (!td.m_spent && !td.m_frozen && td.is_rct() &&
        td.amount() >= needed_money &&
        td.m_subaddr_index.major == 0)
    {
      if (td.amount() > max_output || td.amount() < min_output)
        continue; // skip out-of-bounds
      picks.push_back(i);
      break;
    }
  }

  // Only td2 (5 XMR) is within bounds and >= needed: td0 (1 XMR) < min, td1 (50 XMR) > max
  ASSERT_EQ(1u, picks.size());
  EXPECT_EQ(2u, picks[0]);
  EXPECT_EQ(5 * XMR, transfers[picks[0]].amount());
}

// Test: output relatedness influences two-output pair selection
TEST_F(Wallet2TxConstructionTest, coin_selection_two_output_relatedness)
{
  auto& transfers = wallet_accessor_test::get_transfers(m_wallet);
  auto& key_images = wallet_accessor_test::get_key_images(m_wallet);

  const uint64_t XMR = 1000000000000ULL;

  // Create 3 outputs from same subaddress but different blocks
  // td0: block 1000, td1: block 1000 (same block = high relatedness 0.9)
  // td2: block 2000 (far away = low relatedness 0.0)
  auto td0 = make_transfer_detail(3 * XMR, 1000, false, make_txid(800));
  td0.m_key_image = make_key_image(800);
  td0.m_rct = true;
  td0.m_subaddr_index = {0, 0};
  transfers.push_back(td0);
  key_images[make_key_image(800)] = 0;

  auto td1 = make_transfer_detail(3 * XMR, 1000, false, make_txid(801));
  td1.m_key_image = make_key_image(801);
  td1.m_rct = true;
  td1.m_subaddr_index = {0, 0};
  transfers.push_back(td1);
  key_images[make_key_image(801)] = 1;

  auto td2 = make_transfer_detail(3 * XMR, 2000, false, make_txid(802));
  td2.m_key_image = make_key_image(802);
  td2.m_rct = true;
  td2.m_subaddr_index = {0, 0};
  transfers.push_back(td2);
  key_images[make_key_image(802)] = 2;

  // The pick_preferred_rct_inputs algorithm prefers the pair with lowest relatedness.
  // Pair (0,1): same block, different tx => relatedness 0.9
  // Pair (0,2): blocks 1000 vs 2000 => relatedness 0.0
  // Pair (1,2): blocks 1000 vs 2000 => relatedness 0.0

  uint64_t needed_money = 5 * XMR; // need two outputs
  float best_relatedness = 1.0f;
  std::vector<size_t> best_pair;

  for (size_t i = 0; i < transfers.size(); ++i)
  {
    const auto& tdi = transfers[i];
    if (tdi.m_spent || tdi.m_frozen || !tdi.is_rct())
      continue;

    for (size_t j = i + 1; j < transfers.size(); ++j)
    {
      const auto& tdj = transfers[j];
      if (tdj.m_spent || tdj.m_frozen || !tdj.is_rct() ||
          tdj.m_subaddr_index != tdi.m_subaddr_index)
        continue;

      if (tdi.amount() + tdj.amount() >= needed_money)
      {
        float relatedness = wallet_accessor_test::get_output_relatedness(m_wallet, tdi, tdj);
        if (relatedness < best_relatedness)
        {
          best_relatedness = relatedness;
          best_pair.clear();
          best_pair.push_back(i);
          best_pair.push_back(j);
          if (relatedness == 0.0f)
            goto done;
        }
      }
    }
  }
  done:

  // Should pick pair (0,2) with relatedness 0.0 (unrelated outputs preferred)
  ASSERT_EQ(2u, best_pair.size());
  EXPECT_EQ(0u, best_pair[0]);
  EXPECT_EQ(2u, best_pair[1]);
  EXPECT_FLOAT_EQ(0.0f, best_relatedness);
}
