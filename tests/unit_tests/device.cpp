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
#include "ringct/rctOps.h"
#include "device/device_default.hpp"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/subaddress_index.h"
#include "cryptonote_core/cryptonote_tx_utils.h"

TEST(device, name)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.set_name("test"));
  ASSERT_EQ(dev.get_name(), "test");
}

/*
TEST(device, locking)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.try_lock());
  ASSERT_FALSE(dev.try_lock());
  dev.unlock();
  ASSERT_TRUE(dev.try_lock());
  dev.unlock();
  dev.lock();
  ASSERT_FALSE(dev.try_lock());
  dev.unlock();
  ASSERT_TRUE(dev.try_lock());
  dev.unlock();
}
*/

TEST(device, open_close)
{
  hw::core::device_default dev;
  crypto::secret_key key;
  ASSERT_TRUE(dev.open_tx(key));
  ASSERT_TRUE(dev.close_tx());
}

TEST(device, ops)
{
  hw::core::device_default dev;
  rct::key resd, res;
  crypto::key_derivation derd, der;
  rct::key sk, pk;
  crypto::secret_key sk0, sk1;
  crypto::public_key pk0, pk1;
  crypto::ec_scalar ressc0, ressc1;
  crypto::key_image ki0, ki1;

  rct::skpkGen(sk, pk);
  rct::scalarmultBase((rct::key&)pk0, (rct::key&)sk0);
  rct::scalarmultBase((rct::key&)pk1, (rct::key&)sk1);

  dev.scalarmultKey(resd, pk, sk);
  rct::scalarmultKey(res, pk, sk);
  ASSERT_EQ(resd, res);

  dev.scalarmultBase(resd, sk);
  rct::scalarmultBase(res, sk);
  ASSERT_EQ(resd, res);

  dev.sc_secret_add((crypto::secret_key&)resd, sk0, sk1);
  sc_add((unsigned char*)&res, (unsigned char*)&sk0, (unsigned char*)&sk1);
  ASSERT_EQ(resd, res);

  dev.generate_key_derivation(pk0, sk0, derd);
  crypto::generate_key_derivation(pk0, sk0, der);
  ASSERT_FALSE(memcmp(&derd, &der, sizeof(der)));

  dev.derivation_to_scalar(der, 0, ressc0);
  crypto::derivation_to_scalar(der, 0, ressc1);
  ASSERT_FALSE(memcmp(&ressc0, &ressc1, sizeof(ressc1)));

  dev.derive_secret_key(der, 0, rct::rct2sk(sk), sk0);
  crypto::derive_secret_key(der, 0, rct::rct2sk(sk), sk1);
  ASSERT_EQ(sk0, sk1);

  dev.derive_public_key(der, 0, rct::rct2pk(pk), pk0);
  crypto::derive_public_key(der, 0, rct::rct2pk(pk), pk1);
  ASSERT_EQ(pk0, pk1);

  dev.secret_key_to_public_key(rct::rct2sk(sk), pk0);
  crypto::secret_key_to_public_key(rct::rct2sk(sk), pk1);
  ASSERT_EQ(pk0, pk1);

  dev.generate_key_image(pk0, sk0, ki0);
  crypto::generate_key_image(pk0, sk0, ki1);
  ASSERT_EQ(ki0, ki1);
}

TEST(device, ecdh32)
{
  hw::core::device_default dev;
  rct::ecdhTuple tuple, tuple2;
  rct::key key = rct::skGen();
  tuple.mask = rct::skGen();
  tuple.amount = rct::skGen();
  tuple2 = tuple;
  dev.ecdhEncode(tuple, key, false);
  dev.ecdhDecode(tuple, key, false);
  ASSERT_EQ(tuple2.mask, tuple.mask);
  ASSERT_EQ(tuple2.amount, tuple.amount);
}

TEST(device, type)
{
  hw::core::device_default dev;
  ASSERT_EQ(dev.get_type(), hw::device::device_type::SOFTWARE);
}

TEST(device, init_release)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.init());
  ASSERT_TRUE(dev.release());
}

TEST(device, connect_disconnect)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.connect());
  ASSERT_TRUE(dev.disconnect());
}

TEST(device, set_mode)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.set_mode(hw::device::TRANSACTION_CREATE_REAL));
  ASSERT_TRUE(dev.set_mode(hw::device::TRANSACTION_CREATE_FAKE));
  ASSERT_TRUE(dev.set_mode(hw::device::NONE));
}

TEST(device, scalarmult_identity)
{
  hw::core::device_default dev;
  rct::key result;
  rct::key sk = rct::skGen();

  // Scalar mult of base should produce a valid point
  dev.scalarmultBase(result, sk);
  ASSERT_NE(result, rct::identity());

  // Verify it matches the direct computation
  rct::key expected;
  rct::scalarmultBase(expected, sk);
  ASSERT_EQ(result, expected);
}

TEST(device, generate_keys)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  // Verify the key pair is consistent
  crypto::public_key pk_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(sk, pk_check));
  ASSERT_EQ(pk, pk_check);
}

TEST(device, ecdh8_roundtrip)
{
  hw::core::device_default dev;
  rct::ecdhTuple tuple, tuple2;
  rct::key key = rct::skGen();
  tuple.mask = rct::skGen();
  tuple.amount = rct::skGen();
  tuple2 = tuple;
  dev.ecdhEncode(tuple, key, true);
  dev.ecdhDecode(tuple, key, true);
  // For 8-byte mode, only the first 8 bytes of amount are preserved
  ASSERT_EQ(memcmp(tuple2.amount.bytes, tuple.amount.bytes, 8), 0);
}

