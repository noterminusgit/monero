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

#include "net/net_ssl.h"
#include <boost/asio/ssl.hpp>
#include <vector>

using namespace epee::net_utils;

TEST(net_ssl, ssl_support_from_string_enabled)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_disabled;
  ASSERT_TRUE(ssl_support_from_string(ssl, "enabled"));
  ASSERT_EQ(ssl, ssl_support_t::e_ssl_support_enabled);
}

TEST(net_ssl, ssl_support_from_string_disabled)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_enabled;
  ASSERT_TRUE(ssl_support_from_string(ssl, "disabled"));
  ASSERT_EQ(ssl, ssl_support_t::e_ssl_support_disabled);
}

TEST(net_ssl, ssl_support_from_string_autodetect)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_disabled;
  ASSERT_TRUE(ssl_support_from_string(ssl, "autodetect"));
  ASSERT_EQ(ssl, ssl_support_t::e_ssl_support_autodetect);
}

TEST(net_ssl, ssl_support_from_string_invalid)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_enabled;
  ASSERT_FALSE(ssl_support_from_string(ssl, "invalid_value"));
}

TEST(net_ssl, ssl_support_from_string_empty)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_enabled;
  ASSERT_FALSE(ssl_support_from_string(ssl, ""));
}

TEST(net_ssl, is_ssl_tls_handshake)
{
  // TLS 1.0 Client Hello starts with 0x16, 0x03, 0x01
  const unsigned char tls_hello[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(tls_hello, sizeof(tls_hello)));
}

TEST(net_ssl, is_ssl_tls12_handshake)
{
  // TLS 1.2 Client Hello starts with 0x16, 0x03, 0x03
  const unsigned char tls12_hello[] = {0x16, 0x03, 0x03, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(tls12_hello, sizeof(tls12_hello)));
}

TEST(net_ssl, is_ssl_not_ssl)
{
  // HTTP request
  const unsigned char http_data[] = {'G', 'E', 'T', ' ', '/', ' ', 'H', 'T', 'T'};
  ASSERT_FALSE(is_ssl(http_data, sizeof(http_data)));
}

TEST(net_ssl, is_ssl_too_short)
{
  const unsigned char short_data[] = {0x16, 0x03};
  ASSERT_FALSE(is_ssl(short_data, sizeof(short_data)));
}

TEST(net_ssl, is_ssl_zero_length)
{
  ASSERT_FALSE(is_ssl(nullptr, 0));
}

TEST(net_ssl, ssl_options_disabled)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_FALSE(static_cast<bool>(opts));
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_disabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::none);
}

TEST(net_ssl, ssl_options_enabled)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(static_cast<bool>(opts));
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_autodetect)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_autodetect);
  ASSERT_TRUE(static_cast<bool>(opts));
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_autodetect);
  ASSERT_EQ(opts.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_copy)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ssl_options_t copy = opts;
  ASSERT_EQ(copy.support, opts.support);
  ASSERT_EQ(copy.verification, opts.verification);
}

TEST(net_ssl, ssl_options_move)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ssl_options_t moved = std::move(opts);
  ASSERT_TRUE(static_cast<bool>(moved));
  ASSERT_EQ(moved.support, ssl_support_t::e_ssl_support_enabled);
}

TEST(net_ssl, ssl_options_with_fingerprints)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xAB));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_TRUE(static_cast<bool>(opts));
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
}

TEST(net_ssl, ssl_options_with_ca_path)
{
  ssl_options_t opts({}, "/some/ca/path");
  ASSERT_TRUE(static_cast<bool>(opts));
  ASSERT_EQ(opts.ca_path, "/some/ca/path");
}

TEST(net_ssl, create_ssl_context)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_NO_THROW(opts.create_context());
}

TEST(net_ssl, get_ssl_magic_size)
{
  ASSERT_EQ(get_ssl_magic_size(), 9u);
}

