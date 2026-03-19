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

// ---- P2P protocol structure serialization tests ----

#include "storages/portable_storage_template_helper.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "crypto/crypto.h"
#include "cryptonote_config.h"

TEST(P2PProtocol, BasicNodeDataSerializationRoundtrip)
{
  nodetool::basic_node_data original;
  original.network_id = ::config::NETWORK_ID;
  original.peer_id = 0xDEADBEEFCAFEBABE;
  original.my_port = 18080;
  original.rpc_port = 18081;
  original.rpc_credits_per_hash = 100;
  original.support_flags = 0x01;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  nodetool::basic_node_data restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.network_id, restored.network_id);
  EXPECT_EQ(original.peer_id, restored.peer_id);
  EXPECT_EQ(original.my_port, restored.my_port);
  EXPECT_EQ(original.rpc_port, restored.rpc_port);
  EXPECT_EQ(original.rpc_credits_per_hash, restored.rpc_credits_per_hash);
  EXPECT_EQ(original.support_flags, restored.support_flags);
}

TEST(P2PProtocol, BasicNodeDataDefaultOptionalFields)
{
  // When optional fields are zero, they should still roundtrip correctly
  nodetool::basic_node_data original;
  original.network_id = ::config::testnet::NETWORK_ID;
  original.peer_id = 1;
  original.my_port = 28080;
  original.rpc_port = 0;
  original.rpc_credits_per_hash = 0;
  original.support_flags = 0;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);

  nodetool::basic_node_data restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.network_id, restored.network_id);
  EXPECT_EQ(original.peer_id, restored.peer_id);
  EXPECT_EQ(original.my_port, restored.my_port);
  EXPECT_EQ(0, restored.rpc_port);
  EXPECT_EQ(0u, restored.rpc_credits_per_hash);
  EXPECT_EQ(0u, restored.support_flags);
}

TEST(P2PProtocol, BasicNodeDataNetworkIdPreserved)
{
  // Verify that the 16-byte network_id POD blob survives serialization byte-for-byte
  nodetool::basic_node_data original;
  original.network_id = ::config::stagenet::NETWORK_ID;
  original.peer_id = 0;
  original.my_port = 38080;
  original.rpc_port = 0;
  original.rpc_credits_per_hash = 0;
  original.support_flags = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::basic_node_data restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(0, memcmp(&original.network_id, &restored.network_id, sizeof(original.network_id)));
}

TEST(P2PProtocol, PingResponseSerializationRoundtrip)
{
  nodetool::COMMAND_PING::response_t original;
  original.status = PING_OK_RESPONSE_STATUS_TEXT;
  original.peer_id = 0x1234567890ABCDEF;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  nodetool::COMMAND_PING::response_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.status, restored.status);
  EXPECT_EQ(original.peer_id, restored.peer_id);
}

TEST(P2PProtocol, PingResponseEmptyStatus)
{
  nodetool::COMMAND_PING::response_t original;
  original.status = "";
  original.peer_id = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::COMMAND_PING::response_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ("", restored.status);
  EXPECT_EQ(0u, restored.peer_id);
}

TEST(P2PProtocol, PingRequestSerializationRoundtrip)
{
  // Ping request has no fields but should serialize/deserialize successfully
  nodetool::COMMAND_PING::request_t original;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::COMMAND_PING::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));
}

TEST(P2PProtocol, SupportFlagsResponseSerializationRoundtrip)
{
  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::response_t original;
  original.support_flags = 0xFFFFFFFF;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::response_t restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.support_flags, restored.support_flags);
}

TEST(P2PProtocol, SupportFlagsResponseZero)
{
  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::response_t original;
  original.support_flags = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::response_t restored;
  restored.support_flags = 999; // pre-fill to verify it gets overwritten
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(0u, restored.support_flags);
}

TEST(P2PProtocol, SupportFlagsRequestSerializationRoundtrip)
{
  // Request has no fields but should serialize/deserialize successfully
  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::request_t original;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::request_t restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));
}

