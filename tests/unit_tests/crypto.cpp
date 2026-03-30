// Copyright (c) 2017-2024, The Monero Project
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

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>

extern "C"
{
#include "crypto/crypto-ops.h"
}
#include "crypto/generators.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/merge_mining.h"
#include "ringct/rctOps.h"
#include "ringct/rctTypes.h"

namespace
{
  static constexpr const std::uint8_t source[] = {
    0x8b, 0x65, 0x59, 0x70, 0x15, 0x37, 0x99, 0xaf, 0x2a, 0xea, 0xdc, 0x9f, 0xf1, 0xad, 0xd0, 0xea,
    0x6c, 0x72, 0x51, 0xd5, 0x41, 0x54, 0xcf, 0xa9, 0x2c, 0x17, 0x3a, 0x0d, 0xd3, 0x9c, 0x1f, 0x94,
    0x6c, 0x72, 0x51, 0xd5, 0x41, 0x54, 0xcf, 0xa9, 0x2c, 0x17, 0x3a, 0x0d, 0xd3, 0x9c, 0x1f, 0x94,
    0x8b, 0x65, 0x59, 0x70, 0x15, 0x37, 0x99, 0xaf, 0x2a, 0xea, 0xdc, 0x9f, 0xf1, 0xad, 0xd0, 0xea
  };

  static constexpr const char expected[] =
    "8b655970153799af2aeadc9ff1add0ea6c7251d54154cfa92c173a0dd39c1f94"
    "6c7251d54154cfa92c173a0dd39c1f948b655970153799af2aeadc9ff1add0ea";

  template<typename T> void *addressof(T &t) { return &t; }
  template<> void *addressof(crypto::secret_key &k) { return addressof(unwrap(unwrap(k))); }

  template<typename T>
  bool is_formatted()
  {
    T value{};

    static_assert(alignof(T) == 1, "T must have 1 byte alignment");
    static_assert(sizeof(T) <= sizeof(source), "T is too large for source");
    static_assert(sizeof(T) * 2 <= sizeof(expected), "T is too large for destination");
    std::memcpy(addressof(value), source, sizeof(T));

    std::stringstream out;
    out << "BEGIN" << value << "END";  
    return out.str() == "BEGIN<" + std::string{expected, sizeof(T) * 2} + ">END";
  }
}

TEST(Crypto, Ostream)
{
  EXPECT_TRUE(is_formatted<crypto::hash8>());
  EXPECT_TRUE(is_formatted<crypto::hash>());
  EXPECT_TRUE(is_formatted<crypto::public_key>());
  EXPECT_TRUE(is_formatted<crypto::signature>());
  EXPECT_TRUE(is_formatted<crypto::key_derivation>());
  EXPECT_TRUE(is_formatted<crypto::key_image>());
  EXPECT_TRUE(is_formatted<rct::key>());
}

TEST(Crypto, null_keys)
{
  char zero[32];
  memset(zero, 0, 32);
  ASSERT_EQ(memcmp(crypto::null_skey.data, zero, 32), 0);
  ASSERT_EQ(memcmp(crypto::null_pkey.data, zero, 32), 0);
}

TEST(Crypto, verify_32)
{
  // all bytes are treated the same, so we can brute force just one byte
  unsigned char k0[32] = {0}, k1[32] = {0};
  for (unsigned int i0 = 0; i0 < 256; ++i0)
  {
    k0[0] = i0;
    for (unsigned int i1 = 0; i1 < 256; ++i1)
    {
      k1[0] = i1;
      ASSERT_EQ(!crypto_verify_32(k0, k1), i0 == i1);
    }
  }
}

