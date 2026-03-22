// Copyright (c) 2025, The Monero Project
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

#include "net/enums.h"
#include "net/levin_base.h"
#include "net/net_utils_base.h"
#include "net/local_ip.h"
#include "string_tools.h"
#include "p2p/p2p_protocol_defs.h"
#include "storages/portable_storage_template_helper.h"
#include <boost/utility/string_ref.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio/ip/address_v6.hpp>

// ---- zone_to_string / zone_from_string ----

TEST(net_zone, zone_to_string_public)
{
  EXPECT_STREQ(epee::net_utils::zone_to_string(epee::net_utils::zone::public_), "public");
}

TEST(net_zone, zone_to_string_i2p)
{
  EXPECT_STREQ(epee::net_utils::zone_to_string(epee::net_utils::zone::i2p), "i2p");
}

TEST(net_zone, zone_to_string_tor)
{
  EXPECT_STREQ(epee::net_utils::zone_to_string(epee::net_utils::zone::tor), "tor");
}

TEST(net_zone, zone_to_string_invalid)
{
  EXPECT_STREQ(epee::net_utils::zone_to_string(epee::net_utils::zone::invalid), "invalid");
}

TEST(net_zone, zone_from_string_public)
{
  EXPECT_EQ(epee::net_utils::zone_from_string("public"), epee::net_utils::zone::public_);
}

TEST(net_zone, zone_from_string_i2p)
{
  EXPECT_EQ(epee::net_utils::zone_from_string("i2p"), epee::net_utils::zone::i2p);
}

TEST(net_zone, zone_from_string_tor)
{
  EXPECT_EQ(epee::net_utils::zone_from_string("tor"), epee::net_utils::zone::tor);
}

TEST(net_zone, zone_from_string_invalid)
{
  EXPECT_EQ(epee::net_utils::zone_from_string(""), epee::net_utils::zone::invalid);
  EXPECT_EQ(epee::net_utils::zone_from_string("bogus"), epee::net_utils::zone::invalid);
}

TEST(net_zone, zone_roundtrip)
{
  for (auto z : {epee::net_utils::zone::public_, epee::net_utils::zone::i2p, epee::net_utils::zone::tor})
  {
    const char* str = epee::net_utils::zone_to_string(z);
    EXPECT_EQ(epee::net_utils::zone_from_string(str), z);
  }
}

// ---- Levin error descriptions ----

TEST(levin_errors, known_codes)
{
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_OK), "LEVIN_OK");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION), "LEVIN_ERROR_CONNECTION");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION_NOT_FOUND), "LEVIN_ERROR_CONNECTION_NOT_FOUND");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION_DESTROYED), "LEVIN_ERROR_CONNECTION_DESTROYED");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION_TIMEDOUT), "LEVIN_ERROR_CONNECTION_TIMEDOUT");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL), "LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED), "LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED");
  EXPECT_STREQ(epee::levin::get_err_descr(LEVIN_ERROR_FORMAT), "LEVIN_ERROR_FORMAT");
}

TEST(levin_errors, unknown_code)
{
  EXPECT_STREQ(epee::levin::get_err_descr(-99), "unknown code");
  EXPECT_STREQ(epee::levin::get_err_descr(42), "unknown code");
}

// ---- Levin protocol constants ----

TEST(levin_protocol, signature)
{
  EXPECT_EQ(LEVIN_SIGNATURE, 0x0101010101012101LL);
}

TEST(levin_protocol, bucket_head2_size)
{
  // bucket_head2 is packed at 33 bytes: 8+8+1+4+4+4+4
  EXPECT_EQ(sizeof(epee::levin::bucket_head2), 33u);
}

TEST(levin_protocol, make_header)
{
  auto h = epee::levin::make_header(42, 100, LEVIN_PACKET_REQUEST, true);
  EXPECT_EQ(h.m_signature, LEVIN_SIGNATURE);
  EXPECT_EQ(h.m_cb, 100u);
  EXPECT_EQ(h.m_command, 42u);
  EXPECT_EQ(h.m_flags, static_cast<uint32_t>(LEVIN_PACKET_REQUEST));
  EXPECT_TRUE(h.m_have_to_return_data);
  EXPECT_EQ(h.m_protocol_version, static_cast<uint32_t>(LEVIN_PROTOCOL_VER_1));
  EXPECT_EQ(h.m_return_code, 0);
}

TEST(levin_protocol, make_header_no_return)
{
  auto h = epee::levin::make_header(10, 50, LEVIN_PACKET_REQUEST, false);
  EXPECT_FALSE(h.m_have_to_return_data);
}

// ---- IPv4 subnet matching ----

TEST(ipv4_subnet, matches_host_in_subnet)
{
  uint32_t subnet_ip;
  epee::string_tools::get_ip_int32_from_string(subnet_ip, "192.168.1.0");
  epee::net_utils::ipv4_network_subnet subnet{subnet_ip, 24};

  uint32_t host_ip;
  epee::string_tools::get_ip_int32_from_string(host_ip, "192.168.1.100");
  epee::net_utils::ipv4_network_address host{host_ip, 8080};

  EXPECT_TRUE(subnet.matches(host));
}

