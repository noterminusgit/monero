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

#include <cstdint>
#include <cstring>

#include "ringct/rctTypes.h"
#include "ringct/rctSigs.h"
#include "ringct/rctOps.h"

using namespace rct;

// ============================================================================
// Key generation tests
// ============================================================================

TEST(RingCTOps, skGen_produces_non_zero_key)
{
    key sk = skGen();
    ASSERT_NE(sk, zero());
}

TEST(RingCTOps, skGen_different_each_call)
{
    key sk1 = skGen();
    key sk2 = skGen();
    ASSERT_NE(sk1, sk2);
}

TEST(RingCTOps, skGen_void_overload)
{
    key sk;
    skGen(sk);
    ASSERT_NE(sk, zero());
}

TEST(RingCTOps, pkGen_produces_valid_curve_point)
{
    key pk = pkGen();
    // A valid curve point can be deserialized with ge_frombytes_vartime
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, pk.bytes), 0);
}

TEST(RingCTOps, pkGen_different_each_call)
{
    key pk1 = pkGen();
    key pk2 = pkGen();
    ASSERT_NE(pk1, pk2);
}

TEST(RingCTOps, pkGen_not_identity)
{
    key pk = pkGen();
    ASSERT_NE(pk, identity());
}

TEST(RingCTOps, skpkGen_matching_keypair)
{
    key sk, pk;
    skpkGen(sk, pk);

    // pk should equal sk * G
    key pk_check = scalarmultBase(sk);
    ASSERT_EQ(pk, pk_check);
}

TEST(RingCTOps, skpkGen_tuple_overload)
{
    auto [sk, pk] = skpkGen();

    key pk_check = scalarmultBase(sk);
    ASSERT_EQ(pk, pk_check);
}

TEST(RingCTOps, skpkGen_sk_is_non_zero)
{
    key sk, pk;
    skpkGen(sk, pk);
    ASSERT_NE(sk, zero());
}

TEST(RingCTOps, skvGen_correct_size)
{
    for (size_t n : {1, 5, 10, 32})
    {
        keyV v = skvGen(n);
        ASSERT_EQ(v.size(), n);
    }
}

TEST(RingCTOps, skvGen_all_non_zero)
{
    keyV v = skvGen(10);
    for (const auto &k : v)
    {
        ASSERT_NE(k, zero());
    }
}

TEST(RingCTOps, skvGen_all_distinct)
{
    keyV v = skvGen(10);
    for (size_t i = 0; i < v.size(); ++i)
    {
        for (size_t j = i + 1; j < v.size(); ++j)
        {
            ASSERT_NE(v[i], v[j]);
        }
    }
}

// ============================================================================
// Scalar/point operation tests
// ============================================================================

TEST(RingCTOps, scalarmultBase_consistency)
{
    // Generating the public key from a secret key with scalarmultBase
    // should produce the same result whether we use the void or return overload
    key sk = skGen();
    key pk1 = scalarmultBase(sk);
    key pk2;
    scalarmultBase(pk2, sk);
    ASSERT_EQ(pk1, pk2);
}

TEST(RingCTOps, scalarmultBase_different_scalars_give_different_points)
{
    key sk1 = skGen();
    key sk2 = skGen();
    key pk1 = scalarmultBase(sk1);
    key pk2 = scalarmultBase(sk2);
    ASSERT_NE(pk1, pk2);
}

TEST(RingCTOps, scalarmultKey_with_one_is_identity_mult)
{
    // scalarmultKey(P, 1) should return P
    key pk = pkGen();
    key one = identity(); // identity in scalar terms is 1 = {0x01, 0x00, ...}
    key result = scalarmultKey(pk, one);
    ASSERT_EQ(result, pk);
}

TEST(RingCTOps, scalarmultKey_with_zero_gives_identity)
{
    key pk = pkGen();
    key result = scalarmultKey(pk, zero());
    ASSERT_EQ(result, identity());
}

TEST(RingCTOps, scalarmultKey_void_overload)
{
    key pk = pkGen();
    key scalar = skGen();
    key result1 = scalarmultKey(pk, scalar);
    key result2;
    scalarmultKey(result2, pk, scalar);
    ASSERT_EQ(result1, result2);
}

TEST(RingCTOps, scalarmult8_produces_valid_point)
{
    key pk = pkGen();
    key result = scalarmult8(pk);
    // Result should be a valid curve point
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, result.bytes), 0);
}