TEST(net_ssl, create_rsa_ssl_certificate)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));
  ASSERT_NE(pkey, nullptr);
  ASSERT_NE(cert, nullptr);

  // Test that we can get a fingerprint from the created cert
  std::string fp = get_hr_ssl_fingerprint(cert);
  ASSERT_FALSE(fp.empty());
  // SHA-256 fingerprint should have 32 hex pairs separated by colons: XX:XX:...:XX
  // That's 32*3-1 = 95 characters
  ASSERT_EQ(fp.size(), 95u);

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, has_strong_verification_disabled)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_FALSE(opts.has_strong_verification("example.com"));
}

TEST(net_ssl, has_strong_verification_system_ca)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  // system_ca verification is not "strong" (user-specific) verification
  ASSERT_FALSE(opts.has_strong_verification("example.com"));
}

// ============================================================================
// 1. ssl_support_t enum tests
// ============================================================================

TEST(net_ssl, ssl_support_enum_disabled_value)
{
  ssl_support_t s = ssl_support_t::e_ssl_support_disabled;
  ASSERT_EQ(static_cast<uint8_t>(s), 0);
}

TEST(net_ssl, ssl_support_enum_enabled_value)
{
  ssl_support_t s = ssl_support_t::e_ssl_support_enabled;
  ASSERT_EQ(static_cast<uint8_t>(s), 1);
}

TEST(net_ssl, ssl_support_enum_autodetect_value)
{
  ssl_support_t s = ssl_support_t::e_ssl_support_autodetect;
  ASSERT_EQ(static_cast<uint8_t>(s), 2);
}

TEST(net_ssl, ssl_support_enum_equality)
{
  ssl_support_t a = ssl_support_t::e_ssl_support_enabled;
  ssl_support_t b = ssl_support_t::e_ssl_support_enabled;
  ASSERT_EQ(a, b);
  ASSERT_NE(a, ssl_support_t::e_ssl_support_disabled);
}

TEST(net_ssl, ssl_support_enum_all_distinct)
{
  ASSERT_NE(ssl_support_t::e_ssl_support_disabled, ssl_support_t::e_ssl_support_enabled);
  ASSERT_NE(ssl_support_t::e_ssl_support_enabled, ssl_support_t::e_ssl_support_autodetect);
  ASSERT_NE(ssl_support_t::e_ssl_support_disabled, ssl_support_t::e_ssl_support_autodetect);
}

// ============================================================================
// 2. ssl_options_t construction and comparison
// ============================================================================

TEST(net_ssl, ssl_options_default_disabled_is_falsy)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_FALSE(static_cast<bool>(opts));
}

TEST(net_ssl, ssl_options_enabled_is_truthy)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(static_cast<bool>(opts));
}

TEST(net_ssl, ssl_options_autodetect_is_truthy)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_autodetect);
  ASSERT_TRUE(static_cast<bool>(opts));
}

TEST(net_ssl, ssl_options_disabled_verification_none)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::none);
}

TEST(net_ssl, ssl_options_enabled_verification_system_ca)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_autodetect_verification_system_ca)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_autodetect);
  ASSERT_EQ(opts.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_fingerprint_constructor_enables_ssl)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xDE));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(static_cast<bool>(opts));
}

TEST(net_ssl, ssl_options_fingerprint_constructor_sets_user_certs)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xAA));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
}

TEST(net_ssl, ssl_options_ca_path_constructor)
{
  ssl_options_t opts({}, "/tmp/test_ca.pem");
  ASSERT_EQ(opts.ca_path, "/tmp/test_ca.pem");
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
}

TEST(net_ssl, ssl_options_empty_fingerprints_with_ca)
{
  ssl_options_t opts({}, "/some/ca");
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
  ASSERT_EQ(opts.ca_path, "/some/ca");
}

TEST(net_ssl, ssl_options_copy_preserves_all_fields)
{
  ssl_options_t orig(ssl_support_t::e_ssl_support_autodetect);
  orig.ca_path = "/test/path";
  ssl_options_t copy = orig;
  ASSERT_EQ(copy.support, ssl_support_t::e_ssl_support_autodetect);
  ASSERT_EQ(copy.verification, ssl_verification_t::system_ca);
  ASSERT_EQ(copy.ca_path, "/test/path");
}

