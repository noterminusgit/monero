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
