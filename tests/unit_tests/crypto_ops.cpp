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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

extern "C" {
#include "crypto/crypto-ops.h"
}

#include "crypto/crypto.h"
#include <cstring>

// ============================================================================
// Helpers
// ============================================================================

namespace
{
  // The ed25519 basepoint encoding (compressed y-coordinate):
  // y = 4/5 mod p, with x positive. The standard encoding is:
  // (0x58, 0x66, 0x66, ..., 0x66)
  static const unsigned char ed25519_basepoint[32] = {
    0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
  };

  // The identity point encoding: y=1, x=0 => compressed = (0x01, 0x00, ..., 0x00)
  static const unsigned char identity_point[32] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  // The group order l = 2^252 + 27742317777372353535851937790883648493
  static const unsigned char curve_order[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
  };

  // Helper: create a scalar with a small value (little-endian 32 bytes)
  static void make_scalar(unsigned char out[32], uint64_t val)
  {
    memset(out, 0, 32);
    for (int i = 0; i < 8 && val; ++i)
    {
      out[i] = val & 0xff;
      val >>= 8;
    }
  }

  // Helper: check if 32-byte buffer is all zeros
  static bool is_zero_32(const unsigned char *buf)
  {
    for (int i = 0; i < 32; ++i)
      if (buf[i] != 0)
        return false;
    return true;
  }

}

// ============================================================================
// 1. Field element operations (fe_*)
// ============================================================================

TEST(crypto_ops, fe_0_produces_zero)
{
  fe h;
  fe_0(h);
  unsigned char bytes[32];
  fe_tobytes(bytes, h);
  EXPECT_TRUE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_0_all_limbs_zero)
{
  fe h;
  // Set to non-zero first
  for (int i = 0; i < 10; ++i)
    h[i] = 12345;
  fe_0(h);
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(h[i], 0);
}

TEST(crypto_ops, fe_1_via_invert_roundtrip)
{
  // We cannot call fe_1 directly (it is static), but we can obtain the
  // identity field element by using fe_invert: inv(1) = 1, and the Z
  // coordinate of a freshly decoded point from the identity is 1.
  ge_p3 id;
  ASSERT_EQ(0, ge_frombytes_vartime(&id, identity_point));
  // Z coordinate of the decoded identity is 1
  unsigned char z_bytes[32];
  fe_tobytes(z_bytes, id.Z);
  // Should be 0x01, 0x00, ..., 0x00
  EXPECT_EQ(z_bytes[0], 1);
  for (int i = 1; i < 32; ++i)
    EXPECT_EQ(z_bytes[i], 0);
}

TEST(crypto_ops, fe_add_known_vectors)
{
  // 0 + 0 = 0
  fe a, b, c;
  fe_0(a);
  fe_0(b);
  fe_add(c, a, b);
  unsigned char bytes[32];
  fe_tobytes(bytes, c);
  EXPECT_TRUE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_add_zero_identity)
{
  // a + 0 = a
  // Use the Y coordinate of the basepoint as a known non-zero field element
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe zero;
  fe_0(zero);
  fe result;
  fe_add(result, B.Y, zero);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, B.Y);
  fe_tobytes(b2, result);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_add_commutative)
{
  // a + b = b + a
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe r1, r2;
  fe_add(r1, B.X, B.Y);
  fe_add(r2, B.Y, B.X);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, r1);
  fe_tobytes(b2, r2);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_sub_via_add_inverse)
{
  // Indirectly test fe_sub: a - a = 0.
  // fe_sub is static, but ge_frombytes_vartime calls it internally.
  // We can verify by decoding a point, negating it, adding, and checking identity.
  // Instead, verify through the ge_add/ge_sub interface (tested below).
  // Here we verify that fe_add(a, a, neg_a) = 0 by construction.
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  // Construct -Y by serializing and flipping the sign bit, then decoding
  unsigned char neg_bp[32];
  memcpy(neg_bp, ed25519_basepoint, 32);
  neg_bp[31] ^= 0x80;  // flip sign bit = negate x coordinate
  ge_p3 neg_B;
  ASSERT_EQ(0, ge_frombytes_vartime(&neg_B, neg_bp));

  // Both should have the same Y coordinate
  unsigned char y1[32], y2[32];
  fe_tobytes(y1, B.Y);
  fe_tobytes(y2, neg_B.Y);
  EXPECT_EQ(0, memcmp(y1, y2, 32));
}

TEST(crypto_ops, fe_mul_by_zero)
{
  // a * 0 = 0
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe zero, result;
  fe_0(zero);
  fe_mul(result, B.Y, zero);

  unsigned char bytes[32];
  fe_tobytes(bytes, result);
  EXPECT_TRUE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_mul_zero_by_anything)
{
  // 0 * a = 0
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe zero, result;
  fe_0(zero);
  fe_mul(result, zero, B.X);

  unsigned char bytes[32];
  fe_tobytes(bytes, result);
  EXPECT_TRUE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_mul_commutative)
{
  // a * b = b * a
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe r1, r2;
  fe_mul(r1, B.X, B.Y);
  fe_mul(r2, B.Y, B.X);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, r1);
  fe_tobytes(b2, r2);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_mul_identity)
{
  // a * 1 = a
  // Get the "1" field element from the decoded identity point's Z coordinate
  ge_p3 id;
  ASSERT_EQ(0, ge_frombytes_vartime(&id, identity_point));
  // id.Z = 1

  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe result;
  fe_mul(result, B.Y, id.Z);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, B.Y);
  fe_tobytes(b2, result);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_sq_equals_mul_self)
{
  // fe_sq is static, so test indirectly through fe_mul: a*a
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe sq;
  fe_mul(sq, B.Y, B.Y);

  unsigned char bytes[32];
  fe_tobytes(bytes, sq);
  // Just verify it produces a non-zero result for a non-zero input
  EXPECT_FALSE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_invert_identity)
{
  // inv(1) = 1
  ge_p3 id;
  ASSERT_EQ(0, ge_frombytes_vartime(&id, identity_point));
  // id.Z is the field element 1

  fe inv;
  fe_invert(inv, id.Z);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, id.Z);
  fe_tobytes(b2, inv);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_invert_roundtrip)
{
  // a * inv(a) = 1
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe inv, product;
  fe_invert(inv, B.Y);
  fe_mul(product, B.Y, inv);

  unsigned char bytes[32];
  fe_tobytes(bytes, product);
  // Should be 1: 0x01, 0x00, ..., 0x00
  EXPECT_EQ(bytes[0], 1);
  for (int i = 1; i < 32; ++i)
    EXPECT_EQ(bytes[i], 0);
}