TEST(net_ssl, ssl_options_move_preserves_support)
{
  ssl_options_t orig(ssl_support_t::e_ssl_support_enabled);
  orig.ca_path = "/move/test";
  ssl_options_t moved = std::move(orig);
  ASSERT_EQ(moved.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(moved.ca_path, "/move/test");
}

TEST(net_ssl, ssl_options_copy_assignment)
{
  ssl_options_t a(ssl_support_t::e_ssl_support_enabled);
  ssl_options_t b(ssl_support_t::e_ssl_support_disabled);
  b = a;
  ASSERT_EQ(b.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(b.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_move_assignment)
{
  ssl_options_t a(ssl_support_t::e_ssl_support_autodetect);
  ssl_options_t b(ssl_support_t::e_ssl_support_disabled);
  b = std::move(a);
  ASSERT_EQ(b.support, ssl_support_t::e_ssl_support_autodetect);
  ASSERT_EQ(b.verification, ssl_verification_t::system_ca);
}

TEST(net_ssl, ssl_options_auth_default_empty)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(opts.auth.private_key_path.empty());
  ASSERT_TRUE(opts.auth.certificate_path.empty());
}

// ============================================================================
// 3. ssl_authentication_t tests
// ============================================================================

TEST(net_ssl, ssl_authentication_default_construction)
{
  ssl_authentication_t auth;
  ASSERT_TRUE(auth.private_key_path.empty());
  ASSERT_TRUE(auth.certificate_path.empty());
}

TEST(net_ssl, ssl_authentication_with_paths)
{
  ssl_authentication_t auth;
  auth.private_key_path = "/path/to/key.pem";
  auth.certificate_path = "/path/to/cert.pem";
  ASSERT_EQ(auth.private_key_path, "/path/to/key.pem");
  ASSERT_EQ(auth.certificate_path, "/path/to/cert.pem");
}

TEST(net_ssl, ssl_authentication_copy)
{
  ssl_authentication_t auth;
  auth.private_key_path = "/key";
  auth.certificate_path = "/cert";
  ssl_authentication_t copy = auth;
  ASSERT_EQ(copy.private_key_path, "/key");
  ASSERT_EQ(copy.certificate_path, "/cert");
}

TEST(net_ssl, ssl_authentication_in_options)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  opts.auth.private_key_path = "/my/key.pem";
  opts.auth.certificate_path = "/my/cert.pem";
  ASSERT_EQ(opts.auth.private_key_path, "/my/key.pem");
  ASSERT_EQ(opts.auth.certificate_path, "/my/cert.pem");
}

TEST(net_ssl, ssl_authentication_move)
{
  ssl_authentication_t auth;
  auth.private_key_path = "/move/key";
  auth.certificate_path = "/move/cert";
  ssl_authentication_t moved = std::move(auth);
  ASSERT_EQ(moved.private_key_path, "/move/key");
  ASSERT_EQ(moved.certificate_path, "/move/cert");
}

// ============================================================================
// 4. SSL context creation tests
// ============================================================================

TEST(net_ssl, create_context_disabled_returns_context)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  // Disabled SSL should still return a valid context object (just not configured)
  boost::asio::ssl::context ctx = opts.create_context();
  ASSERT_NE(ctx.native_handle(), nullptr);
}

TEST(net_ssl, create_context_enabled_returns_configured_context)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  boost::asio::ssl::context ctx = opts.create_context();
  ASSERT_NE(ctx.native_handle(), nullptr);
}

TEST(net_ssl, create_context_autodetect_returns_configured_context)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_autodetect);
  boost::asio::ssl::context ctx = opts.create_context();
  ASSERT_NE(ctx.native_handle(), nullptr);
}