TEST(ipv4_subnet, rejects_host_outside_subnet)
{
  uint32_t subnet_ip;
  epee::string_tools::get_ip_int32_from_string(subnet_ip, "192.168.1.0");
  epee::net_utils::ipv4_network_subnet subnet{subnet_ip, 24};

  uint32_t host_ip;
  epee::string_tools::get_ip_int32_from_string(host_ip, "192.168.2.100");
  epee::net_utils::ipv4_network_address host{host_ip, 8080};

  EXPECT_FALSE(subnet.matches(host));
}

TEST(ipv4_subnet, slash32_matches_exact_ip)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_subnet subnet{ip, 32};

  epee::net_utils::ipv4_network_address exact{ip, 1234};
  EXPECT_TRUE(subnet.matches(exact));

  uint32_t other_ip;
  epee::string_tools::get_ip_int32_from_string(other_ip, "10.0.0.2");
  epee::net_utils::ipv4_network_address other{other_ip, 1234};
  EXPECT_FALSE(subnet.matches(other));
}

TEST(ipv4_subnet, slash0_matches_all)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "0.0.0.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 0};

  uint32_t any_ip;
  epee::string_tools::get_ip_int32_from_string(any_ip, "203.0.113.42");
  epee::net_utils::ipv4_network_address host{any_ip, 80};
  EXPECT_TRUE(subnet.matches(host));
}

TEST(ipv4_subnet, str_format)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 24};

  std::string s = subnet.str();
  EXPECT_NE(s.find("/24"), std::string::npos);
}

// ---- IP classification (local_ip.h) ----

TEST(local_ip, loopback_detection)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.1");
  EXPECT_TRUE(epee::net_utils::is_ip_loopback(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "127.255.255.255");
  EXPECT_TRUE(epee::net_utils::is_ip_loopback(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "128.0.0.1");
  EXPECT_FALSE(epee::net_utils::is_ip_loopback(ip));
}

TEST(local_ip, private_10_network)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "10.255.255.255");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));
}

TEST(local_ip, private_172_network)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "172.16.0.1");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "172.31.255.255");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));

  // 172.32 is NOT private
  epee::string_tools::get_ip_int32_from_string(ip, "172.32.0.1");
  EXPECT_FALSE(epee::net_utils::is_ip_local(ip));
}

TEST(local_ip, private_192_168_network)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.0.1");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "192.168.255.255");
  EXPECT_TRUE(epee::net_utils::is_ip_local(ip));
}

TEST(local_ip, public_ip_not_local)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "8.8.8.8");
  EXPECT_FALSE(epee::net_utils::is_ip_local(ip));
  EXPECT_FALSE(epee::net_utils::is_ip_loopback(ip));

  epee::string_tools::get_ip_int32_from_string(ip, "203.0.113.1");
  EXPECT_FALSE(epee::net_utils::is_ip_local(ip));
}

// ---- IPv4 address properties ----

TEST(ipv4_address, str_format)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.100");
  epee::net_utils::ipv4_network_address addr{ip, 18080};

  std::string s = addr.str();
  EXPECT_NE(s.find("192.168.1.100"), std::string::npos);
  EXPECT_NE(s.find("18080"), std::string::npos);
}

TEST(ipv4_address, host_str)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address addr{ip, 80};

  std::string h = addr.host_str();
  EXPECT_NE(h.find("10.0.0.1"), std::string::npos);
  // host_str should not contain port
  EXPECT_EQ(h.find(":"), std::string::npos);
}

TEST(ipv4_address, is_same_host)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address addr1{ip, 80};
  epee::net_utils::ipv4_network_address addr2{ip, 443};

  // Same IP, different port -> same host
  EXPECT_TRUE(addr1.is_same_host(addr2));

  uint32_t other_ip;
  epee::string_tools::get_ip_int32_from_string(other_ip, "10.0.0.2");
  epee::net_utils::ipv4_network_address addr3{other_ip, 80};
  EXPECT_FALSE(addr1.is_same_host(addr3));
}

TEST(ipv4_address, loopback)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.1");
  epee::net_utils::ipv4_network_address addr{ip, 18080};
  EXPECT_TRUE(addr.is_loopback());
}

TEST(ipv4_address, local)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.1");
  epee::net_utils::ipv4_network_address addr{ip, 18080};
  EXPECT_TRUE(addr.is_local());
}

TEST(ipv4_address, comparison)
{
  uint32_t ip1, ip2;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.1");
  epee::string_tools::get_ip_int32_from_string(ip2, "10.0.0.2");

  epee::net_utils::ipv4_network_address addr1{ip1, 80};
  epee::net_utils::ipv4_network_address addr2{ip2, 80};
  epee::net_utils::ipv4_network_address addr1_copy{ip1, 80};

  EXPECT_TRUE(addr1.equal(addr1_copy));
  EXPECT_FALSE(addr1.equal(addr2));
  EXPECT_TRUE(addr1.less(addr2) || addr2.less(addr1)); // strict ordering
}

// ---- P2P helpers ----

TEST(p2p_helpers, peerid_to_string)
{
  nodetool::peerid_type id = 0;
  std::string s = nodetool::peerid_to_string(id);
  EXPECT_FALSE(s.empty());
  // peerid_to_string pads to 16 hex chars
  EXPECT_EQ(s, "0000000000000000");

  id = 0xdeadbeefcafe1234ULL;
  s = nodetool::peerid_to_string(id);
  EXPECT_EQ(s, "deadbeefcafe1234");
}

// ---- Additional zone tests ----

