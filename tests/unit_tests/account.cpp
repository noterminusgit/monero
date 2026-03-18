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

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/subaddress_index.h"
#include "crypto/crypto.h"
#include "device/device.hpp"

TEST(account, encrypt_keys)
{
  cryptonote::keypair recovery_key = cryptonote::keypair::generate(hw::get_device("default"));
  cryptonote::account_base account;
  crypto::secret_key key = account.generate(recovery_key.sec);
  const cryptonote::account_keys keys = account.get_keys();

  ASSERT_EQ(account.get_keys().m_account_address, keys.m_account_address);
  ASSERT_EQ(account.get_keys().m_spend_secret_key, keys.m_spend_secret_key);
  ASSERT_EQ(account.get_keys().m_view_secret_key, keys.m_view_secret_key);
  ASSERT_EQ(account.get_keys().m_multisig_keys, keys.m_multisig_keys);

  crypto::chacha_key chacha_key;
  crypto::generate_chacha_key(&recovery_key, sizeof(recovery_key), chacha_key, 1);

  account.encrypt_keys(chacha_key);

  ASSERT_EQ(account.get_keys().m_account_address, keys.m_account_address);
  ASSERT_NE(account.get_keys().m_spend_secret_key, keys.m_spend_secret_key);
  ASSERT_NE(account.get_keys().m_view_secret_key, keys.m_view_secret_key);

  account.decrypt_viewkey(chacha_key);

  ASSERT_EQ(account.get_keys().m_account_address, keys.m_account_address);
  ASSERT_NE(account.get_keys().m_spend_secret_key, keys.m_spend_secret_key);
  ASSERT_EQ(account.get_keys().m_view_secret_key, keys.m_view_secret_key);

  account.encrypt_viewkey(chacha_key);

  ASSERT_EQ(account.get_keys().m_account_address, keys.m_account_address);
  ASSERT_NE(account.get_keys().m_spend_secret_key, keys.m_spend_secret_key);
  ASSERT_NE(account.get_keys().m_view_secret_key, keys.m_view_secret_key);

  account.decrypt_keys(chacha_key);

  ASSERT_EQ(account.get_keys().m_account_address, keys.m_account_address);
  ASSERT_EQ(account.get_keys().m_spend_secret_key, keys.m_spend_secret_key);
  ASSERT_EQ(account.get_keys().m_view_secret_key, keys.m_view_secret_key);
}

TEST(account, generate_creates_valid_keys)
{
  cryptonote::account_base account;
  crypto::secret_key recovery_key = account.generate();
  const auto& keys = account.get_keys();

  // Keys should not be null
  EXPECT_NE(keys.m_spend_secret_key, crypto::null_skey);
  EXPECT_NE(keys.m_view_secret_key, crypto::null_skey);
  EXPECT_NE(keys.m_account_address.m_spend_public_key, crypto::null_pkey);
  EXPECT_NE(keys.m_account_address.m_view_public_key, crypto::null_pkey);
}

TEST(account, generate_deterministic_from_recovery_key)
{
  cryptonote::keypair recovery_key = cryptonote::keypair::generate(hw::get_device("default"));

  cryptonote::account_base account1;
  account1.generate(recovery_key.sec, true, false);

  cryptonote::account_base account2;
  account2.generate(recovery_key.sec, true, false);

  // Same recovery key should produce same account
  EXPECT_EQ(account1.get_keys().m_spend_secret_key, account2.get_keys().m_spend_secret_key);
  EXPECT_EQ(account1.get_keys().m_view_secret_key, account2.get_keys().m_view_secret_key);
  EXPECT_EQ(account1.get_keys().m_account_address.m_spend_public_key,
            account2.get_keys().m_account_address.m_spend_public_key);
}

TEST(account, different_recovery_keys_produce_different_accounts)
{
  cryptonote::keypair rk1 = cryptonote::keypair::generate(hw::get_device("default"));
  cryptonote::keypair rk2 = cryptonote::keypair::generate(hw::get_device("default"));

  cryptonote::account_base account1;
  account1.generate(rk1.sec);

  cryptonote::account_base account2;
  account2.generate(rk2.sec);

  EXPECT_NE(account1.get_keys().m_spend_secret_key, account2.get_keys().m_spend_secret_key);
  EXPECT_NE(account1.get_keys().m_account_address.m_spend_public_key,
            account2.get_keys().m_account_address.m_spend_public_key);
}