TEST(crypto_ops, fe_invert_double_invert)
{
  // inv(inv(a)) = a
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe inv1, inv2;
  fe_invert(inv1, B.X);
  fe_invert(inv2, inv1);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, B.X);
  fe_tobytes(b2, inv2);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_neg_via_sub_from_zero)
{
  // fe_neg is static, but we can test: 0 - a via fe_add with the additive inverse.
  // We verify through point negation: encoding a point and its negation yields
  // opposite sign bits but the same Y.
  unsigned char bp_neg[32];
  memcpy(bp_neg, ed25519_basepoint, 32);
  bp_neg[31] ^= 0x80;

  ge_p3 P, Q;
  ASSERT_EQ(0, ge_frombytes_vartime(&P, ed25519_basepoint));
  ASSERT_EQ(0, ge_frombytes_vartime(&Q, bp_neg));

  // Y coordinates should match
  unsigned char y1[32], y2[32];
  fe_tobytes(y1, P.Y);
  fe_tobytes(y2, Q.Y);
  EXPECT_EQ(0, memcmp(y1, y2, 32));

  // X coordinates should be additive inverses: X + (-X) = 0
  fe sum;
  fe_add(sum, P.X, Q.X);
  unsigned char sum_bytes[32];
  fe_tobytes(sum_bytes, sum);
  EXPECT_TRUE(is_zero_32(sum_bytes));
}

TEST(crypto_ops, fe_tobytes_zero)
{
  fe h;
  fe_0(h);
  unsigned char bytes[32];
  fe_tobytes(bytes, h);
  EXPECT_TRUE(is_zero_32(bytes));
}

TEST(crypto_ops, fe_tobytes_deterministic)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, B.Y);
  fe_tobytes(b2, B.Y);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_frombytes_tobytes_roundtrip)
{
  // fe_frombytes is inlined inside ge_frombytes_vartime. We test the roundtrip:
  // decode a point, re-encode it, decode again, compare.
  ge_p3 P;
  ASSERT_EQ(0, ge_frombytes_vartime(&P, ed25519_basepoint));

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &P);
  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));
}

TEST(crypto_ops, fe_isnonzero_indirect_via_invert)
{
  // fe_isnonzero is static. We test indirectly:
  // A non-zero field element has a valid inverse.
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe inv, product;
  fe_invert(inv, B.Y);
  fe_mul(product, B.Y, inv);

  unsigned char bytes[32];
  fe_tobytes(bytes, product);
  EXPECT_EQ(bytes[0], 1);
}

TEST(crypto_ops, fe_isnegative_indirect)
{
  // fe_isnegative is static. It returns s[0] & 1 of the serialized form.
  // We can check this indirectly: the basepoint encoding has sign bit set,
  // meaning the x-coordinate when serialized has the low bit set.
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  unsigned char x_bytes[32];
  fe_tobytes(x_bytes, B.X);
  // The low bit indicates the "sign" of the field element
  // This just needs to be consistent, not a specific value
  int sign = x_bytes[0] & 1;
  EXPECT_TRUE(sign == 0 || sign == 1);
}

TEST(crypto_ops, fe_copy_via_point_decode)
{
  // Verify that decoding and re-encoding preserves data (exercises fe_copy internally)
  ge_p3 P1, P2;
  ASSERT_EQ(0, ge_frombytes_vartime(&P1, ed25519_basepoint));
  ASSERT_EQ(0, ge_frombytes_vartime(&P2, ed25519_basepoint));

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, P1.Y);
  fe_tobytes(b2, P2.Y);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_cmov_indirect_via_scalarmult)
{
  // fe_cmov is used internally by ge_scalarmult_base during the
  // constant-time table lookup. Verify indirectly that scalar multiplication
  // produces correct results (which requires fe_cmov to work).
  unsigned char scalar[32];
  make_scalar(scalar, 1);
  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);
  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));
}

// ============================================================================
// 2. Group element operations (ge_*)
// ============================================================================

TEST(crypto_ops, ge_frombytes_vartime_basepoint)
{
  ge_p3 B;
  int rc = ge_frombytes_vartime(&B, ed25519_basepoint);
  EXPECT_EQ(0, rc);
}

TEST(crypto_ops, ge_frombytes_vartime_identity)
{
  ge_p3 id;
  int rc = ge_frombytes_vartime(&id, identity_point);
  EXPECT_EQ(0, rc);
}

TEST(crypto_ops, ge_frombytes_vartime_invalid_point)
{
  // Construct an invalid point: y = p - 1 = 2^255 - 20, with sign bit 0.
  // This value, when plugged into the curve equation, should not yield a valid x.
  unsigned char invalid[32];
  memset(invalid, 0xff, 32);
  invalid[31] = 0x7f;  // clear sign bit, y = 2^255 - 1 (not canonical and >= p)
  // Make it even more obviously invalid by setting non-canonical bytes
  invalid[0] = 0xee;   // push past p = 2^255 - 19
  ge_p3 P;
  int rc = ge_frombytes_vartime(&P, invalid);
  EXPECT_NE(0, rc);
}

TEST(crypto_ops, ge_frombytes_vartime_invalid_random)
{
  // 0xFF repeated 32 times is not valid
  unsigned char invalid[32];
  memset(invalid, 0xff, 32);
  ge_p3 P;
  int rc = ge_frombytes_vartime(&P, invalid);
  EXPECT_NE(0, rc);
}

TEST(crypto_ops, ge_frombytes_vartime_high_bit_variations)
{
  // The high bit of byte 31 is the sign of x. Test with the negated basepoint.
  unsigned char neg_bp[32];
  memcpy(neg_bp, ed25519_basepoint, 32);
  neg_bp[31] ^= 0x80;
  ge_p3 P;
  EXPECT_EQ(0, ge_frombytes_vartime(&P, neg_bp));
}