TEST(net_ssl, create_context_with_fingerprints)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xBB));
  ssl_options_t opts(std::move(fps), "");
  boost::asio::ssl::context ctx = opts.create_context();
  ASSERT_NE(ctx.native_handle(), nullptr);
}

TEST(net_ssl, create_context_with_invalid_ca_path_throws)
{
  ssl_options_t opts({}, "/nonexistent/ca/path.pem");
  ASSERT_ANY_THROW(opts.create_context());
}

TEST(net_ssl, create_context_with_invalid_auth_key_throws)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  opts.auth.private_key_path = "/nonexistent/key.pem";
  opts.auth.certificate_path = "/nonexistent/cert.pem";
  // Should throw because the key/cert files don't exist
  ASSERT_ANY_THROW(opts.create_context());
}

TEST(net_ssl, create_context_disabled_no_throw)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_NO_THROW(opts.create_context());
}

TEST(net_ssl, create_context_enabled_no_throw)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_NO_THROW(opts.create_context());
}

TEST(net_ssl, create_context_enabled_generates_cert)
{
  // When no auth paths are given, create_context generates a self-signed cert
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  boost::asio::ssl::context ctx = opts.create_context();
  // The context should have a certificate loaded
  SSL* ssl = SSL_new(ctx.native_handle());
  ASSERT_NE(ssl, nullptr);
  X509* cert = SSL_get_certificate(ssl);
  ASSERT_NE(cert, nullptr);
  SSL_free(ssl);
}

TEST(net_ssl, create_context_autodetect_generates_cert)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_autodetect);
  boost::asio::ssl::context ctx = opts.create_context();
  SSL* ssl = SSL_new(ctx.native_handle());
  ASSERT_NE(ssl, nullptr);
  X509* cert = SSL_get_certificate(ssl);
  ASSERT_NE(cert, nullptr);
  SSL_free(ssl);
}

// ============================================================================
// 5. Certificate creation tests
// ============================================================================

TEST(net_ssl, create_rsa_ssl_certificate_success)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));
  ASSERT_NE(pkey, nullptr);
  ASSERT_NE(cert, nullptr);
  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, create_rsa_ssl_certificate_different_each_time)
{
  EVP_PKEY *pkey1 = nullptr, *pkey2 = nullptr;
  X509 *cert1 = nullptr, *cert2 = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey1, cert1));
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey2, cert2));

  // Fingerprints should differ (different keys)
  std::string fp1 = get_hr_ssl_fingerprint(cert1);
  std::string fp2 = get_hr_ssl_fingerprint(cert2);
  ASSERT_NE(fp1, fp2);

  EVP_PKEY_free(pkey1);
  EVP_PKEY_free(pkey2);
  X509_free(cert1);
  X509_free(cert2);
}

TEST(net_ssl, create_rsa_ssl_certificate_fingerprint_format)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));

  std::string fp = get_hr_ssl_fingerprint(cert);
  // SHA-256: 32 bytes => 32 hex pairs separated by 31 colons => 32*2 + 31 = 95 chars
  ASSERT_EQ(fp.size(), 95u);

  // Verify format: every 3rd character (starting at index 2) should be ':'
  for (size_t i = 2; i < fp.size(); i += 3)
  {
    ASSERT_EQ(fp[i], ':');
  }

  // Verify hex characters
  for (size_t i = 0; i < fp.size(); ++i)
  {
    if (i % 3 == 2)
      continue; // skip colons
    char c = fp[i];
    bool is_hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    ASSERT_TRUE(is_hex) << "Non-hex character at position " << i << ": '" << c << "'";
  }

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, create_rsa_ssl_certificate_valid_key_type)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));
  ASSERT_EQ(EVP_PKEY_base_id(pkey), EVP_PKEY_RSA);
  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, create_rsa_ssl_certificate_has_public_key)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));
  EVP_PKEY *pub = X509_get_pubkey(cert);
  ASSERT_NE(pub, nullptr);
  EVP_PKEY_free(pub);
  EVP_PKEY_free(pkey);
  X509_free(cert);
}