TEST(RingCTOps, scalarmult8_equals_scalarmultKey_by_eight)
{
    key pk = pkGen();
    key result8 = scalarmult8(pk);
    key resultKey = scalarmultKey(pk, EIGHT);
    ASSERT_EQ(result8, resultKey);
}

TEST(RingCTOps, scalarmult8_p3_overload)
{
    key pk = pkGen();
    ge_p3 p3;
    scalarmult8(p3, pk);
    key result_from_p3;
    ge_p3_tobytes(result_from_p3.bytes, &p3);
    key result_from_key = scalarmult8(pk);
    ASSERT_EQ(result_from_p3, result_from_key);
}

TEST(RingCTOps, addKeys_commutativity)
{
    key a = scalarmultBase(skGen());
    key b = scalarmultBase(skGen());
    key ab, ba;
    addKeys(ab, a, b);
    addKeys(ba, b, a);
    ASSERT_EQ(ab, ba);
}

TEST(RingCTOps, addKeys_return_overload)
{
    key a = scalarmultBase(skGen());
    key b = scalarmultBase(skGen());
    key ab_void;
    addKeys(ab_void, a, b);
    key ab_ret = addKeys(a, b);
    ASSERT_EQ(ab_void, ab_ret);
}

TEST(RingCTOps, addKeys_with_identity)
{
    // A + identity == A
    key a = scalarmultBase(skGen());
    key result = addKeys(a, identity());
    ASSERT_EQ(result, a);
}

TEST(RingCTOps, addKeys_vector_overload)
{
    keyV keys;
    key sum_manual = identity();
    for (int i = 0; i < 5; ++i)
    {
        key pt = scalarmultBase(skGen());
        keys.push_back(pt);
        addKeys(sum_manual, sum_manual, pt);
    }
    key sum_vec = addKeys(keys);
    ASSERT_EQ(sum_vec, sum_manual);
}

TEST(RingCTOps, subKeys_self_gives_identity)
{
    key a = scalarmultBase(skGen());
    key result;
    subKeys(result, a, a);
    ASSERT_EQ(result, identity());
}

TEST(RingCTOps, subKeys_inverse_of_addKeys)
{
    key a = scalarmultBase(skGen());
    key b = scalarmultBase(skGen());
    key sum = addKeys(a, b);
    key diff;
    subKeys(diff, sum, b);
    ASSERT_EQ(diff, a);
}

TEST(RingCTOps, equalKeys_reflexive)
{
    key k = skGen();
    ASSERT_TRUE(equalKeys(k, k));
}

TEST(RingCTOps, equalKeys_distinct_keys)
{
    key a = skGen();
    key b = skGen();
    ASSERT_FALSE(equalKeys(a, b));
}

TEST(RingCTOps, equalKeys_zero)
{
    key z = zero();
    key z2 = zero();
    ASSERT_TRUE(equalKeys(z, z2));
}

TEST(RingCTOps, equalKeys_copy)
{
    key original = skGen();
    key copied = copy(original);
    ASSERT_TRUE(equalKeys(original, copied));
}

TEST(RingCTOps, addKeys1_matches_manual)
{
    // addKeys1(aGB, a, B) computes aG + B
    key a = skGen();
    key B = scalarmultBase(skGen());
    key aG = scalarmultBase(a);

    key result_addKeys1;
    addKeys1(result_addKeys1, a, B);

    key result_manual = addKeys(aG, B);
    ASSERT_EQ(result_addKeys1, result_manual);
}

TEST(RingCTOps, addKeys2_matches_manual)
{
    // addKeys2(aGbB, a, b, B) computes aG + bB
    key a = skGen();
    key b = skGen();
    key B = scalarmultBase(skGen());

    key result;
    addKeys2(result, a, b, B);

    key aG = scalarmultBase(a);
    key bB = scalarmultKey(B, b);
    key manual = addKeys(aG, bB);
    ASSERT_EQ(result, manual);
}

// ============================================================================
// Commitment operation tests
// ============================================================================