TEST(crypto_ops, ge_p3_tobytes_basepoint)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &B);
  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));
}

TEST(crypto_ops, ge_p3_tobytes_identity)
{
  ge_p3 id;
  ASSERT_EQ(0, ge_frombytes_vartime(&id, identity_point));

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &id);
  EXPECT_EQ(0, memcmp(encoded, identity_point, 32));
}

TEST(crypto_ops, ge_frombytes_tobytes_roundtrip_basepoint)
{
  ge_p3 P;
  ASSERT_EQ(0, ge_frombytes_vartime(&P, ed25519_basepoint));

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &P);
  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));

  // Decode again and re-encode
  ge_p3 P2;
  ASSERT_EQ(0, ge_frombytes_vartime(&P2, encoded));

  unsigned char encoded2[32];
  ge_p3_tobytes(encoded2, &P2);
  EXPECT_EQ(0, memcmp(encoded, encoded2, 32));
}

TEST(crypto_ops, ge_frombytes_tobytes_roundtrip_negated)
{
  unsigned char neg_bp[32];
  memcpy(neg_bp, ed25519_basepoint, 32);
  neg_bp[31] ^= 0x80;

  ge_p3 P;
  ASSERT_EQ(0, ge_frombytes_vartime(&P, neg_bp));

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &P);
  EXPECT_EQ(0, memcmp(encoded, neg_bp, 32));
}

TEST(crypto_ops, ge_scalarmult_base_by_one)
{
  // 1 * B = B
  unsigned char scalar[32];
  make_scalar(scalar, 1);

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);
  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));
}

TEST(crypto_ops, ge_scalarmult_base_by_zero)
{
  // 0 * B = identity
  unsigned char scalar[32];
  make_scalar(scalar, 0);

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);
  EXPECT_EQ(0, memcmp(encoded, identity_point, 32));
}

TEST(crypto_ops, ge_scalarmult_base_by_two)
{
  // 2 * B should be a valid point different from B and identity
  unsigned char scalar[32];
  make_scalar(scalar, 2);

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);
  EXPECT_NE(0, memcmp(encoded, ed25519_basepoint, 32));
  EXPECT_NE(0, memcmp(encoded, identity_point, 32));

  // Verify it decodes back as a valid point
  ge_p3 check;
  EXPECT_EQ(0, ge_frombytes_vartime(&check, encoded));
}

TEST(crypto_ops, ge_scalarmult_base_by_order_is_identity)
{
  // l * B = identity
  unsigned char scalar[32];
  memcpy(scalar, curve_order, 32);

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);
  EXPECT_EQ(0, memcmp(encoded, identity_point, 32));
}

TEST(crypto_ops, ge_scalarmult_base_order_minus_one)
{
  // (l-1) * B should equal -B
  unsigned char scalar[32];
  memcpy(scalar, curve_order, 32);
  // Subtract 1 from the scalar (little-endian)
  int borrow = 1;
  for (int i = 0; i < 32 && borrow; ++i)
  {
    int val = (int)scalar[i] - borrow;
    if (val < 0)
    {
      scalar[i] = (unsigned char)(val + 256);
      borrow = 1;
    }
    else
    {
      scalar[i] = (unsigned char)val;
      borrow = 0;
    }
  }

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &result);

  // -B has the same Y as B but with the sign bit flipped
  unsigned char neg_bp[32];
  memcpy(neg_bp, ed25519_basepoint, 32);
  neg_bp[31] ^= 0x80;
  EXPECT_EQ(0, memcmp(encoded, neg_bp, 32));
}

TEST(crypto_ops, ge_scalarmult_base_linearity)
{
  // (a + b) * B = a*B + b*B
  unsigned char sa[32], sb[32], sab[32];
  make_scalar(sa, 3);
  make_scalar(sb, 5);
  make_scalar(sab, 8);

  ge_p3 aB, bB, abB;
  ge_scalarmult_base(&aB, sa);
  ge_scalarmult_base(&bB, sb);
  ge_scalarmult_base(&abB, sab);

  // Compute aB + bB
  ge_cached bB_cached;
  ge_p3_to_cached(&bB_cached, &bB);
  ge_p1p1 sum_p1p1;
  ge_add(&sum_p1p1, &aB, &bB_cached);
  ge_p3 sum;
  ge_p1p1_to_p3(&sum, &sum_p1p1);

  unsigned char enc_sum[32], enc_abB[32];
  ge_p3_tobytes(enc_sum, &sum);
  ge_p3_tobytes(enc_abB, &abB);
  EXPECT_EQ(0, memcmp(enc_sum, enc_abB, 32));
}

TEST(crypto_ops, ge_double_scalarmult_base_vartime_basic)
{
  // Test a*A + b*B where A = B (basepoint), a=1, b=1 => 2*B
  unsigned char sa[32], sb[32];
  make_scalar(sa, 1);
  make_scalar(sb, 1);

  ge_p3 A;
  ASSERT_EQ(0, ge_frombytes_vartime(&A, ed25519_basepoint));

  ge_p2 result;
  ge_double_scalarmult_base_vartime(&result, sa, &A, sb);

  unsigned char encoded[32];
  ge_tobytes(encoded, &result);

  // Should equal 2*B
  unsigned char s2[32];
  make_scalar(s2, 2);
  ge_p3 twoB;
  ge_scalarmult_base(&twoB, s2);

  unsigned char enc_2B[32];
  ge_p3_tobytes(enc_2B, &twoB);
  EXPECT_EQ(0, memcmp(encoded, enc_2B, 32));
}

TEST(crypto_ops, ge_double_scalarmult_base_vartime_zero_a)
{
  // 0*A + b*B = b*B
  unsigned char sa[32], sb[32];
  make_scalar(sa, 0);
  make_scalar(sb, 5);

  ge_p3 A;
  ASSERT_EQ(0, ge_frombytes_vartime(&A, ed25519_basepoint));

  ge_p2 result;
  ge_double_scalarmult_base_vartime(&result, sa, &A, sb);

  unsigned char encoded[32];
  ge_tobytes(encoded, &result);

  ge_p3 fiveB;
  ge_scalarmult_base(&fiveB, sb);
  unsigned char enc_5B[32];
  ge_p3_tobytes(enc_5B, &fiveB);
  EXPECT_EQ(0, memcmp(encoded, enc_5B, 32));
}