// ============================================================================
// 6. TLS detection tests (is_ssl)
// ============================================================================

TEST(net_ssl, is_ssl_tls10_client_hello)
{
  // TLS 1.0: record=0x16, major=0x03, minor=0x01
  // length=0x00,0x05, ClientHello=0x01, handshake_length matching
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_tls11_client_hello)
{
  // TLS 1.1: record=0x16, major=0x03, minor=0x02
  const unsigned char data[] = {0x16, 0x03, 0x02, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_tls12_client_hello_v2)
{
  // TLS 1.2: record=0x16, major=0x03, minor=0x03
  const unsigned char data[] = {0x16, 0x03, 0x03, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_tls13_client_hello)
{
  // TLS 1.3 sends record layer as TLS 1.0 (0x03,0x01) or 1.2 (0x03,0x03) for compat
  // Using 0x03,0x01 record layer which is standard for TLS 1.3 ClientHello
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_TRUE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_http_get_request)
{
  const char http[] = "GET / HTTP/1.1\r\n";
  ASSERT_FALSE(is_ssl(reinterpret_cast<const unsigned char*>(http), strlen(http)));
}

TEST(net_ssl, is_ssl_http_post_request)
{
  const char http[] = "POST /json_rpc HTTP/1.1\r\n";
  ASSERT_FALSE(is_ssl(reinterpret_cast<const unsigned char*>(http), strlen(http)));
}

TEST(net_ssl, is_ssl_empty_data)
{
  ASSERT_FALSE(is_ssl(nullptr, 0));
}

TEST(net_ssl, is_ssl_one_byte)
{
  const unsigned char data[] = {0x16};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_eight_bytes_too_short)
{
  // Exactly 8 bytes -- one less than the magic size of 9
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_wrong_record_type)
{
  // record type 0x17 is application data, not handshake
  const unsigned char data[] = {0x17, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_wrong_major_version)
{
  // Major version != 3
  const unsigned char data[] = {0x16, 0x02, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_not_client_hello)
{
  // data[5] = 0x02 (ServerHello instead of ClientHello)
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x02, 0x00, 0x00, 0x01};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_length_mismatch)
{
  // The length check: data[3]*256 + data[4] == data[7]*256 + data[8] + 4
  // data[3]=0x00, data[4]=0x05 => 5
  // data[7]=0x00, data[8]=0x02 => 2, 2+4=6 != 5 => mismatch
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x00, 0x00, 0x02};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_random_binary_data)
{
  const unsigned char data[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_all_zeros)
{
  const unsigned char data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}

// ============================================================================
// 7. Fingerprint handling tests
// ============================================================================

TEST(net_ssl, fingerprint_sha256_size)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));

  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  ASSERT_TRUE(X509_digest(cert, EVP_sha256(), md, &n));
  ASSERT_EQ(n, 32u); // SHA-256 produces 32 bytes

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, fingerprint_from_cert_matches_hr)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));

  // Get raw digest
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  ASSERT_TRUE(X509_digest(cert, EVP_sha256(), md, &n));

  // Get human-readable fingerprint
  std::string hr = get_hr_ssl_fingerprint(cert);

  // Build expected HR from raw digest
  std::string expected;
  for (unsigned int i = 0; i < n; ++i)
  {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X", md[i]);
    if (i > 0) expected += ':';
    expected += buf;
  }
  ASSERT_EQ(hr, expected);

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, fingerprint_options_with_multiple_fingerprints)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0x11));
  fps.push_back(std::vector<std::uint8_t>(32, 0x22));
  fps.push_back(std::vector<std::uint8_t>(32, 0x33));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_enabled);
}

TEST(net_ssl, fingerprint_options_sorted_after_construction)
{
  // Fingerprints should be sorted by the constructor for binary_search
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xCC));
  fps.push_back(std::vector<std::uint8_t>(32, 0xAA));
  fps.push_back(std::vector<std::uint8_t>(32, 0xBB));

  // We can't directly access fingerprints_ (private), but we can verify
  // the constructor doesn't throw
  ASSERT_NO_THROW(ssl_options_t opts(std::move(fps), ""));
}