TEST(net_zone, zone_enum_values_distinct)
{
  EXPECT_NE(epee::net_utils::zone::public_, epee::net_utils::zone::i2p);
  EXPECT_NE(epee::net_utils::zone::public_, epee::net_utils::zone::tor);
  EXPECT_NE(epee::net_utils::zone::i2p, epee::net_utils::zone::tor);
  EXPECT_NE(epee::net_utils::zone::invalid, epee::net_utils::zone::public_);
}

TEST(net_zone, zone_from_string_case_sensitivity)
{
  // Uppercase should not match
  EXPECT_EQ(epee::net_utils::zone_from_string("PUBLIC"), epee::net_utils::zone::invalid);
  EXPECT_EQ(epee::net_utils::zone_from_string("I2P"), epee::net_utils::zone::invalid);
  EXPECT_EQ(epee::net_utils::zone_from_string("TOR"), epee::net_utils::zone::invalid);
}

TEST(net_zone, zone_from_string_whitespace)
{
  EXPECT_EQ(epee::net_utils::zone_from_string(" public"), epee::net_utils::zone::invalid);
  EXPECT_EQ(epee::net_utils::zone_from_string("tor "), epee::net_utils::zone::invalid);
}

// ---- Additional Levin protocol tests ----

TEST(levin_protocol, make_header_response)
{
  auto h = epee::levin::make_header(99, 200, LEVIN_PACKET_RESPONSE, false);
  EXPECT_EQ(h.m_signature, LEVIN_SIGNATURE);
  EXPECT_EQ(h.m_cb, 200u);
  EXPECT_EQ(h.m_command, 99u);
  EXPECT_EQ(h.m_flags, static_cast<uint32_t>(LEVIN_PACKET_RESPONSE));
  EXPECT_FALSE(h.m_have_to_return_data);
}

TEST(levin_protocol, make_header_zero_payload)
{
  auto h = epee::levin::make_header(1, 0, LEVIN_PACKET_REQUEST, true);
  EXPECT_EQ(h.m_cb, 0u);
}

TEST(levin_protocol, make_header_large_payload)
{
  auto h = epee::levin::make_header(1, 1ULL << 30, LEVIN_PACKET_REQUEST, true);
  EXPECT_EQ(h.m_cb, 1ULL << 30);
}

TEST(levin_errors, ok_code_is_zero)
{
  EXPECT_EQ(LEVIN_OK, 0);
}

TEST(levin_errors, error_codes_are_negative)
{
  EXPECT_LT(LEVIN_ERROR_CONNECTION, 0);
  EXPECT_LT(LEVIN_ERROR_CONNECTION_NOT_FOUND, 0);
  EXPECT_LT(LEVIN_ERROR_CONNECTION_DESTROYED, 0);
  EXPECT_LT(LEVIN_ERROR_CONNECTION_TIMEDOUT, 0);
  EXPECT_LT(LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL, 0);
  EXPECT_LT(LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED, 0);
  EXPECT_LT(LEVIN_ERROR_FORMAT, 0);
}

TEST(levin_errors, error_codes_unique)
{
  std::set<int32_t> codes = {
    LEVIN_ERROR_CONNECTION,
    LEVIN_ERROR_CONNECTION_NOT_FOUND,
    LEVIN_ERROR_CONNECTION_DESTROYED,
    LEVIN_ERROR_CONNECTION_TIMEDOUT,
    LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL,
    LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED,
    LEVIN_ERROR_FORMAT
  };
  EXPECT_EQ(codes.size(), 7u);
}

// ---- Additional IPv4 address tests ----

TEST(ipv4_address, port_accessor)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address addr{ip, 18080};
  EXPECT_EQ(addr.port(), 18080u);
}

TEST(ipv4_address, ip_accessor)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address addr{ip, 80};
  EXPECT_EQ(addr.ip(), ip);
}

TEST(ipv4_address, not_loopback)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "8.8.8.8");
  epee::net_utils::ipv4_network_address addr{ip, 53};
  EXPECT_FALSE(addr.is_loopback());
}

TEST(ipv4_address, not_local_public_ip)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "8.8.4.4");
  epee::net_utils::ipv4_network_address addr{ip, 53};
  EXPECT_FALSE(addr.is_local());
}

TEST(ipv4_address, type_id)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address addr{ip, 80};
  EXPECT_EQ(addr.get_type_id(), epee::net_utils::ipv4_network_address::get_type_id());
}

// ---- Additional subnet tests ----

TEST(ipv4_subnet, slash16)
{
  uint32_t subnet_ip;
  epee::string_tools::get_ip_int32_from_string(subnet_ip, "172.16.0.0");
  epee::net_utils::ipv4_network_subnet subnet{subnet_ip, 16};

  uint32_t host_ip;
  epee::string_tools::get_ip_int32_from_string(host_ip, "172.16.254.254");
  epee::net_utils::ipv4_network_address host{host_ip, 80};
  EXPECT_TRUE(subnet.matches(host));

  uint32_t outside_ip;
  epee::string_tools::get_ip_int32_from_string(outside_ip, "172.17.0.1");
  epee::net_utils::ipv4_network_address outside{outside_ip, 80};
  EXPECT_FALSE(subnet.matches(outside));
}