TEST(RingCTOps, commit_with_zero_mask_equals_zeroCommit)
{
    // commit(amount, zero) should compute 0*G + amount*H = amount*H
    // zeroCommit(amount) computes G + amount*H (where G is the basepoint, i.e. mask=1)
    // They differ because zeroCommit uses mask=1 (identity scalar), not mask=0.
    // But commit(amount, identity()) should equal zeroCommit(amount).
    for (xmr_amount amount : {0ULL, 1ULL, 100ULL, 1000000000ULL})
    {
        key c = commit(amount, identity());
        key z = zeroCommit(amount);
        ASSERT_EQ(c, z);
    }
}

TEST(RingCTOps, genC_produces_valid_commitment)
{
    key a = skGen();
    xmr_amount amount = 42;
    key C;
    genC(C, a, amount);

    // C should be a valid curve point
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, C.bytes), 0);

    // C should equal commit(amount, a)
    key c2 = commit(amount, a);
    ASSERT_EQ(C, c2);
}

TEST(RingCTOps, genC_different_masks_different_commitments)
{
    xmr_amount amount = 42;
    key a1 = skGen();
    key a2 = skGen();
    key C1, C2;
    genC(C1, a1, amount);
    genC(C2, a2, amount);
    ASSERT_NE(C1, C2);
}

TEST(RingCTOps, zeroCommit_deterministic)
{
    for (xmr_amount amount : {0ULL, 1ULL, 42ULL, 1000000000ULL, 0xFFFFFFFFFFFFFFFFULL})
    {
        key c1 = zeroCommit(amount);
        key c2 = zeroCommit(amount);
        ASSERT_EQ(c1, c2);
    }
}

TEST(RingCTOps, zeroCommit_different_amounts_different_results)
{
    key c1 = zeroCommit(0);
    key c2 = zeroCommit(1);
    key c3 = zeroCommit(100);
    ASSERT_NE(c1, c2);
    ASSERT_NE(c2, c3);
    ASSERT_NE(c1, c3);
}

TEST(RingCTOps, commit_is_additive_in_mask)
{
    // commit(amount, a) + commit(0, b) == commit(amount, a+b)
    // because (aG + amtH) + (bG + 0H) = (a+b)G + amtH
    xmr_amount amount = 100;
    key a = skGen();
    key b = skGen();
    key ab;
    sc_add(ab.bytes, a.bytes, b.bytes);

    key ca = commit(amount, a);
    key cb = commit(0, b);
    key sum = addKeys(ca, cb);
    key cab = commit(amount, ab);
    ASSERT_EQ(sum, cab);
}

TEST(RingCTOps, commit_is_additive_in_amount)
{
    // commit(a1, mask) + commit(a2, mask2) should equal a commitment to a1+a2
    // with combined masks: (m1*G + a1*H) + (m2*G + a2*H) = (m1+m2)*G + (a1+a2)*H
    xmr_amount a1 = 100, a2 = 200;
    key m1 = skGen();
    key m2 = skGen();

    key c1 = commit(a1, m1);
    key c2 = commit(a2, m2);
    key sum = addKeys(c1, c2);

    key m_combined;
    sc_add(m_combined.bytes, m1.bytes, m2.bytes);
    key c_combined = commit(a1 + a2, m_combined);
    ASSERT_EQ(sum, c_combined);
}

// ============================================================================
// ECDH encode/decode tests
// ============================================================================

TEST(RingCTOps, ecdhEncode_decode_roundtrip_v2_true)
{
    ecdhTuple original;
    original.mask = skGen();
    original.amount = skGen();
    // v2 mode only uses first 8 bytes of amount
    memset(original.amount.bytes + 8, 0, 24);
    key sharedSec = skGen();

    ecdhTuple encoded = original;
    ecdhEncode(encoded, sharedSec, true);
    ecdhDecode(encoded, sharedSec, true);

    // First 8 bytes of amount should roundtrip
    ASSERT_EQ(memcmp(original.amount.bytes, encoded.amount.bytes, 8), 0);
}

TEST(RingCTOps, ecdhEncode_decode_roundtrip_v2_false)
{
    ecdhTuple original;
    original.mask = skGen();
    original.amount = skGen();
    key sharedSec = skGen();

    ecdhTuple encoded = original;
    ecdhEncode(encoded, sharedSec, false);
    ecdhDecode(encoded, sharedSec, false);

    ASSERT_EQ(original.mask, encoded.mask);
    ASSERT_EQ(original.amount, encoded.amount);
}