TEST(device, multiple_tx_open_close)
{
  hw::core::device_default dev;
  crypto::secret_key key;

  // Open and close multiple transactions
  ASSERT_TRUE(dev.open_tx(key));
  ASSERT_TRUE(dev.close_tx());
  ASSERT_TRUE(dev.open_tx(key));
  ASSERT_TRUE(dev.close_tx());
}

TEST(device, verify_keys_valid)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);
  ASSERT_TRUE(dev.verify_keys(sk, pk));
}

TEST(device, verify_keys_invalid)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);
  // Corrupt the public key
  pk.data[0] ^= 0xFF;
  ASSERT_FALSE(dev.verify_keys(sk, pk));
}

TEST(device, derive_view_tag)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation derivation;
  dev.generate_key_derivation(pk, sk, derivation);

  crypto::view_tag vt1, vt2;
  dev.derive_view_tag(derivation, 0, vt1);
  dev.derive_view_tag(derivation, 1, vt2);
  // Different output indices should produce different view tags (very likely)
  // Just verify it doesn't crash; tags may collide rarely
  (void)vt1;
  (void)vt2;
}

TEST(device, derive_view_tag_deterministic)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation derivation;
  dev.generate_key_derivation(pk, sk, derivation);

  crypto::view_tag vt1, vt2;
  dev.derive_view_tag(derivation, 5, vt1);
  dev.derive_view_tag(derivation, 5, vt2);
  ASSERT_EQ(vt1, vt2);
}

TEST(device, derive_subaddress_public_key)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation derivation;
  dev.generate_key_derivation(pk, sk, derivation);

  crypto::public_key derived;
  ASSERT_TRUE(dev.derive_subaddress_public_key(pk, derivation, 0, derived));
  ASSERT_NE(derived, pk);
}

TEST(device, subaddress_derivation)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx{0, 1};

  crypto::public_key spend_pk = dev.get_subaddress_spend_public_key(keys, idx);
  // Subaddress spend key should differ from main spend key
  ASSERT_NE(spend_pk, keys.m_account_address.m_spend_public_key);
}

TEST(device, encrypt_payment_id_roundtrip)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::hash8 pid;
  memset(&pid, 0xAB, sizeof(pid));
  crypto::hash8 original_pid = pid;

  dev.encrypt_payment_id(pid, pk, sk);
  // After encryption, pid should be different
  ASSERT_NE(memcmp(&pid, &original_pid, sizeof(pid)), 0);

  // Decrypt (encrypt again with same params)
  dev.encrypt_payment_id(pid, pk, sk);
  ASSERT_EQ(memcmp(&pid, &original_pid, sizeof(pid)), 0);
}

TEST(device, get_device_default)
{
  // get_device("default") should return a software device
  hw::device &dev = hw::get_device("default");
  ASSERT_EQ(dev.get_type(), hw::device::device_type::SOFTWARE);
}

TEST(device, commitment_mask)
{
  hw::core::device_default dev;
  rct::key amount_key = rct::skGen();
  rct::key mask = dev.genCommitmentMask(amount_key);
  ASSERT_NE(mask, rct::zero());
  ASSERT_NE(mask, rct::identity());
}

TEST(device, mlsag_hash)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());
  rct::key hash;
  dev.mlsag_hash(data, hash);
  ASSERT_NE(hash, rct::zero());
}

TEST(device, clsag_hash)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());
  rct::key hash;
  dev.clsag_hash(data, hash);
  ASSERT_NE(hash, rct::zero());
}

// =========================================================================
// 1. Device identity and lifecycle tests
// =========================================================================

TEST(device, get_mode_default)
{
  hw::core::device_default dev;
  ASSERT_EQ(dev.get_mode(), hw::device::NONE);
}

TEST(device, set_mode_and_verify)
{
  hw::core::device_default dev;
  dev.set_mode(hw::device::TRANSACTION_CREATE_REAL);
  ASSERT_EQ(dev.get_mode(), hw::device::TRANSACTION_CREATE_REAL);
  dev.set_mode(hw::device::TRANSACTION_CREATE_FAKE);
  ASSERT_EQ(dev.get_mode(), hw::device::TRANSACTION_CREATE_FAKE);
  dev.set_mode(hw::device::TRANSACTION_PARSE);
  ASSERT_EQ(dev.get_mode(), hw::device::TRANSACTION_PARSE);
  dev.set_mode(hw::device::NONE);
  ASSERT_EQ(dev.get_mode(), hw::device::NONE);
}

TEST(device, set_name_empty)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.set_name(""));
  ASSERT_EQ(dev.get_name(), "");
}

TEST(device, set_name_long)
{
  hw::core::device_default dev;
  std::string long_name(1024, 'x');
  ASSERT_TRUE(dev.set_name(long_name));
  ASSERT_EQ(dev.get_name(), long_name);
}

TEST(device, set_name_overwrite)
{
  hw::core::device_default dev;
  ASSERT_TRUE(dev.set_name("first"));
  ASSERT_EQ(dev.get_name(), "first");
  ASSERT_TRUE(dev.set_name("second"));
  ASSERT_EQ(dev.get_name(), "second");
}

TEST(device, multiple_init_release_cycles)
{
  hw::core::device_default dev;
  for (int i = 0; i < 10; ++i)
  {
    ASSERT_TRUE(dev.init());
    ASSERT_TRUE(dev.release());
  }
}

TEST(device, multiple_connect_disconnect_cycles)
{
  hw::core::device_default dev;
  for (int i = 0; i < 10; ++i)
  {
    ASSERT_TRUE(dev.connect());
    ASSERT_TRUE(dev.disconnect());
  }
}

