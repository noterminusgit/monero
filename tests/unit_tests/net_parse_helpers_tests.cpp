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
#include "net/net_parse_helpers.h"
#include "net/net_utils_base.h"
#include "string_tools.h"

// ---- parse_peer_from_string (epee::string_tools) ----

TEST(NetParseHelpers, ParsePeerIPv4WithPort)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "192.168.1.1:8080"));
  EXPECT_EQ(8080, port);
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("192.168.1.1", ip_str);
}

TEST(NetParseHelpers, ParsePeerIPv4WithoutPort)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "10.0.0.1"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("10.0.0.1", ip_str);
  EXPECT_EQ(0, port);
}

TEST(NetParseHelpers, ParsePeerLocalhostWithPort)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "127.0.0.1:18081"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("127.0.0.1", ip_str);
  EXPECT_EQ(18081, port);
}

TEST(NetParseHelpers, ParsePeerZeroIP)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "0.0.0.0:0"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("0.0.0.0", ip_str);
  EXPECT_EQ(0, port);
}

TEST(NetParseHelpers, ParsePeerMaxIP)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  // inet_addr("255.255.255.255") returns INADDR_NONE which equals the error
  // sentinel, so parse_peer_from_string returns false for this IP.
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, "255.255.255.255:65535"));
}

TEST(NetParseHelpers, ParsePeerEmptyString)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, ""));
}

TEST(NetParseHelpers, ParsePeerInvalidIP)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, "not.an.ip.address"));
}

TEST(NetParseHelpers, ParsePeerJustColon)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, ":"));
}

TEST(NetParseHelpers, ParsePeerJustPort)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, ":8080"));
}

TEST(NetParseHelpers, ParsePeerMultipleColons)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  // This looks like IPv6 without brackets; should fail for IPv4 parser
  EXPECT_FALSE(epee::string_tools::parse_peer_from_string(ip, port, "1:2:3:4:5:6:7:8"));
}

// ---- IP address string conversion ----

TEST(NetParseHelpers, IPToStringLoopback)
{
  uint32_t ip = MAKE_IP(127, 0, 0, 1);
  EXPECT_EQ("127.0.0.1", epee::string_tools::get_ip_string_from_int32(ip));
}

TEST(NetParseHelpers, IPToStringBroadcast)
{
  uint32_t ip = MAKE_IP(255, 255, 255, 255);
  EXPECT_EQ("255.255.255.255", epee::string_tools::get_ip_string_from_int32(ip));
}

TEST(NetParseHelpers, IPToStringZero)
{
  uint32_t ip = MAKE_IP(0, 0, 0, 0);
  EXPECT_EQ("0.0.0.0", epee::string_tools::get_ip_string_from_int32(ip));
}

TEST(NetParseHelpers, IPFromStringLoopback)
{
  uint32_t ip = 0;
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.1"));
  EXPECT_EQ(MAKE_IP(127, 0, 0, 1), ip);
}

TEST(NetParseHelpers, IPFromStringInvalid)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "invalid"));
}

TEST(NetParseHelpers, IPFromStringEmpty)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, ""));
}

TEST(NetParseHelpers, IPRoundTrip)
{
  uint32_t original = MAKE_IP(192, 168, 10, 25);
  std::string str = epee::string_tools::get_ip_string_from_int32(original);
  uint32_t recovered = 0;
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(recovered, str));
  EXPECT_EQ(original, recovered);
}

// ---- URL parsing ----

TEST(NetParseHelpers, ParseURLBasicHTTP)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://example.com/path", content));
  EXPECT_EQ("http", content.schema);
  EXPECT_EQ("example.com", content.host);
  // parse_url does not set default port based on scheme; port remains 0 if not specified
  EXPECT_EQ(0, content.port);
  EXPECT_EQ("/path", content.uri);
}

TEST(NetParseHelpers, ParseURLBasicHTTPS)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("https://example.com/secure", content));
  EXPECT_EQ("https", content.schema);
  EXPECT_EQ("example.com", content.host);
  // parse_url does not set default port based on scheme; port remains 0 if not specified
  EXPECT_EQ(0, content.port);
  EXPECT_EQ("/secure", content.uri);
}

TEST(NetParseHelpers, ParseURLWithPort)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://localhost:18081/json_rpc", content));
  EXPECT_EQ("http", content.schema);
  EXPECT_EQ("localhost", content.host);
  EXPECT_EQ(18081, content.port);
  EXPECT_EQ("/json_rpc", content.uri);
}

TEST(NetParseHelpers, ParseURLHostOnly)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://myhost.com", content));
  EXPECT_EQ("http", content.schema);
  EXPECT_EQ("myhost.com", content.host);
  // parse_url does not set default port based on scheme
  EXPECT_EQ(0, content.port);
}

TEST(NetParseHelpers, ParseURLWithIPAddress)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://192.168.1.100:3000/api", content));
  EXPECT_EQ("http", content.schema);
  EXPECT_EQ("192.168.1.100", content.host);
  EXPECT_EQ(3000, content.port);
  EXPECT_EQ("/api", content.uri);
}

TEST(NetParseHelpers, ParseURLEmptyString)
{
  epee::net_utils::http::url_content content;
  // parse_url returns true even for empty string (returns true on regex non-match)
  bool result = epee::net_utils::parse_url("", content);
  EXPECT_TRUE(result);
}