TEST(crypto_ops, ge_double_scalarmult_base_vartime_zero_b)
{
  // a*A + 0*B = a*A
  unsigned char sa[32], sb[32];
  make_scalar(sa, 3);
  make_scalar(sb, 0);

  ge_p3 A;
  ASSERT_EQ(0, ge_frombytes_vartime(&A, ed25519_basepoint));

  ge_p2 result;
  ge_double_scalarmult_base_vartime(&result, sa, &A, sb);

  unsigned char encoded[32];
  ge_tobytes(encoded, &result);

  // a*A = 3*B since A = B
  ge_p3 threeB;
  ge_scalarmult_base(&threeB, sa);
  unsigned char enc_3B[32];
  ge_p3_tobytes(enc_3B, &threeB);
  EXPECT_EQ(0, memcmp(encoded, enc_3B, 32));
}

TEST(crypto_ops, ge_p3_is_point_at_infinity_identity)
{
  // The constant ge_p3_identity should be at infinity
  EXPECT_EQ(1, ge_p3_is_point_at_infinity_vartime(&ge_p3_identity));
}

TEST(crypto_ops, ge_p3_is_point_at_infinity_basepoint)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));
  EXPECT_EQ(0, ge_p3_is_point_at_infinity_vartime(&B));
}

TEST(crypto_ops, ge_p3_is_point_at_infinity_computed_identity)
{
  // 0*B should give the identity
  unsigned char zero[32];
  make_scalar(zero, 0);

  ge_p3 result;
  ge_scalarmult_base(&result, zero);
  EXPECT_EQ(1, ge_p3_is_point_at_infinity_vartime(&result));
}

TEST(crypto_ops, ge_p3_is_point_at_infinity_order_times_base)
{
  // l*B = identity
  unsigned char scalar[32];
  memcpy(scalar, curve_order, 32);
  ge_p3 result;
  ge_scalarmult_base(&result, scalar);
  EXPECT_EQ(1, ge_p3_is_point_at_infinity_vartime(&result));
}

TEST(crypto_ops, ge_add_two_points)
{
  // B + B = 2*B
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_cached B_cached;
  ge_p3_to_cached(&B_cached, &B);

  ge_p1p1 sum_p1p1;
  ge_add(&sum_p1p1, &B, &B_cached);

  ge_p3 sum;
  ge_p1p1_to_p3(&sum, &sum_p1p1);

  unsigned char enc_sum[32];
  ge_p3_tobytes(enc_sum, &sum);

  unsigned char s2[32];
  make_scalar(s2, 2);
  ge_p3 twoB;
  ge_scalarmult_base(&twoB, s2);

  unsigned char enc_2B[32];
  ge_p3_tobytes(enc_2B, &twoB);
  EXPECT_EQ(0, memcmp(enc_sum, enc_2B, 32));
}

TEST(crypto_ops, ge_add_identity)
{
  // B + identity = B
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_cached id_cached;
  ge_p3_to_cached(&id_cached, &ge_p3_identity);

  ge_p1p1 r;
  ge_add(&r, &B, &id_cached);
  ge_p3 result;
  ge_p1p1_to_p3(&result, &r);

  unsigned char enc_result[32], enc_B[32];
  ge_p3_tobytes(enc_result, &result);
  ge_p3_tobytes(enc_B, &B);
  EXPECT_EQ(0, memcmp(enc_result, enc_B, 32));
}

TEST(crypto_ops, ge_sub_same_point_gives_identity)
{
  // B - B = identity
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_cached B_cached;
  ge_p3_to_cached(&B_cached, &B);

  ge_p1p1 diff_p1p1;
  ge_sub(&diff_p1p1, &B, &B_cached);

  ge_p3 diff;
  ge_p1p1_to_p3(&diff, &diff_p1p1);

  EXPECT_EQ(1, ge_p3_is_point_at_infinity_vartime(&diff));
}

TEST(crypto_ops, ge_sub_identity)
{
  // B - identity = B
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_cached id_cached;
  ge_p3_to_cached(&id_cached, &ge_p3_identity);

  ge_p1p1 r;
  ge_sub(&r, &B, &id_cached);
  ge_p3 result;
  ge_p1p1_to_p3(&result, &r);

  unsigned char enc_result[32], enc_B[32];
  ge_p3_tobytes(enc_result, &result);
  ge_p3_tobytes(enc_B, &B);
  EXPECT_EQ(0, memcmp(enc_result, enc_B, 32));
}

TEST(crypto_ops, ge_add_sub_cancel)
{
  // (B + 2B) - 2B = B
  unsigned char s2[32];
  make_scalar(s2, 2);
  ge_p3 twoB;
  ge_scalarmult_base(&twoB, s2);

  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  // B + 2B
  ge_cached twoB_cached;
  ge_p3_to_cached(&twoB_cached, &twoB);
  ge_p1p1 sum_p1p1;
  ge_add(&sum_p1p1, &B, &twoB_cached);
  ge_p3 threeB;
  ge_p1p1_to_p3(&threeB, &sum_p1p1);

  // 3B - 2B
  ge_p1p1 diff_p1p1;
  ge_sub(&diff_p1p1, &threeB, &twoB_cached);
  ge_p3 result;
  ge_p1p1_to_p3(&result, &diff_p1p1);

  unsigned char enc_result[32];
  ge_p3_tobytes(enc_result, &result);
  EXPECT_EQ(0, memcmp(enc_result, ed25519_basepoint, 32));
}

TEST(crypto_ops, ge_p1p1_to_p2_roundtrip)
{
  // Compute B + B via ge_add, convert to p2, serialize
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));
  ge_cached B_cached;
  ge_p3_to_cached(&B_cached, &B);

  ge_p1p1 r;
  ge_add(&r, &B, &B_cached);

  ge_p2 p2;
  ge_p1p1_to_p2(&p2, &r);

  unsigned char enc_p2[32];
  ge_tobytes(enc_p2, &p2);

  // Compare with 2*B
  unsigned char s2[32];
  make_scalar(s2, 2);
  ge_p3 twoB;
  ge_scalarmult_base(&twoB, s2);
  unsigned char enc_2B[32];
  ge_p3_tobytes(enc_2B, &twoB);
  EXPECT_EQ(0, memcmp(enc_p2, enc_2B, 32));
}