TEST(RingCTOps, ecdhEncode_changes_values)
{
    ecdhTuple original;
    original.mask = skGen();
    original.amount = skGen();
    key sharedSec = skGen();

    ecdhTuple encoded = original;
    ecdhEncode(encoded, sharedSec, false);

    // After encoding, values should differ from original
    ASSERT_NE(original.mask, encoded.mask);
}

TEST(RingCTOps, ecdhEncode_different_secrets_different_results)
{
    ecdhTuple original;
    original.mask = skGen();
    original.amount = skGen();

    key sec1 = skGen();
    key sec2 = skGen();

    ecdhTuple enc1 = original;
    ecdhTuple enc2 = original;
    ecdhEncode(enc1, sec1, false);
    ecdhEncode(enc2, sec2, false);

    ASSERT_NE(enc1.mask, enc2.mask);
}

TEST(RingCTOps, ecdhEncode_wrong_secret_fails_decode)
{
    ecdhTuple original;
    original.mask = skGen();
    original.amount = skGen();
    key correctSec = skGen();
    key wrongSec = skGen();

    ecdhTuple encoded = original;
    ecdhEncode(encoded, correctSec, false);
    ecdhDecode(encoded, wrongSec, false);

    ASSERT_NE(original.mask, encoded.mask);
    ASSERT_NE(original.amount, encoded.amount);
}

TEST(RingCTOps, ecdhEncode_v2_mask_is_commitment_mask)
{
    // In v2 mode, the decoded mask should be genCommitmentMask(sharedSec)
    key sharedSec = skGen();
    ecdhTuple tuple;
    tuple.mask = zero();
    tuple.amount = zero();

    ecdhTuple encoded = tuple;
    ecdhEncode(encoded, sharedSec, true);
    ecdhDecode(encoded, sharedSec, true);

    key expectedMask = genCommitmentMask(sharedSec);
    ASSERT_EQ(encoded.mask, expectedMask);
}

// ============================================================================
// Hash operation tests
// ============================================================================

TEST(RingCTOps, cn_fast_hash_deterministic)
{
    key input = skGen();
    key hash1 = cn_fast_hash(input);
    key hash2 = cn_fast_hash(input);
    ASSERT_EQ(hash1, hash2);
}

TEST(RingCTOps, cn_fast_hash_different_inputs_different_hashes)
{
    key in1 = skGen();
    key in2 = skGen();
    key h1 = cn_fast_hash(in1);
    key h2 = cn_fast_hash(in2);
    ASSERT_NE(h1, h2);
}

TEST(RingCTOps, cn_fast_hash_not_identity)
{
    key input = skGen();
    key hash = cn_fast_hash(input);
    ASSERT_NE(hash, identity());
    ASSERT_NE(hash, zero());
}

TEST(RingCTOps, cn_fast_hash_void_overload)
{
    key input = skGen();
    key h1 = cn_fast_hash(input);
    key h2;
    cn_fast_hash(h2, input);
    ASSERT_EQ(h1, h2);
}

TEST(RingCTOps, cn_fast_hash_arbitrary_data)
{
    key h1, h2;
    unsigned char data1[64] = {0};
    unsigned char data2[64] = {0};
    data2[0] = 1;
    cn_fast_hash(h1, data1, 64);
    cn_fast_hash(h2, data2, 64);
    ASSERT_NE(h1, h2);
}

TEST(RingCTOps, cn_fast_hash_keyV_overload)
{
    keyV keys;
    keys.push_back(skGen());
    keys.push_back(skGen());
    key h1 = cn_fast_hash(keys);
    key h2 = cn_fast_hash(keys);
    ASSERT_EQ(h1, h2);
    ASSERT_NE(h1, zero());
}

TEST(RingCTOps, hash_to_scalar_produces_valid_scalar)
{
    key input = skGen();
    key scalar = hash_to_scalar(input);

    // A valid scalar must be less than the curve order L.
    // After sc_reduce32, the result should be the same if it was already reduced.
    key reduced;
    memcpy(&reduced, &scalar, 32);
    sc_reduce32(reduced.bytes);
    ASSERT_EQ(scalar, reduced);
}

TEST(RingCTOps, hash_to_scalar_deterministic)
{
    key input = skGen();
    key s1 = hash_to_scalar(input);
    key s2 = hash_to_scalar(input);
    ASSERT_EQ(s1, s2);
}

