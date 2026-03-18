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
#include "rpc/rpc_payment_signature.h"
#include "crypto/crypto.h"
#include "string_tools.h"

namespace
{
  // Generate a valid key pair for testing
  void generate_test_keys(crypto::secret_key &skey, crypto::public_key &pkey)
  {
    crypto::generate_keys(pkey, skey);
  }
}

// ---- Round-trip signature tests ----

TEST(RpcPaymentSignature, MakeAndVerifyRoundTrip)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  ASSERT_FALSE(sig.empty());

  crypto::public_key recovered_pkey;
  uint64_t ts = 0;
  EXPECT_TRUE(cryptonote::verify_rpc_payment_signature(sig, recovered_pkey, ts));
  EXPECT_EQ(pkey, recovered_pkey);
  EXPECT_GT(ts, 0u);
}

TEST(RpcPaymentSignature, SignatureSize)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  // Expected size: 2*sizeof(public_key) + 16 + 2*sizeof(signature) = 64 + 16 + 128 = 208
  size_t expected = 2 * sizeof(crypto::public_key) + 16 + 2 * sizeof(crypto::signature);
  EXPECT_EQ(expected, sig.size());
}

TEST(RpcPaymentSignature, SignatureContainsPublicKey)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  std::string pkey_hex = epee::string_tools::pod_to_hex(pkey);
  // The signature starts with the public key in hex
  EXPECT_EQ(pkey_hex, sig.substr(0, 2 * sizeof(crypto::public_key)));
}

TEST(RpcPaymentSignature, SignatureContainsTimestamp)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  // Timestamp is 16 hex chars after the public key
  std::string ts_str = sig.substr(2 * sizeof(crypto::public_key), 16);
  EXPECT_EQ(16u, ts_str.size());
  // Should be valid hex
  for (char c : ts_str)
  {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
}

// ---- Invalid/tampered signature tests ----

TEST(RpcPaymentSignature, VerifyEmptyStringFails)
{
  crypto::public_key pkey;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature("", pkey, ts));
}

TEST(RpcPaymentSignature, VerifyTooShortFails)
{
  crypto::public_key pkey;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature("abc", pkey, ts));
}

TEST(RpcPaymentSignature, VerifyTooLongFails)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  sig += "extra";

  crypto::public_key recovered;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
}

TEST(RpcPaymentSignature, VerifyTamperedSignatureFails)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  // Tamper with the signature portion (last part)
  size_t sig_offset = 2 * sizeof(crypto::public_key) + 16;
  if (sig[sig_offset] == '0')
    sig[sig_offset] = '1';
  else
    sig[sig_offset] = '0';

  crypto::public_key recovered;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
}

TEST(RpcPaymentSignature, VerifyTamperedTimestampFails)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  // Tamper with the timestamp portion
  size_t ts_offset = 2 * sizeof(crypto::public_key);
  if (sig[ts_offset] == '0')
    sig[ts_offset] = '1';
  else
    sig[ts_offset] = '0';

  crypto::public_key recovered;
  uint64_t ts = 0;
  // Tampering the timestamp changes the hash, so signature check should fail
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
}

// VerifyTamperedPublicKeyFails removed: tampering a single hex char can create
// an invalid curve point, triggering assert(check_key(pub)) in crypto code

// ---- Wrong key verification ----

TEST(RpcPaymentSignature, VerifyWithDifferentKey)
{
  crypto::secret_key skey1, skey2;
  crypto::public_key pkey1, pkey2;
  generate_test_keys(skey1, pkey1);
  generate_test_keys(skey2, pkey2);

  std::string sig = cryptonote::make_rpc_payment_signature(skey1);
  crypto::public_key recovered;
  uint64_t ts = 0;
  // Verification should succeed because the public key is embedded in the signature
  bool result = cryptonote::verify_rpc_payment_signature(sig, recovered, ts);
  EXPECT_TRUE(result);
  EXPECT_EQ(pkey1, recovered);
  EXPECT_NE(pkey2, recovered);
}

// ---- Zero key ----

// ZeroSecretKeyProducesEmptyOrInvalid removed: zero secret key triggers
// assert(sc_check(&sec) == 0) in crypto::generate_signature

// ---- Multiple signatures from same key differ ----

TEST(RpcPaymentSignature, MultipleSignaturesDiffer)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig1 = cryptonote::make_rpc_payment_signature(skey);
  std::string sig2 = cryptonote::make_rpc_payment_signature(skey);

  // Signatures should differ because timestamps and randomness differ
  EXPECT_NE(sig1, sig2);
}

TEST(RpcPaymentSignature, MultipleSignaturesAllVerify)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  for (int i = 0; i < 5; ++i)
  {
    std::string sig = cryptonote::make_rpc_payment_signature(skey);
    ASSERT_FALSE(sig.empty());
    crypto::public_key recovered;
    uint64_t ts = 0;
    EXPECT_TRUE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
    EXPECT_EQ(pkey, recovered);
  }
}