TEST(account, public_keys_match_secret_keys)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  // Verify spend public key matches spend secret key
  crypto::public_key spend_pub_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(keys.m_spend_secret_key, spend_pub_check));
  EXPECT_EQ(keys.m_account_address.m_spend_public_key, spend_pub_check);

  // Verify view public key matches view secret key
  crypto::public_key view_pub_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(keys.m_view_secret_key, view_pub_check));
  EXPECT_EQ(keys.m_account_address.m_view_public_key, view_pub_check);
}

TEST(account, get_public_address_str_not_empty)
{
  cryptonote::account_base account;
  account.generate();

  std::string addr_mainnet = account.get_public_address_str(cryptonote::MAINNET);
  std::string addr_testnet = account.get_public_address_str(cryptonote::TESTNET);
  std::string addr_stagenet = account.get_public_address_str(cryptonote::STAGENET);

  EXPECT_FALSE(addr_mainnet.empty());
  EXPECT_FALSE(addr_testnet.empty());
  EXPECT_FALSE(addr_stagenet.empty());

  // Different networks should produce different address strings
  EXPECT_NE(addr_mainnet, addr_testnet);
  EXPECT_NE(addr_mainnet, addr_stagenet);
}

TEST(account, encrypt_decrypt_roundtrip)
{
  cryptonote::account_base account;
  account.generate();
  const auto original_keys = account.get_keys();

  crypto::chacha_key chacha_key;
  crypto::generate_chacha_key(&original_keys.m_spend_secret_key,
    sizeof(original_keys.m_spend_secret_key), chacha_key, 1);

  account.encrypt_keys(chacha_key);
  // After encryption, secret keys should differ
  EXPECT_NE(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);

  account.decrypt_keys(chacha_key);
  // After decryption, keys should match original
  EXPECT_EQ(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);
  EXPECT_EQ(account.get_keys().m_view_secret_key, original_keys.m_view_secret_key);
}

TEST(account, multisig_keys_initially_empty)
{
  cryptonote::account_base account;
  account.generate();
  EXPECT_TRUE(account.get_keys().m_multisig_keys.empty());
}

TEST(account, set_null_creation_timestamp)
{
  cryptonote::account_base account;
  account.generate();
  account.set_createtime(0);
  EXPECT_EQ(account.get_createtime(), 0u);
}

TEST(account, creation_timestamp)
{
  cryptonote::account_base account;
  account.generate();
  uint64_t t = 1609459200;
  account.set_createtime(t);
  EXPECT_EQ(account.get_createtime(), t);
}

TEST(account, address_encode_decode_mainnet)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, addr);
  ASSERT_FALSE(str.empty());
  // Mainnet addresses start with '4'
  ASSERT_EQ(str[0], '4');

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));
  ASSERT_EQ(info.address, addr);
  ASSERT_FALSE(info.is_subaddress);
  ASSERT_FALSE(info.has_payment_id);
}

TEST(account, address_encode_decode_testnet)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  std::string str = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, addr);
  ASSERT_FALSE(str.empty());
  // Testnet addresses start with '9' or 'A'
  ASSERT_TRUE(str[0] == '9' || str[0] == 'A');

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, str));
  ASSERT_EQ(info.address, addr);
}

TEST(account, address_wrong_network_fails)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  std::string mainnet_str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, addr);
  cryptonote::address_parse_info info;
  // Mainnet address should fail parsing as testnet
  ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, mainnet_str));
}

TEST(account, invalid_address_string_fails)
{
  cryptonote::address_parse_info info;
  ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, "invalid_address_string"));
  ASSERT_FALSE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, ""));
}

TEST(account, integrated_address_roundtrip)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  crypto::hash8 payment_id;
  memset(&payment_id, 0xab, sizeof(payment_id));

  std::string integrated_str = cryptonote::get_account_integrated_address_as_str(
    cryptonote::MAINNET, addr, payment_id);
  ASSERT_FALSE(integrated_str.empty());

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, integrated_str));
  ASSERT_EQ(info.address, addr);
  ASSERT_TRUE(info.has_payment_id);
  ASSERT_EQ(info.payment_id, payment_id);
}