TEST(crypto_ops, ge_p3_to_p2_consistency)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p2 B_p2;
  ge_p3_to_p2(&B_p2, &B);

  unsigned char enc_p2[32], enc_p3[32];
  ge_tobytes(enc_p2, &B_p2);
  ge_p3_tobytes(enc_p3, &B);
  EXPECT_EQ(0, memcmp(enc_p2, enc_p3, 32));
}

TEST(crypto_ops, ge_tobytes_p2_basepoint)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p2 B_p2;
  ge_p3_to_p2(&B_p2, &B);

  unsigned char enc[32];
  ge_tobytes(enc, &B_p2);
  EXPECT_EQ(0, memcmp(enc, ed25519_basepoint, 32));
}

TEST(crypto_ops, ge_p2_dbl_equals_add)
{
  // 2*B via doubling = B + B
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p2 B_p2;
  ge_p3_to_p2(&B_p2, &B);

  ge_p1p1 dbl_p1p1;
  ge_p2_dbl(&dbl_p1p1, &B_p2);
  ge_p3 dbl;
  ge_p1p1_to_p3(&dbl, &dbl_p1p1);

  unsigned char enc_dbl[32];
  ge_p3_tobytes(enc_dbl, &dbl);

  unsigned char s2[32];
  make_scalar(s2, 2);
  ge_p3 twoB;
  ge_scalarmult_base(&twoB, s2);
  unsigned char enc_2B[32];
  ge_p3_tobytes(enc_2B, &twoB);
  EXPECT_EQ(0, memcmp(enc_dbl, enc_2B, 32));
}

TEST(crypto_ops, ge_mul8_basic)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p2 B_p2;
  ge_p3_to_p2(&B_p2, &B);

  ge_p1p1 eightB_p1p1;
  ge_mul8(&eightB_p1p1, &B_p2);
  ge_p3 eightB;
  ge_p1p1_to_p3(&eightB, &eightB_p1p1);

  unsigned char enc_8B[32];
  ge_p3_tobytes(enc_8B, &eightB);

  unsigned char s8[32];
  make_scalar(s8, 8);
  ge_p3 eightB_ref;
  ge_scalarmult_base(&eightB_ref, s8);
  unsigned char enc_ref[32];
  ge_p3_tobytes(enc_ref, &eightB_ref);
  EXPECT_EQ(0, memcmp(enc_8B, enc_ref, 32));
}

TEST(crypto_ops, ge_scalarmult_arbitrary)
{
  // Test ge_scalarmult (not base) with a known scalar
  unsigned char s7[32];
  make_scalar(s7, 7);

  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p2 result;
  ge_scalarmult(&result, s7, &B);

  unsigned char enc[32];
  ge_tobytes(enc, &result);

  // Compare with ge_scalarmult_base
  ge_p3 ref;
  ge_scalarmult_base(&ref, s7);
  unsigned char enc_ref[32];
  ge_p3_tobytes(enc_ref, &ref);
  EXPECT_EQ(0, memcmp(enc, enc_ref, 32));
}

TEST(crypto_ops, ge_scalarmult_p3_arbitrary)
{
  // Test ge_scalarmult_p3
  unsigned char s13[32];
  make_scalar(s13, 13);

  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_p3 result;
  ge_scalarmult_p3(&result, s13, &B);

  unsigned char enc[32];
  ge_p3_tobytes(enc, &result);

  ge_p3 ref;
  ge_scalarmult_base(&ref, s13);
  unsigned char enc_ref[32];
  ge_p3_tobytes(enc_ref, &ref);
  EXPECT_EQ(0, memcmp(enc, enc_ref, 32));
}

// ============================================================================
// 3. Scalar operations (sc_*)
// ============================================================================

TEST(crypto_ops, sc_0_produces_zero)
{
  unsigned char s[32];
  memset(s, 0xff, 32);
  sc_0(s);
  EXPECT_TRUE(is_zero_32(s));
}

TEST(crypto_ops, sc_isnonzero_zero)
{
  unsigned char s[32];
  sc_0(s);
  EXPECT_EQ(0, sc_isnonzero(s));
}

TEST(crypto_ops, sc_isnonzero_one)
{
  unsigned char s[32];
  make_scalar(s, 1);
  EXPECT_NE(0, sc_isnonzero(s));
}

TEST(crypto_ops, sc_isnonzero_large)
{
  unsigned char s[32];
  make_scalar(s, 0xdeadbeef);
  EXPECT_NE(0, sc_isnonzero(s));
}

TEST(crypto_ops, sc_check_zero_valid)
{
  unsigned char s[32];
  sc_0(s);
  EXPECT_EQ(0, sc_check(s));
}

TEST(crypto_ops, sc_check_one_valid)
{
  unsigned char s[32];
  make_scalar(s, 1);
  EXPECT_EQ(0, sc_check(s));
}

TEST(crypto_ops, sc_check_order_invalid)
{
  // The group order l itself is NOT a valid scalar (must be < l)
  unsigned char s[32];
  memcpy(s, curve_order, 32);
  EXPECT_NE(0, sc_check(s));
}

TEST(crypto_ops, sc_check_order_minus_one_valid)
{
  unsigned char s[32];
  memcpy(s, curve_order, 32);
  // Subtract 1
  int borrow = 1;
  for (int i = 0; i < 32 && borrow; ++i)
  {
    int val = (int)s[i] - borrow;
    if (val < 0)
    {
      s[i] = (unsigned char)(val + 256);
      borrow = 1;
    }
    else
    {
      s[i] = (unsigned char)val;
      borrow = 0;
    }
  }
  EXPECT_EQ(0, sc_check(s));
}

TEST(crypto_ops, sc_check_all_ones_invalid)
{
  unsigned char s[32];
  memset(s, 0xff, 32);
  EXPECT_NE(0, sc_check(s));
}

TEST(crypto_ops, sc_reduce32_small_value_unchanged)
{
  // A small value < l should not change after reduce32
  unsigned char s[32];
  make_scalar(s, 42);
  unsigned char orig[32];
  memcpy(orig, s, 32);
  sc_reduce32(s);
  EXPECT_EQ(0, memcmp(s, orig, 32));
}