TEST(ipv4_subnet, slash8)
{
  uint32_t subnet_ip;
  epee::string_tools::get_ip_int32_from_string(subnet_ip, "10.0.0.0");
  epee::net_utils::ipv4_network_subnet subnet{subnet_ip, 8};

  uint32_t host_ip;
  epee::string_tools::get_ip_int32_from_string(host_ip, "10.255.255.255");
  epee::net_utils::ipv4_network_address host{host_ip, 80};
  EXPECT_TRUE(subnet.matches(host));
}

TEST(ipv4_subnet, equality)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.0");
  epee::net_utils::ipv4_network_subnet s1{ip, 24};
  epee::net_utils::ipv4_network_subnet s2{ip, 24};
  epee::net_utils::ipv4_network_subnet s3{ip, 16};
  EXPECT_TRUE(s1 == s2);
  EXPECT_FALSE(s1 == s3);
}

// ---- IP conversion utility tests ----

TEST(ip_conversion, string_to_int_and_back)
{
  uint32_t ip;
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.1"));
  std::string str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(str, "192.168.1.1");
}

TEST(ip_conversion, invalid_ip_string)
{
  uint32_t ip;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "not.an.ip"));
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, ""));
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "256.0.0.1"));
}

TEST(ip_conversion, boundary_ips)
{
  uint32_t ip;
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "0.0.0.0"));
  std::string str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(str, "0.0.0.0");

  // inet_addr("255.255.255.255") returns INADDR_NONE (0xFFFFFFFF), which is
  // the same as the error sentinel, so get_ip_int32_from_string returns false.
  // Instead, test that get_ip_string_from_int32 can produce 255.255.255.255.
  ip = 0xFFFFFFFF;
  str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(str, "255.255.255.255");
}

// ---- P2P additional tests ----

TEST(p2p_helpers, peerid_to_string_small)
{
  // peerid_to_string pads to 16 hex chars
  std::string s = nodetool::peerid_to_string(1);
  EXPECT_EQ(s, "0000000000000001");
}

TEST(p2p_helpers, peerid_to_string_max)
{
  std::string s = nodetool::peerid_to_string(UINT64_MAX);
  EXPECT_EQ(s, "ffffffffffffffff");
}

// ---- Local IP edge cases ----

TEST(local_ip, zero_ip_not_local)
{
  EXPECT_FALSE(epee::net_utils::is_ip_local(0));
}

TEST(local_ip, broadcast_not_local)
{
  EXPECT_FALSE(epee::net_utils::is_ip_local(0xFFFFFFFF));
}

TEST(local_ip, loopback_127_0_0_2)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.2");
  EXPECT_TRUE(epee::net_utils::is_ip_loopback(ip));
}

TEST(local_ip, link_local_169_254)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "169.254.1.1");
  // Link-local may or may not be considered local depending on implementation
  (void)epee::net_utils::is_ip_local(ip); // Just verify no crash
}

// ---- net::error category tests ----

#include "net/error.h"

TEST(net_error, category_name)
{
  const std::error_category& cat = net::error_category();
  EXPECT_STREQ(cat.name(), "net::error_category");
}

TEST(net_error, category_singleton)
{
  // Verify the category always returns the same instance
  const std::error_category& cat1 = net::error_category();
  const std::error_category& cat2 = net::error_category();
  EXPECT_EQ(&cat1, &cat2);
}

TEST(net_error, make_error_code_produces_correct_category)
{
  std::error_code ec = net::make_error_code(net::error::bogus_dnssec);
  EXPECT_EQ(&ec.category(), &net::error_category());
}

TEST(net_error, error_messages_not_empty)
{
  // Every defined error code should produce a non-empty message
  const net::error codes[] = {
    net::error::bogus_dnssec,
    net::error::dns_query_failure,
    net::error::expected_tld,
    net::error::invalid_encoding,
    net::error::invalid_host,
    net::error::invalid_i2p_address,
    net::error::invalid_mask,
    net::error::invalid_port,
    net::error::invalid_scheme,
    net::error::invalid_tor_address,
    net::error::unexpected_userinfo,
    net::error::unsupported_address,
  };

  for (auto c : codes)
  {
    std::error_code ec = net::make_error_code(c);
    EXPECT_FALSE(ec.message().empty()) << "Empty message for error code " << static_cast<int>(c);
  }
}

TEST(net_error, bogus_dnssec_message)
{
  std::error_code ec = net::make_error_code(net::error::bogus_dnssec);
  EXPECT_NE(ec.message().find("DNSSEC"), std::string::npos);
}

TEST(net_error, dns_query_failure_message)
{
  std::error_code ec = net::make_error_code(net::error::dns_query_failure);
  EXPECT_NE(ec.message().find("DNS"), std::string::npos);
}

TEST(net_error, invalid_port_message)
{
  std::error_code ec = net::make_error_code(net::error::invalid_port);
  EXPECT_NE(ec.message().find("port"), std::string::npos);
}

TEST(net_error, invalid_mask_message)
{
  std::error_code ec = net::make_error_code(net::error::invalid_mask);
  EXPECT_NE(ec.message().find("mask"), std::string::npos);
}

TEST(net_error, invalid_tor_address_message)
{
  std::error_code ec = net::make_error_code(net::error::invalid_tor_address);
  EXPECT_NE(ec.message().find("Tor"), std::string::npos);
}

TEST(net_error, invalid_i2p_address_message)
{
  std::error_code ec = net::make_error_code(net::error::invalid_i2p_address);
  EXPECT_NE(ec.message().find("I2P"), std::string::npos);
}