TEST(device, open_tx_close_tx_multiple_cycles)
{
  hw::core::device_default dev;
  for (int i = 0; i < 5; ++i)
  {
    crypto::secret_key key;
    ASSERT_TRUE(dev.open_tx(key));
    ASSERT_TRUE(dev.close_tx());
  }
}

TEST(device, open_tx_produces_valid_key)
{
  hw::core::device_default dev;
  crypto::secret_key tx_key;
  ASSERT_TRUE(dev.open_tx(tx_key));
  // tx_key should be a valid secret key: verify it maps to a public key
  crypto::public_key pub;
  ASSERT_TRUE(crypto::secret_key_to_public_key(tx_key, pub));
  ASSERT_TRUE(dev.close_tx());
}

TEST(device, open_tx_different_keys)
{
  hw::core::device_default dev;
  crypto::secret_key key1, key2;
  ASSERT_TRUE(dev.open_tx(key1));
  ASSERT_TRUE(dev.close_tx());
  ASSERT_TRUE(dev.open_tx(key2));
  ASSERT_TRUE(dev.close_tx());
  ASSERT_NE(key1, key2);
}

TEST(device, device_protocol_default)
{
  hw::core::device_default dev;
  ASSERT_EQ(dev.device_protocol(), hw::device::PROTOCOL_DEFAULT);
}

// =========================================================================
// 2. Key generation tests
// =========================================================================

TEST(device, generate_keys_roundtrip)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  // Verify secret_key_to_public_key round-trip
  crypto::public_key pk_check;
  ASSERT_TRUE(dev.secret_key_to_public_key(sk, pk_check));
  ASSERT_EQ(pk, pk_check);
}

TEST(device, generate_keys_different_each_time)
{
  hw::core::device_default dev;
  crypto::public_key pk1, pk2;
  crypto::secret_key sk1, sk2;
  dev.generate_keys(pk1, sk1);
  dev.generate_keys(pk2, sk2);
  ASSERT_NE(pk1, pk2);
  ASSERT_NE(sk1, sk2);
}

TEST(device, generate_keys_multiple_unique)
{
  hw::core::device_default dev;
  std::vector<crypto::public_key> pks;
  for (int i = 0; i < 10; ++i)
  {
    crypto::public_key pk;
    crypto::secret_key sk;
    dev.generate_keys(pk, sk);
    for (const auto &prev : pks)
      ASSERT_NE(pk, prev);
    pks.push_back(pk);
  }
}

TEST(device, generate_keys_with_recovery)
{
  hw::core::device_default dev;
  crypto::public_key pk1, pk2;
  crypto::secret_key sk1, sk2;
  dev.generate_keys(pk1, sk1);

  // Recover the same key pair from the secret key
  dev.generate_keys(pk2, sk2, sk1, true);
  ASSERT_EQ(pk1, pk2);
  ASSERT_EQ(sk1, sk2);
}

TEST(device, generate_chacha_key_deterministic)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  crypto::chacha_key chacha1, chacha2;
  dev.generate_chacha_key(keys, chacha1, 1);
  dev.generate_chacha_key(keys, chacha2, 1);
  ASSERT_EQ(memcmp(&chacha1, &chacha2, sizeof(chacha1)), 0);
}

TEST(device, generate_chacha_key_different_for_different_accounts)
{
  hw::core::device_default dev;
  cryptonote::account_base account1, account2;
  // Use generate() without recovery so each account gets its own random keys
  account1.generate();
  account2.generate();

  crypto::chacha_key chacha1, chacha2;
  dev.generate_chacha_key(account1.get_keys(), chacha1, 1);
  dev.generate_chacha_key(account2.get_keys(), chacha2, 1);
  ASSERT_NE(memcmp(&chacha1, &chacha2, sizeof(chacha1)), 0);
}

TEST(device, get_subaddress_spend_public_key_zero_index)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx{0, 0};
  crypto::public_key spend_pk = dev.get_subaddress_spend_public_key(keys, idx);
  // Index {0,0} should return the main spend public key
  ASSERT_EQ(spend_pk, keys.m_account_address.m_spend_public_key);
}

TEST(device, get_subaddress_spend_public_key_nonzero_index)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx{0, 1};
  crypto::public_key spend_pk = dev.get_subaddress_spend_public_key(keys, idx);
  ASSERT_NE(spend_pk, keys.m_account_address.m_spend_public_key);
}

TEST(device, get_subaddress_spend_public_key_different_indices)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{0, 2};
  crypto::public_key pk1 = dev.get_subaddress_spend_public_key(keys, idx1);
  crypto::public_key pk2 = dev.get_subaddress_spend_public_key(keys, idx2);
  ASSERT_NE(pk1, pk2);
}

TEST(device, get_subaddress_zero_vs_nonzero)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx0{0, 0};
  cryptonote::subaddress_index idx1{0, 1};
  auto addr0 = dev.get_subaddress(keys, idx0);
  auto addr1 = dev.get_subaddress(keys, idx1);

  // Index {0,0} should return the main address
  ASSERT_EQ(addr0.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  ASSERT_EQ(addr0.m_view_public_key, keys.m_account_address.m_view_public_key);
  // Index {0,1} should differ
  ASSERT_NE(addr1.m_spend_public_key, addr0.m_spend_public_key);
  ASSERT_NE(addr1.m_view_public_key, addr0.m_view_public_key);
}

TEST(device, get_subaddress_secret_key_deterministic)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx{0, 1};
  crypto::secret_key sk1 = dev.get_subaddress_secret_key(keys.m_view_secret_key, idx);
  crypto::secret_key sk2 = dev.get_subaddress_secret_key(keys.m_view_secret_key, idx);
  ASSERT_EQ(sk1, sk2);
}

