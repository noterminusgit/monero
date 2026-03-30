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

// Phase C: wallet2 sync-related tests -- key derivation, output scanning,
// key images, subaddresses, block parsing, tx prefix hash, view-key scanning.

#include "gtest/gtest.h"
#include "wallet/wallet2.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/account.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "device/device_default.hpp"

// key_derivation lacks comparison operators needed by gtest
namespace crypto {
  inline bool operator==(const key_derivation &a, const key_derivation &b)
  {
    return !memcmp(&a, &b, sizeof(a));
  }
  inline bool operator!=(const key_derivation &a, const key_derivation &b)
  {
    return !(a == b);
  }
}

namespace
{
  // Helper: generate a fresh account and return its keys
  cryptonote::account_base make_account()
  {
    cryptonote::account_base acct;
    acct.generate();
    return acct;
  }

  // Helper: generate a random keypair
  void random_keypair(crypto::public_key &pub, crypto::secret_key &sec)
  {
    crypto::generate_keys(pub, sec);
  }
}

// ==========================================================================
// 1. Key derivation chain (using crypto functions directly)
// ==========================================================================

TEST(wallet2_sync, generate_key_derivation_succeeds)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  random_keypair(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));
}

TEST(wallet2_sync, generate_key_derivation_deterministic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  random_keypair(pub, sec);

  crypto::key_derivation d1, d2;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, d1));
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, d2));
  ASSERT_EQ(d1, d2);
}

TEST(wallet2_sync, generate_key_derivation_different_keys_differ)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  random_keypair(pub1, sec1);
  random_keypair(pub2, sec2);

  crypto::key_derivation d1, d2;
  ASSERT_TRUE(crypto::generate_key_derivation(pub1, sec1, d1));
  ASSERT_TRUE(crypto::generate_key_derivation(pub2, sec2, d2));
  ASSERT_NE(d1, d2);
}

TEST(wallet2_sync, derive_public_key_succeeds)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  // Simulate: tx sender generates R = r*G, recipient computes derivation = a*R
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  crypto::public_key derived_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, derived_pub));
}

TEST(wallet2_sync, derive_secret_key_from_derivation)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  crypto::secret_key derived_sec;
  crypto::derive_secret_key(derivation, 0, keys.m_spend_secret_key, derived_sec);

  // The derived secret key should not be all zeros
  ASSERT_NE(derived_sec, crypto::secret_key{});
}

TEST(wallet2_sync, derive_pub_from_sec_matches_derive_public_key)
{
  // Core consistency check: derive secret key, compute its public key,
  // and verify it matches derive_public_key result.
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  // Derive public key from derivation + spend public key
  crypto::public_key derived_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, derived_pub));

  // Derive secret key from derivation + spend secret key
  crypto::secret_key derived_sec;
  crypto::derive_secret_key(derivation, 0, keys.m_spend_secret_key, derived_sec);

  // Compute public key from derived secret key
  crypto::public_key pub_from_sec;
  ASSERT_TRUE(crypto::secret_key_to_public_key(derived_sec, pub_from_sec));

  ASSERT_EQ(derived_pub, pub_from_sec);
}

TEST(wallet2_sync, different_output_indices_produce_different_keys)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  crypto::public_key pk0, pk1, pk2;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, pk0));
  ASSERT_TRUE(crypto::derive_public_key(derivation, 1, keys.m_account_address.m_spend_public_key, pk1));
  ASSERT_TRUE(crypto::derive_public_key(derivation, 2, keys.m_account_address.m_spend_public_key, pk2));

  ASSERT_NE(pk0, pk1);
  ASSERT_NE(pk1, pk2);
  ASSERT_NE(pk0, pk2);
}

TEST(wallet2_sync, different_output_indices_produce_different_secret_keys)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  crypto::secret_key sk0, sk1;
  crypto::derive_secret_key(derivation, 0, keys.m_spend_secret_key, sk0);
  crypto::derive_secret_key(derivation, 1, keys.m_spend_secret_key, sk1);
  ASSERT_NE(sk0, sk1);
}