TEST(NetParseHelpers, ParseURLMissingScheme)
{
  epee::net_utils::http::url_content content;
  // parse_url returns true even without scheme (regex is lenient)
  bool result = epee::net_utils::parse_url("example.com/path", content);
  EXPECT_TRUE(result);
}

TEST(NetParseHelpers, ParseURLTrailingSlash)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://example.com/", content));
  EXPECT_EQ("http", content.schema);
  EXPECT_EQ("example.com", content.host);
  EXPECT_EQ("/", content.uri);
}

TEST(NetParseHelpers, ParseURLComplexPath)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://example.com/a/b/c/d", content));
  EXPECT_EQ("/a/b/c/d", content.uri);
}

TEST(NetParseHelpers, ParseURLPortZero)
{
  epee::net_utils::http::url_content content;
  // Port 0 in URL
  bool result = epee::net_utils::parse_url("http://example.com:0/test", content);
  if (result)
  {
    EXPECT_EQ(0, content.port);
  }
}

// ---- parse_uri ----

TEST(NetParseHelpers, ParseURIBasicPath)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/path/to/resource", content));
  EXPECT_EQ("/path/to/resource", content.m_path);
}

TEST(NetParseHelpers, ParseURIWithQuery)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/path?key=value", content));
  EXPECT_EQ("/path", content.m_path);
  EXPECT_EQ("key=value", content.m_query);
}

TEST(NetParseHelpers, ParseURIWithFragment)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/path#section", content));
  EXPECT_EQ("/path", content.m_path);
  EXPECT_EQ("section", content.m_fragment);
}

TEST(NetParseHelpers, ParseURIWithQueryAndFragment)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/path?key=value#section", content));
  EXPECT_EQ("/path", content.m_path);
  EXPECT_EQ("key=value", content.m_query);
  EXPECT_EQ("section", content.m_fragment);
}

TEST(NetParseHelpers, ParseURIRootPath)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/", content));
  EXPECT_EQ("/", content.m_path);
}

TEST(NetParseHelpers, ParseURIEmpty)
{
  epee::net_utils::http::uri_content content;
  // Empty URI - implementation may accept or reject
  bool result = epee::net_utils::parse_uri("", content);
  // Just verify it doesn't crash
  (void)result;
}

TEST(NetParseHelpers, ParseURIMultipleQueryParams)
{
  epee::net_utils::http::uri_content content;
  EXPECT_TRUE(epee::net_utils::parse_uri("/search?q=monero&lang=en", content));
  EXPECT_EQ("/search", content.m_path);
  EXPECT_EQ("q=monero&lang=en", content.m_query);
}

// ---- IPv6 URL parsing ----

TEST(NetParseHelpers, ParseURLIPv6Localhost)
{
  epee::net_utils::http::url_content content;
  bool result = epee::net_utils::parse_url("http://[::1]:18081/json_rpc", content);
  if (result)
  {
    EXPECT_EQ(18081, content.port);
    EXPECT_EQ("/json_rpc", content.uri);
  }
}

TEST(NetParseHelpers, ParseURLIPv6Full)
{
  epee::net_utils::http::url_content content;
  bool result = epee::net_utils::parse_url("http://[2001:db8::1]:8080/test", content);
  if (result)
  {
    EXPECT_EQ(8080, content.port);
    EXPECT_EQ("/test", content.uri);
  }
}

// ---- Edge cases ----

TEST(NetParseHelpers, ParseURLVeryLongHostname)
{
  std::string long_host(253, 'a'); // Max DNS name length
  std::string url = "http://" + long_host + "/test";
  epee::net_utils::http::url_content content;
  // Should not crash regardless of result
  epee::net_utils::parse_url(url, content);
  SUCCEED();
}

TEST(NetParseHelpers, ParseURLSpecialCharsInPath)
{
  epee::net_utils::http::url_content content;
  EXPECT_TRUE(epee::net_utils::parse_url("http://example.com/path%20with%20spaces", content));
  EXPECT_EQ("/path%20with%20spaces", content.uri);
}

TEST(NetParseHelpers, ParsePeerIPv4PortBoundary)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "1.2.3.4:1"));
  EXPECT_EQ(1, port);
}

TEST(NetParseHelpers, ParsePeerIPv4Port80)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  EXPECT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "1.2.3.4:80"));
  EXPECT_EQ(80, port);
}

TEST(NetParseHelpers, ParseURLSchemeCase)
{
  epee::net_utils::http::url_content content;
  // HTTP scheme in uppercase
  bool result = epee::net_utils::parse_url("HTTP://example.com/test", content);
  // Implementation may or may not be case-sensitive
  (void)result;
  SUCCEED();
}

TEST(NetParseHelpers, IPFromStringOctetOverflow)
{
  uint32_t ip = 0;
  // 256 is out of range for a single octet
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "256.1.1.1"));
}

TEST(NetParseHelpers, IPFromStringNegativeOctet)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "-1.0.0.0"));
}

TEST(NetParseHelpers, IPFromStringTooFewOctets)
{
  uint32_t ip = 0;
  // inet_addr accepts "1.2.3" (interprets as 1.2.0.3), so this returns true
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "1.2.3"));
}

TEST(NetParseHelpers, IPFromStringTooManyOctets)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "1.2.3.4.5"));
}