TEST(device, get_subaddress_secret_key_different_indices)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{0, 2};
  crypto::secret_key sk1 = dev.get_subaddress_secret_key(keys.m_view_secret_key, idx1);
  crypto::secret_key sk2 = dev.get_subaddress_secret_key(keys.m_view_secret_key, idx2);
  ASSERT_NE(sk1, sk2);
}

TEST(device, get_subaddress_spend_public_keys_batch)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  auto batch = dev.get_subaddress_spend_public_keys(keys, 0, 0, 5);
  ASSERT_EQ(batch.size(), 5u);

  // Verify each batch entry matches individual computation
  for (uint32_t i = 0; i < 5; ++i)
  {
    cryptonote::subaddress_index idx{0, i};
    crypto::public_key single = dev.get_subaddress_spend_public_key(keys, idx);
    ASSERT_EQ(batch[i], single);
  }
}

TEST(device, derive_subaddress_public_key_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation derivation;
  dev.generate_key_derivation(pk, sk, derivation);

  crypto::public_key derived1, derived2;
  ASSERT_TRUE(dev.derive_subaddress_public_key(pk, derivation, 0, derived1));
  ASSERT_TRUE(dev.derive_subaddress_public_key(pk, derivation, 0, derived2));
  ASSERT_EQ(derived1, derived2);
}

TEST(device, derive_subaddress_public_key_different_indices)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation derivation;
  dev.generate_key_derivation(pk, sk, derivation);

  crypto::public_key derived0, derived1;
  ASSERT_TRUE(dev.derive_subaddress_public_key(pk, derivation, 0, derived0));
  ASSERT_TRUE(dev.derive_subaddress_public_key(pk, derivation, 1, derived1));
  ASSERT_NE(derived0, derived1);
}

// =========================================================================
// 3. Cryptographic operations tests
// =========================================================================

TEST(device, scalarmultKey_with_identity)
{
  hw::core::device_default dev;
  rct::key result;
  rct::key sk = rct::skGen();
  // scalarmultKey(identity, scalar) should give identity
  dev.scalarmultKey(result, rct::identity(), sk);
  ASSERT_EQ(result, rct::identity());
}

TEST(device, scalarmultBase_with_one)
{
  hw::core::device_default dev;
  rct::key result;
  // scalarmultBase(1) should give the generator G
  rct::key one = rct::identity();
  one.bytes[0] = 1;
  memset(one.bytes + 1, 0, 31);
  dev.scalarmultBase(result, one);
  ASSERT_NE(result, rct::identity());
  ASSERT_NE(result, rct::zero());
}

TEST(device, scalarmultBase_consistency)
{
  hw::core::device_default dev;
  rct::key sk = rct::skGen();
  rct::key res_dev, res_rct;
  dev.scalarmultBase(res_dev, sk);
  rct::scalarmultBase(res_rct, sk);
  ASSERT_EQ(res_dev, res_rct);
}

TEST(device, scalarmultKey_consistency)
{
  hw::core::device_default dev;
  rct::key sk, pk;
  rct::skpkGen(sk, pk);
  rct::key scalar = rct::skGen();
  rct::key res_dev, res_rct;
  dev.scalarmultKey(res_dev, pk, scalar);
  rct::scalarmultKey(res_rct, pk, scalar);
  ASSERT_EQ(res_dev, res_rct);
}

TEST(device, sc_secret_add_commutativity)
{
  hw::core::device_default dev;
  crypto::secret_key a, b;
  crypto::public_key dummy;
  dev.generate_keys(dummy, a);
  dev.generate_keys(dummy, b);

  crypto::secret_key ab, ba;
  dev.sc_secret_add(ab, a, b);
  dev.sc_secret_add(ba, b, a);
  ASSERT_EQ(ab, ba);
}

TEST(device, sc_secret_add_consistency_with_sc_add)
{
  hw::core::device_default dev;
  crypto::secret_key a, b;
  crypto::public_key dummy;
  dev.generate_keys(dummy, a);
  dev.generate_keys(dummy, b);

  crypto::secret_key result_dev;
  dev.sc_secret_add(result_dev, a, b);

  unsigned char result_raw[32];
  sc_add(result_raw, (const unsigned char*)&a, (const unsigned char*)&b);
  ASSERT_EQ(memcmp(&result_dev, result_raw, 32), 0);
}

TEST(device, generate_key_derivation_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der_dev, der_crypto;
  ASSERT_TRUE(dev.generate_key_derivation(pk, sk, der_dev));
  ASSERT_TRUE(crypto::generate_key_derivation(pk, sk, der_crypto));
  ASSERT_EQ(memcmp(&der_dev, &der_crypto, sizeof(der_dev)), 0);
}

TEST(device, generate_key_derivation_deterministic)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der1, der2;
  ASSERT_TRUE(dev.generate_key_derivation(pk, sk, der1));
  ASSERT_TRUE(dev.generate_key_derivation(pk, sk, der2));
  ASSERT_EQ(memcmp(&der1, &der2, sizeof(der1)), 0);
}

TEST(device, derivation_to_scalar_different_indices)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::ec_scalar sc0, sc1, sc100;
  dev.derivation_to_scalar(der, 0, sc0);
  dev.derivation_to_scalar(der, 1, sc1);
  dev.derivation_to_scalar(der, 100, sc100);

  ASSERT_NE(memcmp(&sc0, &sc1, sizeof(sc0)), 0);
  ASSERT_NE(memcmp(&sc0, &sc100, sizeof(sc0)), 0);
  ASSERT_NE(memcmp(&sc1, &sc100, sizeof(sc1)), 0);
}

TEST(device, derivation_to_scalar_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::ec_scalar sc_dev, sc_crypto;
  dev.derivation_to_scalar(der, 42, sc_dev);
  crypto::derivation_to_scalar(der, 42, sc_crypto);
  ASSERT_EQ(memcmp(&sc_dev, &sc_crypto, sizeof(sc_dev)), 0);
}