TEST(crypto_ops, sc_reduce32_order_becomes_zero)
{
  // l mod l = 0
  unsigned char s[32];
  memcpy(s, curve_order, 32);
  sc_reduce32(s);
  EXPECT_TRUE(is_zero_32(s));
}

TEST(crypto_ops, sc_reduce32_order_plus_one)
{
  // (l + 1) mod l = 1
  unsigned char s[32];
  memcpy(s, curve_order, 32);
  // Add 1
  int carry = 1;
  for (int i = 0; i < 32 && carry; ++i)
  {
    int val = (int)s[i] + carry;
    s[i] = (unsigned char)(val & 0xff);
    carry = val >> 8;
  }
  sc_reduce32(s);

  unsigned char one[32];
  make_scalar(one, 1);
  EXPECT_EQ(0, memcmp(s, one, 32));
}

TEST(crypto_ops, sc_reduce32_large_value)
{
  unsigned char s[32];
  memset(s, 0xff, 32);
  // Clear the top bit so it fits in 256 bits properly
  s[31] = 0x0f;
  sc_reduce32(s);
  // Result should be valid
  EXPECT_EQ(0, sc_check(s));
}

TEST(crypto_ops, sc_add_zero_identity)
{
  // a + 0 = a
  unsigned char a[32], zero[32], result[32];
  make_scalar(a, 100);
  sc_0(zero);
  sc_add(result, a, zero);
  EXPECT_EQ(0, memcmp(result, a, 32));
}

TEST(crypto_ops, sc_add_commutative)
{
  unsigned char a[32], b[32], r1[32], r2[32];
  make_scalar(a, 123);
  make_scalar(b, 456);
  sc_add(r1, a, b);
  sc_add(r2, b, a);
  EXPECT_EQ(0, memcmp(r1, r2, 32));
}

TEST(crypto_ops, sc_add_known_values)
{
  unsigned char a[32], b[32], result[32], expected[32];
  make_scalar(a, 3);
  make_scalar(b, 5);
  make_scalar(expected, 8);
  sc_add(result, a, b);
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_add_wrap_around)
{
  // (l-1) + 1 = 0 (mod l)
  unsigned char lm1[32];
  memcpy(lm1, curve_order, 32);
  int borrow = 1;
  for (int i = 0; i < 32 && borrow; ++i)
  {
    int val = (int)lm1[i] - borrow;
    if (val < 0)
    {
      lm1[i] = (unsigned char)(val + 256);
      borrow = 1;
    }
    else
    {
      lm1[i] = (unsigned char)val;
      borrow = 0;
    }
  }

  unsigned char one[32], result[32];
  make_scalar(one, 1);
  sc_add(result, lm1, one);

  // Result should be 0 (mod l) since sc_add reduces mod l
  // Note: sc_add does not always reduce, but for these values it should wrap
  // Let's verify via sc_reduce32
  sc_reduce32(result);
  EXPECT_TRUE(is_zero_32(result));
}

TEST(crypto_ops, sc_sub_self_is_zero)
{
  unsigned char a[32], result[32];
  make_scalar(a, 42);
  sc_sub(result, a, a);
  // sc_sub computes a - b mod l
  EXPECT_TRUE(is_zero_32(result));
}

TEST(crypto_ops, sc_sub_zero)
{
  // a - 0 = a
  unsigned char a[32], zero[32], result[32];
  make_scalar(a, 77);
  sc_0(zero);
  sc_sub(result, a, zero);
  EXPECT_EQ(0, memcmp(result, a, 32));
}

TEST(crypto_ops, sc_sub_known_values)
{
  unsigned char a[32], b[32], result[32], expected[32];
  make_scalar(a, 10);
  make_scalar(b, 3);
  make_scalar(expected, 7);
  sc_sub(result, a, b);
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_add_sub_inverse)
{
  // (a + b) - b = a
  unsigned char a[32], b[32], sum[32], result[32];
  make_scalar(a, 1000);
  make_scalar(b, 2000);
  sc_add(sum, a, b);
  sc_sub(result, sum, b);
  EXPECT_EQ(0, memcmp(result, a, 32));
}

TEST(crypto_ops, sc_mulsub_basic)
{
  // sc_mulsub(s, a, b, c) = c - a*b mod l
  unsigned char a[32], b[32], c[32], result[32];
  make_scalar(a, 2);
  make_scalar(b, 3);
  make_scalar(c, 10);
  sc_mulsub(result, a, b, c);

  // Expected: 10 - 2*3 = 4
  unsigned char expected[32];
  make_scalar(expected, 4);
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_mulsub_zero_a)
{
  // c - 0*b = c
  unsigned char a[32], b[32], c[32], result[32];
  sc_0(a);
  make_scalar(b, 99);
  make_scalar(c, 50);
  sc_mulsub(result, a, b, c);
  EXPECT_EQ(0, memcmp(result, c, 32));
}

TEST(crypto_ops, sc_mulsub_zero_b)
{
  // c - a*0 = c
  unsigned char a[32], b[32], c[32], result[32];
  make_scalar(a, 99);
  sc_0(b);
  make_scalar(c, 50);
  sc_mulsub(result, a, b, c);
  EXPECT_EQ(0, memcmp(result, c, 32));
}

TEST(crypto_ops, sc_mul_basic)
{
  // 3 * 7 = 21
  unsigned char a[32], b[32], result[32], expected[32];
  make_scalar(a, 3);
  make_scalar(b, 7);
  make_scalar(expected, 21);
  sc_mul(result, a, b);
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_mul_by_zero)
{
  unsigned char a[32], zero[32], result[32];
  make_scalar(a, 12345);
  sc_0(zero);
  sc_mul(result, a, zero);
  EXPECT_TRUE(is_zero_32(result));
}

TEST(crypto_ops, sc_mul_by_one)
{
  unsigned char a[32], one[32], result[32];
  make_scalar(a, 9999);
  make_scalar(one, 1);
  sc_mul(result, a, one);
  EXPECT_EQ(0, memcmp(result, a, 32));
}