TEST(account, key_derivation_roundtrip)
{
  cryptonote::account_base sender, receiver;
  sender.generate();
  receiver.generate();

  const auto& sender_keys = sender.get_keys();
  const auto& receiver_keys = receiver.get_keys();

  // sender derives shared secret: r * A (receiver view public key)
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(
    receiver_keys.m_account_address.m_view_public_key,
    sender_keys.m_spend_secret_key, derivation));

  // receiver derives same shared secret: a * R (sender ephemeral public key)
  crypto::public_key sender_pub;
  ASSERT_TRUE(crypto::secret_key_to_public_key(sender_keys.m_spend_secret_key, sender_pub));

  crypto::key_derivation derivation2;
  ASSERT_TRUE(crypto::generate_key_derivation(
    sender_pub, receiver_keys.m_view_secret_key, derivation2));

  ASSERT_EQ(0, memcmp(&derivation, &derivation2, sizeof(derivation)));
}

TEST(account, derive_public_key)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  // Generate a key derivation from own keys
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(
    keys.m_account_address.m_view_public_key,
    keys.m_view_secret_key, derivation));

  // Derive a public key at index 0
  crypto::public_key derived;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0,
    keys.m_account_address.m_spend_public_key, derived));
  ASSERT_NE(derived, crypto::null_pkey);

  // Different index should give different key
  crypto::public_key derived2;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 1,
    keys.m_account_address.m_spend_public_key, derived2));
  ASSERT_NE(derived, derived2);
}

TEST(account, signature_roundtrip)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  crypto::hash msg;
  memset(&msg, 0x42, sizeof(msg));

  crypto::signature sig;
  crypto::generate_signature(msg,
    keys.m_account_address.m_spend_public_key,
    keys.m_spend_secret_key, sig);

  ASSERT_TRUE(crypto::check_signature(msg,
    keys.m_account_address.m_spend_public_key, sig));

  // Tampered message should fail
  crypto::hash tampered;
  memset(&tampered, 0x43, sizeof(tampered));
  ASSERT_FALSE(crypto::check_signature(tampered,
    keys.m_account_address.m_spend_public_key, sig));
}

TEST(account, key_image_generation)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  crypto::key_image ki;
  crypto::generate_key_image(
    keys.m_account_address.m_spend_public_key,
    keys.m_spend_secret_key, ki);
  crypto::key_image null_ki;
  memset(&null_ki, 0, sizeof(null_ki));
  ASSERT_NE(ki, null_ki);

  // Same input should produce same key image
  crypto::key_image ki2;
  crypto::generate_key_image(
    keys.m_account_address.m_spend_public_key,
    keys.m_spend_secret_key, ki2);
  ASSERT_EQ(ki, ki2);
}

TEST(account, subaddress_index_equality)
{
  cryptonote::subaddress_index idx1 = {0, 0};
  cryptonote::subaddress_index idx2 = {0, 0};
  cryptonote::subaddress_index idx3 = {1, 0};
  cryptonote::subaddress_index idx4 = {0, 1};

  ASSERT_EQ(idx1, idx2);
  ASSERT_NE(idx1, idx3);
  ASSERT_NE(idx1, idx4);
  ASSERT_TRUE(idx1.is_zero());
  ASSERT_FALSE(idx3.is_zero());
}

TEST(account, subaddress_generation)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  hw::device& dev = hw::get_device("default");

  // Subaddress {0,0} should be the main address
  cryptonote::subaddress_index idx0 = {0, 0};
  cryptonote::account_public_address subaddr0 = dev.get_subaddress(keys, idx0);
  ASSERT_EQ(subaddr0, keys.m_account_address);

  // Subaddress {0,1} should differ from main address
  cryptonote::subaddress_index idx1 = {0, 1};
  cryptonote::account_public_address subaddr1 = dev.get_subaddress(keys, idx1);
  ASSERT_NE(subaddr1, keys.m_account_address);

  // Subaddress generation should be deterministic
  cryptonote::account_public_address subaddr1_again = dev.get_subaddress(keys, idx1);
  ASSERT_EQ(subaddr1, subaddr1_again);

  // Different indices should give different subaddresses
  cryptonote::subaddress_index idx2 = {0, 2};
  cryptonote::account_public_address subaddr2 = dev.get_subaddress(keys, idx2);
  ASSERT_NE(subaddr1, subaddr2);
}