TEST(RingCTOps, hash_to_scalar_different_inputs)
{
    key in1 = skGen();
    key in2 = skGen();
    key s1 = hash_to_scalar(in1);
    key s2 = hash_to_scalar(in2);
    ASSERT_NE(s1, s2);
}

TEST(RingCTOps, hash_to_scalar_void_overload)
{
    key input = skGen();
    key s1 = hash_to_scalar(input);
    key s2;
    hash_to_scalar(s2, input);
    ASSERT_EQ(s1, s2);
}

TEST(RingCTOps, hash_to_scalar_arbitrary_data)
{
    unsigned char data[100];
    memset(data, 0x42, sizeof(data));
    key h;
    hash_to_scalar(h, data, sizeof(data));
    // Result should be reduced
    key reduced;
    memcpy(&reduced, &h, 32);
    sc_reduce32(reduced.bytes);
    ASSERT_EQ(h, reduced);
}

TEST(RingCTOps, hash_to_p3_produces_valid_point)
{
    key input = skGen();
    ge_p3 p3;
    hash_to_p3(p3, input);

    // Convert to bytes and check it's a valid point
    key point;
    ge_p3_tobytes(point.bytes, &p3);
    ge_p3 p3_check;
    ASSERT_EQ(ge_frombytes_vartime(&p3_check, point.bytes), 0);
}

TEST(RingCTOps, hash_to_p3_deterministic)
{
    key input = skGen();
    ge_p3 p3_1, p3_2;
    hash_to_p3(p3_1, input);
    hash_to_p3(p3_2, input);

    key pt1, pt2;
    ge_p3_tobytes(pt1.bytes, &p3_1);
    ge_p3_tobytes(pt2.bytes, &p3_2);
    ASSERT_EQ(pt1, pt2);
}

TEST(RingCTOps, hash_to_p3_different_inputs_different_points)
{
    key in1 = skGen();
    key in2 = skGen();
    ge_p3 p3_1, p3_2;
    hash_to_p3(p3_1, in1);
    hash_to_p3(p3_2, in2);

    key pt1, pt2;
    ge_p3_tobytes(pt1.bytes, &p3_1);
    ge_p3_tobytes(pt2.bytes, &p3_2);
    ASSERT_NE(pt1, pt2);
}

TEST(RingCTOps, hash_to_p3_not_identity)
{
    key input = skGen();
    ge_p3 p3;
    hash_to_p3(p3, input);

    key point;
    ge_p3_tobytes(point.bytes, &p3);
    ASSERT_NE(point, identity());
}

// ============================================================================
// Subgroup check tests
// ============================================================================

TEST(RingCTOps, isInMainSubgroup_for_pkGen_point)
{
    // Points generated by pkGen should be in the main subgroup
    // pkGen generates a random scalar and multiplies by G, so the result
    // is always in the main subgroup
    key pk = scalarmultBase(skGen());
    ASSERT_TRUE(isInMainSubgroup(pk));
}

TEST(RingCTOps, isInMainSubgroup_for_scalarmultBase)
{
    for (int i = 0; i < 5; ++i)
    {
        key sk = skGen();
        key pk = scalarmultBase(sk);
        ASSERT_TRUE(isInMainSubgroup(pk));
    }
}

TEST(RingCTOps, isInMainSubgroup_identity_point)
{
    // The identity point (0,1) has order 1 which divides L, so it is
    // in the main subgroup.
    ASSERT_TRUE(isInMainSubgroup(identity()));
}

TEST(RingCTOps, isInMainSubgroup_basepoint_H)
{
    ASSERT_TRUE(isInMainSubgroup(H));
}

TEST(RingCTOps, isInMainSubgroup_basepoint_G)
{
    ASSERT_TRUE(isInMainSubgroup(G));
}

TEST(RingCTOps, isInMainSubgroup_scalarmultH)
{
    key sk = skGen();
    key point = scalarmultH(sk);
    ASSERT_TRUE(isInMainSubgroup(point));
}

// ============================================================================
// Key matrix tests
// ============================================================================

TEST(RingCTOps, keyMInit_correct_dimensions)
{
    size_t rows = 3;
    size_t cols = 5;
    keyM m = keyMInit(rows, cols);

    // keyMInit creates a matrix indexed by column first
    ASSERT_EQ(m.size(), cols);
    for (size_t c = 0; c < cols; ++c)
    {
        ASSERT_EQ(m[c].size(), rows);
    }
}

