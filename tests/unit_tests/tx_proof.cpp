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

#include "crypto/crypto.h"
extern "C" {
#include "crypto/crypto-ops.h"
}
#include "crypto/hash.h"
#include <boost/algorithm/string.hpp>

static inline unsigned char *operator &(crypto::ec_point &point) {
    return &reinterpret_cast<unsigned char &>(point);
  }

static inline unsigned char *operator &(crypto::ec_scalar &scalar) {
    return &reinterpret_cast<unsigned char &>(scalar);
  }

TEST(tx_proof, prove_verify_v2)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    // A = aG
    // B = bG
    crypto::secret_key a,b;
    crypto::public_key A,B;
    crypto::generate_keys(A, a, a, false);
    crypto::generate_keys(B, b, b, false);

    // R_B = rB
    crypto::public_key R_B;
    ge_p3 B_p3;
    ASSERT_EQ(ge_frombytes_vartime(&B_p3,&B), 0);
    ge_p2 R_B_p2;
    ge_scalarmult(&R_B_p2, &unwrap(r), &B_p3);
    ge_tobytes(&R_B, &R_B_p2);

    // R_G = rG
    crypto::public_key R_G;
    ASSERT_EQ(ge_frombytes_vartime(&B_p3,&B), 0);
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    // D = rA
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3,&A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::signature sig;

    // Message data
    crypto::hash prefix_hash;
    char data[] = "hash input";
    crypto::cn_fast_hash(data,sizeof(data)-1,prefix_hash);

    // Generate/verify valid v1 proof with standard address
    crypto::generate_tx_proof_v1(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 1));

    // Generate/verify valid v1 proof with subaddress
    crypto::generate_tx_proof_v1(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_B, A, B, D, sig, 1));

    // Generate/verify valid v2 proof with standard address
    crypto::generate_tx_proof(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 2));

    // Generate/verify valid v2 proof with subaddress
    crypto::generate_tx_proof(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_B, A, B, D, sig, 2));

    // Try to verify valid v2 proofs as v1 proof (bad)
    crypto::generate_tx_proof(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 1));
    crypto::generate_tx_proof(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A, B, D, sig, 1));

    // Randomly-distributed test points
    crypto::secret_key evil_a, evil_b, evil_d, evil_r;
    crypto::public_key evil_A, evil_B, evil_D, evil_R;
    crypto::generate_keys(evil_A, evil_a, evil_a, false);
    crypto::generate_keys(evil_B, evil_b, evil_b, false);
    crypto::generate_keys(evil_D, evil_d, evil_d, false);
    crypto::generate_keys(evil_R, evil_r, evil_r, false);

    // Selectively choose bad point in v2 proof (bad)
    crypto::generate_tx_proof(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, evil_R, A, B, D, sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, evil_A, B, D, sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A, evil_B, D, sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A, B, evil_D, sig, 2));

    // Try to verify valid v1 proofs as v2 proof (bad)
    crypto::generate_tx_proof_v1(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 2));
    crypto::generate_tx_proof_v1(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A, B, D, sig, 2));
}

// ---------------------------------------------------------------------------
// Test: V1 proof verification fails with wrong individual keys
// ---------------------------------------------------------------------------
TEST(tx_proof, v1_wrong_keys_fail)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a, b;
    crypto::public_key A, B;
    crypto::generate_keys(A, a, a, false);
    crypto::generate_keys(B, b, b, false);

    // R_G = r*G
    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    // D = r*A
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::hash prefix_hash;
    char data[] = "v1 wrong key test";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    crypto::signature sig;

    // Generate valid v1 proof (standard address)
    crypto::generate_tx_proof_v1(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 1));

    // Wrong R
    crypto::secret_key evil_r;
    crypto::public_key evil_R;
    crypto::generate_keys(evil_R, evil_r, evil_r, false);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, evil_R, A, boost::none, D, sig, 1));

    // Wrong A
    crypto::secret_key evil_a;
    crypto::public_key evil_A;
    crypto::generate_keys(evil_A, evil_a, evil_a, false);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, evil_A, boost::none, D, sig, 1));

    // Wrong D
    crypto::secret_key evil_d;
    crypto::public_key evil_D;
    crypto::generate_keys(evil_D, evil_d, evil_d, false);
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, evil_D, sig, 1));
}