TEST(account, subaddress_address_string)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();
  hw::device& dev = hw::get_device("default");

  cryptonote::subaddress_index idx = {0, 1};
  cryptonote::account_public_address subaddr = dev.get_subaddress(keys, idx);

  std::string str = cryptonote::get_account_address_as_str(cryptonote::MAINNET, true, subaddr);
  ASSERT_FALSE(str.empty());
  // Subaddresses start with '8'
  ASSERT_EQ(str[0], '8');

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, str));
  ASSERT_TRUE(info.is_subaddress);
  ASSERT_EQ(info.address, subaddr);
}

// ===== Phase 7 extended tests =====

TEST(account, default_construction)
{
  cryptonote::account_base account;
  // Before generate(), the account should have null/zero keys
  const auto& keys = account.get_keys();
  ASSERT_EQ(keys.m_spend_secret_key, crypto::null_skey);
  ASSERT_EQ(keys.m_view_secret_key, crypto::null_skey);
}

TEST(account, generate_returns_nonzero_recovery_key)
{
  cryptonote::account_base account;
  crypto::secret_key recovery_key = account.generate();
  ASSERT_NE(recovery_key, crypto::null_skey);
}

TEST(account, generate_produces_different_keys_each_time)
{
  cryptonote::account_base account1, account2;
  account1.generate();
  account2.generate();
  ASSERT_NE(account1.get_keys().m_spend_secret_key, account2.get_keys().m_spend_secret_key);
  ASSERT_NE(account1.get_keys().m_view_secret_key, account2.get_keys().m_view_secret_key);
  ASSERT_NE(account1.get_keys().m_account_address.m_spend_public_key,
            account2.get_keys().m_account_address.m_spend_public_key);
}

TEST(account, get_keys_returns_nonzero_after_generate)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();
  ASSERT_NE(keys.m_spend_secret_key, crypto::null_skey);
  ASSERT_NE(keys.m_view_secret_key, crypto::null_skey);
  ASSERT_NE(keys.m_account_address.m_spend_public_key, crypto::null_pkey);
  ASSERT_NE(keys.m_account_address.m_view_public_key, crypto::null_pkey);
}

TEST(account, get_public_address_valid_struct)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;
  // Both public keys should be non-null
  ASSERT_NE(addr.m_spend_public_key, crypto::null_pkey);
  ASSERT_NE(addr.m_view_public_key, crypto::null_pkey);
  // They should also be different from each other
  ASSERT_NE(addr.m_spend_public_key, addr.m_view_public_key);
}

TEST(account, address_string_conversion_roundtrip_all_networks)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  for (auto net : {cryptonote::MAINNET, cryptonote::TESTNET, cryptonote::STAGENET})
  {
    std::string str = cryptonote::get_account_address_as_str(net, false, addr);
    ASSERT_FALSE(str.empty());

    cryptonote::address_parse_info info;
    ASSERT_TRUE(cryptonote::get_account_address_from_str(info, net, str));
    ASSERT_EQ(info.address, addr);
    ASSERT_FALSE(info.is_subaddress);
    ASSERT_FALSE(info.has_payment_id);
  }
}

TEST(account, view_key_derived_from_spend_key)
{
  // In Monero, the view key is derived from spend key via Keccak hash
  // Verify that they are related but not equal
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();

  ASSERT_NE(keys.m_spend_secret_key, keys.m_view_secret_key);

  // View public key should match view secret key
  crypto::public_key view_pub_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(keys.m_view_secret_key, view_pub_check));
  ASSERT_EQ(keys.m_account_address.m_view_public_key, view_pub_check);
}