TEST(wallet2_sync, derivation_with_real_tx_key)
{
  // Simulate the full sender flow: generate tx keypair, use recipient's view key
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  // Sender generates tx key r, R = r*G
  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  // Sender computes derivation = r * A (recipient view public key)
  crypto::key_derivation sender_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, sender_derivation));

  // Recipient computes derivation = a * R
  crypto::key_derivation recipient_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recipient_derivation));

  // Both derivations should be identical (ECDH)
  ASSERT_EQ(sender_derivation, recipient_derivation);
}

// ==========================================================================
// 2. Output scanning helpers
// ==========================================================================

TEST(wallet2_sync, generate_key_image_helper_succeeds)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  // Generate a tx keypair
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  // Compute derivation as sender would: r * A
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(keys.m_account_address.m_view_public_key, tx_sec, derivation));

  // Compute the output public key
  crypto::public_key output_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pub));

  // Build subaddress map (main address only)
  std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
  subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

  std::vector<crypto::public_key> additional_tx_pub_keys;
  cryptonote::keypair in_ephemeral;
  crypto::key_image ki;
  ASSERT_TRUE(cryptonote::generate_key_image_helper(keys, subaddresses, output_pub,
    tx_pub, additional_tx_pub_keys, 0, in_ephemeral, ki, keys.get_device()));
}

TEST(wallet2_sync, key_image_uniqueness_for_different_outputs)
{
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(keys.m_account_address.m_view_public_key, tx_sec, derivation));

  std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
  subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};
  std::vector<crypto::public_key> additional_tx_pub_keys;

  // Output index 0
  crypto::public_key out_pub0;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, out_pub0));
  cryptonote::keypair eph0;
  crypto::key_image ki0;
  ASSERT_TRUE(cryptonote::generate_key_image_helper(keys, subaddresses, out_pub0,
    tx_pub, additional_tx_pub_keys, 0, eph0, ki0, keys.get_device()));

  // Output index 1
  crypto::public_key out_pub1;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 1, keys.m_account_address.m_spend_public_key, out_pub1));
  cryptonote::keypair eph1;
  crypto::key_image ki1;
  ASSERT_TRUE(cryptonote::generate_key_image_helper(keys, subaddresses, out_pub1,
    tx_pub, additional_tx_pub_keys, 1, eph1, ki1, keys.get_device()));

  ASSERT_NE(ki0, ki1);
}

// ==========================================================================
// 3. Key image operations
// ==========================================================================

TEST(wallet2_sync, key_image_is_32_bytes)
{
  ASSERT_EQ(sizeof(crypto::key_image), 32u);
}

TEST(wallet2_sync, different_keys_produce_different_key_images)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  random_keypair(pub1, sec1);
  random_keypair(pub2, sec2);

  crypto::key_image ki1, ki2;
  crypto::generate_key_image(pub1, sec1, ki1);
  crypto::generate_key_image(pub2, sec2, ki2);
  ASSERT_NE(ki1, ki2);
}

TEST(wallet2_sync, key_image_is_deterministic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  random_keypair(pub, sec);

  crypto::key_image ki1, ki2;
  crypto::generate_key_image(pub, sec, ki1);
  crypto::generate_key_image(pub, sec, ki2);
  ASSERT_EQ(ki1, ki2);
}

TEST(wallet2_sync, key_image_not_identity)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  random_keypair(pub, sec);

  crypto::key_image ki;
  crypto::generate_key_image(pub, sec, ki);

  // Key image should not be all zeros
  crypto::key_image zero_ki = {};
  ASSERT_NE(memcmp(&ki, &zero_ki, sizeof(ki)), 0);
}