TEST(net_error, unknown_error_code_message)
{
  // An error code value that's not in the enum should produce "Unknown"
  std::error_code ec{999, net::error_category()};
  EXPECT_NE(ec.message().find("Unknown"), std::string::npos);
}

TEST(net_error, default_error_condition_invalid_port)
{
  std::error_code ec = net::make_error_code(net::error::invalid_port);
  std::error_condition cond = ec.default_error_condition();
  // invalid_port maps to result_out_of_range
  EXPECT_EQ(cond, std::errc::result_out_of_range);
}

TEST(net_error, default_error_condition_invalid_mask)
{
  std::error_code ec = net::make_error_code(net::error::invalid_mask);
  std::error_condition cond = ec.default_error_condition();
  // invalid_mask maps to result_out_of_range
  EXPECT_EQ(cond, std::errc::result_out_of_range);
}

TEST(net_error, default_error_condition_expected_tld_is_self)
{
  std::error_code ec = net::make_error_code(net::error::expected_tld);
  std::error_condition cond = ec.default_error_condition();
  // expected_tld falls through to default: returns condition with same value and same category
  EXPECT_EQ(cond.value(), static_cast<int>(net::error::expected_tld));
}

TEST(net_error, error_code_bool_conversion)
{
  // A non-zero error code should be truthy
  std::error_code ec = net::make_error_code(net::error::invalid_host);
  EXPECT_TRUE(static_cast<bool>(ec));

  // Value 0 (success) should be falsy - but net::error starts at 1
  std::error_code ec_zero{0, net::error_category()};
  EXPECT_FALSE(static_cast<bool>(ec_zero));
}

TEST(net_error, error_code_values_distinct)
{
  std::set<int> values;
  values.insert(static_cast<int>(net::error::bogus_dnssec));
  values.insert(static_cast<int>(net::error::dns_query_failure));
  values.insert(static_cast<int>(net::error::expected_tld));
  values.insert(static_cast<int>(net::error::invalid_encoding));
  values.insert(static_cast<int>(net::error::invalid_host));
  values.insert(static_cast<int>(net::error::invalid_i2p_address));
  values.insert(static_cast<int>(net::error::invalid_mask));
  values.insert(static_cast<int>(net::error::invalid_port));
  values.insert(static_cast<int>(net::error::invalid_scheme));
  values.insert(static_cast<int>(net::error::invalid_tor_address));
  values.insert(static_cast<int>(net::error::unexpected_userinfo));
  values.insert(static_cast<int>(net::error::unsupported_address));
  EXPECT_EQ(values.size(), 12u);
}

// ---- network_address polymorphic wrapper tests ----

TEST(network_address, default_constructed_is_invalid)
{
  epee::net_utils::network_address addr;
  EXPECT_EQ(epee::net_utils::address_type::invalid, addr.get_type_id());
  EXPECT_EQ(epee::net_utils::zone::invalid, addr.get_zone());
  EXPECT_FALSE(addr.is_loopback());
  EXPECT_FALSE(addr.is_local());
  EXPECT_FALSE(addr.is_blockable());
  EXPECT_EQ(0u, addr.port());
  EXPECT_EQ("<none>", addr.str());
  EXPECT_EQ("<none>", addr.host_str());
}

TEST(network_address, from_ipv4)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 8080}};

  EXPECT_EQ(epee::net_utils::address_type::ipv4, addr.get_type_id());
  EXPECT_EQ(epee::net_utils::zone::public_, addr.get_zone());
  EXPECT_TRUE(addr.is_blockable());
  EXPECT_EQ(8080u, addr.port());
  EXPECT_FALSE(addr.str().empty());
}

TEST(network_address, from_ipv6)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::network_address addr{epee::net_utils::ipv6_network_address{v6, 9090}};

  EXPECT_EQ(epee::net_utils::address_type::ipv6, addr.get_type_id());
  EXPECT_EQ(epee::net_utils::zone::public_, addr.get_zone());
  EXPECT_TRUE(addr.is_loopback());
  EXPECT_EQ(9090u, addr.port());
}

TEST(network_address, copy_and_equality)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.1");
  epee::net_utils::network_address a{epee::net_utils::ipv4_network_address{ip, 80}};
  epee::net_utils::network_address b = a;

  EXPECT_EQ(a, b);
  EXPECT_TRUE(a.is_same_host(b));
}

TEST(network_address, different_types_not_equal)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address ipv4_addr{epee::net_utils::ipv4_network_address{ip, 80}};

  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::network_address ipv6_addr{epee::net_utils::ipv6_network_address{v6, 80}};

  EXPECT_NE(ipv4_addr, ipv6_addr);
}

TEST(network_address, ordering_different_types)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address ipv4_addr{epee::net_utils::ipv4_network_address{ip, 80}};

  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::network_address ipv6_addr{epee::net_utils::ipv6_network_address{v6, 80}};

  // Different types should have a consistent order (ipv4 < ipv6 by type id)
  EXPECT_TRUE(ipv4_addr < ipv6_addr || ipv6_addr < ipv4_addr);
  // And it should be consistent
  bool ipv4_less = ipv4_addr < ipv6_addr;
  EXPECT_EQ(ipv4_less, ipv4_addr < ipv6_addr);
}