TEST(Crypto, tree_branch)
{
  crypto::hash inputs[6];
  crypto::hash branch[8];
  crypto::hash branch_1[8 + 1];
  crypto::hash root, root2;
  size_t depth;
  uint32_t path, path2;

  auto hasher = [](const crypto::hash &h0, const crypto::hash &h1) -> crypto::hash
  {
    char buffer[64];
    memcpy(buffer, &h0, 32);
    memcpy(buffer + 32, &h1, 32);
    crypto::hash res;
    cn_fast_hash(buffer, 64, res);
    return res;
  };

  for (int n = 0; n < 6; ++n)
  {
    memset(&inputs[n], 0, 32);
    inputs[n].data[0] = n + 1;
  }

  // empty
  ASSERT_FALSE(crypto::tree_branch((const char(*)[32])inputs, 0, crypto::null_hash.data, (char(*)[32])branch, &depth, &path));

  // one, matching
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 1, inputs[0].data, (char(*)[32])branch, &depth, &path));
  ASSERT_EQ(depth, 0);
  ASSERT_EQ(path, 0);
  ASSERT_TRUE(crypto::tree_path(1, 0, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 1, root.data);
  ASSERT_EQ(root, inputs[0]);
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // one, not found
  ASSERT_FALSE(crypto::tree_branch((const char(*)[32])inputs, 1, inputs[1].data, (char(*)[32])branch, &depth, &path));

  // two, index 0
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 2, inputs[0].data, (char(*)[32])branch, &depth, &path));
  ASSERT_EQ(depth, 1);
  ASSERT_EQ(path, 0);
  ASSERT_TRUE(crypto::tree_path(2, 0, &path2));
  ASSERT_EQ(path, path2);
  ASSERT_EQ(branch[0], inputs[1]);
  crypto::tree_hash((const char(*)[32])inputs, 2, root.data);
  ASSERT_EQ(root, hasher(inputs[0], inputs[1]));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // two, index 1
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 2, inputs[1].data, (char(*)[32])branch, &depth, &path));
  ASSERT_EQ(depth, 1);
  ASSERT_EQ(path, 1);
  ASSERT_TRUE(crypto::tree_path(2, 1, &path2));
  ASSERT_EQ(path, path2);
  ASSERT_EQ(branch[0], inputs[0]);
  crypto::tree_hash((const char(*)[32])inputs, 2, root.data);
  ASSERT_EQ(root, hasher(inputs[0], inputs[1]));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // two, not found
  ASSERT_FALSE(crypto::tree_branch((const char(*)[32])inputs, 2, inputs[2].data, (char(*)[32])branch, &depth, &path));

  // a b c 0
  //  x   y
  //    z

  // three, index 0
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 3, inputs[0].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 1);
  ASSERT_LE(depth, 2);
  ASSERT_TRUE(crypto::tree_path(3, 0, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 3, root.data);
  ASSERT_EQ(root, hasher(inputs[0], hasher(inputs[1], inputs[2])));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // three, index 1
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 3, inputs[1].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 1);
  ASSERT_LE(depth, 2);
  ASSERT_TRUE(crypto::tree_path(3, 1, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 3, root.data);
  ASSERT_EQ(root, hasher(inputs[0], hasher(inputs[1], inputs[2])));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // three, index 2
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 3, inputs[2].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 1);
  ASSERT_LE(depth, 2);
  ASSERT_TRUE(crypto::tree_path(3, 2, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 3, root.data);
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::tree_branch_hash(inputs[2].data, (const char(*)[32])branch, depth, path, root2.data));
  ASSERT_EQ(root, root2);

  // three, not found
  ASSERT_FALSE(crypto::tree_branch((const char(*)[32])inputs, 3, inputs[3].data, (char(*)[32])branch, &depth, &path));

  // a b c d e 0 0 0
  //    x   y
  //      z
  //    w

  // five, index 0
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 5, inputs[0].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 2);
  ASSERT_LE(depth, 3);
  ASSERT_TRUE(crypto::tree_path(5, 0, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 5, root.data);
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[5].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // five, index 1
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 5, inputs[1].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 2);
  ASSERT_LE(depth, 3);
  ASSERT_TRUE(crypto::tree_path(5, 1, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 5, root.data);
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[5].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // five, index 2
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 5, inputs[2].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 2);
  ASSERT_LE(depth, 3);
  ASSERT_TRUE(crypto::tree_path(5, 2, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 5, root.data);
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[5].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // five, index 4
  ASSERT_TRUE(crypto::tree_branch((const char(*)[32])inputs, 5, inputs[4].data, (char(*)[32])branch, &depth, &path));
  ASSERT_GE(depth, 2);
  ASSERT_LE(depth, 3);
  ASSERT_TRUE(crypto::tree_path(5, 4, &path2));
  ASSERT_EQ(path, path2);
  crypto::tree_hash((const char(*)[32])inputs, 5, root.data);
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[0].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[1].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[2].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[3].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_TRUE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[5].data, root.data, (const char(*)[32])branch, depth, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(crypto::null_hash.data, root.data, (const char(*)[32])branch, depth, path));

  // a version with an extra (dummy) hash
  memcpy(branch_1, branch, sizeof(branch));
  branch_1[depth] = crypto::null_hash;

  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth - 1, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch_1, depth + 1, path));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path ^ 1));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path ^ 2));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])branch, depth, path ^ 3));
  ASSERT_FALSE(crypto::is_branch_in_tree(inputs[4].data, root.data, (const char(*)[32])(branch_1 + 1), depth, path));

  // five, not found
  ASSERT_FALSE(crypto::tree_branch((const char(*)[32])inputs, 5, crypto::null_hash.data, (char(*)[32])branch, &depth, &path));

  // depth encoding roundtrip
  for (uint32_t n_chains = 1; n_chains <= 256; ++n_chains)
  {
    for (uint32_t nonce = 0xffffffff - 512; nonce != 1025; ++nonce)
    {
      const uint64_t depth = cryptonote::encode_mm_depth(n_chains, nonce);
      uint32_t n_chains_2, nonce_2;
      ASSERT_TRUE(cryptonote::decode_mm_depth(depth, n_chains_2, nonce_2));
      ASSERT_EQ(n_chains, n_chains_2);
      ASSERT_EQ(nonce, nonce_2);
    }
  }

  // 257 chains is too much
  try { cryptonote::encode_mm_depth(257, 0); ASSERT_TRUE(false); }
  catch (...) {}
}

TEST(Crypto, generator_consistency)
{
  // crypto/generators.h
  const crypto::public_key G{crypto::get_G()};
  const crypto::public_key H{crypto::get_H()};
  const ge_p3 H_p3 = crypto::get_H_p3();

  // crypto/crypto-ops.h
  ASSERT_TRUE(memcmp(&H_p3, &ge_p3_H, sizeof(ge_p3)) == 0);

  // ringct/rctOps.h
  ASSERT_TRUE(memcmp(G.data, rct::G.bytes, 32) == 0);

  // ringct/rctTypes.h
  ASSERT_TRUE(memcmp(H.data, rct::H.bytes, 32) == 0);
}

// ===== Additional crypto tests for key generation and operations =====

TEST(Crypto, generate_keys_produces_valid_pair)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  // Secret key should not be null
  ASSERT_NE(sec, crypto::null_skey);

  // Public key should not be null
  ASSERT_NE(pub, crypto::null_pkey);

  // Public key should be valid on the curve
  ASSERT_TRUE(crypto::check_key(pub));
}

TEST(Crypto, generate_keys_different_each_time)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  ASSERT_NE(pub1, pub2);
  ASSERT_NE(sec1, sec2);
}

TEST(Crypto, check_key_valid)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  ASSERT_TRUE(crypto::check_key(pub));
}

TEST(Crypto, check_key_null_is_identity)
{
  // The null public key (all zeros) represents the identity point
  // which is technically valid in Monero's ed25519 implementation
  ASSERT_TRUE(crypto::check_key(crypto::null_pkey));
}

TEST(Crypto, check_key_random_bytes_likely_invalid)
{
  // Random 32 bytes are very unlikely to be a valid curve point
  crypto::public_key random_key;
  memset(random_key.data, 0xFF, sizeof(random_key.data));

  // 0xFF...FF is not a valid ed25519 point
  ASSERT_FALSE(crypto::check_key(random_key));
}