// ---------------------------------------------------------------------------
// Test: V2 proof with subaddress: each individual wrong key fails
// ---------------------------------------------------------------------------
TEST(tx_proof, v2_subaddress_individual_wrong_keys)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a, b;
    crypto::public_key A, B;
    crypto::generate_keys(A, a, a, false);
    crypto::generate_keys(B, b, b, false);

    // R_B = r*B
    crypto::public_key R_B;
    ge_p3 B_p3;
    ASSERT_EQ(ge_frombytes_vartime(&B_p3, &B), 0);
    ge_p2 R_B_p2;
    ge_scalarmult(&R_B_p2, &unwrap(r), &B_p3);
    ge_tobytes(&R_B, &R_B_p2);

    // D = r*A
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::hash prefix_hash;
    char data[] = "v2 subaddr wrong keys";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    crypto::signature sig;
    crypto::generate_tx_proof(prefix_hash, R_B, A, B, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_B, A, B, D, sig, 2));

    // Each wrong key independently should fail
    crypto::secret_key e_r, e_a, e_b, e_d;
    crypto::public_key E_R, E_A, E_B, E_D;
    crypto::generate_keys(E_R, e_r, e_r, false);
    crypto::generate_keys(E_A, e_a, e_a, false);
    crypto::generate_keys(E_B, e_b, e_b, false);
    crypto::generate_keys(E_D, e_d, e_d, false);

    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, E_R, A,   B,   D,   sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, E_A, B,   D,   sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A,   E_B, D,   sig, 2));
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_B, A,   B,   E_D, sig, 2));
}

// ---------------------------------------------------------------------------
// Test: Corrupted signature bytes should fail verification
// ---------------------------------------------------------------------------
TEST(tx_proof, corrupted_signature_fails)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a;
    crypto::public_key A;
    crypto::generate_keys(A, a, a, false);

    // R_G = r*G
    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    // D = r*A
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::hash prefix_hash;
    char data[] = "corrupt sig test";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    crypto::signature sig;
    crypto::generate_tx_proof(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 2));

    // Flip a byte in sig.c
    crypto::signature bad_sig = sig;
    reinterpret_cast<unsigned char*>(&bad_sig.c)[0] ^= 0x01;
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, bad_sig, 2));

    // Flip a byte in sig.r
    bad_sig = sig;
    reinterpret_cast<unsigned char*>(&bad_sig.r)[0] ^= 0x01;
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, bad_sig, 2));
}

// ---------------------------------------------------------------------------
// Test: Different message hashes produce different proofs that don't cross-verify
// ---------------------------------------------------------------------------
TEST(tx_proof, different_messages_dont_cross_verify)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a;
    crypto::public_key A;
    crypto::generate_keys(A, a, a, false);

    // R_G = r*G
    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    // D = r*A
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    // Two different messages
    crypto::hash hash1, hash2;
    char data1[] = "message one";
    char data2[] = "message two";
    crypto::cn_fast_hash(data1, sizeof(data1) - 1, hash1);
    crypto::cn_fast_hash(data2, sizeof(data2) - 1, hash2);

    crypto::signature sig1, sig2;

    // V2 proofs for each message
    crypto::generate_tx_proof(hash1, R_G, A, boost::none, D, r, sig1);
    crypto::generate_tx_proof(hash2, R_G, A, boost::none, D, r, sig2);

    // Each proof verifies with its own message
    ASSERT_TRUE(crypto::check_tx_proof(hash1, R_G, A, boost::none, D, sig1, 2));
    ASSERT_TRUE(crypto::check_tx_proof(hash2, R_G, A, boost::none, D, sig2, 2));

    // Cross-verify should fail
    ASSERT_FALSE(crypto::check_tx_proof(hash2, R_G, A, boost::none, D, sig1, 2));
    ASSERT_FALSE(crypto::check_tx_proof(hash1, R_G, A, boost::none, D, sig2, 2));
}