TEST(wallet2_sync, key_image_from_derived_keys)
{
  // Full flow: derive output keys, then generate key image from them
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(keys.m_account_address.m_view_public_key, tx_sec, derivation));

  crypto::public_key derived_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, derived_pub));

  crypto::secret_key derived_sec;
  crypto::derive_secret_key(derivation, 0, keys.m_spend_secret_key, derived_sec);

  crypto::key_image ki;
  crypto::generate_key_image(derived_pub, derived_sec, ki);

  crypto::key_image zero_ki = {};
  ASSERT_NE(memcmp(&ki, &zero_ki, sizeof(ki)), 0);
}

// ==========================================================================
// 4. Subaddress key derivation
// ==========================================================================

TEST(wallet2_sync, subaddress_derivation_index_0_0_is_main)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx{0, 0};
  crypto::public_key spend_pk = dev.get_subaddress_spend_public_key(keys, idx);
  ASSERT_EQ(spend_pk, keys.m_account_address.m_spend_public_key);
}

TEST(wallet2_sync, subaddress_derivation_nonzero_differs_from_main)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx{0, 1};
  crypto::public_key spend_pk = dev.get_subaddress_spend_public_key(keys, idx);
  ASSERT_NE(spend_pk, keys.m_account_address.m_spend_public_key);
}

TEST(wallet2_sync, subaddress_different_minor_indices_differ)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{0, 2};
  crypto::public_key pk1 = dev.get_subaddress_spend_public_key(keys, idx1);
  crypto::public_key pk2 = dev.get_subaddress_spend_public_key(keys, idx2);
  ASSERT_NE(pk1, pk2);
}

TEST(wallet2_sync, subaddress_different_major_indices_differ)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx1{0, 1};
  cryptonote::subaddress_index idx2{1, 1};
  crypto::public_key pk1 = dev.get_subaddress_spend_public_key(keys, idx1);
  crypto::public_key pk2 = dev.get_subaddress_spend_public_key(keys, idx2);
  ASSERT_NE(pk1, pk2);
}

TEST(wallet2_sync, subaddress_derivation_is_deterministic)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx{1, 5};
  crypto::public_key pk1 = dev.get_subaddress_spend_public_key(keys, idx);
  crypto::public_key pk2 = dev.get_subaddress_spend_public_key(keys, idx);
  ASSERT_EQ(pk1, pk2);
}

TEST(wallet2_sync, subaddress_full_address_nonzero_differs)
{
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx0{0, 0};
  cryptonote::subaddress_index idx1{0, 1};
  auto addr0 = dev.get_subaddress(keys, idx0);
  auto addr1 = dev.get_subaddress(keys, idx1);

  // Main address matches
  ASSERT_EQ(addr0.m_spend_public_key, keys.m_account_address.m_spend_public_key);
  ASSERT_EQ(addr0.m_view_public_key, keys.m_account_address.m_view_public_key);
  // Subaddress differs in both spend and view keys
  ASSERT_NE(addr1.m_spend_public_key, addr0.m_spend_public_key);
  ASSERT_NE(addr1.m_view_public_key, addr0.m_view_public_key);
}

TEST(wallet2_sync, subaddress_key_derivation_with_tx_key)
{
  // Verify key derivation works with a subaddress spend public key
  hw::core::device_default dev;
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  cryptonote::subaddress_index idx{0, 3};
  crypto::public_key sub_spend_pk = dev.get_subaddress_spend_public_key(keys, idx);

  // Generate a tx key
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  // Derivation using view secret key works regardless of subaddress
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  // Can derive public key from subaddress spend key
  crypto::public_key derived_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, sub_spend_pk, derived_pub));
  ASSERT_NE(derived_pub, sub_spend_pk);
}

TEST(wallet2_sync, derive_subaddress_public_key_inverse)
{
  // derive_subaddress_public_key undoes derive_public_key
  auto acct = make_account();
  const auto &keys = acct.get_keys();

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  random_keypair(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, keys.m_view_secret_key, derivation));

  crypto::public_key output_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, keys.m_account_address.m_spend_public_key, output_pub));

  // derive_subaddress_public_key should recover the spend public key
  crypto::public_key recovered;
  ASSERT_TRUE(crypto::derive_subaddress_public_key(output_pub, derivation, 0, recovered));
  ASSERT_EQ(recovered, keys.m_account_address.m_spend_public_key);
}