TEST(Crypto, secret_key_to_public_key_consistency)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec;
  crypto::generate_keys(pub1, sec);

  // Recompute public key from secret key
  ASSERT_TRUE(crypto::secret_key_to_public_key(sec, pub2));
  ASSERT_EQ(pub1, pub2);
}

TEST(Crypto, secret_key_to_public_key_deterministic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  // Compute public key twice from the same secret key
  crypto::public_key pub_a, pub_b;
  ASSERT_TRUE(crypto::secret_key_to_public_key(sec, pub_a));
  ASSERT_TRUE(crypto::secret_key_to_public_key(sec, pub_b));
  ASSERT_EQ(pub_a, pub_b);
}

TEST(Crypto, different_secret_keys_different_public_keys)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  // Different secret keys should yield different public keys
  ASSERT_NE(sec1, sec2);
  ASSERT_NE(pub1, pub2);
}

TEST(Crypto, generate_key_derivation_roundtrip)
{
  // A derives shared secret using B's public key and A's private key
  // B derives shared secret using A's public key and B's private key
  // Both should match (Diffie-Hellman)
  crypto::public_key pub_a, pub_b;
  crypto::secret_key sec_a, sec_b;
  crypto::generate_keys(pub_a, sec_a);
  crypto::generate_keys(pub_b, sec_b);

  crypto::key_derivation d_ab, d_ba;
  ASSERT_TRUE(crypto::generate_key_derivation(pub_b, sec_a, d_ab));
  ASSERT_TRUE(crypto::generate_key_derivation(pub_a, sec_b, d_ba));

  ASSERT_EQ(0, memcmp(&d_ab, &d_ba, sizeof(d_ab)));
}

TEST(Crypto, generate_key_derivation_with_null_key)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  // Null public key (identity) is technically valid in Monero's
  // implementation, so derivation succeeds but produces a
  // degenerate result
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(crypto::null_pkey, sec, derivation));
}

TEST(Crypto, derive_public_key_basic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  // Self-derivation
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  crypto::public_key derived;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, pub, derived));
  ASSERT_NE(derived, crypto::null_pkey);
  ASSERT_TRUE(crypto::check_key(derived));
}

TEST(Crypto, derive_public_key_different_indices)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  crypto::public_key derived0, derived1, derived2;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, pub, derived0));
  ASSERT_TRUE(crypto::derive_public_key(derivation, 1, pub, derived1));
  ASSERT_TRUE(crypto::derive_public_key(derivation, 2, pub, derived2));

  // Different indices should produce different keys
  ASSERT_NE(derived0, derived1);
  ASSERT_NE(derived0, derived2);
  ASSERT_NE(derived1, derived2);

  // All should be valid
  ASSERT_TRUE(crypto::check_key(derived0));
  ASSERT_TRUE(crypto::check_key(derived1));
  ASSERT_TRUE(crypto::check_key(derived2));
}

TEST(Crypto, derive_secret_key_matches_public)
{
  // Generate sender and receiver
  crypto::public_key send_pub, recv_pub;
  crypto::secret_key send_sec, recv_sec;
  crypto::generate_keys(send_pub, send_sec);
  crypto::generate_keys(recv_pub, recv_sec);

  // Generate key derivation
  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(recv_pub, send_sec, derivation));

  // Derive public key (sender side)
  crypto::public_key eph_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, recv_pub, eph_pub));

  // Derive the same derivation from receiver side
  crypto::key_derivation derivation2;
  ASSERT_TRUE(crypto::generate_key_derivation(send_pub, recv_sec, derivation2));
  ASSERT_EQ(0, memcmp(&derivation, &derivation2, sizeof(derivation)));

  // Derive secret key (receiver side)
  crypto::secret_key eph_sec;
  crypto::derive_secret_key(derivation2, 0, recv_sec, eph_sec);

  // The derived secret key should produce the derived public key
  crypto::public_key eph_pub_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(eph_sec, eph_pub_check));
  ASSERT_EQ(eph_pub, eph_pub_check);
}

TEST(Crypto, derive_subaddress_public_key_reverses_derive)
{
  crypto::public_key base_pub;
  crypto::secret_key base_sec;
  crypto::generate_keys(base_pub, base_sec);

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, base_sec, derivation));

  // Derive a public key
  crypto::public_key derived;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 0, base_pub, derived));

  // Reverse it
  crypto::public_key recovered;
  ASSERT_TRUE(crypto::derive_subaddress_public_key(derived, derivation, 0, recovered));
  ASSERT_EQ(recovered, base_pub);
}

TEST(Crypto, generate_signature_verify)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::hash msg;
  crypto::cn_fast_hash("test message", 12, msg);

  crypto::signature sig;
  crypto::generate_signature(msg, pub, sec, sig);

  ASSERT_TRUE(crypto::check_signature(msg, pub, sig));
}

TEST(Crypto, signature_wrong_message_fails)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::hash msg1, msg2;
  crypto::cn_fast_hash("message1", 8, msg1);
  crypto::cn_fast_hash("message2", 8, msg2);

  crypto::signature sig;
  crypto::generate_signature(msg1, pub, sec, sig);

  ASSERT_TRUE(crypto::check_signature(msg1, pub, sig));
  ASSERT_FALSE(crypto::check_signature(msg2, pub, sig));
}

TEST(Crypto, signature_wrong_key_fails)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  crypto::hash msg;
  crypto::cn_fast_hash("test", 4, msg);

  crypto::signature sig;
  crypto::generate_signature(msg, pub1, sec1, sig);

  ASSERT_TRUE(crypto::check_signature(msg, pub1, sig));
  ASSERT_FALSE(crypto::check_signature(msg, pub2, sig));
}

TEST(Crypto, key_image_deterministic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_image ki1, ki2;
  crypto::generate_key_image(pub, sec, ki1);
  crypto::generate_key_image(pub, sec, ki2);

  ASSERT_EQ(ki1, ki2);
}

TEST(Crypto, key_image_different_keys_different_images)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  crypto::key_image ki1, ki2;
  crypto::generate_key_image(pub1, sec1, ki1);
  crypto::generate_key_image(pub2, sec2, ki2);

  ASSERT_NE(ki1, ki2);
}