// ---------------------------------------------------------------------------
// Test: V2 standard address proof should fail when B is unexpectedly provided
// ---------------------------------------------------------------------------
TEST(tx_proof, v2_standard_address_wrong_optional_B)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a, b;
    crypto::public_key A, B;
    crypto::generate_keys(A, a, a, false);
    crypto::generate_keys(B, b, b, false);

    // R_G = r*G (standard address: no B)
    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    // D = r*A
    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::hash prefix_hash;
    char data[] = "standard vs subaddr";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    // Generate proof for standard address (no B)
    crypto::signature sig;
    crypto::generate_tx_proof(prefix_hash, R_G, A, boost::none, D, r, sig);
    ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sig, 2));

    // Verification with a spurious B should fail because the hash commits to B
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, B, D, sig, 2));
}

// ---------------------------------------------------------------------------
// Test: Multiple proofs with same parameters are all independently valid
// ---------------------------------------------------------------------------
TEST(tx_proof, multiple_proofs_all_valid)
{
    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::secret_key a;
    crypto::public_key A;
    crypto::generate_keys(A, a, a, false);

    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    crypto::public_key D;
    ge_p3 A_p3;
    ASSERT_EQ(ge_frombytes_vartime(&A_p3, &A), 0);
    ge_p2 D_p2;
    ge_scalarmult(&D_p2, &unwrap(r), &A_p3);
    ge_tobytes(&D, &D_p2);

    crypto::hash prefix_hash;
    char data[] = "multiple proofs";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    // Generate multiple proofs (each uses a random k internally)
    const int NUM_PROOFS = 5;
    crypto::signature sigs[NUM_PROOFS];
    for (int i = 0; i < NUM_PROOFS; ++i)
    {
        crypto::generate_tx_proof(prefix_hash, R_G, A, boost::none, D, r, sigs[i]);
    }

    // All should verify
    for (int i = 0; i < NUM_PROOFS; ++i)
    {
        ASSERT_TRUE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, D, sigs[i], 2));
    }

    // Signatures should differ (different random k each time)
    // At least one pair should differ (probabilistically certain)
    bool found_different = false;
    for (int i = 1; i < NUM_PROOFS; ++i)
    {
        if (memcmp(&sigs[0], &sigs[i], sizeof(crypto::signature)) != 0)
        {
            found_different = true;
            break;
        }
    }
    ASSERT_TRUE(found_different);
}

// ---------------------------------------------------------------------------
// Test: Zero/identity point as D should fail proof generation gracefully
// ---------------------------------------------------------------------------
TEST(tx_proof, zero_point_D_check_fails)
{
    // Construct a zero/identity point - all zeros is not a valid curve point
    crypto::public_key zero_key;
    memset(&zero_key, 0, sizeof(zero_key));

    crypto::secret_key a;
    crypto::public_key A;
    crypto::generate_keys(A, a, a, false);

    crypto::secret_key r;
    crypto::random32_unbiased(&r);

    crypto::public_key R_G;
    ge_p3 R_G_p3;
    ge_scalarmult_base(&R_G_p3, &unwrap(r));
    ge_p3_tobytes(&R_G, &R_G_p3);

    crypto::hash prefix_hash;
    char data[] = "zero point test";
    crypto::cn_fast_hash(data, sizeof(data) - 1, prefix_hash);

    // check_tx_proof should return false for invalid points
    crypto::signature dummy_sig;
    memset(&dummy_sig, 0, sizeof(dummy_sig));

    // Zero D should fail verification (invalid point)
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, A, boost::none, zero_key, dummy_sig, 2));

    // Zero R should fail verification
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, zero_key, A, boost::none, A, dummy_sig, 2));

    // Zero A should fail verification
    ASSERT_FALSE(crypto::check_tx_proof(prefix_hash, R_G, zero_key, boost::none, A, dummy_sig, 2));
}