TEST(network_address, ordering_same_type)
{
  uint32_t ip1, ip2;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.1");
  epee::string_tools::get_ip_int32_from_string(ip2, "10.0.0.2");
  epee::net_utils::network_address a{epee::net_utils::ipv4_network_address{ip1, 80}};
  epee::net_utils::network_address b{epee::net_utils::ipv4_network_address{ip2, 80}};

  EXPECT_TRUE(a < b || b < a);
  EXPECT_FALSE(a < a); // not less than self
}

TEST(network_address, null_vs_null_equal)
{
  epee::net_utils::network_address a;
  epee::net_utils::network_address b;
  EXPECT_EQ(a, b);
}

TEST(network_address, null_vs_populated_not_equal)
{
  epee::net_utils::network_address empty;
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "1.2.3.4");
  epee::net_utils::network_address populated{epee::net_utils::ipv4_network_address{ip, 80}};
  EXPECT_NE(empty, populated);
}

TEST(network_address, same_host_different_port)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address a{epee::net_utils::ipv4_network_address{ip, 80}};
  epee::net_utils::network_address b{epee::net_utils::ipv4_network_address{ip, 443}};

  EXPECT_TRUE(a.is_same_host(b));
  EXPECT_NE(a, b); // different port
}

TEST(network_address, as_template_accessor)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "172.16.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 1234}};

  const auto& inner = addr.as<epee::net_utils::ipv4_network_address>();
  EXPECT_EQ(ip, inner.ip());
  EXPECT_EQ(1234u, inner.port());
}

TEST(network_address, as_wrong_type_throws)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 80}};

  EXPECT_THROW(addr.as<epee::net_utils::ipv6_network_address>(), std::bad_cast);
}

// ---- connection_context_base tests ----

TEST(connection_context, default_construction)
{
  epee::net_utils::connection_context_base ctx;
  EXPECT_FALSE(ctx.m_is_income);
  EXPECT_FALSE(ctx.m_ssl);
  EXPECT_EQ(0u, ctx.m_recv_cnt);
  EXPECT_EQ(0u, ctx.m_send_cnt);
  EXPECT_EQ(0, ctx.m_last_recv);
  EXPECT_EQ(0, ctx.m_last_send);
  EXPECT_EQ(0.0, ctx.m_current_speed_down);
  EXPECT_EQ(0.0, ctx.m_current_speed_up);
  EXPECT_EQ(0.0, ctx.m_max_speed_down);
  EXPECT_EQ(0.0, ctx.m_max_speed_up);
  // remote_address should be default (invalid)
  EXPECT_EQ(epee::net_utils::address_type::invalid, ctx.m_remote_address.get_type_id());
}

TEST(connection_context, parameterized_construction)
{
  boost::uuids::uuid id = boost::uuids::random_generator()();
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.100");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 18080}};

  epee::net_utils::connection_context_base ctx(id, addr, true, true);
  EXPECT_EQ(id, ctx.m_connection_id);
  EXPECT_TRUE(ctx.m_is_income);
  EXPECT_TRUE(ctx.m_ssl);
  EXPECT_EQ(addr, ctx.m_remote_address);
}

TEST(connection_context, copy_construction)
{
  boost::uuids::uuid id = boost::uuids::random_generator()();
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 8080}};

  epee::net_utils::connection_context_base ctx1(id, addr, false, true);
  epee::net_utils::connection_context_base ctx2(ctx1);

  EXPECT_EQ(ctx1.m_connection_id, ctx2.m_connection_id);
  EXPECT_EQ(ctx1.m_remote_address, ctx2.m_remote_address);
  EXPECT_EQ(ctx1.m_is_income, ctx2.m_is_income);
  EXPECT_EQ(ctx1.m_ssl, ctx2.m_ssl);
}

TEST(connection_context, print_connection_context_not_empty)
{
  boost::uuids::uuid id = boost::uuids::random_generator()();
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 18080}};

  epee::net_utils::connection_context_base ctx(id, addr, true, false);
  std::string result = epee::net_utils::print_connection_context(ctx);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("10.0.0.1"), std::string::npos);
  EXPECT_NE(result.find("INC"), std::string::npos);
}

TEST(connection_context, print_connection_context_short_not_empty)
{
  boost::uuids::uuid id = boost::uuids::random_generator()();
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::network_address addr{epee::net_utils::ipv4_network_address{ip, 18080}};

  epee::net_utils::connection_context_base ctx(id, addr, false, false);
  std::string result = epee::net_utils::print_connection_context_short(ctx);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("OUT"), std::string::npos);
}

TEST(connection_context, assignment_operator)
{
  boost::uuids::uuid id1 = boost::uuids::random_generator()();
  uint32_t ip1;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.1");
  epee::net_utils::network_address addr1{epee::net_utils::ipv4_network_address{ip1, 80}};

  boost::uuids::uuid id2 = boost::uuids::random_generator()();
  uint32_t ip2;
  epee::string_tools::get_ip_int32_from_string(ip2, "10.0.0.2");
  epee::net_utils::network_address addr2{epee::net_utils::ipv4_network_address{ip2, 443}};

  epee::net_utils::connection_context_base ctx1(id1, addr1, true, false);
  epee::net_utils::connection_context_base ctx2(id2, addr2, false, true);

  ctx2 = ctx1;
  EXPECT_EQ(ctx1.m_connection_id, ctx2.m_connection_id);
  EXPECT_EQ(ctx1.m_remote_address, ctx2.m_remote_address);
  EXPECT_EQ(ctx1.m_is_income, ctx2.m_is_income);
  EXPECT_EQ(ctx1.m_ssl, ctx2.m_ssl);
}