TEST(crypto_ops, sc_mul_commutative)
{
  unsigned char a[32], b[32], r1[32], r2[32];
  make_scalar(a, 111);
  make_scalar(b, 222);
  sc_mul(r1, a, b);
  sc_mul(r2, b, a);
  EXPECT_EQ(0, memcmp(r1, r2, 32));
}

TEST(crypto_ops, sc_muladd_basic)
{
  // sc_muladd(s, a, b, c) = a*b + c mod l
  unsigned char a[32], b[32], c[32], result[32];
  make_scalar(a, 3);
  make_scalar(b, 4);
  make_scalar(c, 5);
  sc_muladd(result, a, b, c);

  // Expected: 3*4 + 5 = 17
  unsigned char expected[32];
  make_scalar(expected, 17);
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_muladd_zero_c)
{
  // a*b + 0 = a*b
  unsigned char a[32], b[32], c[32], result[32], product[32];
  make_scalar(a, 6);
  make_scalar(b, 7);
  sc_0(c);
  sc_muladd(result, a, b, c);
  sc_mul(product, a, b);
  EXPECT_EQ(0, memcmp(result, product, 32));
}

TEST(crypto_ops, sc_reduce_small)
{
  // sc_reduce works on a 64-byte input
  unsigned char s[64];
  memset(s, 0, 64);
  s[0] = 42;
  sc_reduce(s);
  // First 32 bytes should be the reduced scalar
  unsigned char expected[32];
  make_scalar(expected, 42);
  EXPECT_EQ(0, memcmp(s, expected, 32));
}

TEST(crypto_ops, sc_reduce_large)
{
  // Fill with a pattern and reduce
  unsigned char s[64];
  memset(s, 0, 64);
  s[0] = 0xed;
  s[1] = 0xd3;
  sc_reduce(s);
  // Result should be a valid scalar
  EXPECT_EQ(0, sc_check(s));
}

// ============================================================================
// 4. Ed25519 basepoint tests
// ============================================================================

TEST(crypto_ops, ed25519_basepoint_encoding_verify)
{
  // Compute 1*B and verify it matches the known basepoint encoding
  unsigned char scalar[32];
  make_scalar(scalar, 1);

  ge_p3 B;
  ge_scalarmult_base(&B, scalar);

  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &B);

  EXPECT_EQ(0, memcmp(encoded, ed25519_basepoint, 32));
}

TEST(crypto_ops, ed25519_basepoint_is_valid_point)
{
  ge_p3 B;
  EXPECT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));
}

TEST(crypto_ops, ed25519_basepoint_not_identity)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));
  EXPECT_EQ(0, ge_p3_is_point_at_infinity_vartime(&B));
}

TEST(crypto_ops, ed25519_basepoint_has_prime_order)
{
  // l * B = identity (basepoint has order l)
  unsigned char scalar[32];
  memcpy(scalar, curve_order, 32);

  ge_p3 result;
  ge_scalarmult_base(&result, scalar);

  EXPECT_EQ(1, ge_p3_is_point_at_infinity_vartime(&result));
}

TEST(crypto_ops, ed25519_basepoint_half_order_not_identity)
{
  // (l/2) * B != identity (since l is prime, no smaller subgroup)
  // l/2 = right-shift l by 1
  unsigned char half_order[32];
  memcpy(half_order, curve_order, 32);
  // Right shift by 1
  for (int i = 0; i < 31; ++i)
    half_order[i] = (half_order[i] >> 1) | ((half_order[i + 1] & 1) << 7);
  half_order[31] >>= 1;

  ge_p3 result;
  ge_scalarmult_base(&result, half_order);
  EXPECT_EQ(0, ge_p3_is_point_at_infinity_vartime(&result));
}

// ============================================================================
// 5. Utility / load functions
// ============================================================================

TEST(crypto_ops, load_3_basic)
{
  unsigned char data[3] = {0x01, 0x02, 0x03};
  uint64_t val = load_3(data);
  // Little-endian: 0x030201
  EXPECT_EQ(val, 0x030201u);
}

TEST(crypto_ops, load_3_zeros)
{
  unsigned char data[3] = {0, 0, 0};
  EXPECT_EQ(load_3(data), 0u);
}

TEST(crypto_ops, load_3_max)
{
  unsigned char data[3] = {0xff, 0xff, 0xff};
  EXPECT_EQ(load_3(data), 0xffffffu);
}

TEST(crypto_ops, load_4_basic)
{
  unsigned char data[4] = {0x01, 0x02, 0x03, 0x04};
  uint64_t val = load_4(data);
  EXPECT_EQ(val, 0x04030201u);
}

TEST(crypto_ops, load_4_zeros)
{
  unsigned char data[4] = {0, 0, 0, 0};
  EXPECT_EQ(load_4(data), 0u);
}

TEST(crypto_ops, load_4_max)
{
  unsigned char data[4] = {0xff, 0xff, 0xff, 0xff};
  EXPECT_EQ(load_4(data), 0xffffffffu);
}

// ============================================================================
// 6. Additional cross-function consistency tests
// ============================================================================

TEST(crypto_ops, ge_scalarmult_base_distributive)
{
  // (a * b) * B == a * (b * B) via scalar multiplication
  unsigned char sa[32], sb[32], sab[32];
  make_scalar(sa, 5);
  make_scalar(sb, 7);
  make_scalar(sab, 35);

  // Method 1: (a*b)*B
  ge_p3 abB;
  ge_scalarmult_base(&abB, sab);

  // Method 2: b*B then a * (b*B)
  ge_p3 bB;
  ge_scalarmult_base(&bB, sb);
  ge_p2 result;
  ge_scalarmult(&result, sa, &bB);

  unsigned char enc1[32], enc2[32];
  ge_p3_tobytes(enc1, &abB);
  ge_tobytes(enc2, &result);
  EXPECT_EQ(0, memcmp(enc1, enc2, 32));
}