TEST(net_ssl, fingerprint_empty_list_constructor)
{
  std::vector<std::vector<std::uint8_t>> fps;
  ssl_options_t opts(std::move(fps), "");
  ASSERT_EQ(opts.support, ssl_support_t::e_ssl_support_enabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
}

TEST(net_ssl, fingerprint_with_ca_path_constructor)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xFF));
  ssl_options_t opts(std::move(fps), "/some/ca");
  ASSERT_EQ(opts.ca_path, "/some/ca");
  ASSERT_EQ(opts.verification, ssl_verification_t::user_certificates);
}

TEST(net_ssl, fingerprint_hr_format_colon_separated)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));

  std::string fp = get_hr_ssl_fingerprint(cert);
  // Count colons: should be 31 (32 hex pairs - 1)
  size_t colon_count = 0;
  for (char c : fp)
  {
    if (c == ':') ++colon_count;
  }
  ASSERT_EQ(colon_count, 31u);

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, fingerprint_hr_uppercase_hex)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_rsa_ssl_certificate(pkey, cert));

  std::string fp = get_hr_ssl_fingerprint(cert);
  // All hex chars should be uppercase
  for (size_t i = 0; i < fp.size(); ++i)
  {
    if (i % 3 == 2) continue; // skip colons
    char c = fp[i];
    ASSERT_FALSE(c >= 'a' && c <= 'f') << "Lowercase hex at index " << i;
  }

  EVP_PKEY_free(pkey);
  X509_free(cert);
}

TEST(net_ssl, fingerprint_single_entry_32_bytes)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0x42));
  ssl_options_t opts(std::move(fps), "");
  // Should create successfully with standard 32-byte fingerprint
  ASSERT_TRUE(static_cast<bool>(opts));
}

TEST(net_ssl, fingerprint_nonstandard_size)
{
  // Non-32-byte fingerprints (e.g., SHA-1 = 20 bytes) should still be stored
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(20, 0x42));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_TRUE(static_cast<bool>(opts));
}

// ============================================================================
// 8. has_strong_verification and related tests
// ============================================================================

TEST(net_ssl, has_strong_verification_user_certificates)
{
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xAA));
  ssl_options_t opts(std::move(fps), "");
  // user_certificates verification IS strong
  ASSERT_TRUE(opts.has_strong_verification("example.com"));
}

TEST(net_ssl, has_strong_verification_none)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_FALSE(opts.has_strong_verification("example.com"));
}

TEST(net_ssl, has_strong_verification_onion_address)
{
  // .onion addresses are always considered strong
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(opts.has_strong_verification("abcdef1234567890.onion"));
}

TEST(net_ssl, has_strong_verification_i2p_address)
{
  // .i2p addresses are always considered strong
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_TRUE(opts.has_strong_verification("something.i2p"));
}

TEST(net_ssl, has_strong_verification_regular_domain_system_ca)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  // system_ca is not strong for regular domains
  ASSERT_FALSE(opts.has_strong_verification("google.com"));
}

// ============================================================================
// Additional ssl_support_from_string edge case tests
// ============================================================================

TEST(net_ssl, ssl_support_from_string_case_sensitive)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_disabled;
  ASSERT_FALSE(ssl_support_from_string(ssl, "Enabled"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "ENABLED"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "Disabled"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "DISABLED"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "Autodetect"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "AUTODETECT"));
}

TEST(net_ssl, ssl_support_from_string_does_not_modify_on_failure)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_enabled;
  ASSERT_FALSE(ssl_support_from_string(ssl, "garbage"));
  // ssl should remain unchanged
  ASSERT_EQ(ssl, ssl_support_t::e_ssl_support_enabled);
}

TEST(net_ssl, ssl_support_from_string_whitespace)
{
  ssl_support_t ssl = ssl_support_t::e_ssl_support_disabled;
  ASSERT_FALSE(ssl_support_from_string(ssl, " enabled"));
  ASSERT_FALSE(ssl_support_from_string(ssl, "enabled "));
  ASSERT_FALSE(ssl_support_from_string(ssl, " enabled "));
}