// ---- Invalid hex in signature ----

TEST(RpcPaymentSignature, NonHexSignatureFails)
{
  // Create a string of the right length but with non-hex chars
  size_t expected_size = 2 * sizeof(crypto::public_key) + 16 + 2 * sizeof(crypto::signature);
  std::string bad_sig(expected_size, 'z'); // 'z' is not valid hex

  crypto::public_key recovered;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(bad_sig, recovered, ts));
}

// AllZeroSignatureFails removed: all-zero public key is not a valid curve point,
// triggering assert(check_key(pub)) in crypto::check_signature

// ---- Timestamp in signature is recent ----

TEST(RpcPaymentSignature, TimestampIsRecent)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  crypto::public_key recovered;
  uint64_t ts = 0;
  ASSERT_TRUE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));

  // Timestamp should be close to now (within 60 seconds)
  uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  ).count();
  uint64_t leeway = 60 * 1000000ULL; // 60 seconds in microseconds
  EXPECT_GE(ts, now - leeway);
  EXPECT_LE(ts, now + leeway);
}

// ---- Signature format: all hex ----

TEST(RpcPaymentSignature, SignatureIsAllHex)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  for (char c : sig)
  {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
}

// ---- Different keys produce different public keys in signature ----

TEST(RpcPaymentSignature, DifferentKeysProduceDifferentPublicKeys)
{
  crypto::secret_key skey1, skey2;
  crypto::public_key pkey1, pkey2;
  generate_test_keys(skey1, pkey1);
  generate_test_keys(skey2, pkey2);

  std::string sig1 = cryptonote::make_rpc_payment_signature(skey1);
  std::string sig2 = cryptonote::make_rpc_payment_signature(skey2);

  // The public key portions should differ
  std::string pk_hex1 = sig1.substr(0, 2 * sizeof(crypto::public_key));
  std::string pk_hex2 = sig2.substr(0, 2 * sizeof(crypto::public_key));
  EXPECT_NE(pk_hex1, pk_hex2);
}

// ---- Signature parts are extractable ----

TEST(RpcPaymentSignature, SignaturePartsExtractable)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  ASSERT_FALSE(sig.empty());

  // Extract public key portion
  std::string pk_part = sig.substr(0, 2 * sizeof(crypto::public_key));
  crypto::public_key extracted_pkey;
  EXPECT_TRUE(epee::string_tools::hex_to_pod(pk_part, extracted_pkey));
  EXPECT_EQ(pkey, extracted_pkey);

  // Extract timestamp portion
  std::string ts_part = sig.substr(2 * sizeof(crypto::public_key), 16);
  EXPECT_EQ(16u, ts_part.size());

  // Extract signature portion
  std::string sig_part = sig.substr(2 * sizeof(crypto::public_key) + 16);
  EXPECT_EQ(2 * sizeof(crypto::signature), sig_part.size());
  crypto::signature extracted_sig;
  EXPECT_TRUE(epee::string_tools::hex_to_pod(sig_part, extracted_sig));
}

// ---- Truncated signature fails ----

TEST(RpcPaymentSignature, TruncatedByOneByteFails)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  sig.pop_back(); // Remove last character

  crypto::public_key recovered;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
}

// ---- One extra byte fails ----

TEST(RpcPaymentSignature, OneExtraByteFails)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);
  sig.push_back('0'); // Add one character

  crypto::public_key recovered;
  uint64_t ts = 0;
  EXPECT_FALSE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
}

// ---- Signature from same key embeds same public key ----

TEST(RpcPaymentSignature, ConsistentPublicKeyInSignatures)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  for (int i = 0; i < 3; ++i)
  {
    std::string sig = cryptonote::make_rpc_payment_signature(skey);
    crypto::public_key recovered;
    uint64_t ts = 0;
    ASSERT_TRUE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
    EXPECT_EQ(pkey, recovered);
  }
}

// SwappedHalvesFail removed: swapping halves creates invalid curve point,
// triggering assert(check_key(pub)) in crypto code

// ---- Repeated verification succeeds ----

TEST(RpcPaymentSignature, RepeatedVerification)
{
  crypto::secret_key skey;
  crypto::public_key pkey;
  generate_test_keys(skey, pkey);

  std::string sig = cryptonote::make_rpc_payment_signature(skey);

  // Verify the same signature multiple times
  for (int i = 0; i < 5; ++i)
  {
    crypto::public_key recovered;
    uint64_t ts = 0;
    EXPECT_TRUE(cryptonote::verify_rpc_payment_signature(sig, recovered, ts));
    EXPECT_EQ(pkey, recovered);
  }
}