TEST(account, account_keys_consistency_after_generate)
{
  cryptonote::account_base account;
  account.generate();
  const auto keys1 = account.get_keys();
  const auto keys2 = account.get_keys();

  // Repeated calls to get_keys() should return same values
  ASSERT_EQ(keys1.m_spend_secret_key, keys2.m_spend_secret_key);
  ASSERT_EQ(keys1.m_view_secret_key, keys2.m_view_secret_key);
  ASSERT_EQ(keys1.m_account_address.m_spend_public_key, keys2.m_account_address.m_spend_public_key);
  ASSERT_EQ(keys1.m_account_address.m_view_public_key, keys2.m_account_address.m_view_public_key);
}

TEST(account, double_encrypt_decrypt)
{
  cryptonote::account_base account;
  account.generate();
  const auto original_keys = account.get_keys();

  crypto::chacha_key key1, key2;
  crypto::generate_chacha_key(&original_keys.m_spend_secret_key,
    sizeof(original_keys.m_spend_secret_key), key1, 1);
  crypto::generate_chacha_key(&original_keys.m_view_secret_key,
    sizeof(original_keys.m_view_secret_key), key2, 1);

  // First encrypt/decrypt cycle
  account.encrypt_keys(key1);
  ASSERT_NE(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);
  account.decrypt_keys(key1);
  ASSERT_EQ(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);

  // Second encrypt/decrypt with different key
  account.encrypt_keys(key2);
  ASSERT_NE(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);
  account.decrypt_keys(key2);
  ASSERT_EQ(account.get_keys().m_spend_secret_key, original_keys.m_spend_secret_key);
}

TEST(account, createtime_default)
{
  cryptonote::account_base account;
  // Default createtime should be 0
  ASSERT_EQ(account.get_createtime(), 0u);
}

TEST(account, createtime_preserves_value)
{
  cryptonote::account_base account;
  account.generate();
  uint64_t times[] = {0, 1, 1609459200, 0xFFFFFFFFFFFFFFFF};
  for (uint64_t t : times)
  {
    account.set_createtime(t);
    ASSERT_EQ(account.get_createtime(), t);
  }
}

TEST(account, subaddress_different_accounts_different)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();
  hw::device& dev = hw::get_device("default");

  // Different major indices should yield different subaddresses
  cryptonote::subaddress_index idx_0_1 = {0, 1};
  cryptonote::subaddress_index idx_1_0 = {1, 0};
  cryptonote::subaddress_index idx_1_1 = {1, 1};

  auto sub_0_1 = dev.get_subaddress(keys, idx_0_1);
  auto sub_1_0 = dev.get_subaddress(keys, idx_1_0);
  auto sub_1_1 = dev.get_subaddress(keys, idx_1_1);

  ASSERT_NE(sub_0_1, sub_1_0);
  ASSERT_NE(sub_0_1, sub_1_1);
  ASSERT_NE(sub_1_0, sub_1_1);
}

TEST(account, subaddress_string_testnet)
{
  cryptonote::account_base account;
  account.generate();
  const auto& keys = account.get_keys();
  hw::device& dev = hw::get_device("default");

  cryptonote::subaddress_index idx = {0, 1};
  auto subaddr = dev.get_subaddress(keys, idx);

  std::string str = cryptonote::get_account_address_as_str(cryptonote::TESTNET, true, subaddr);
  ASSERT_FALSE(str.empty());

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, str));
  ASSERT_TRUE(info.is_subaddress);
  ASSERT_EQ(info.address, subaddr);
}

TEST(account, integrated_address_testnet)
{
  cryptonote::account_base account;
  account.generate();
  const auto& addr = account.get_keys().m_account_address;

  crypto::hash8 payment_id;
  memset(&payment_id, 0xcd, sizeof(payment_id));

  std::string integrated_str = cryptonote::get_account_integrated_address_as_str(
    cryptonote::TESTNET, addr, payment_id);
  ASSERT_FALSE(integrated_str.empty());

  cryptonote::address_parse_info info;
  ASSERT_TRUE(cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, integrated_str));
  ASSERT_TRUE(info.has_payment_id);
  ASSERT_EQ(info.payment_id, payment_id);
  ASSERT_EQ(info.address, addr);
}