// ==========================================================================
// 5. Block parsing helpers
// ==========================================================================

TEST(wallet2_sync, parse_block_from_empty_blob_fails)
{
  cryptonote::block b;
  cryptonote::blobdata empty_blob;
  ASSERT_FALSE(cryptonote::parse_and_validate_block_from_blob(empty_blob, b));
}

TEST(wallet2_sync, parse_block_from_garbage_blob_fails)
{
  cryptonote::block b;
  cryptonote::blobdata garbage(64, '\xff');
  ASSERT_FALSE(cryptonote::parse_and_validate_block_from_blob(garbage, b));
}

TEST(wallet2_sync, parse_block_from_valid_blob_succeeds)
{
  // Build a minimal valid block, serialize, then re-parse
  cryptonote::block b;
  b.major_version = 1;
  b.minor_version = 0;
  b.timestamp = 1234567890;
  b.prev_id = crypto::null_hash;
  b.nonce = 42;
  b.miner_tx.version = 1;
  b.miner_tx.unlock_time = 0;

  cryptonote::blobdata blob = cryptonote::block_to_blob(b);
  ASSERT_FALSE(blob.empty());

  cryptonote::block b2;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2));
  ASSERT_EQ(b2.major_version, 1);
  ASSERT_EQ(b2.nonce, 42u);
}

TEST(wallet2_sync, parse_block_preserves_hash)
{
  cryptonote::block b;
  b.major_version = 1;
  b.minor_version = 0;
  b.timestamp = 1000;
  b.prev_id = crypto::null_hash;
  b.nonce = 100;
  b.miner_tx.version = 1;
  b.miner_tx.unlock_time = 0;

  crypto::hash original_hash = cryptonote::get_block_hash(b);

  cryptonote::blobdata blob = cryptonote::block_to_blob(b);
  cryptonote::block b2;
  crypto::hash parsed_hash;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(blob, b2, parsed_hash));
  ASSERT_EQ(original_hash, parsed_hash);
}

TEST(wallet2_sync, block_height_from_miner_tx)
{
  // construct_miner_tx encodes height in the coinbase tx
  cryptonote::account_base acct;
  acct.generate();
  const auto &addr = acct.get_keys().m_account_address;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(42, 300000, 0, 128, 0, addr, miner_tx));

  cryptonote::block b;
  b.major_version = 1;
  b.minor_version = 0;
  b.timestamp = 1000;
  b.prev_id = crypto::null_hash;
  b.nonce = 0;
  b.miner_tx = miner_tx;

  ASSERT_EQ(cryptonote::get_block_height(b), 42u);
}

TEST(wallet2_sync, block_height_different_values)
{
  cryptonote::account_base acct;
  acct.generate();
  const auto &addr = acct.get_keys().m_account_address;

  for (uint64_t h : {0ULL, 1ULL, 100ULL, 999999ULL})
  {
    cryptonote::transaction miner_tx;
    ASSERT_TRUE(cryptonote::construct_miner_tx(h, 300000, 0, 128, 0, addr, miner_tx));
    cryptonote::block b;
    b.miner_tx = miner_tx;
    ASSERT_EQ(cryptonote::get_block_height(b), h);
  }
}

// ==========================================================================
// 6. Transaction prefix hash
// ==========================================================================

TEST(wallet2_sync, tx_prefix_hash_consistency)
{
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 0;

  crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx);
  crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
  ASSERT_EQ(h1, h2);
}

TEST(wallet2_sync, tx_prefix_hash_not_null)
{
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 0;

  crypto::hash h = cryptonote::get_transaction_prefix_hash(tx);
  ASSERT_NE(h, crypto::null_hash);
}

TEST(wallet2_sync, different_txs_have_different_prefix_hashes)
{
  cryptonote::transaction tx1, tx2;
  tx1.version = 2;
  tx1.unlock_time = 0;

  tx2.version = 2;
  tx2.unlock_time = 100;

  crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx1);
  crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx2);
  ASSERT_NE(h1, h2);
}