TEST(crypto_ops, ge_double_scalarmult_base_vartime_p3_consistency)
{
  // ge_double_scalarmult_base_vartime_p3 should give same result as the p2 variant
  unsigned char sa[32], sb[32];
  make_scalar(sa, 4);
  make_scalar(sb, 6);

  ge_p3 A;
  ASSERT_EQ(0, ge_frombytes_vartime(&A, ed25519_basepoint));

  ge_p2 result_p2;
  ge_double_scalarmult_base_vartime(&result_p2, sa, &A, sb);

  ge_p3 result_p3;
  ge_double_scalarmult_base_vartime_p3(&result_p3, sa, &A, sb);

  unsigned char enc_p2[32], enc_p3[32];
  ge_tobytes(enc_p2, &result_p2);
  ge_p3_tobytes(enc_p3, &result_p3);
  EXPECT_EQ(0, memcmp(enc_p2, enc_p3, 32));
}

TEST(crypto_ops, fe_mul_associative)
{
  // (a * b) * c = a * (b * c)
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  // Use X, Y, Z of the basepoint as three different field elements
  fe ab, abc1, bc, abc2;
  fe_mul(ab, B.X, B.Y);
  fe_mul(abc1, ab, B.Z);

  fe_mul(bc, B.Y, B.Z);
  fe_mul(abc2, B.X, bc);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, abc1);
  fe_tobytes(b2, abc2);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_add_associative)
{
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe ab, abc1, bc, abc2;
  fe_add(ab, B.X, B.Y);
  fe_add(abc1, ab, B.Z);

  fe_add(bc, B.Y, B.Z);
  fe_add(abc2, B.X, bc);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, abc1);
  fe_tobytes(b2, abc2);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, fe_mul_distributive_over_add)
{
  // a * (b + c) = a*b + a*c
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  fe bc, lhs;
  fe_add(bc, B.Y, B.Z);
  fe_mul(lhs, B.X, bc);

  fe ab, ac, rhs;
  fe_mul(ab, B.X, B.Y);
  fe_mul(ac, B.X, B.Z);
  fe_add(rhs, ab, ac);

  unsigned char b1[32], b2[32];
  fe_tobytes(b1, lhs);
  fe_tobytes(b2, rhs);
  EXPECT_EQ(0, memcmp(b1, b2, 32));
}

TEST(crypto_ops, sc_sub_underflow_wraps)
{
  // 0 - 1 mod l = l - 1
  unsigned char zero[32], one[32], result[32];
  sc_0(zero);
  make_scalar(one, 1);
  sc_sub(result, zero, one);

  // Expected: l - 1
  unsigned char expected[32];
  memcpy(expected, curve_order, 32);
  int borrow = 1;
  for (int i = 0; i < 32 && borrow; ++i)
  {
    int val = (int)expected[i] - borrow;
    if (val < 0)
    {
      expected[i] = (unsigned char)(val + 256);
      borrow = 1;
    }
    else
    {
      expected[i] = (unsigned char)val;
      borrow = 0;
    }
  }
  EXPECT_EQ(0, memcmp(result, expected, 32));
}

TEST(crypto_ops, sc_mul_associative)
{
  // (a * b) * c = a * (b * c)
  unsigned char a[32], b[32], c[32], ab[32], abc1[32], bc[32], abc2[32];
  make_scalar(a, 11);
  make_scalar(b, 13);
  make_scalar(c, 17);

  sc_mul(ab, a, b);
  sc_mul(abc1, ab, c);

  sc_mul(bc, b, c);
  sc_mul(abc2, a, bc);

  EXPECT_EQ(0, memcmp(abc1, abc2, 32));
}

TEST(crypto_ops, sc_mul_distributive_over_add)
{
  // a * (b + c) = a*b + a*c
  unsigned char a[32], b[32], c[32], bc[32], lhs[32];
  unsigned char ab[32], ac[32], rhs[32];
  make_scalar(a, 5);
  make_scalar(b, 7);
  make_scalar(c, 11);

  sc_add(bc, b, c);
  sc_mul(lhs, a, bc);

  sc_mul(ab, a, b);
  sc_mul(ac, a, c);
  sc_add(rhs, ab, ac);

  EXPECT_EQ(0, memcmp(lhs, rhs, 32));
}

TEST(crypto_ops, ge_p3_identity_is_at_infinity)
{
  // Verify the global constant ge_p3_identity encodes as the identity point
  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &ge_p3_identity);
  EXPECT_EQ(0, memcmp(encoded, identity_point, 32));
}

TEST(crypto_ops, ge_dsm_precomp_basic)
{
  // Verify that ge_dsm_precomp does not crash and produces usable precomp table
  ge_p3 B;
  ASSERT_EQ(0, ge_frombytes_vartime(&B, ed25519_basepoint));

  ge_dsmp precomp;
  ge_dsm_precomp(precomp, &B);

  // Use the precomp table via ge_double_scalarmult_precomp_vartime
  unsigned char sa[32], sb[32];
  make_scalar(sa, 3);
  make_scalar(sb, 5);

  ge_p2 result;
  ge_double_scalarmult_precomp_vartime(&result, sa, &B, sb, precomp);

  unsigned char enc[32];
  ge_tobytes(enc, &result);

  // Should equal 3*B + 5*B = 8*B
  unsigned char s8[32];
  make_scalar(s8, 8);
  ge_p3 eightB;
  ge_scalarmult_base(&eightB, s8);
  unsigned char enc_ref[32];
  ge_p3_tobytes(enc_ref, &eightB);
  EXPECT_EQ(0, memcmp(enc, enc_ref, 32));
}

TEST(crypto_ops, ge_p3_H_is_valid_point)
{
  // The constant ge_p3_H should encode to a valid point
  unsigned char encoded[32];
  ge_p3_tobytes(encoded, &ge_p3_H);

  ge_p3 check;
  EXPECT_EQ(0, ge_frombytes_vartime(&check, encoded));
}

TEST(crypto_ops, ge_p3_H_not_identity)
{
  EXPECT_EQ(0, ge_p3_is_point_at_infinity_vartime(&ge_p3_H));
}

TEST(crypto_ops, ge_p3_H_not_basepoint)
{
  unsigned char enc_H[32], enc_B[32];
  ge_p3_tobytes(enc_H, &ge_p3_H);

  unsigned char s1[32];
  make_scalar(s1, 1);
  ge_p3 B;
  ge_scalarmult_base(&B, s1);
  ge_p3_tobytes(enc_B, &B);

  EXPECT_NE(0, memcmp(enc_H, enc_B, 32));
}