TEST(Crypto, key_image_not_null)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_image ki;
  crypto::generate_key_image(pub, sec, ki);

  crypto::key_image null_ki;
  memset(&null_ki, 0, sizeof(null_ki));
  ASSERT_NE(ki, null_ki);
}

TEST(Crypto, derivation_to_scalar)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  // Derive scalars at different indices
  crypto::ec_scalar scalar0, scalar1;
  crypto::derivation_to_scalar(derivation, 0, scalar0);
  crypto::derivation_to_scalar(derivation, 1, scalar1);

  // Different indices should yield different scalars
  ASSERT_NE(0, memcmp(&scalar0, &scalar1, sizeof(scalar0)));

  // Same index should yield same scalar
  crypto::ec_scalar scalar0_again;
  crypto::derivation_to_scalar(derivation, 0, scalar0_again);
  ASSERT_EQ(0, memcmp(&scalar0, &scalar0_again, sizeof(scalar0)));
}

TEST(Crypto, cn_fast_hash_deterministic)
{
  const char* data = "test data for hashing";
  crypto::hash h1, h2;
  crypto::cn_fast_hash(data, strlen(data), h1);
  crypto::cn_fast_hash(data, strlen(data), h2);
  ASSERT_EQ(h1, h2);
}

TEST(Crypto, cn_fast_hash_different_input_different_output)
{
  crypto::hash h1, h2;
  crypto::cn_fast_hash("data1", 5, h1);
  crypto::cn_fast_hash("data2", 5, h2);
  ASSERT_NE(h1, h2);
}

TEST(Crypto, cn_fast_hash_empty_input)
{
  crypto::hash h;
  crypto::cn_fast_hash("", 0, h);
  // Should not be null hash (empty input still produces a valid hash)
  ASSERT_NE(h, crypto::null_hash);
}

TEST(Crypto, cn_fast_hash_single_byte)
{
  crypto::hash h;
  const char byte = 0x42;
  crypto::cn_fast_hash(&byte, 1, h);
  ASSERT_NE(h, crypto::null_hash);
}

TEST(Crypto, hash_to_scalar_deterministic)
{
  const char* data = "hash to scalar test";
  crypto::ec_scalar s1, s2;
  crypto::hash_to_scalar(data, strlen(data), s1);
  crypto::hash_to_scalar(data, strlen(data), s2);
  ASSERT_EQ(0, memcmp(&s1, &s2, sizeof(s1)));
}

TEST(Crypto, hash_to_scalar_different_input)
{
  crypto::ec_scalar s1, s2;
  crypto::hash_to_scalar("input1", 6, s1);
  crypto::hash_to_scalar("input2", 6, s2);
  ASSERT_NE(0, memcmp(&s1, &s2, sizeof(s1)));
}

TEST(Crypto, rand_generates_bytes)
{
  uint8_t bytes[32];
  memset(bytes, 0, sizeof(bytes));
  crypto::rand(sizeof(bytes), bytes);

  // Very unlikely that all 32 bytes remain zero after random generation
  uint8_t zero[32] = {0};
  ASSERT_NE(0, memcmp(bytes, zero, sizeof(bytes)));
}

TEST(Crypto, rand_template_nonzero)
{
  // Generate random 32-byte value using template
  crypto::ec_scalar s = crypto::rand<crypto::ec_scalar>();
  crypto::ec_scalar zero;
  memset(&zero, 0, sizeof(zero));
  // Very unlikely to be all zeros
  ASSERT_NE(0, memcmp(&s, &zero, sizeof(s)));
}

TEST(Crypto, generate_keys_with_recovery)
{
  // Generate initial keys
  crypto::public_key pub1;
  crypto::secret_key sec1;
  crypto::secret_key recovery = crypto::generate_keys(pub1, sec1);

  // Recover keys from recovery key
  crypto::public_key pub2;
  crypto::secret_key sec2;
  crypto::generate_keys(pub2, sec2, recovery, true);

  // Should produce the same key pair
  ASSERT_EQ(pub1, pub2);
  ASSERT_EQ(sec1, sec2);
}

TEST(Crypto, generate_keys_recovery_deterministic)
{
  // Generate a recovery key
  crypto::public_key pub_tmp;
  crypto::secret_key sec_tmp;
  crypto::secret_key recovery = crypto::generate_keys(pub_tmp, sec_tmp);

  // Recover twice with the same recovery key
  crypto::public_key pub_a, pub_b;
  crypto::secret_key sec_a, sec_b;
  crypto::generate_keys(pub_a, sec_a, recovery, true);
  crypto::generate_keys(pub_b, sec_b, recovery, true);

  ASSERT_EQ(pub_a, pub_b);
  ASSERT_EQ(sec_a, sec_b);
}

TEST(Crypto, view_tag_derivation)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  // Different indices should generally produce different view tags
  // (though collisions are possible since view tags are only 1 byte)
  crypto::view_tag vt0, vt1;
  crypto::derive_view_tag(derivation, 0, vt0);
  crypto::derive_view_tag(derivation, 1, vt1);

  // Deterministic
  crypto::view_tag vt0_check;
  crypto::derive_view_tag(derivation, 0, vt0_check);
  ASSERT_EQ(vt0.data, vt0_check.data);
}

TEST(Crypto, ring_signature_single_key)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  // Generate key image
  crypto::key_image ki;
  crypto::generate_key_image(pub, sec, ki);

  // Generate a prefix hash
  crypto::hash prefix;
  crypto::cn_fast_hash("ring sig test", 13, prefix);

  // Create a ring signature with a single key (trivial ring)
  const crypto::public_key* pubs[] = {&pub};
  crypto::signature sig;
  crypto::generate_ring_signature(prefix, ki, pubs, 1, sec, 0, &sig);

  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 1, &sig));

  // Wrong prefix should fail
  crypto::hash wrong_prefix;
  crypto::cn_fast_hash("wrong message", 13, wrong_prefix);
  ASSERT_FALSE(crypto::check_ring_signature(wrong_prefix, ki, pubs, 1, &sig));
}