// ============================================================================
// Additional context and verification tests
// ============================================================================

TEST(net_ssl, ssl_verification_enum_values)
{
  ASSERT_EQ(static_cast<uint8_t>(ssl_verification_t::none), 0);
  ASSERT_EQ(static_cast<uint8_t>(ssl_verification_t::system_ca), 1);
  ASSERT_EQ(static_cast<uint8_t>(ssl_verification_t::user_certificates), 2);
  ASSERT_EQ(static_cast<uint8_t>(ssl_verification_t::user_ca), 3);
}

TEST(net_ssl, ssl_verification_all_distinct)
{
  ASSERT_NE(ssl_verification_t::none, ssl_verification_t::system_ca);
  ASSERT_NE(ssl_verification_t::system_ca, ssl_verification_t::user_certificates);
  ASSERT_NE(ssl_verification_t::user_certificates, ssl_verification_t::user_ca);
  ASSERT_NE(ssl_verification_t::none, ssl_verification_t::user_ca);
}

TEST(net_ssl, get_ssl_magic_size_value)
{
  // The magic size should be 9 bytes
  ASSERT_EQ(get_ssl_magic_size(), 9u);
}

TEST(net_ssl, get_ssl_magic_size_constexpr)
{
  // Verify it's usable in constexpr context
  constexpr size_t magic = get_ssl_magic_size();
  ASSERT_EQ(magic, 9u);
}

TEST(net_ssl, create_context_user_certificates_no_ca_no_throw)
{
  // User certificates mode with empty ca_path should not throw
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xDD));
  ssl_options_t opts(std::move(fps), "");
  ASSERT_NO_THROW(opts.create_context());
}

TEST(net_ssl, ssl_options_fingerprints_and_ca_create_context)
{
  // Fingerprints with no real CA file (empty path) -- should still work
  std::vector<std::vector<std::uint8_t>> fps;
  fps.push_back(std::vector<std::uint8_t>(32, 0xEE));
  fps.push_back(std::vector<std::uint8_t>(32, 0xFF));
  ssl_options_t opts(std::move(fps), "");
  boost::asio::ssl::context ctx = opts.create_context();
  ASSERT_NE(ctx.native_handle(), nullptr);
}

TEST(net_ssl, has_strong_verification_onion_even_with_none_verification)
{
  // .onion bypass is checked before verification mode
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_EQ(opts.verification, ssl_verification_t::none);
  // .onion still returns true regardless of verification setting
  ASSERT_TRUE(opts.has_strong_verification("test.onion"));
}

TEST(net_ssl, has_strong_verification_i2p_even_with_none_verification)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_disabled);
  ASSERT_TRUE(opts.has_strong_verification("test.i2p"));
}

TEST(net_ssl, has_strong_verification_empty_host)
{
  ssl_options_t opts(ssl_support_t::e_ssl_support_enabled);
  ASSERT_FALSE(opts.has_strong_verification(""));
}

TEST(net_ssl, is_ssl_larger_valid_payload)
{
  // A more realistic TLS ClientHello with larger length
  // record_length = 0x01, 0x00 = 256
  // handshake_length = 0x00, 0xFC = 252, and 252 + 4 = 256
  const unsigned char data[] = {0x16, 0x03, 0x03, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFC};
  ASSERT_TRUE(is_ssl(data, sizeof(data)));
}

TEST(net_ssl, is_ssl_data6_nonzero_rejects)
{
  // data[6] must be 0 for the length check to work with small values
  // data[3]*256+data[4] = 5, data[6] != 0 means data[7]*256+data[8]+4 won't match
  const unsigned char data[] = {0x16, 0x03, 0x01, 0x00, 0x05, 0x01, 0x01, 0x00, 0x01};
  ASSERT_FALSE(is_ssl(data, sizeof(data)));
}