TEST(RingCTOps, keyMInit_single_cell)
{
    keyM m = keyMInit(1, 1);
    ASSERT_EQ(m.size(), 1u);
    ASSERT_EQ(m[0].size(), 1u);
}

TEST(RingCTOps, keyMInit_large_dimensions)
{
    keyM m = keyMInit(10, 20);
    ASSERT_EQ(m.size(), 20u);
    for (size_t c = 0; c < 20; ++c)
    {
        ASSERT_EQ(m[c].size(), 10u);
    }
}

TEST(RingCTOps, keyMInit_zero_rows)
{
    keyM m = keyMInit(0, 5);
    ASSERT_EQ(m.size(), 5u);
    for (size_t c = 0; c < 5; ++c)
    {
        ASSERT_EQ(m[c].size(), 0u);
    }
}

TEST(RingCTOps, keyMInit_zero_cols)
{
    keyM m = keyMInit(5, 0);
    ASSERT_EQ(m.size(), 0u);
}

// ============================================================================
// Range proof tests
// ============================================================================

TEST(RingCTOps, proveRange_verRange_basic_roundtrip)
{
    key C, mask;
    xmr_amount amount = 12345;
    rangeSig sig = proveRange(C, mask, amount);
    ASSERT_TRUE(verRange(C, sig));
}

TEST(RingCTOps, proveRange_verRange_zero_amount)
{
    key C, mask;
    rangeSig sig = proveRange(C, mask, 0);
    ASSERT_TRUE(verRange(C, sig));
}

TEST(RingCTOps, proveRange_verRange_one)
{
    key C, mask;
    rangeSig sig = proveRange(C, mask, 1);
    ASSERT_TRUE(verRange(C, sig));
}

TEST(RingCTOps, proveRange_commitment_is_valid_point)
{
    key C, mask;
    proveRange(C, mask, 42);
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, C.bytes), 0);
}

TEST(RingCTOps, proveRange_commitment_matches_commit)
{
    key C, mask;
    xmr_amount amount = 7777;
    proveRange(C, mask, amount);

    // The commitment C should equal commit(amount, mask)
    key c_check = commit(amount, mask);
    ASSERT_EQ(C, c_check);
}

TEST(RingCTOps, proveRange_tampered_commitment_fails)
{
    key C, mask;
    rangeSig sig = proveRange(C, mask, 100);

    // Tamper with the commitment
    key C_bad = scalarmultBase(skGen());
    ASSERT_FALSE(verRange(C_bad, sig));
}

TEST(RingCTOps, proveRange_is_non_deterministic)
{
    key C1, mask1, C2, mask2;
    xmr_amount amount = 500;
    rangeSig sig1 = proveRange(C1, mask1, amount);
    rangeSig sig2 = proveRange(C2, mask2, amount);

    // Same amount but different random masks => different commitments
    ASSERT_NE(C1, C2);
    ASSERT_NE(mask1, mask2);
    // Verify both proofs are valid
    ASSERT_TRUE(verRange(C1, sig1));
    ASSERT_TRUE(verRange(C2, sig2));
}

// ============================================================================
// Utility / copy / zero / identity tests
// ============================================================================

TEST(RingCTOps, copy_function)
{
    key original = skGen();
    key copied = copy(original);
    ASSERT_EQ(original, copied);
    ASSERT_TRUE(equalKeys(original, copied));
}

TEST(RingCTOps, copy_void_overload)
{
    key original = skGen();
    key copied;
    copy(copied, original);
    ASSERT_EQ(original, copied);
}

TEST(RingCTOps, zero_function)
{
    key z = zero();
    for (int i = 0; i < 32; ++i)
    {
        ASSERT_EQ(z.bytes[i], 0);
    }
}

TEST(RingCTOps, zero_void_overload)
{
    key k = skGen();
    zero(k);
    for (int i = 0; i < 32; ++i)
    {
        ASSERT_EQ(k.bytes[i], 0);
    }
}

TEST(RingCTOps, identity_function)
{
    key id = identity();
    ASSERT_EQ(id.bytes[0], 0x01);
    for (int i = 1; i < 32; ++i)
    {
        ASSERT_EQ(id.bytes[i], 0);
    }
}

TEST(RingCTOps, identity_void_overload)
{
    key id;
    identity(id);
    key id2 = identity();
    ASSERT_EQ(id, id2);
}