TEST(Crypto, ring_signature_two_keys)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  // Key image from the real signer (key at index 0)
  crypto::key_image ki;
  crypto::generate_key_image(pub1, sec1, ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring sig 2 keys", 15, prefix);

  const crypto::public_key* pubs[] = {&pub1, &pub2};
  crypto::signature sigs[2];

  // Sign with sec1 at index 0
  crypto::generate_ring_signature(prefix, ki, pubs, 2, sec1, 0, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 2, sigs));

  // Tampered signature should fail
  sigs[0].c.data[0] ^= 1;
  ASSERT_FALSE(crypto::check_ring_signature(prefix, ki, pubs, 2, sigs));
}

TEST(Crypto, ec_scalar_sizes)
{
  ASSERT_EQ(sizeof(crypto::ec_scalar), 32u);
  ASSERT_EQ(sizeof(crypto::ec_point), 32u);
  ASSERT_EQ(sizeof(crypto::public_key), 32u);
  ASSERT_EQ(sizeof(crypto::key_derivation), 32u);
  ASSERT_EQ(sizeof(crypto::key_image), 32u);
  ASSERT_EQ(sizeof(crypto::signature), 64u);
  ASSERT_EQ(sizeof(crypto::view_tag), 1u);
  ASSERT_EQ(sizeof(crypto::hash), 32u);
  ASSERT_EQ(sizeof(crypto::hash8), 8u);
}

// ===== cn_slow_hash variant tests =====

TEST(Crypto, cn_slow_hash_variant0_deterministic)
{
  const char data[] = "test input for cn_slow_hash";
  crypto::hash h1, h2;
  crypto::cn_slow_hash(data, sizeof(data) - 1, h1, 0);
  crypto::cn_slow_hash(data, sizeof(data) - 1, h2, 0);
  ASSERT_EQ(h1, h2);
  ASSERT_NE(h1, crypto::null_hash);
}

TEST(Crypto, cn_slow_hash_variant1_deterministic)
{
  // Variant 1 requires at least 43 bytes of input
  const char data[] = "This input is long enough for variant 1 testing!!";
  static_assert(sizeof(data) - 1 >= 43, "variant 1 needs >= 43 bytes");
  crypto::hash h1, h2;
  crypto::cn_slow_hash(data, sizeof(data) - 1, h1, 1);
  crypto::cn_slow_hash(data, sizeof(data) - 1, h2, 1);
  ASSERT_EQ(h1, h2);
  ASSERT_NE(h1, crypto::null_hash);
}

TEST(Crypto, cn_slow_hash_variant2_deterministic)
{
  // Variant 2 requires at least 43 bytes of input
  const char data[] = "This input is long enough for variant 2 testing!!";
  static_assert(sizeof(data) - 1 >= 43, "variant 2 needs >= 43 bytes");
  crypto::hash h1, h2;
  crypto::cn_slow_hash(data, sizeof(data) - 1, h1, 2);
  crypto::cn_slow_hash(data, sizeof(data) - 1, h2, 2);
  ASSERT_EQ(h1, h2);
  ASSERT_NE(h1, crypto::null_hash);
}

TEST(Crypto, cn_slow_hash_different_variants_differ)
{
  // Use a long enough input for all variants
  const char data[] = "This input is long enough for all variant testing!!!";
  static_assert(sizeof(data) - 1 >= 43, "needs >= 43 bytes");
  crypto::hash h0, h1, h2;
  crypto::cn_slow_hash(data, sizeof(data) - 1, h0, 0);
  crypto::cn_slow_hash(data, sizeof(data) - 1, h1, 1);
  crypto::cn_slow_hash(data, sizeof(data) - 1, h2, 2);
  ASSERT_NE(h0, h1);
  ASSERT_NE(h0, h2);
  ASSERT_NE(h1, h2);
}

TEST(Crypto, cn_slow_hash_variant0_empty_input)
{
  crypto::hash h;
  crypto::cn_slow_hash("", 0, h, 0);
  ASSERT_NE(h, crypto::null_hash);
}

TEST(Crypto, cn_slow_hash_variant0_single_byte)
{
  const char byte = 0x00;
  crypto::hash h;
  crypto::cn_slow_hash(&byte, 1, h, 0);
  ASSERT_NE(h, crypto::null_hash);
}

// ===== tree_hash tests =====

TEST(Crypto, tree_hash_single_leaf_identity)
{
  crypto::hash leaf;
  crypto::cn_fast_hash("leaf0", 5, leaf);

  crypto::hash root;
  crypto::tree_hash(&leaf, 1, root);

  // Single leaf: root == leaf
  ASSERT_EQ(root, leaf);
}

TEST(Crypto, tree_hash_two_leaves)
{
  crypto::hash leaves[2];
  crypto::cn_fast_hash("leaf_a", 6, leaves[0]);
  crypto::cn_fast_hash("leaf_b", 6, leaves[1]);

  crypto::hash root;
  crypto::tree_hash(leaves, 2, root);

  // Root should be the hash of the concatenation of the two leaves
  char buffer[64];
  memcpy(buffer, &leaves[0], 32);
  memcpy(buffer + 32, &leaves[1], 32);
  crypto::hash expected;
  crypto::cn_fast_hash(buffer, 64, expected);

  ASSERT_EQ(root, expected);
}

TEST(Crypto, tree_hash_three_leaves)
{
  crypto::hash leaves[3];
  crypto::cn_fast_hash("three_a", 7, leaves[0]);
  crypto::cn_fast_hash("three_b", 7, leaves[1]);
  crypto::cn_fast_hash("three_c", 7, leaves[2]);

  crypto::hash root;
  crypto::tree_hash(leaves, 3, root);

  ASSERT_NE(root, crypto::null_hash);
  ASSERT_NE(root, leaves[0]);
  ASSERT_NE(root, leaves[1]);
  ASSERT_NE(root, leaves[2]);
}