TEST(device, derive_secret_key_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::secret_key derived_dev, derived_crypto;
  dev.derive_secret_key(der, 0, sk, derived_dev);
  crypto::derive_secret_key(der, 0, sk, derived_crypto);
  ASSERT_EQ(derived_dev, derived_crypto);
}

TEST(device, derive_secret_key_different_indices)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::secret_key derived0, derived1;
  dev.derive_secret_key(der, 0, sk, derived0);
  dev.derive_secret_key(der, 1, sk, derived1);
  ASSERT_NE(derived0, derived1);
}

TEST(device, derive_public_key_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::public_key derived_dev, derived_crypto;
  ASSERT_TRUE(dev.derive_public_key(der, 0, pk, derived_dev));
  ASSERT_TRUE(crypto::derive_public_key(der, 0, pk, derived_crypto));
  ASSERT_EQ(derived_dev, derived_crypto);
}

TEST(device, derive_public_key_different_indices)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::public_key derived0, derived1;
  ASSERT_TRUE(dev.derive_public_key(der, 0, pk, derived0));
  ASSERT_TRUE(dev.derive_public_key(der, 1, pk, derived1));
  ASSERT_NE(derived0, derived1);
}

TEST(device, secret_key_to_public_key_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::public_key pk_dev, pk_crypto;
  ASSERT_TRUE(dev.secret_key_to_public_key(sk, pk_dev));
  ASSERT_TRUE(crypto::secret_key_to_public_key(sk, pk_crypto));
  ASSERT_EQ(pk_dev, pk_crypto);
  ASSERT_EQ(pk_dev, pk);
}

TEST(device, generate_key_image_consistency)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_image ki_dev, ki_crypto;
  dev.generate_key_image(pk, sk, ki_dev);
  crypto::generate_key_image(pk, sk, ki_crypto);
  ASSERT_EQ(ki_dev, ki_crypto);
}

TEST(device, generate_key_image_deterministic)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_image ki1, ki2;
  dev.generate_key_image(pk, sk, ki1);
  dev.generate_key_image(pk, sk, ki2);
  ASSERT_EQ(ki1, ki2);
}

TEST(device, generate_key_image_different_keys)
{
  hw::core::device_default dev;
  crypto::public_key pk1, pk2;
  crypto::secret_key sk1, sk2;
  dev.generate_keys(pk1, sk1);
  dev.generate_keys(pk2, sk2);

  crypto::key_image ki1, ki2;
  dev.generate_key_image(pk1, sk1, ki1);
  dev.generate_key_image(pk2, sk2, ki2);
  ASSERT_NE(ki1, ki2);
}

TEST(device, conceal_derivation_noop)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der, der_original;
  dev.generate_key_derivation(pk, sk, der);
  der_original = der;

  std::vector<crypto::public_key> additional_pub_keys;
  std::vector<crypto::key_derivation> additional_derivations;

  // conceal_derivation is a no-op for software device; should return true
  ASSERT_TRUE(dev.conceal_derivation(der, pk, additional_pub_keys, der_original, additional_derivations));
  // derivation should remain unchanged
  ASSERT_EQ(memcmp(&der, &der_original, sizeof(der)), 0);
}

TEST(device, derive_public_and_secret_key_correspondence)
{
  // Verify that derived public key = derived_secret_key * G
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  crypto::public_key derived_pub;
  crypto::secret_key derived_sec;
  ASSERT_TRUE(dev.derive_public_key(der, 0, pk, derived_pub));
  dev.derive_secret_key(der, 0, sk, derived_sec);

  crypto::public_key pub_from_sec;
  ASSERT_TRUE(dev.secret_key_to_public_key(derived_sec, pub_from_sec));
  ASSERT_EQ(derived_pub, pub_from_sec);
}

// =========================================================================
// 4. ECDH extended tests
// =========================================================================

TEST(device, ecdh32_encode_decode_roundtrip)
{
  hw::core::device_default dev;
  rct::ecdhTuple original, tuple;
  rct::key key = rct::skGen();
  original.mask = rct::skGen();
  original.amount = rct::skGen();
  tuple = original;

  dev.ecdhEncode(tuple, key, false);
  // After encoding, values should differ
  ASSERT_NE(tuple.mask, original.mask);
  ASSERT_NE(tuple.amount, original.amount);

  dev.ecdhDecode(tuple, key, false);
  ASSERT_EQ(tuple.mask, original.mask);
  ASSERT_EQ(tuple.amount, original.amount);
}

TEST(device, ecdh32_wrong_key_fails)
{
  hw::core::device_default dev;
  rct::ecdhTuple original, tuple;
  rct::key keyA = rct::skGen();
  rct::key keyB = rct::skGen();
  original.mask = rct::skGen();
  original.amount = rct::skGen();
  tuple = original;

  dev.ecdhEncode(tuple, keyA, false);
  dev.ecdhDecode(tuple, keyB, false);

  // Decoding with wrong key should not recover original
  ASSERT_NE(tuple.mask, original.mask);
  ASSERT_NE(tuple.amount, original.amount);
}

TEST(device, ecdh8_encode_decode_roundtrip)
{
  hw::core::device_default dev;
  rct::ecdhTuple original, tuple;
  rct::key key = rct::skGen();

  // For 8-byte mode, amount is typically small (fits in 8 bytes)
  memset(&original.amount, 0, sizeof(original.amount));
  uint64_t amount_val = 123456789ULL;
  memcpy(original.amount.bytes, &amount_val, sizeof(amount_val));
  original.mask = rct::skGen();

  tuple = original;
  dev.ecdhEncode(tuple, key, true);
  dev.ecdhDecode(tuple, key, true);
  // First 8 bytes of amount should be preserved
  ASSERT_EQ(memcmp(tuple.amount.bytes, original.amount.bytes, 8), 0);
}

