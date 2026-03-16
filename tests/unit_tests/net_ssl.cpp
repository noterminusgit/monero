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

TEST(net_ssl, create_ec_ssl_certificate)
{
  EVP_PKEY *pkey = nullptr;
  X509 *cert = nullptr;
  ASSERT_TRUE(create_ec_ssl_certificate(pkey, cert));
  ASSERT_NE(pkey, nullptr);
  ASSERT_NE(cert, nullptr);
  EVP_PKEY_free(pkey);
  X509_free(cert);
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