TEST(Crypto, tree_hash_five_leaves)
{
  crypto::hash leaves[5];
  for (int i = 0; i < 5; ++i)
  {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "five_%d", i);
    crypto::cn_fast_hash(buf, len, leaves[i]);
  }

  crypto::hash root;
  crypto::tree_hash(leaves, 5, root);

  ASSERT_NE(root, crypto::null_hash);

  // Deterministic
  crypto::hash root2;
  crypto::tree_hash(leaves, 5, root2);
  ASSERT_EQ(root, root2);
}

TEST(Crypto, tree_hash_eight_leaves)
{
  crypto::hash leaves[8];
  for (int i = 0; i < 8; ++i)
  {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "eight_%d", i);
    crypto::cn_fast_hash(buf, len, leaves[i]);
  }

  crypto::hash root;
  crypto::tree_hash(leaves, 8, root);

  ASSERT_NE(root, crypto::null_hash);

  // Different from a 5-leaf tree
  crypto::hash leaves5[5];
  for (int i = 0; i < 5; ++i)
    leaves5[i] = leaves[i];

  crypto::hash root5;
  crypto::tree_hash(leaves5, 5, root5);
  ASSERT_NE(root, root5);
}

TEST(Crypto, tree_hash_order_matters)
{
  crypto::hash leaves_a[3], leaves_b[3];
  crypto::cn_fast_hash("order_x", 7, leaves_a[0]);
  crypto::cn_fast_hash("order_y", 7, leaves_a[1]);
  crypto::cn_fast_hash("order_z", 7, leaves_a[2]);

  // Swap first two
  leaves_b[0] = leaves_a[1];
  leaves_b[1] = leaves_a[0];
  leaves_b[2] = leaves_a[2];

  crypto::hash root_a, root_b;
  crypto::tree_hash(leaves_a, 3, root_a);
  crypto::tree_hash(leaves_b, 3, root_b);

  ASSERT_NE(root_a, root_b);
}

// ===== Key derivation chain tests =====

TEST(Crypto, key_derivation_chain_full_roundtrip)
{
  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  crypto::public_key recv_pub;
  crypto::secret_key recv_sec;
  crypto::generate_keys(recv_pub, recv_sec);

  // Sender computes derivation using receiver's public key
  crypto::key_derivation derivation_sender;
  ASSERT_TRUE(crypto::generate_key_derivation(recv_pub, tx_sec, derivation_sender));

  // Receiver computes derivation using transaction public key
  crypto::key_derivation derivation_receiver;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, recv_sec, derivation_receiver));

  // Both derivations must match
  ASSERT_EQ(0, memcmp(&derivation_sender, &derivation_receiver, sizeof(derivation_sender)));

  // Sender derives ephemeral public key
  crypto::public_key eph_pub;
  ASSERT_TRUE(crypto::derive_public_key(derivation_sender, 0, recv_pub, eph_pub));

  // Receiver derives ephemeral secret key
  crypto::secret_key eph_sec;
  crypto::derive_secret_key(derivation_receiver, 0, recv_sec, eph_sec);

  // The secret key should produce the same public key
  crypto::public_key eph_pub_check;
  ASSERT_TRUE(crypto::secret_key_to_public_key(eph_sec, eph_pub_check));
  ASSERT_EQ(eph_pub, eph_pub_check);
}

TEST(Crypto, key_derivation_multiple_indices_different_keys)
{
  crypto::public_key pub_a, pub_b;
  crypto::secret_key sec_a, sec_b;
  crypto::generate_keys(pub_a, sec_a);
  crypto::generate_keys(pub_b, sec_b);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub_b, sec_a, derivation));

  // Derive at multiple indices
  const size_t NUM_INDICES = 5;
  crypto::public_key derived_pubs[NUM_INDICES];
  for (size_t i = 0; i < NUM_INDICES; ++i)
  {
    ASSERT_TRUE(crypto::derive_public_key(derivation, i, pub_a, derived_pubs[i]));
    ASSERT_TRUE(crypto::check_key(derived_pubs[i]));
  }

  // All derived public keys must be different
  for (size_t i = 0; i < NUM_INDICES; ++i)
    for (size_t j = i + 1; j < NUM_INDICES; ++j)
      ASSERT_NE(derived_pubs[i], derived_pubs[j]);
}

TEST(Crypto, key_derivation_secret_indices_different)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  crypto::secret_key eph_sec0, eph_sec1, eph_sec2;
  crypto::derive_secret_key(derivation, 0, sec, eph_sec0);
  crypto::derive_secret_key(derivation, 1, sec, eph_sec1);
  crypto::derive_secret_key(derivation, 2, sec, eph_sec2);

  ASSERT_NE(eph_sec0, eph_sec1);
  ASSERT_NE(eph_sec0, eph_sec2);
  ASSERT_NE(eph_sec1, eph_sec2);
}

TEST(Crypto, derive_pub_from_sec_matches_direct)
{
  crypto::public_key recv_pub;
  crypto::secret_key recv_sec;
  crypto::generate_keys(recv_pub, recv_sec);

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, recv_sec, derivation));

  for (size_t idx = 0; idx < 3; ++idx)
  {
    // Derive public key directly
    crypto::public_key eph_pub_direct;
    ASSERT_TRUE(crypto::derive_public_key(derivation, idx, recv_pub, eph_pub_direct));

    // Derive secret key and then compute public key from it
    crypto::secret_key eph_sec;
    crypto::derive_secret_key(derivation, idx, recv_sec, eph_sec);
    crypto::public_key eph_pub_indirect;
    ASSERT_TRUE(crypto::secret_key_to_public_key(eph_sec, eph_pub_indirect));

    ASSERT_EQ(eph_pub_direct, eph_pub_indirect);
  }
}

TEST(Crypto, key_derivation_different_keypairs_differ)
{
  crypto::public_key pub1, pub2, pub3;
  crypto::secret_key sec1, sec2, sec3;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);
  crypto::generate_keys(pub3, sec3);

  crypto::key_derivation d12, d13;
  ASSERT_TRUE(crypto::generate_key_derivation(pub2, sec1, d12));
  ASSERT_TRUE(crypto::generate_key_derivation(pub3, sec1, d13));

  // Different target keys produce different derivations
  ASSERT_NE(0, memcmp(&d12, &d13, sizeof(d12)));
}