TEST(device, ecdh8_wrong_key_fails)
{
  hw::core::device_default dev;
  rct::ecdhTuple original, tuple;
  rct::key keyA = rct::skGen();
  rct::key keyB = rct::skGen();

  memset(&original.amount, 0, sizeof(original.amount));
  uint64_t amount_val = 42ULL;
  memcpy(original.amount.bytes, &amount_val, sizeof(amount_val));
  original.mask = rct::skGen();

  tuple = original;
  dev.ecdhEncode(tuple, keyA, true);
  dev.ecdhDecode(tuple, keyB, true);
  // First 8 bytes should not match
  ASSERT_NE(memcmp(tuple.amount.bytes, original.amount.bytes, 8), 0);
}

TEST(device, ecdh_encode_is_not_identity)
{
  hw::core::device_default dev;
  rct::ecdhTuple tuple;
  rct::key key = rct::skGen();
  tuple.mask = rct::skGen();
  tuple.amount = rct::skGen();
  rct::ecdhTuple original = tuple;

  dev.ecdhEncode(tuple, key, false);
  // Encoded values should differ from originals
  bool mask_same = (tuple.mask == original.mask);
  bool amount_same = (tuple.amount == original.amount);
  ASSERT_FALSE(mask_same && amount_same);
}

// =========================================================================
// 5. MLSAG/CLSAG operations tests
// =========================================================================

TEST(device, mlsag_prehash_deterministic)
{
  hw::core::device_default dev;
  rct::keyV hashes;
  hashes.push_back(rct::skGen());
  hashes.push_back(rct::skGen());
  hashes.push_back(rct::skGen());
  rct::ctkeyV outPk;

  rct::key prehash1, prehash2;
  dev.mlsag_prehash("blob", 1, 1, hashes, outPk, prehash1);
  dev.mlsag_prehash("blob", 1, 1, hashes, outPk, prehash2);
  ASSERT_EQ(prehash1, prehash2);
}

TEST(device, mlsag_prehash_different_hashes)
{
  hw::core::device_default dev;
  rct::keyV hashes1, hashes2;
  hashes1.push_back(rct::skGen());
  hashes2.push_back(rct::skGen());
  rct::ctkeyV outPk;

  rct::key prehash1, prehash2;
  dev.mlsag_prehash("blob", 1, 1, hashes1, outPk, prehash1);
  dev.mlsag_prehash("blob", 1, 1, hashes2, outPk, prehash2);
  ASSERT_NE(prehash1, prehash2);
}

TEST(device, mlsag_prepare_simple)
{
  hw::core::device_default dev;
  rct::key a, aG;
  ASSERT_TRUE(dev.mlsag_prepare(a, aG));
  // aG should equal a * G
  rct::key expected_aG;
  rct::scalarmultBase(expected_aG, a);
  ASSERT_EQ(aG, expected_aG);
}

TEST(device, mlsag_prepare_full)
{
  hw::core::device_default dev;
  // Generate a valid curve point H = hash_to_p3(random) converted to key
  rct::key tmp = rct::skGen();
  ge_p3 H_p3;
  rct::hash_to_p3(H_p3, tmp);
  rct::key H;
  ge_p3_tobytes(H.bytes, &H_p3);
  rct::key xx = rct::skGen();
  rct::key a, aG, aHP, II;

  ASSERT_TRUE(dev.mlsag_prepare(H, xx, a, aG, aHP, II));

  // Verify aG = a*G
  rct::key expected_aG;
  rct::scalarmultBase(expected_aG, a);
  ASSERT_EQ(aG, expected_aG);

  // Verify aHP = a*H
  rct::key expected_aHP;
  rct::scalarmultKey(expected_aHP, H, a);
  ASSERT_EQ(aHP, expected_aHP);

  // Verify II = xx*H
  rct::key expected_II;
  rct::scalarmultKey(expected_II, H, xx);
  ASSERT_EQ(II, expected_II);
}

TEST(device, mlsag_hash_deterministic)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());

  rct::key hash1, hash2;
  dev.mlsag_hash(data, hash1);
  dev.mlsag_hash(data, hash2);
  ASSERT_EQ(hash1, hash2);
}

TEST(device, mlsag_hash_different_inputs)
{
  hw::core::device_default dev;
  rct::keyV data1, data2;
  data1.push_back(rct::skGen());
  data2.push_back(rct::skGen());

  rct::key hash1, hash2;
  dev.mlsag_hash(data1, hash1);
  dev.mlsag_hash(data2, hash2);
  ASSERT_NE(hash1, hash2);
}

TEST(device, mlsag_sign_basic)
{
  hw::core::device_default dev;
  const size_t rows = 2;
  const size_t dsRows = 1;

  rct::key c = rct::skGen();
  rct::keyV xx(rows), alpha(rows), ss(rows);
  for (size_t i = 0; i < rows; ++i)
  {
    xx[i] = rct::skGen();
    alpha[i] = rct::skGen();
    ss[i] = rct::zero();
  }

  ASSERT_TRUE(dev.mlsag_sign(c, xx, alpha, rows, dsRows, ss));

  // Verify: ss[j] = alpha[j] - c * xx[j]
  for (size_t j = 0; j < rows; ++j)
  {
    rct::key expected;
    sc_mulsub(expected.bytes, c.bytes, xx[j].bytes, alpha[j].bytes);
    ASSERT_EQ(ss[j], expected);
  }
}