TEST(P2PProtocol, CoreSyncDataSerializationRoundtrip)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 2500000;
  original.cumulative_difficulty = 0xFEDCBA9876543210;
  original.cumulative_difficulty_top64 = 0x0123456789ABCDEF;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 16;
  original.pruning_seed = 384;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  cryptonote::CORE_SYNC_DATA restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.current_height, restored.current_height);
  EXPECT_EQ(original.cumulative_difficulty, restored.cumulative_difficulty);
  EXPECT_EQ(original.cumulative_difficulty_top64, restored.cumulative_difficulty_top64);
  EXPECT_EQ(original.top_id, restored.top_id);
  EXPECT_EQ(original.top_version, restored.top_version);
  EXPECT_EQ(original.pruning_seed, restored.pruning_seed);
}

TEST(P2PProtocol, CoreSyncDataDefaultOptionalFields)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 1;
  original.cumulative_difficulty = 1;
  original.cumulative_difficulty_top64 = 0;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 0;
  original.pruning_seed = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  cryptonote::CORE_SYNC_DATA restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(original.current_height, restored.current_height);
  EXPECT_EQ(original.cumulative_difficulty, restored.cumulative_difficulty);
  EXPECT_EQ(0u, restored.cumulative_difficulty_top64);
  EXPECT_EQ(original.top_id, restored.top_id);
  EXPECT_EQ(0, restored.top_version);
  EXPECT_EQ(0u, restored.pruning_seed);
}

TEST(P2PProtocol, CoreSyncDataTopIdBlobPreserved)
{
  // Ensure the 32-byte hash POD blob survives roundtrip
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = 100;
  original.cumulative_difficulty = 50;
  original.cumulative_difficulty_top64 = 0;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 0;
  original.pruning_seed = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  cryptonote::CORE_SYNC_DATA restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(0, memcmp(original.top_id.data, restored.top_id.data, sizeof(crypto::hash)));
}

TEST(P2PProtocol, CoreSyncDataLargeHeight)
{
  cryptonote::CORE_SYNC_DATA original;
  original.current_height = UINT64_MAX;
  original.cumulative_difficulty = UINT64_MAX;
  original.cumulative_difficulty_top64 = UINT64_MAX;
  original.top_id = crypto::rand<crypto::hash>();
  original.top_version = 255;
  original.pruning_seed = UINT32_MAX;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  cryptonote::CORE_SYNC_DATA restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(UINT64_MAX, restored.current_height);
  EXPECT_EQ(UINT64_MAX, restored.cumulative_difficulty);
  EXPECT_EQ(UINT64_MAX, restored.cumulative_difficulty_top64);
  EXPECT_EQ(255, restored.top_version);
  EXPECT_EQ(UINT32_MAX, restored.pruning_seed);
}

TEST(P2PProtocol, NetworkConfigSerializationRoundtrip)
{
  nodetool::network_config original;
  original.max_out_connection_count = 8;
  original.max_in_connection_count = 32;
  original.handshake_interval = 60;
  original.packet_max_size = 50000000;
  original.config_id = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::network_config restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(original.max_out_connection_count, restored.max_out_connection_count);
  EXPECT_EQ(original.max_in_connection_count, restored.max_in_connection_count);
  EXPECT_EQ(original.handshake_interval, restored.handshake_interval);
  EXPECT_EQ(original.packet_max_size, restored.packet_max_size);
  EXPECT_EQ(original.config_id, restored.config_id);
}

TEST(P2PProtocol, CommandIds)
{
  // Verify command IDs match expected values from the protocol
  // Use local copies to avoid ODR-use of static const members
  const int ping_id = nodetool::COMMAND_PING::ID;
  const int support_flags_id = nodetool::COMMAND_REQUEST_SUPPORT_FLAGS::ID;
  EXPECT_EQ(1003, ping_id);
  EXPECT_EQ(1007, support_flags_id);
}