TEST(Crypto, derive_subaddress_public_key_multiple_indices)
{
  crypto::public_key base_pub;
  crypto::secret_key base_sec;
  crypto::generate_keys(base_pub, base_sec);

  crypto::public_key tx_pub;
  crypto::secret_key tx_sec;
  crypto::generate_keys(tx_pub, tx_sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, base_sec, derivation));

  for (size_t idx = 0; idx < 4; ++idx)
  {
    crypto::public_key derived;
    ASSERT_TRUE(crypto::derive_public_key(derivation, idx, base_pub, derived));

    crypto::public_key recovered;
    ASSERT_TRUE(crypto::derive_subaddress_public_key(derived, derivation, idx, recovered));
    ASSERT_EQ(recovered, base_pub);
  }
}

TEST(Crypto, key_derivation_large_index)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  // Large index should still work
  crypto::public_key derived;
  ASSERT_TRUE(crypto::derive_public_key(derivation, 1000000, pub, derived));
  ASSERT_TRUE(crypto::check_key(derived));
  ASSERT_NE(derived, pub);
}

TEST(Crypto, key_derivation_scalar_at_large_index)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  crypto::ec_scalar s0, s_large;
  crypto::derivation_to_scalar(derivation, 0, s0);
  crypto::derivation_to_scalar(derivation, 999999, s_large);

  ASSERT_NE(0, memcmp(&s0, &s_large, sizeof(s0)));
}

TEST(Crypto, key_derivation_chain_with_view_tag)
{
  crypto::public_key tx_pub, recv_pub;
  crypto::secret_key tx_sec, recv_sec;
  crypto::generate_keys(tx_pub, tx_sec);
  crypto::generate_keys(recv_pub, recv_sec);

  crypto::key_derivation d1, d2;
  ASSERT_TRUE(crypto::generate_key_derivation(recv_pub, tx_sec, d1));
  ASSERT_TRUE(crypto::generate_key_derivation(tx_pub, recv_sec, d2));
  ASSERT_EQ(0, memcmp(&d1, &d2, sizeof(d1)));

  // Both sides should derive the same view tag
  crypto::view_tag vt1, vt2;
  crypto::derive_view_tag(d1, 0, vt1);
  crypto::derive_view_tag(d2, 0, vt2);
  ASSERT_EQ(vt1.data, vt2.data);
}

TEST(Crypto, derive_secret_key_deterministic)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_derivation derivation;
  ASSERT_TRUE(crypto::generate_key_derivation(pub, sec, derivation));

  crypto::secret_key eph_a, eph_b;
  crypto::derive_secret_key(derivation, 42, sec, eph_a);
  crypto::derive_secret_key(derivation, 42, sec, eph_b);
  ASSERT_EQ(eph_a, eph_b);
}

// ===== Ring signature tests =====

TEST(Crypto, ring_signature_size_1_index_0)
{
  crypto::public_key pub;
  crypto::secret_key sec;
  crypto::generate_keys(pub, sec);

  crypto::key_image ki;
  crypto::generate_key_image(pub, sec, ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring1", 5, prefix);

  const crypto::public_key* pubs[] = {&pub};
  crypto::signature sig;
  crypto::generate_ring_signature(prefix, ki, pubs, 1, sec, 0, &sig);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 1, &sig));
}

TEST(Crypto, ring_signature_size_2_index_0)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  crypto::key_image ki;
  crypto::generate_key_image(pub1, sec1, ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring2idx0", 9, prefix);

  const crypto::public_key* pubs[] = {&pub1, &pub2};
  crypto::signature sigs[2];
  crypto::generate_ring_signature(prefix, ki, pubs, 2, sec1, 0, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 2, sigs));
}

TEST(Crypto, ring_signature_size_2_index_1)
{
  crypto::public_key pub1, pub2;
  crypto::secret_key sec1, sec2;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);

  crypto::key_image ki;
  crypto::generate_key_image(pub2, sec2, ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring2idx1", 9, prefix);

  const crypto::public_key* pubs[] = {&pub1, &pub2};
  crypto::signature sigs[2];
  crypto::generate_ring_signature(prefix, ki, pubs, 2, sec2, 1, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 2, sigs));
}

