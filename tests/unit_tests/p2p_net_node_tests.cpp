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
#include "p2p/net_peerlist.h"
#include "p2p/p2p_protocol_defs.h"
#include "net/net_utils_base.h"
#include "string_tools.h"

#include <boost/asio/ip/address_v6.hpp>

namespace
{
  epee::net_utils::network_address make_ipv4_addr(uint32_t ip, uint16_t port)
  {
    return epee::net_utils::ipv4_network_address(ip, port);
  }

  epee::net_utils::network_address make_ipv4_addr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port)
  {
    return epee::net_utils::ipv4_network_address(MAKE_IP(a, b, c, d), port);
  }
}

// ---- IPv4 network address construction ----

TEST(P2PNetNode, IPv4AddressConstruction)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(192, 168, 1, 100), 18080);
  EXPECT_EQ(MAKE_IP(192, 168, 1, 100), addr.ip());
  EXPECT_EQ(18080, addr.port());
}

TEST(P2PNetNode, IPv4AddressDefaultConstruction)
{
  epee::net_utils::ipv4_network_address addr;
  EXPECT_EQ(0u, addr.ip());
  EXPECT_EQ(0u, addr.port());
}

TEST(P2PNetNode, IPv4AddressStr)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(10, 0, 0, 1), 8080);
  std::string str = addr.str();
  EXPECT_FALSE(str.empty());
  // Should contain the IP and port
  EXPECT_NE(std::string::npos, str.find("10.0.0.1"));
  EXPECT_NE(std::string::npos, str.find("8080"));
}

TEST(P2PNetNode, IPv4AddressHostStr)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(10, 0, 0, 1), 8080);
  std::string host = addr.host_str();
  EXPECT_EQ("10.0.0.1", host);
}

// ---- Loopback detection ----

TEST(P2PNetNode, IPv4LoopbackDetection)
{
  epee::net_utils::ipv4_network_address loopback(MAKE_IP(127, 0, 0, 1), 18080);
  EXPECT_TRUE(loopback.is_loopback());
}

TEST(P2PNetNode, IPv4NonLoopback)
{
  epee::net_utils::ipv4_network_address non_loopback(MAKE_IP(192, 168, 1, 1), 18080);
  EXPECT_FALSE(non_loopback.is_loopback());
}

TEST(P2PNetNode, IPv4Loopback127x)
{
  // Any 127.x.x.x is loopback
  epee::net_utils::ipv4_network_address addr(MAKE_IP(127, 1, 2, 3), 8080);
  EXPECT_TRUE(addr.is_loopback());
}

// ---- Local address detection ----

TEST(P2PNetNode, IPv4LocalAddress10)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(10, 0, 0, 1), 18080);
  EXPECT_TRUE(addr.is_local());
}

TEST(P2PNetNode, IPv4LocalAddress192168)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(192, 168, 1, 1), 18080);
  EXPECT_TRUE(addr.is_local());
}

TEST(P2PNetNode, IPv4LocalAddress172)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(172, 16, 0, 1), 18080);
  EXPECT_TRUE(addr.is_local());
}

TEST(P2PNetNode, IPv4NotLocal)
{
  epee::net_utils::ipv4_network_address addr(MAKE_IP(8, 8, 8, 8), 18080);
  EXPECT_FALSE(addr.is_local());
}

// ---- IPv4 comparison ----

TEST(P2PNetNode, IPv4EqualSameAddress)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 2, 3, 4), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(1, 2, 3, 4), 100);
  EXPECT_TRUE(a.equal(b));
  EXPECT_EQ(a, b);
}

TEST(P2PNetNode, IPv4NotEqualDifferentIP)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 2, 3, 4), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(5, 6, 7, 8), 100);
  EXPECT_FALSE(a.equal(b));
  EXPECT_NE(a, b);
}

TEST(P2PNetNode, IPv4NotEqualDifferentPort)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 2, 3, 4), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(1, 2, 3, 4), 200);
  EXPECT_FALSE(a.equal(b));
  EXPECT_NE(a, b);
}

TEST(P2PNetNode, IPv4SameHostDifferentPort)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 2, 3, 4), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(1, 2, 3, 4), 200);
  EXPECT_TRUE(a.is_same_host(b));
}