TEST(wallet2_sync, different_version_txs_differ)
{
  cryptonote::transaction tx1, tx2;
  tx1.version = 1;
  tx1.unlock_time = 0;

  tx2.version = 2;
  tx2.unlock_time = 0;

  crypto::hash h1 = cryptonote::get_transaction_prefix_hash(tx1);
  crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx2);
  ASSERT_NE(h1, h2);
}

TEST(wallet2_sync, tx_prefix_hash_via_out_param)
{
  cryptonote::transaction tx;
  tx.version = 2;
  tx.unlock_time = 0;

  crypto::hash h1;
  cryptonote::get_transaction_prefix_hash(tx, h1);
  crypto::hash h2 = cryptonote::get_transaction_prefix_hash(tx);
  ASSERT_EQ(h1, h2);
}

TEST(wallet2_sync, miner_tx_prefix_hash_valid)
{
  cryptonote::account_base acct;
  acct.generate();
  const auto &addr = acct.get_keys().m_account_address;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(10, 300000, 0, 128, 0, addr, miner_tx));

  crypto::hash h = cryptonote::get_transaction_prefix_hash(miner_tx);
  ASSERT_NE(h, crypto::null_hash);
}

TEST(wallet2_sync, two_miner_txs_different_heights_differ)
{
  cryptonote::account_base acct;
  acct.generate();
  const auto &addr = acct.get_keys().m_account_address;

  cryptonote::transaction mtx1, mtx2;
  ASSERT_TRUE(cryptonote::construct_miner_tx(10, 300000, 0, 128, 0, addr, mtx1));
  ASSERT_TRUE(cryptonote::construct_miner_tx(11, 300000, 0, 128, 0, addr, mtx2));

  crypto::hash h1 = cryptonote::get_transaction_prefix_hash(mtx1);
  crypto::hash h2 = cryptonote::get_transaction_prefix_hash(mtx2);
  ASSERT_NE(h1, h2);
}

// ==========================================================================
// 7. View key scanning
// ==========================================================================

TEST(wallet2_sync, view_key_scanning_basic_ecdh)
{
  // The math: sender picks r, computes R=r*G, shared_secret = r*A
  // Recipient computes shared_secret = a*R. Both get the same value.
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  // Sender: shared = r * A (view pub)
  crypto::key_derivation sender_shared;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, sender_shared));

  // Recipient: shared = a * R
  crypto::key_derivation recv_shared;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recv_shared));

  ASSERT_EQ(sender_shared, recv_shared);
}

TEST(wallet2_sync, view_key_scanning_output_ownership)
{
  // Full flow: sender creates output, recipient uses view key to detect ownership
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  // Sender generates tx key
  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  // Sender computes output key for recipient at index 0
  crypto::key_derivation sender_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, sender_derivation));

  crypto::public_key output_key;
  ASSERT_TRUE(crypto::derive_public_key(sender_derivation, 0,
    rkeys.m_account_address.m_spend_public_key, output_key));

  // Recipient scans: compute derivation from R and view secret key
  crypto::key_derivation recv_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recv_derivation));

  // Recipient derives expected output key at index 0
  crypto::public_key expected_output_key;
  ASSERT_TRUE(crypto::derive_public_key(recv_derivation, 0,
    rkeys.m_account_address.m_spend_public_key, expected_output_key));

  // Output belongs to us: keys match
  ASSERT_EQ(output_key, expected_output_key);
}