TEST(device, clsag_prepare_basic)
{
  hw::core::device_default dev;
  rct::key p = rct::skGen();
  rct::key z = rct::skGen();
  rct::key tmp2 = rct::skGen();
  ge_p3 H_p3b;
  rct::hash_to_p3(H_p3b, tmp2);
  rct::key H;
  ge_p3_tobytes(H.bytes, &H_p3b);
  rct::key I, D, a, aG, aH;

  ASSERT_TRUE(dev.clsag_prepare(p, z, I, D, H, a, aG, aH));

  // Verify aG = a*G
  rct::key expected_aG;
  rct::scalarmultBase(expected_aG, a);
  ASSERT_EQ(aG, expected_aG);

  // Verify aH = a*H
  rct::key expected_aH;
  rct::scalarmultKey(expected_aH, H, a);
  ASSERT_EQ(aH, expected_aH);

  // Verify I = p*H
  rct::key expected_I;
  rct::scalarmultKey(expected_I, H, p);
  ASSERT_EQ(I, expected_I);

  // Verify D = z*H
  rct::key expected_D;
  rct::scalarmultKey(expected_D, H, z);
  ASSERT_EQ(D, expected_D);
}

TEST(device, clsag_hash_deterministic)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());

  rct::key hash1, hash2;
  dev.clsag_hash(data, hash1);
  dev.clsag_hash(data, hash2);
  ASSERT_EQ(hash1, hash2);
}

TEST(device, clsag_hash_different_inputs)
{
  hw::core::device_default dev;
  rct::keyV data1, data2;
  data1.push_back(rct::skGen());
  data2.push_back(rct::skGen());

  rct::key hash1, hash2;
  dev.clsag_hash(data1, hash1);
  dev.clsag_hash(data2, hash2);
  ASSERT_NE(hash1, hash2);
}

TEST(device, clsag_sign_basic)
{
  hw::core::device_default dev;
  rct::key c = rct::skGen();
  rct::key a = rct::skGen();
  rct::key p = rct::skGen();
  rct::key z = rct::skGen();
  rct::key mu_P = rct::skGen();
  rct::key mu_C = rct::skGen();
  rct::key s;

  ASSERT_TRUE(dev.clsag_sign(c, a, p, z, mu_P, mu_C, s));

  // Verify: s = a - c * (mu_P * p + mu_C * z)
  rct::key expected_s;
  rct::key s0_p_mu_P;
  sc_mul(s0_p_mu_P.bytes, mu_P.bytes, p.bytes);
  rct::key s0_add_z_mu_C;
  sc_muladd(s0_add_z_mu_C.bytes, mu_C.bytes, z.bytes, s0_p_mu_P.bytes);
  sc_mulsub(expected_s.bytes, c.bytes, s0_add_z_mu_C.bytes, a.bytes);
  ASSERT_EQ(s, expected_s);
}

// =========================================================================
// 6. Account operations tests
// =========================================================================

TEST(device, open_tx_encrypt_payment_id_close_tx)
{
  hw::core::device_default dev;
  crypto::secret_key tx_key;
  ASSERT_TRUE(dev.open_tx(tx_key));

  // Get the corresponding public key
  crypto::public_key tx_pub;
  ASSERT_TRUE(dev.secret_key_to_public_key(tx_key, tx_pub));

  crypto::secret_key view_sk;
  crypto::public_key view_pk;
  dev.generate_keys(view_pk, view_sk);

  crypto::hash8 pid;
  memset(&pid, 0x42, sizeof(pid));
  crypto::hash8 original = pid;

  ASSERT_TRUE(dev.encrypt_payment_id(pid, view_pk, view_sk));
  ASSERT_NE(memcmp(&pid, &original, sizeof(pid)), 0);

  // Decrypt
  ASSERT_TRUE(dev.encrypt_payment_id(pid, view_pk, view_sk));
  ASSERT_EQ(memcmp(&pid, &original, sizeof(pid)), 0);

  ASSERT_TRUE(dev.close_tx());
}

TEST(device, encrypt_payment_id_different_keys)
{
  hw::core::device_default dev;
  crypto::public_key pk1, pk2;
  crypto::secret_key sk1, sk2;
  dev.generate_keys(pk1, sk1);
  dev.generate_keys(pk2, sk2);

  crypto::hash8 pid1, pid2;
  memset(&pid1, 0xAA, sizeof(pid1));
  pid2 = pid1;

  dev.encrypt_payment_id(pid1, pk1, sk1);
  dev.encrypt_payment_id(pid2, pk2, sk2);
  // Different keys should produce different encryptions
  ASSERT_NE(memcmp(&pid1, &pid2, sizeof(pid1)), 0);
}

TEST(device, commitment_mask_deterministic)
{
  hw::core::device_default dev;
  rct::key amount_key = rct::skGen();
  rct::key mask1 = dev.genCommitmentMask(amount_key);
  rct::key mask2 = dev.genCommitmentMask(amount_key);
  ASSERT_EQ(mask1, mask2);
}

TEST(device, commitment_mask_different_keys)
{
  hw::core::device_default dev;
  rct::key key1 = rct::skGen();
  rct::key key2 = rct::skGen();
  rct::key mask1 = dev.genCommitmentMask(key1);
  rct::key mask2 = dev.genCommitmentMask(key2);
  ASSERT_NE(mask1, mask2);
}

TEST(device, verify_keys_generated_pair)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);
  ASSERT_TRUE(dev.verify_keys(sk, pk));
}

TEST(device, verify_keys_wrong_pair)
{
  hw::core::device_default dev;
  crypto::public_key pk1, pk2;
  crypto::secret_key sk1, sk2;
  dev.generate_keys(pk1, sk1);
  dev.generate_keys(pk2, sk2);
  // sk1 does not match pk2
  ASSERT_FALSE(dev.verify_keys(sk1, pk2));
}