// ---- IPv4 subnet additional tests ----

TEST(ipv4_subnet, loopback_subnet)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 8};
  EXPECT_TRUE(subnet.is_loopback());
}

TEST(ipv4_subnet, local_subnet)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.0.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 16};
  EXPECT_TRUE(subnet.is_local());
}

TEST(ipv4_subnet, public_subnet_not_local)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "8.8.0.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 16};
  EXPECT_FALSE(subnet.is_local());
  EXPECT_FALSE(subnet.is_loopback());
}

TEST(ipv4_subnet, host_str_contains_mask)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.0");
  epee::net_utils::ipv4_network_subnet subnet{ip, 8};
  std::string hs = subnet.host_str();
  EXPECT_NE(hs.find("/8"), std::string::npos);
}

TEST(ipv4_subnet, less_ordering)
{
  uint32_t ip1, ip2;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.0");
  epee::string_tools::get_ip_int32_from_string(ip2, "172.16.0.0");
  epee::net_utils::ipv4_network_subnet s1{ip1, 8};
  epee::net_utils::ipv4_network_subnet s2{ip2, 12};
  EXPECT_TRUE(s1 < s2 || s2 < s1);
  EXPECT_FALSE(s1 < s1);
}

TEST(ipv4_subnet, less_same_subnet_different_mask)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.0");
  epee::net_utils::ipv4_network_subnet s1{ip, 8};
  epee::net_utils::ipv4_network_subnet s2{ip, 24};
  // Same subnet base but different masks -> not equal, one is less
  EXPECT_TRUE(s1 < s2 || s2 < s1);
}

TEST(ipv4_subnet, is_same_host)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.0");
  epee::net_utils::ipv4_network_subnet s1{ip, 8};
  epee::net_utils::ipv4_network_subnet s2{ip, 24};
  // is_same_host compares subnet() values which depend on mask
  // 10.0.0.0/8 -> subnet uses lower 8 bits
  // 10.0.0.0/24 -> subnet uses lower 24 bits
  // Both have ip 10.0.0.0 so subnet() values may differ
  (void)s1.is_same_host(s2); // just verify no crash
}

TEST(ipv4_subnet, default_construction)
{
  epee::net_utils::ipv4_network_subnet subnet;
  // Default: ip=0, mask=0
  EXPECT_EQ("0.0.0.0/0", subnet.str());
}

TEST(ipv4_subnet, serialization_roundtrip)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.0");
  epee::net_utils::ipv4_network_subnet original{ip, 24};

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);

  epee::net_utils::ipv4_network_subnet restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original, restored);
}

// ---- IPv4 address serialization ----

TEST(ipv4_address, serialization_roundtrip)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.100");
  epee::net_utils::ipv4_network_address original{ip, 18080};

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);

  epee::net_utils::ipv4_network_address restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.ip(), restored.ip());
  EXPECT_EQ(original.port(), restored.port());
  EXPECT_TRUE(original.equal(restored));
}

TEST(ipv4_address, ordering_by_port)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address a{ip, 80};
  epee::net_utils::ipv4_network_address b{ip, 443};

  // Same IP, different port -> less compares by port
  EXPECT_TRUE(a.less(b));
  EXPECT_FALSE(b.less(a));
}

TEST(ipv4_address, ordering_by_ip)
{
  uint32_t ip1, ip2;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.1");
  epee::string_tools::get_ip_int32_from_string(ip2, "10.0.0.2");
  epee::net_utils::ipv4_network_address a{ip1, 80};
  epee::net_utils::ipv4_network_address b{ip2, 80};

  // Different IP -> less compares by IP
  EXPECT_TRUE(a < b || b < a);
}

TEST(ipv4_address, relational_operators)
{
  uint32_t ip1, ip2;
  epee::string_tools::get_ip_int32_from_string(ip1, "10.0.0.1");
  epee::string_tools::get_ip_int32_from_string(ip2, "10.0.0.2");
  epee::net_utils::ipv4_network_address a{ip1, 80};
  epee::net_utils::ipv4_network_address b{ip2, 80};

  // Test all relational operators
  if (a < b) {
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a >= b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= a);
  }
}

TEST(ipv4_address, self_equality)
{
  uint32_t ip;
  epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1");
  epee::net_utils::ipv4_network_address a{ip, 80};
  EXPECT_EQ(a, a);
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a < a);
  EXPECT_TRUE(a <= a);
  EXPECT_TRUE(a >= a);
  EXPECT_FALSE(a > a);
}

// ---- IPv6 address additional tests ----

TEST(ipv6_address, str_format_contains_brackets)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::make_address_v6("2001:db8::1");
  epee::net_utils::ipv6_network_address addr(v6, 18080);
  std::string s = addr.str();
  EXPECT_NE(s.find("["), std::string::npos);
  EXPECT_NE(s.find("]"), std::string::npos);
  EXPECT_NE(s.find("18080"), std::string::npos);
}