TEST(Crypto, ring_signature_size_11_first_index)
{
  const size_t RING_SIZE = 11;
  const size_t REAL_IDX = 0;

  crypto::public_key pubs_arr[RING_SIZE];
  crypto::secret_key secs_arr[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    crypto::generate_keys(pubs_arr[i], secs_arr[i]);

  crypto::key_image ki;
  crypto::generate_key_image(pubs_arr[REAL_IDX], secs_arr[REAL_IDX], ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring11first", 11, prefix);

  const crypto::public_key* pub_ptrs[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    pub_ptrs[i] = &pubs_arr[i];

  crypto::signature sigs[RING_SIZE];
  crypto::generate_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, secs_arr[REAL_IDX], REAL_IDX, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, sigs));
}

TEST(Crypto, ring_signature_size_11_last_index)
{
  const size_t RING_SIZE = 11;
  const size_t REAL_IDX = RING_SIZE - 1;

  crypto::public_key pubs_arr[RING_SIZE];
  crypto::secret_key secs_arr[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    crypto::generate_keys(pubs_arr[i], secs_arr[i]);

  crypto::key_image ki;
  crypto::generate_key_image(pubs_arr[REAL_IDX], secs_arr[REAL_IDX], ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring11last", 10, prefix);

  const crypto::public_key* pub_ptrs[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    pub_ptrs[i] = &pubs_arr[i];

  crypto::signature sigs[RING_SIZE];
  crypto::generate_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, secs_arr[REAL_IDX], REAL_IDX, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, sigs));
}

TEST(Crypto, ring_signature_size_11_middle_index)
{
  const size_t RING_SIZE = 11;
  const size_t REAL_IDX = 5;

  crypto::public_key pubs_arr[RING_SIZE];
  crypto::secret_key secs_arr[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    crypto::generate_keys(pubs_arr[i], secs_arr[i]);

  crypto::key_image ki;
  crypto::generate_key_image(pubs_arr[REAL_IDX], secs_arr[REAL_IDX], ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("ring11mid", 9, prefix);

  const crypto::public_key* pub_ptrs[RING_SIZE];
  for (size_t i = 0; i < RING_SIZE; ++i)
    pub_ptrs[i] = &pubs_arr[i];

  crypto::signature sigs[RING_SIZE];
  crypto::generate_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, secs_arr[REAL_IDX], REAL_IDX, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pub_ptrs, RING_SIZE, sigs));
}

TEST(Crypto, ring_signature_wrong_key_fails)
{
  crypto::public_key pub1, pub2, pub_wrong;
  crypto::secret_key sec1, sec2, sec_wrong;
  crypto::generate_keys(pub1, sec1);
  crypto::generate_keys(pub2, sec2);
  crypto::generate_keys(pub_wrong, sec_wrong);

  crypto::key_image ki;
  crypto::generate_key_image(pub1, sec1, ki);

  crypto::hash prefix;
  crypto::cn_fast_hash("wrong_key_test", 14, prefix);

  const crypto::public_key* pubs[] = {&pub1, &pub2};
  crypto::signature sigs[2];
  crypto::generate_ring_signature(prefix, ki, pubs, 2, sec1, 0, sigs);
  ASSERT_TRUE(crypto::check_ring_signature(prefix, ki, pubs, 2, sigs));

  // Substitute a wrong public key
  const crypto::public_key* pubs_wrong[] = {&pub_wrong, &pub2};
  ASSERT_FALSE(crypto::check_ring_signature(prefix, ki, pubs_wrong, 2, sigs));
}

// ===== Constant-time secret key sort regression tests (Bug #2) =====

namespace {
  // Constant-time less-than comparator for secret keys, matching the one
  // used in multisig_account_kex_impl.cpp. This is duplicated here for
  // testing purposes to verify the comparator produces correct ordering.
  static bool ct_secret_key_less(const crypto::secret_key &key1, const crypto::secret_key &key2)
  {
    const unsigned char *a = reinterpret_cast<const unsigned char*>(&key1);
    const unsigned char *b = reinterpret_cast<const unsigned char*>(&key2);
    unsigned gt = 0;
    unsigned lt = 0;
    for (size_t i = 0; i < sizeof(crypto::secret_key); ++i)
    {
      unsigned not_done = 1u - (gt | lt);
      unsigned ai = a[i], bi = b[i];
      gt |= not_done & ((bi - ai) >> 8) & 1u;
      lt |= not_done & ((ai - bi) >> 8) & 1u;
    }
    return lt != 0;
  }
}

TEST(Crypto, ct_sort_matches_memcmp_sort)
{
  // Generate a set of random secret keys and verify that the constant-time
  // comparator produces the same sort order as memcmp.
  const size_t N = 20;
  std::vector<crypto::secret_key> keys_ct(N), keys_memcmp(N);
  for (size_t i = 0; i < N; ++i)
  {
    keys_ct[i] = rct::rct2sk(rct::skGen());
    keys_memcmp[i] = keys_ct[i];
  }

  // Sort with memcmp (reference)
  std::sort(keys_memcmp.begin(), keys_memcmp.end(),
    [](const crypto::secret_key &a, const crypto::secret_key &b) {
      return memcmp(&a, &b, sizeof(crypto::secret_key)) < 0;
    });

  // Sort with constant-time comparator
  std::sort(keys_ct.begin(), keys_ct.end(), ct_secret_key_less);

  // Both sorts must produce identical ordering
  for (size_t i = 0; i < N; ++i)
  {
    ASSERT_EQ(0, memcmp(&keys_ct[i], &keys_memcmp[i], sizeof(crypto::secret_key)))
      << "Mismatch at index " << i;
  }
}

TEST(Crypto, ct_sort_identical_keys)
{
  // Sorting a vector of identical keys should not crash or reorder.
  crypto::secret_key key = rct::rct2sk(rct::skGen());
  std::vector<crypto::secret_key> keys(5, key);

  std::sort(keys.begin(), keys.end(), ct_secret_key_less);

  for (size_t i = 0; i < keys.size(); ++i)
  {
    ASSERT_EQ(0, memcmp(&keys[i], &key, sizeof(crypto::secret_key)));
  }
}

TEST(Crypto, ct_sort_keys_differ_only_in_last_byte)
{
  // Two keys that differ only in the last byte (byte index 31, most significant
  // in the big-endian interpretation used by the comparator).
  crypto::secret_key k1, k2;
  memset(&k1, 0xAA, sizeof(k1));
  memset(&k2, 0xAA, sizeof(k2));
  reinterpret_cast<unsigned char*>(&k2)[31] = 0xBB; // k2 > k1

  // ct comparator should agree with memcmp
  int cmp = memcmp(&k1, &k2, sizeof(crypto::secret_key));
  bool ct_result = ct_secret_key_less(k1, k2);
  ASSERT_EQ(ct_result, cmp < 0);

  // Reverse comparison
  bool ct_reverse = ct_secret_key_less(k2, k1);
  ASSERT_EQ(ct_reverse, memcmp(&k2, &k1, sizeof(crypto::secret_key)) < 0);

  // Equal keys
  ASSERT_FALSE(ct_secret_key_less(k1, k1));
}

TEST(Crypto, ct_sort_keys_differ_only_in_first_byte)
{
  // Two keys that differ only in byte index 0 (least significant byte).
  crypto::secret_key k1, k2;
  memset(&k1, 0xCC, sizeof(k1));
  memset(&k2, 0xCC, sizeof(k2));
  reinterpret_cast<unsigned char*>(&k1)[0] = 0x01;
  reinterpret_cast<unsigned char*>(&k2)[0] = 0x02;

  bool ct_result = ct_secret_key_less(k1, k2);
  int cmp = memcmp(&k1, &k2, sizeof(crypto::secret_key));
  ASSERT_EQ(ct_result, cmp < 0);
}