TEST(P2PNetNode, IPv4DifferentHost)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 2, 3, 4), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(5, 6, 7, 8), 100);
  EXPECT_FALSE(a.is_same_host(b));
}

TEST(P2PNetNode, IPv4LessOrdering)
{
  epee::net_utils::ipv4_network_address a(MAKE_IP(1, 0, 0, 0), 100);
  epee::net_utils::ipv4_network_address b(MAKE_IP(2, 0, 0, 0), 100);
  EXPECT_TRUE(a.less(b));
  EXPECT_FALSE(b.less(a));
}

// ---- IPv6 network address ----

TEST(P2PNetNode, IPv6AddressConstruction)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address addr(v6, 18080);
  EXPECT_EQ(v6, addr.ip());
  EXPECT_EQ(18080, addr.port());
}

TEST(P2PNetNode, IPv6AddressDefaultConstruction)
{
  epee::net_utils::ipv6_network_address addr;
  EXPECT_EQ(boost::asio::ip::address_v6::loopback(), addr.ip());
  EXPECT_EQ(0, addr.port());
}

TEST(P2PNetNode, IPv6LoopbackDetection)
{
  epee::net_utils::ipv6_network_address addr(boost::asio::ip::address_v6::loopback(), 18080);
  EXPECT_TRUE(addr.is_loopback());
}

TEST(P2PNetNode, IPv6NonLoopback)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::make_address_v6("2001:db8::1");
  epee::net_utils::ipv6_network_address addr(v6, 18080);
  EXPECT_FALSE(addr.is_loopback());
}

TEST(P2PNetNode, IPv6EqualSameAddress)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address a(v6, 100);
  epee::net_utils::ipv6_network_address b(v6, 100);
  EXPECT_TRUE(a.equal(b));
  EXPECT_EQ(a, b);
}

TEST(P2PNetNode, IPv6DifferentAddresses)
{
  boost::asio::ip::address_v6 v6a = boost::asio::ip::address_v6::loopback();
  boost::asio::ip::address_v6 v6b = boost::asio::ip::make_address_v6("2001:db8::1");
  epee::net_utils::ipv6_network_address a(v6a, 100);
  epee::net_utils::ipv6_network_address b(v6b, 100);
  EXPECT_FALSE(a.equal(b));
  EXPECT_NE(a, b);
}

TEST(P2PNetNode, IPv6SameHostDifferentPort)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address a(v6, 100);
  epee::net_utils::ipv6_network_address b(v6, 200);
  EXPECT_TRUE(a.is_same_host(b));
}

TEST(P2PNetNode, IPv6Str)
{
  boost::asio::ip::address_v6 v6 = boost::asio::ip::address_v6::loopback();
  epee::net_utils::ipv6_network_address addr(v6, 18080);
  std::string str = addr.str();
  EXPECT_FALSE(str.empty());
}

// ---- network_address wrapper ----

TEST(P2PNetNode, NetworkAddressIPv4TypeId)
{
  epee::net_utils::network_address addr = make_ipv4_addr(192, 168, 1, 1, 8080);
  EXPECT_EQ(epee::net_utils::address_type::ipv4, addr.get_type_id());
}

TEST(P2PNetNode, NetworkAddressIPv4Str)
{
  epee::net_utils::network_address addr = make_ipv4_addr(10, 0, 0, 1, 3000);
  std::string str = addr.str();
  EXPECT_FALSE(str.empty());
}

TEST(P2PNetNode, NetworkAddressIPv4IsLoopback)
{
  epee::net_utils::network_address addr = make_ipv4_addr(127, 0, 0, 1, 8080);
  EXPECT_TRUE(addr.is_loopback());
}

TEST(P2PNetNode, NetworkAddressIPv4IsLocal)
{
  epee::net_utils::network_address addr = make_ipv4_addr(192, 168, 1, 1, 8080);
  EXPECT_TRUE(addr.is_local());
}

TEST(P2PNetNode, NetworkAddressIPv4Comparison)
{
  epee::net_utils::network_address a = make_ipv4_addr(1, 2, 3, 4, 100);
  epee::net_utils::network_address b = make_ipv4_addr(1, 2, 3, 4, 100);
  EXPECT_EQ(a, b);
}

TEST(P2PNetNode, NetworkAddressIPv4Zone)
{
  epee::net_utils::network_address addr = make_ipv4_addr(8, 8, 8, 8, 80);
  EXPECT_EQ(epee::net_utils::zone::public_, addr.get_zone());
}