TEST(RingCTOps, curveOrder_function)
{
    key l = curveOrder();
    ASSERT_EQ(l, L);
}

TEST(RingCTOps, curveOrder_void_overload)
{
    key l;
    curveOrder(l);
    ASSERT_EQ(l, L);
}

// ============================================================================
// scalarmultH tests
// ============================================================================

TEST(RingCTOps, scalarmultH_zero_gives_identity)
{
    key result = scalarmultH(zero());
    ASSERT_EQ(result, identity());
}

TEST(RingCTOps, scalarmultH_one_gives_H)
{
    key result = scalarmultH(identity()); // identity = scalar 1
    ASSERT_EQ(result, H);
}

TEST(RingCTOps, scalarmultH_produces_valid_point)
{
    key scalar = skGen();
    key result = scalarmultH(scalar);
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, result.bytes), 0);
}

// ============================================================================
// precomp / addKeys3 tests
// ============================================================================

TEST(RingCTOps, precomp_addKeys3_matches_manual)
{
    // addKeys3(aAbB) = a*A + b*B
    key a = skGen();
    key b = skGen();
    key A = scalarmultBase(skGen());
    key B = scalarmultBase(skGen());

    ge_dsmp B_precomp;
    precomp(B_precomp, B);

    key result;
    addKeys3(result, a, A, b, B_precomp);

    key aA = scalarmultKey(A, a);
    key bB = scalarmultKey(B, b);
    key manual = addKeys(aA, bB);
    ASSERT_EQ(result, manual);
}

// ============================================================================
// genAmountEncodingFactor / genCommitmentMask tests
// ============================================================================

TEST(RingCTOps, genAmountEncodingFactor_deterministic)
{
    key k = skGen();
    key f1 = genAmountEncodingFactor(k);
    key f2 = genAmountEncodingFactor(k);
    ASSERT_EQ(f1, f2);
}

TEST(RingCTOps, genAmountEncodingFactor_different_keys)
{
    key k1 = skGen();
    key k2 = skGen();
    key f1 = genAmountEncodingFactor(k1);
    key f2 = genAmountEncodingFactor(k2);
    ASSERT_NE(f1, f2);
}

TEST(RingCTOps, genCommitmentMask_deterministic)
{
    key sk = skGen();
    key m1 = genCommitmentMask(sk);
    key m2 = genCommitmentMask(sk);
    ASSERT_EQ(m1, m2);
}

TEST(RingCTOps, genCommitmentMask_different_keys)
{
    key k1 = skGen();
    key k2 = skGen();
    key m1 = genCommitmentMask(k1);
    key m2 = genCommitmentMask(k2);
    ASSERT_NE(m1, m2);
}

TEST(RingCTOps, genCommitmentMask_is_valid_scalar)
{
    key sk = skGen();
    key mask = genCommitmentMask(sk);
    key reduced;
    memcpy(&reduced, &mask, 32);
    sc_reduce32(reduced.bytes);
    ASSERT_EQ(mask, reduced);
}

// ============================================================================
// addKeys vector sum tests
// ============================================================================

TEST(RingCTOps, addKeys_vector_sum)
{
    keyV keys;
    key sum_manual = identity();
    for (int i = 0; i < 5; ++i)
    {
        key pt = scalarmultBase(skGen());
        keys.push_back(pt);
        addKeys(sum_manual, sum_manual, pt);
    }

    key sum = addKeys(keys);
    ASSERT_EQ(sum, sum_manual);
}

TEST(RingCTOps, addKeys_vector_single_element)
{
    keyV keys;
    key pt = scalarmultBase(skGen());
    keys.push_back(pt);

    key sum = addKeys(keys);
    // Sum of a single element via the vector overload
    key expected = addKeys(identity(), pt);
    ASSERT_EQ(sum, expected);
}

// ============================================================================
// d2h / h2d roundtrip tests
// ============================================================================

TEST(RingCTOps, d2h_h2d_roundtrip_various)
{
    for (uint64_t v : {0ULL, 1ULL, 255ULL, 256ULL, 65535ULL, 1000000000ULL,
                       0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL})
    {
        key k = d2h(v);
        uint64_t recovered = h2d(k);
        ASSERT_EQ(v, recovered);
    }
}

TEST(RingCTOps, d2h_zero_is_zero_key)
{
    key k = d2h(0);
    ASSERT_EQ(k, zero());
}