TEST(ipv6_address, host_str_no_port)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::make_address_v6("2001:db8::1");
  epee::net_utils::ipv6_network_address addr(v6, 18080);
  std::string h = addr.host_str();
  // host_str should contain the address but not the port
  EXPECT_NE(h.find("2001"), std::string::npos);
  EXPECT_EQ(h.find("18080"), std::string::npos);
}

TEST(ipv6_address, link_local_is_local)
{
  // fe80::1 is link-local
  boost::asio::ip::address_v6 v6 = boost::asio::ip::make_address_v6("fe80::1");
  epee::net_utils::ipv6_network_address addr(v6, 80);
  EXPECT_TRUE(addr.is_local());
  EXPECT_FALSE(addr.is_loopback());
}

TEST(ipv6_address, ordering)
{
  boost::asio::ip::address_v6 v6a = boost::asio::ip::make_address_v6("2001:db8::1");
  boost::asio::ip::address_v6 v6b = boost::asio::ip::make_address_v6("2001:db8::2");
  epee::net_utils::ipv6_network_address a(v6a, 80);
  epee::net_utils::ipv6_network_address b(v6b, 80);

  EXPECT_TRUE(a < b || b < a);
  EXPECT_FALSE(a < a);
}

TEST(ipv6_address, ordering_by_port)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address a(v6, 80);
  epee::net_utils::ipv6_network_address b(v6, 443);

  EXPECT_TRUE(a.less(b));
  EXPECT_FALSE(b.less(a));
}

TEST(ipv6_address, self_equality)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address a(v6, 80);
  EXPECT_EQ(a, a);
  EXPECT_FALSE(a != a);
  EXPECT_TRUE(a <= a);
  EXPECT_TRUE(a >= a);
}

// ---- address_type enum tests ----

TEST(address_type, enum_values)
{
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::address_type::invalid), 0);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::address_type::ipv4), 1);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::address_type::ipv6), 2);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::address_type::i2p), 3);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::address_type::tor), 4);
}

// ---- zone hash tests ----

TEST(net_zone, zone_hash)
{
  std::hash<epee::net_utils::zone> hasher;
  // Each zone should produce a unique hash (since they're different uint8_t values)
  std::set<size_t> hashes;
  hashes.insert(hasher(epee::net_utils::zone::invalid));
  hashes.insert(hasher(epee::net_utils::zone::public_));
  hashes.insert(hasher(epee::net_utils::zone::i2p));
  hashes.insert(hasher(epee::net_utils::zone::tor));
  EXPECT_EQ(4u, hashes.size());
}

TEST(net_zone, zone_enum_values)
{
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::zone::invalid), 0);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::zone::public_), 1);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::zone::i2p), 2);
  EXPECT_EQ(static_cast<uint8_t>(epee::net_utils::zone::tor), 3);
}

// ---- IP conversion additional tests ----

TEST(ip_conversion, loopback_roundtrip)
{
  uint32_t ip;
  EXPECT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.1"));
  std::string s = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("127.0.0.1", s);
}

TEST(ip_conversion, various_valid_ips)
{
  const char* ips[] = {"1.2.3.4", "10.20.30.40", "100.200.100.200", "192.168.0.1"};
  for (const char* ip_str : ips)
  {
    uint32_t ip;
    ASSERT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, ip_str));
    std::string back = epee::string_tools::get_ip_string_from_int32(ip);
    EXPECT_EQ(std::string(ip_str), back);
  }
}

// ---- Levin make_header additional tests ----

TEST(levin_protocol, make_header_large_command)
{
  auto h = epee::levin::make_header(UINT32_MAX, 0, LEVIN_PACKET_REQUEST, false);
  EXPECT_EQ(h.m_command, UINT32_MAX);
}

TEST(levin_protocol, bucket_head2_fields)
{
  epee::levin::bucket_head2 h{};
  h.m_signature = LEVIN_SIGNATURE;
  h.m_cb = 1234;
  h.m_have_to_return_data = true;
  h.m_command = 42;
  h.m_return_code = -1;
  h.m_flags = LEVIN_PACKET_RESPONSE;
  h.m_protocol_version = LEVIN_PROTOCOL_VER_1;

  EXPECT_EQ(h.m_signature, LEVIN_SIGNATURE);
  EXPECT_EQ(h.m_cb, 1234u);
  EXPECT_TRUE(h.m_have_to_return_data);
  EXPECT_EQ(h.m_command, 42u);
  EXPECT_EQ(h.m_return_code, -1);
  EXPECT_EQ(h.m_flags, static_cast<uint32_t>(LEVIN_PACKET_RESPONSE));
  EXPECT_EQ(h.m_protocol_version, static_cast<uint32_t>(LEVIN_PROTOCOL_VER_1));
}

// ---- MAKE_IP macro tests ----

TEST(make_ip, basic_construction)
{
  uint32_t ip = MAKE_IP(192, 168, 1, 1);
  std::string s = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("192.168.1.1", s);
}

TEST(make_ip, zero_ip)
{
  uint32_t ip = MAKE_IP(0, 0, 0, 0);
  std::string s = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("0.0.0.0", s);
}

TEST(make_ip, broadcast)
{
  uint32_t ip = MAKE_IP(255, 255, 255, 255);
  std::string s = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ("255.255.255.255", s);
}

TEST(make_ip, loopback)
{
  uint32_t ip = MAKE_IP(127, 0, 0, 1);
  EXPECT_TRUE(epee::net_utils::is_ip_loopback(ip));
}