TEST(device, generate_output_ephemeral_keys_basic)
{
  hw::core::device_default dev;

  // Create sender account
  cryptonote::account_base sender;
  crypto::secret_key sender_rk;
  sender.generate(sender_rk, true, false);

  // Create receiver account
  cryptonote::account_base receiver;
  crypto::secret_key receiver_rk;
  receiver.generate(receiver_rk, true, false);

  // Generate tx key
  crypto::secret_key tx_key;
  dev.open_tx(tx_key);
  crypto::public_key txkey_pub;
  dev.secret_key_to_public_key(tx_key, txkey_pub);

  // Setup destination
  cryptonote::tx_destination_entry dst;
  dst.amount = 1000000;
  dst.addr = receiver.get_keys().m_account_address;
  dst.is_subaddress = false;

  boost::optional<cryptonote::account_public_address> change_addr;
  std::vector<crypto::secret_key> additional_tx_keys;
  std::vector<crypto::public_key> additional_tx_public_keys;
  std::vector<rct::key> amount_keys;
  crypto::public_key out_eph_public_key;
  crypto::view_tag vt;

  ASSERT_TRUE(dev.generate_output_ephemeral_keys(
    2, sender.get_keys(), txkey_pub, tx_key,
    dst, change_addr, 0,
    false, additional_tx_keys,
    additional_tx_public_keys, amount_keys,
    out_eph_public_key, true, vt));

  // Output ephemeral key should be a valid point (non-zero)
  ASSERT_NE(out_eph_public_key, crypto::public_key{});
  // For tx version > 1, should have an amount key
  ASSERT_EQ(amount_keys.size(), 1u);

  dev.close_tx();
}

TEST(device, generate_output_ephemeral_keys_change_address)
{
  hw::core::device_default dev;

  // Create sender account (also acts as change addr)
  cryptonote::account_base sender;
  crypto::secret_key sender_rk;
  sender.generate(sender_rk, true, false);

  crypto::secret_key tx_key;
  dev.open_tx(tx_key);
  crypto::public_key txkey_pub;
  dev.secret_key_to_public_key(tx_key, txkey_pub);

  // Sending change to self
  cryptonote::tx_destination_entry dst;
  dst.amount = 500000;
  dst.addr = sender.get_keys().m_account_address;
  dst.is_subaddress = false;

  boost::optional<cryptonote::account_public_address> change_addr = sender.get_keys().m_account_address;
  std::vector<crypto::secret_key> additional_tx_keys;
  std::vector<crypto::public_key> additional_tx_public_keys;
  std::vector<rct::key> amount_keys;
  crypto::public_key out_eph_public_key;
  crypto::view_tag vt;

  ASSERT_TRUE(dev.generate_output_ephemeral_keys(
    2, sender.get_keys(), txkey_pub, tx_key,
    dst, change_addr, 0,
    false, additional_tx_keys,
    additional_tx_public_keys, amount_keys,
    out_eph_public_key, false, vt));

  ASSERT_NE(out_eph_public_key, crypto::public_key{});
  ASSERT_EQ(amount_keys.size(), 1u);

  dev.close_tx();
}

TEST(device, view_tag_different_output_indices)
{
  hw::core::device_default dev;
  crypto::public_key pk;
  crypto::secret_key sk;
  dev.generate_keys(pk, sk);

  crypto::key_derivation der;
  dev.generate_key_derivation(pk, sk, der);

  // Collect view tags for indices 0..9
  std::vector<crypto::view_tag> tags(10);
  for (size_t i = 0; i < 10; ++i)
    dev.derive_view_tag(der, i, tags[i]);

  // Not all should be the same (probability of 10 collisions is astronomically low)
  bool all_same = true;
  for (size_t i = 1; i < 10; ++i)
  {
    if (tags[i] != tags[0])
    {
      all_same = false;
      break;
    }
  }
  ASSERT_FALSE(all_same);
}

TEST(device, get_subaddress_major_indices)
{
  hw::core::device_default dev;
  cryptonote::account_base account;
  crypto::secret_key recovery_key;
  account.generate(recovery_key, true, false);

  const auto &keys = account.get_keys();
  cryptonote::subaddress_index idx_0_1{0, 1};
  cryptonote::subaddress_index idx_1_0{1, 0};
  cryptonote::subaddress_index idx_1_1{1, 1};

  auto addr_0_1 = dev.get_subaddress(keys, idx_0_1);
  auto addr_1_0 = dev.get_subaddress(keys, idx_1_0);
  auto addr_1_1 = dev.get_subaddress(keys, idx_1_1);

  // All subaddresses should be different
  ASSERT_NE(addr_0_1.m_spend_public_key, addr_1_0.m_spend_public_key);
  ASSERT_NE(addr_0_1.m_spend_public_key, addr_1_1.m_spend_public_key);
  ASSERT_NE(addr_1_0.m_spend_public_key, addr_1_1.m_spend_public_key);
}

TEST(device, mlsag_hash_consistency_with_hash_to_scalar)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());

  rct::key hash_dev;
  dev.mlsag_hash(data, hash_dev);

  rct::key hash_direct = rct::hash_to_scalar(data);
  ASSERT_EQ(hash_dev, hash_direct);
}

TEST(device, clsag_hash_consistency_with_hash_to_scalar)
{
  hw::core::device_default dev;
  rct::keyV data;
  data.push_back(rct::skGen());
  data.push_back(rct::skGen());

  rct::key hash_dev;
  dev.clsag_hash(data, hash_dev);

  rct::key hash_direct = rct::hash_to_scalar(data);
  ASSERT_EQ(hash_dev, hash_direct);
}