TEST(wallet2_sync, view_key_scanning_output_not_ours)
{
  // Different recipient should not match
  auto recipient = make_account();
  auto other = make_account();
  const auto &rkeys = recipient.get_keys();
  const auto &okeys = other.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  // Output sent to recipient
  crypto::key_derivation sender_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, sender_derivation));

  crypto::public_key output_key;
  ASSERT_TRUE(crypto::derive_public_key(sender_derivation, 0,
    rkeys.m_account_address.m_spend_public_key, output_key));

  // Other party scans with their view key
  crypto::key_derivation other_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, okeys.m_view_secret_key, other_derivation));

  crypto::public_key other_expected;
  ASSERT_TRUE(crypto::derive_public_key(other_derivation, 0,
    okeys.m_account_address.m_spend_public_key, other_expected));

  // Should NOT match
  ASSERT_NE(output_key, other_expected);
}

TEST(wallet2_sync, view_key_scanning_multiple_outputs)
{
  // Sender creates multiple outputs to same recipient, each at different index
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, derivation));

  std::vector<crypto::public_key> output_keys(3);
  for (size_t i = 0; i < 3; ++i)
  {
    ASSERT_TRUE(crypto::derive_public_key(derivation, i,
      rkeys.m_account_address.m_spend_public_key, output_keys[i]));
  }

  // Recipient scans and finds all outputs
  crypto::key_derivation recv_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recv_derivation));

  for (size_t i = 0; i < 3; ++i)
  {
    crypto::public_key expected;
    ASSERT_TRUE(crypto::derive_public_key(recv_derivation, i,
      rkeys.m_account_address.m_spend_public_key, expected));
    ASSERT_EQ(output_keys[i], expected);
  }
}

TEST(wallet2_sync, view_key_scanning_spend_key_needed_for_key_image)
{
  // View key alone can detect outputs, but spend key is needed for key image
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, derivation));

  // Derive output keys
  crypto::public_key output_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0,
    rkeys.m_account_address.m_spend_public_key, output_pub));

  // Need spend secret key to derive the secret key for key image
  crypto::secret_key output_sec;
  crypto::derive_secret_key(derivation, 0, rkeys.m_spend_secret_key, output_sec);

  // Verify the secret key corresponds to the public key
  crypto::public_key check_pub;
  ASSERT_TRUE(crypto::secret_key_to_public_key(output_sec, check_pub));
  ASSERT_EQ(output_pub, check_pub);

  // Now we can generate key image
  crypto::key_image ki;
  crypto::generate_key_image(output_pub, output_sec, ki);

  crypto::key_image zero_ki = {};
  ASSERT_NE(memcmp(&ki, &zero_ki, sizeof(ki)), 0);
}

TEST(wallet2_sync, view_key_scanning_derive_subaddress_public_key_check)
{
  // When scanning, use derive_subaddress_public_key to recover the spend key
  // and look it up in the subaddress map
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  // Sender creates output to main address
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, derivation));

  crypto::public_key output_key;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0,
    rkeys.m_account_address.m_spend_public_key, output_key));

  // Recipient: recover spend public key from output key
  crypto::key_derivation recv_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recv_derivation));

  crypto::public_key recovered_spend_pk;
  ASSERT_TRUE(crypto::derive_subaddress_public_key(output_key, recv_derivation, 0, recovered_spend_pk));

  // Should match our spend public key
  ASSERT_EQ(recovered_spend_pk, rkeys.m_account_address.m_spend_public_key);
}

TEST(wallet2_sync, view_key_scanning_wrong_index_fails_match)
{
  auto recipient = make_account();
  const auto &rkeys = recipient.get_keys();

  crypto::public_key R;
  crypto::secret_key r;
  random_keypair(R, r);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(rkeys.m_account_address.m_view_public_key, r, derivation));

  crypto::public_key output_key;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0,
    rkeys.m_account_address.m_spend_public_key, output_key));

  // Recipient tries to recover spend key at wrong index
  crypto::key_derivation recv_derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(R, rkeys.m_view_secret_key, recv_derivation));

  crypto::public_key recovered_wrong;
  ASSERT_TRUE(crypto::derive_subaddress_public_key(output_key, recv_derivation, 1, recovered_wrong));

  // Should NOT match our spend public key (wrong index)
  ASSERT_NE(recovered_wrong, rkeys.m_account_address.m_spend_public_key);
}