// ---- Peerlist entry ----

TEST(P2PNetNode, PeerlistEntryCreation)
{
  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(192, 168, 1, 1, 18080);
  pe.id = 12345;
  pe.last_seen = 1000;
  pe.pruning_seed = 0;
  pe.rpc_port = 18081;
  pe.rpc_credits_per_hash = 0;

  EXPECT_EQ(12345u, pe.id);
  EXPECT_EQ(1000, pe.last_seen);
  EXPECT_EQ(18081, pe.rpc_port);
}

// ---- Peer ID to string ----

TEST(P2PNetNode, PeerIdToStringNonZero)
{
  nodetool::peerid_type id = 0xDEADBEEF;
  std::string str = nodetool::peerid_to_string(id);
  EXPECT_EQ(16u, str.size());
  EXPECT_NE(std::string::npos, str.find("deadbeef"));
}

TEST(P2PNetNode, PeerIdToStringZero)
{
  nodetool::peerid_type id = 0;
  std::string str = nodetool::peerid_to_string(id);
  EXPECT_EQ(16u, str.size());
  EXPECT_EQ("0000000000000000", str);
}

TEST(P2PNetNode, PeerIdToStringMax)
{
  nodetool::peerid_type id = UINT64_MAX;
  std::string str = nodetool::peerid_to_string(id);
  EXPECT_EQ(16u, str.size());
  EXPECT_EQ("ffffffffffffffff", str);
}

// ---- Peerlist manager basic operations ----

TEST(P2PNetNode, PeerlistManagerInit)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false);
  EXPECT_EQ(0u, plm.get_white_peers_count());
  EXPECT_EQ(0u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistManagerAddWhite)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe.id = 1;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe);
  EXPECT_EQ(1u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistManagerAddGray)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(8, 8, 4, 4, 18080);
  pe.id = 2;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  plm.append_with_peer_gray(pe);
  EXPECT_EQ(1u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistLoopbackRejected)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false);

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(127, 0, 0, 1, 18080);
  pe.id = 1;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe);
  // Loopback should be rejected
  EXPECT_EQ(0u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistLocalRejectedWhenDisallowed)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false); // allow_local_ip = false

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(192, 168, 1, 1, 18080);
  pe.id = 1;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe);
  // Local IPs should be rejected when allow_local_ip is false
  EXPECT_EQ(0u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistLocalAllowedWhenEnabled)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true); // allow_local_ip = true

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(192, 168, 1, 1, 18080);
  pe.id = 1;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe);
  EXPECT_EQ(1u, plm.get_white_peers_count());
}

// ---- ipv4_network_subnet ----

TEST(P2PNetNode, IPv4SubnetConstruction)
{
  epee::net_utils::ipv4_network_subnet subnet(MAKE_IP(192, 168, 1, 0), 24);
  EXPECT_FALSE(subnet.str().empty());
}

TEST(P2PNetNode, IPv4SubnetEquality)
{
  epee::net_utils::ipv4_network_subnet a(MAKE_IP(10, 0, 0, 0), 8);
  epee::net_utils::ipv4_network_subnet b(MAKE_IP(10, 0, 0, 0), 8);
  EXPECT_EQ(a, b);
}

TEST(P2PNetNode, IPv4SubnetInequality)
{
  epee::net_utils::ipv4_network_subnet a(MAKE_IP(10, 0, 0, 0), 8);
  epee::net_utils::ipv4_network_subnet b(MAKE_IP(172, 16, 0, 0), 12);
  EXPECT_NE(a, b);
}

TEST(P2PNetNode, IPv4SubnetMatches)
{
  epee::net_utils::ipv4_network_subnet subnet(MAKE_IP(192, 168, 1, 0), 24);
  epee::net_utils::ipv4_network_address addr(MAKE_IP(192, 168, 1, 100), 8080);
  EXPECT_TRUE(subnet.matches(addr));
}

TEST(P2PNetNode, IPv4SubnetDoesNotMatch)
{
  epee::net_utils::ipv4_network_subnet subnet(MAKE_IP(192, 168, 1, 0), 24);
  epee::net_utils::ipv4_network_address addr(MAKE_IP(192, 168, 2, 100), 8080);
  EXPECT_FALSE(subnet.matches(addr));
}