TEST(RingCTOps, d2h_different_values_different_keys)
{
    key k1 = d2h(1);
    key k2 = d2h(2);
    ASSERT_NE(k1, k2);
}

// ============================================================================
// randXmrAmount tests
// ============================================================================

TEST(RingCTOps, randXmrAmount_within_bounds)
{
    for (int i = 0; i < 20; ++i)
    {
        xmr_amount limit = 1000;
        xmr_amount val = randXmrAmount(limit);
        ASSERT_LT(val, limit);
    }
}

// ============================================================================
// ctskpkGen tests
// ============================================================================

TEST(RingCTOps, ctskpkGen_produces_valid_commitment)
{
    xmr_amount amount = 12345;
    auto [sk, pk] = ctskpkGen(amount);

    // pk.dest should be a valid curve point
    ge_p3 p3;
    ASSERT_EQ(ge_frombytes_vartime(&p3, pk.dest.bytes), 0);
    ASSERT_EQ(ge_frombytes_vartime(&p3, pk.mask.bytes), 0);

    // sk.dest * G == pk.dest
    key pk_check = scalarmultBase(sk.dest);
    ASSERT_EQ(pk_check, pk.dest);
}

// ============================================================================
// toPointCheckOrder tests
// ============================================================================

TEST(RingCTOps, toPointCheckOrder_valid_point)
{
    key pk = scalarmultBase(skGen());
    ge_p3 p3;
    ASSERT_TRUE(toPointCheckOrder(&p3, pk.bytes));
}

TEST(RingCTOps, toPointCheckOrder_invalid_bytes)
{
    // All 0xFF bytes is not a valid point
    unsigned char invalid[32];
    memset(invalid, 0xFF, 32);
    ge_p3 p3;
    ASSERT_FALSE(toPointCheckOrder(&p3, invalid));
}

// ============================================================================
// Key operator tests
// ============================================================================

TEST(RingCTOps, key_equality_operator)
{
    key a = skGen();
    key b = copy(a);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
}

TEST(RingCTOps, key_inequality_operator)
{
    key a = skGen();
    key b = skGen();
    ASSERT_TRUE(a != b);
    ASSERT_FALSE(a == b);
}

TEST(RingCTOps, key_index_operator)
{
    key k = zero();
    k[0] = 0x42;
    ASSERT_EQ(k[0], 0x42);
    ASSERT_EQ(k[1], 0x00);
}

// ============================================================================
// cn_fast_hash128 / hash_to_scalar128 tests
// ============================================================================

TEST(RingCTOps, cn_fast_hash128_deterministic)
{
    unsigned char data[128];
    memset(data, 0xAB, sizeof(data));
    key h1 = cn_fast_hash128(data);
    key h2 = cn_fast_hash128(data);
    ASSERT_EQ(h1, h2);
}

TEST(RingCTOps, hash_to_scalar128_is_reduced)
{
    unsigned char data[128];
    memset(data, 0xCD, sizeof(data));
    key s = hash_to_scalar128(data);
    key reduced;
    memcpy(&reduced, &s, 32);
    sc_reduce32(reduced.bytes);
    ASSERT_EQ(s, reduced);
}

// ============================================================================
// cn_fast_hash / hash_to_scalar for ctkeyV
// ============================================================================

TEST(RingCTOps, cn_fast_hash_ctkeyV_deterministic)
{
    ctkeyV pc;
    ctkey ct;
    ct.dest = scalarmultBase(skGen());
    ct.mask = scalarmultBase(skGen());
    pc.push_back(ct);
    ct.dest = scalarmultBase(skGen());
    ct.mask = scalarmultBase(skGen());
    pc.push_back(ct);

    key h1 = cn_fast_hash(pc);
    key h2 = cn_fast_hash(pc);
    ASSERT_EQ(h1, h2);
}

TEST(RingCTOps, hash_to_scalar_ctkeyV_is_reduced)
{
    ctkeyV pc;
    ctkey ct;
    ct.dest = scalarmultBase(skGen());
    ct.mask = scalarmultBase(skGen());
    pc.push_back(ct);

    key s = hash_to_scalar(pc);
    key reduced;
    memcpy(&reduced, &s, 32);
    sc_reduce32(reduced.bytes);
    ASSERT_EQ(s, reduced);
}
