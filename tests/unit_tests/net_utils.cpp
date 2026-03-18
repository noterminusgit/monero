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
#include <boost/utility/string_ref.hpp>

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
