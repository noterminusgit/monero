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

// ---- Peerlist removal operations ----

TEST(P2PNetNode, PeerlistRemoveFromWhite)
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

  plm.remove_from_peer_white(pe);
  EXPECT_EQ(0u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistRemoveFromGray)
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

  plm.remove_from_peer_gray(pe);
  EXPECT_EQ(0u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistRemoveNonExistentWhite)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(1, 2, 3, 4, 18080);
  pe.id = 1;
  pe.last_seen = 0;
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  // Removing a non-existent peer should not crash and return true
  EXPECT_TRUE(plm.remove_from_peer_white(pe));
  EXPECT_EQ(0u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistRemoveNonExistentGray)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(1, 2, 3, 4, 18080);
  pe.id = 1;
  pe.last_seen = 0;
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;

  EXPECT_TRUE(plm.remove_from_peer_gray(pe));
  EXPECT_EQ(0u, plm.get_gray_peers_count());
}

// ---- Peerlist anchor operations ----

TEST(P2PNetNode, PeerlistAnchorAppend)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::anchor_peerlist_entry ape;
  ape.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  ape.id = 100;
  ape.first_seen = time(NULL);

  EXPECT_TRUE(plm.append_with_peer_anchor(ape));
}

TEST(P2PNetNode, PeerlistAnchorDuplicateIgnored)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::anchor_peerlist_entry ape;
  ape.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  ape.id = 100;
  ape.first_seen = time(NULL);

  EXPECT_TRUE(plm.append_with_peer_anchor(ape));
  // Adding same address again should succeed (no-op)
  EXPECT_TRUE(plm.append_with_peer_anchor(ape));
}

TEST(P2PNetNode, PeerlistGetAndEmptyAnchor)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::anchor_peerlist_entry ape1;
  ape1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  ape1.id = 1;
  ape1.first_seen = 1000;

  nodetool::anchor_peerlist_entry ape2;
  ape2.adr = make_ipv4_addr(8, 8, 4, 4, 18080);
  ape2.id = 2;
  ape2.first_seen = 2000;

  plm.append_with_peer_anchor(ape1);
  plm.append_with_peer_anchor(ape2);

  std::vector<nodetool::anchor_peerlist_entry> apl;
  EXPECT_TRUE(plm.get_and_empty_anchor_peerlist(apl));
  EXPECT_EQ(2u, apl.size());

  // After emptying, getting again should return empty
  std::vector<nodetool::anchor_peerlist_entry> apl2;
  EXPECT_TRUE(plm.get_and_empty_anchor_peerlist(apl2));
  EXPECT_EQ(0u, apl2.size());
}

TEST(P2PNetNode, PeerlistRemoveFromAnchor)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  epee::net_utils::network_address addr = make_ipv4_addr(8, 8, 8, 8, 18080);
  nodetool::anchor_peerlist_entry ape;
  ape.adr = addr;
  ape.id = 1;
  ape.first_seen = 1000;

  plm.append_with_peer_anchor(ape);
  EXPECT_TRUE(plm.remove_from_peer_anchor(addr));

  // After removal, anchor list should be empty
  std::vector<nodetool::anchor_peerlist_entry> apl;
  plm.get_and_empty_anchor_peerlist(apl);
  EXPECT_EQ(0u, apl.size());
}

// ---- Peerlist merge ----

TEST(P2PNetNode, PeerlistMerge)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  std::vector<nodetool::peerlist_entry> peers;
  for (int i = 1; i <= 5; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    peers.push_back(pe);
  }

  EXPECT_TRUE(plm.merge_peerlist(peers));
  // merge_peerlist adds to gray
  EXPECT_EQ(5u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistMergeSkipsLoopback)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false);

  std::vector<nodetool::peerlist_entry> peers;
  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(127, 0, 0, 1, 18080);
  pe.id = 1;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;
  peers.push_back(pe);

  EXPECT_TRUE(plm.merge_peerlist(peers));
  EXPECT_EQ(0u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistMergeWithFilter)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  std::vector<nodetool::peerlist_entry> peers;
  for (int i = 1; i <= 5; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    peers.push_back(pe);
  }

  // Filter that only accepts peers with id > 3
  auto filter = [](const nodetool::peerlist_entry& pe) { return pe.id > 3; };
  EXPECT_TRUE(plm.merge_peerlist(peers, filter));
  EXPECT_EQ(2u, plm.get_gray_peers_count()); // only id=4 and id=5
}

// ---- Peerlist get by index ----

TEST(P2PNetNode, PeerlistGetWhitePeerByIndex)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 0;
  pe1.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe1);

  nodetool::peerlist_entry result;
  EXPECT_TRUE(plm.get_white_peer_by_index(result, 0));
  EXPECT_EQ(pe1.id, result.id);
  EXPECT_EQ(pe1.adr, result.adr);
}

TEST(P2PNetNode, PeerlistGetWhitePeerByIndexOutOfBounds)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry result;
  EXPECT_FALSE(plm.get_white_peer_by_index(result, 0));
  EXPECT_FALSE(plm.get_white_peer_by_index(result, 100));
}

TEST(P2PNetNode, PeerlistGetGrayPeerByIndex)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 4, 4, 18080);
  pe1.id = 2;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 0;
  pe1.rpc_credits_per_hash = 0;

  plm.append_with_peer_gray(pe1);

  nodetool::peerlist_entry result;
  EXPECT_TRUE(plm.get_gray_peer_by_index(result, 0));
  EXPECT_EQ(pe1.id, result.id);
}

TEST(P2PNetNode, PeerlistGetGrayPeerByIndexOutOfBounds)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry result;
  EXPECT_FALSE(plm.get_gray_peer_by_index(result, 0));
}

// ---- Peerlist get_peerlist ----

TEST(P2PNetNode, PeerlistGetPeerlistBothLists)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 0;
  pe1.rpc_credits_per_hash = 0;

  nodetool::peerlist_entry pe2;
  pe2.adr = make_ipv4_addr(8, 8, 4, 4, 18080);
  pe2.id = 2;
  pe2.last_seen = time(NULL);
  pe2.pruning_seed = 0;
  pe2.rpc_port = 0;
  pe2.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe1);
  plm.append_with_peer_gray(pe2);

  std::vector<nodetool::peerlist_entry> gray, white;
  plm.get_peerlist(gray, white);
  EXPECT_EQ(1u, white.size());
  EXPECT_EQ(1u, gray.size());
}

TEST(P2PNetNode, PeerlistGetPeerlistTypes)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 0;
  pe1.rpc_credits_per_hash = 0;

  plm.append_with_peer_white(pe1);

  nodetool::peerlist_types peers;
  plm.get_peerlist(peers);
  EXPECT_EQ(1u, peers.white.size());
  EXPECT_EQ(0u, peers.gray.size());
}

// ---- Peerlist get_peerlist_head ----

TEST(P2PNetNode, PeerlistGetHead)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 10; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL) + i; // different times
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  std::vector<nodetool::peerlist_entry> head;
  EXPECT_TRUE(plm.get_peerlist_head(head, false, 5));
  EXPECT_EQ(5u, head.size());
}

TEST(P2PNetNode, PeerlistGetHeadAnonymized)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 5; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = 1000 + i;
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  std::vector<nodetool::peerlist_entry> head;
  EXPECT_TRUE(plm.get_peerlist_head(head, true, 3));
  EXPECT_LE(head.size(), 3u);
  // Anonymized peers should have last_seen = 0
  for (const auto& pe : head)
    EXPECT_EQ(0, pe.last_seen);
}

TEST(P2PNetNode, PeerlistGetHeadEmpty)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  std::vector<nodetool::peerlist_entry> head;
  EXPECT_TRUE(plm.get_peerlist_head(head, false, 10));
  EXPECT_EQ(0u, head.size());
}

// ---- Peerlist foreach ----

TEST(P2PNetNode, PeerlistForEachWhite)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 3; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  int count = 0;
  plm.foreach(true, [&count](const nodetool::peerlist_entry& pe) {
    ++count;
    return true; // continue
  });
  EXPECT_EQ(3, count);
}

TEST(P2PNetNode, PeerlistForEachGray)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 4; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_gray(pe);
  }

  int count = 0;
  plm.foreach(false, [&count](const nodetool::peerlist_entry& pe) {
    ++count;
    return true;
  });
  EXPECT_EQ(4, count);
}

TEST(P2PNetNode, PeerlistForEachEarlyStop)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 5; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  int count = 0;
  bool result = plm.foreach(true, [&count](const nodetool::peerlist_entry& pe) {
    ++count;
    return count < 2; // stop after 2nd
  });
  EXPECT_FALSE(result); // should return false when stopped early
  EXPECT_EQ(2, count);
}

// ---- Peerlist filter ----

TEST(P2PNetNode, PeerlistFilterWhite)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 5; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }
  EXPECT_EQ(5u, plm.get_white_peers_count());

  // Filter out peers with odd id (drop returns true)
  size_t filtered = plm.filter(true, [](const nodetool::peerlist_entry& pe) {
    return pe.id % 2 != 0; // drop odd
  });
  EXPECT_EQ(3u, filtered); // 1, 3, 5 removed
  EXPECT_EQ(2u, plm.get_white_peers_count()); // 2, 4 remain
}

TEST(P2PNetNode, PeerlistFilterGray)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 4; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_gray(pe);
  }

  // Filter out all
  size_t filtered = plm.filter(false, [](const nodetool::peerlist_entry&) { return true; });
  EXPECT_EQ(4u, filtered);
  EXPECT_EQ(0u, plm.get_gray_peers_count());
}

TEST(P2PNetNode, PeerlistFilterNone)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  for (int i = 1; i <= 3; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, 8, static_cast<uint8_t>(i), 1, 18080);
    pe.id = i;
    pe.last_seen = time(NULL);
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  // Filter that drops nothing
  size_t filtered = plm.filter(true, [](const nodetool::peerlist_entry&) { return false; });
  EXPECT_EQ(0u, filtered);
  EXPECT_EQ(3u, plm.get_white_peers_count());
}

// ---- Peerlist is_host_allowed ----

TEST(P2PNetNode, IsHostAllowed_PublicIP)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false);

  epee::net_utils::network_address addr = make_ipv4_addr(8, 8, 8, 8, 18080);
  EXPECT_TRUE(plm.is_host_allowed(addr));
}

TEST(P2PNetNode, IsHostAllowed_LoopbackRejected)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  epee::net_utils::network_address addr = make_ipv4_addr(127, 0, 0, 1, 18080);
  EXPECT_FALSE(plm.is_host_allowed(addr));
}

TEST(P2PNetNode, IsHostAllowed_LocalRejectedWhenDisabled)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, false);

  epee::net_utils::network_address addr = make_ipv4_addr(192, 168, 1, 1, 18080);
  EXPECT_FALSE(plm.is_host_allowed(addr));
}

TEST(P2PNetNode, IsHostAllowed_LocalAcceptedWhenEnabled)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  epee::net_utils::network_address addr = make_ipv4_addr(192, 168, 1, 1, 18080);
  EXPECT_TRUE(plm.is_host_allowed(addr));
}

// ---- Peerlist set_peer_just_seen ----

TEST(P2PNetNode, PeerlistSetPeerJustSeen)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  epee::net_utils::network_address addr = make_ipv4_addr(8, 8, 8, 8, 18080);
  EXPECT_TRUE(plm.set_peer_just_seen(42, addr, 0, 18081, 0));
  EXPECT_EQ(1u, plm.get_white_peers_count());

  // Calling again should update, not add duplicate
  EXPECT_TRUE(plm.set_peer_just_seen(42, addr, 384, 18081, 100));
  EXPECT_EQ(1u, plm.get_white_peers_count());
}

// ---- Peerlist white-gray interaction ----

TEST(P2PNetNode, PeerlistWhiteRemovesFromGray)
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

  // First add to gray
  plm.append_with_peer_gray(pe);
  EXPECT_EQ(1u, plm.get_gray_peers_count());
  EXPECT_EQ(0u, plm.get_white_peers_count());

  // Now add same address to white - should remove from gray
  plm.append_with_peer_white(pe);
  EXPECT_EQ(0u, plm.get_gray_peers_count());
  EXPECT_EQ(1u, plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistGraySkipsIfInWhite)
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

  // First add to white
  plm.append_with_peer_white(pe);
  EXPECT_EQ(1u, plm.get_white_peers_count());

  // Try adding same address to gray - should be skipped
  plm.append_with_peer_gray(pe);
  EXPECT_EQ(0u, plm.get_gray_peers_count());
  EXPECT_EQ(1u, plm.get_white_peers_count());
}

// ---- Peerlist get_random_gray_peer ----

TEST(P2PNetNode, PeerlistGetRandomGrayPeerEmpty)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe;
  EXPECT_FALSE(plm.get_random_gray_peer(pe));
}

TEST(P2PNetNode, PeerlistGetRandomGrayPeerNonEmpty)
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
  plm.append_with_peer_gray(pe);

  nodetool::peerlist_entry result;
  EXPECT_TRUE(plm.get_random_gray_peer(result));
  EXPECT_EQ(pe.adr, result.adr);
}

// ---- Peerlist init with pre-populated data ----

TEST(P2PNetNode, PeerlistInitWithPrePopulated)
{
  nodetool::peerlist_types init_peers;

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 0;
  pe1.rpc_credits_per_hash = 0;
  init_peers.white.push_back(pe1);

  nodetool::peerlist_entry pe2;
  pe2.adr = make_ipv4_addr(8, 8, 4, 4, 18080);
  pe2.id = 2;
  pe2.last_seen = time(NULL);
  pe2.pruning_seed = 0;
  pe2.rpc_port = 0;
  pe2.rpc_credits_per_hash = 0;
  init_peers.gray.push_back(pe2);

  nodetool::peerlist_manager plm;
  plm.init(std::move(init_peers), true);

  EXPECT_EQ(1u, plm.get_white_peers_count());
  EXPECT_EQ(1u, plm.get_gray_peers_count());
}

// ---- print_peerlist_to_string ----

TEST(P2PNetNode, PrintPeerlistToString)
{
  std::vector<nodetool::peerlist_entry> pl;

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe.id = 0xDEADBEEF;
  pe.last_seen = time(NULL);
  pe.pruning_seed = 384;
  pe.rpc_port = 18081;
  pe.rpc_credits_per_hash = 0;
  pl.push_back(pe);

  std::string result = nodetool::print_peerlist_to_string(pl);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("8.8.8.8"), std::string::npos);
  EXPECT_NE(result.find("deadbeef"), std::string::npos);
}

TEST(P2PNetNode, PrintPeerlistToStringEmpty)
{
  std::vector<nodetool::peerlist_entry> pl;
  std::string result = nodetool::print_peerlist_to_string(pl);
  EXPECT_TRUE(result.empty());
}

TEST(P2PNetNode, PrintPeerlistToStringNeverSeen)
{
  std::vector<nodetool::peerlist_entry> pl;

  nodetool::peerlist_entry pe;
  pe.adr = make_ipv4_addr(1, 2, 3, 4, 80);
  pe.id = 0;
  pe.last_seen = 0; // never seen
  pe.pruning_seed = 0;
  pe.rpc_port = 0;
  pe.rpc_credits_per_hash = 0;
  pl.push_back(pe);

  std::string result = nodetool::print_peerlist_to_string(pl);
  EXPECT_NE(result.find("never"), std::string::npos);
}

// ---- Peerlist entry serialization ----

TEST(P2PProtocol, PeerlistEntrySerializationRoundtrip)
{
  nodetool::peerlist_entry original;
  original.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  original.id = 0xDEADBEEFCAFEBABE;
  original.last_seen = 1234567890;
  original.pruning_seed = 384;
  original.rpc_port = 18081;
  original.rpc_credits_per_hash = 100;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  nodetool::peerlist_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.adr, restored.adr);
  EXPECT_EQ(original.id, restored.id);
  EXPECT_EQ(original.last_seen, restored.last_seen);
  EXPECT_EQ(original.pruning_seed, restored.pruning_seed);
  EXPECT_EQ(original.rpc_port, restored.rpc_port);
  EXPECT_EQ(original.rpc_credits_per_hash, restored.rpc_credits_per_hash);
}

TEST(P2PProtocol, PeerlistEntryDefaultOptionals)
{
  nodetool::peerlist_entry original;
  original.adr = make_ipv4_addr(1, 2, 3, 4, 80);
  original.id = 1;
  original.last_seen = 0;
  original.pruning_seed = 0;
  original.rpc_port = 0;
  original.rpc_credits_per_hash = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(original, blob));

  nodetool::peerlist_entry restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size())));

  EXPECT_EQ(0, restored.last_seen);
  EXPECT_EQ(0u, restored.pruning_seed);
  EXPECT_EQ(0, restored.rpc_port);
  EXPECT_EQ(0u, restored.rpc_credits_per_hash);
}

TEST(P2PProtocol, AnchorPeerlistEntrySerializationRoundtrip)
{
  nodetool::anchor_peerlist_entry original;
  original.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  original.id = 42;
  original.first_seen = 9999999;

  epee::byte_slice blob;
  bool res = epee::serialization::store_t_to_binary(original, blob);
  ASSERT_TRUE(res);
  ASSERT_FALSE(blob.empty());

  nodetool::anchor_peerlist_entry restored;
  res = epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(blob.data(), blob.size()));
  ASSERT_TRUE(res);

  EXPECT_EQ(original.adr, restored.adr);
  EXPECT_EQ(original.id, restored.id);
  EXPECT_EQ(original.first_seen, restored.first_seen);
}

// ---- Additional protocol command IDs ----

TEST(P2PProtocol, AllCommandIdsExpected)
{
  // Verify all BC_COMMANDS_POOL_BASE offsets
  const int new_block_id = cryptonote::NOTIFY_NEW_BLOCK::ID;
  const int new_txs_id = cryptonote::NOTIFY_NEW_TRANSACTIONS::ID;
  const int req_get_objects_id = cryptonote::NOTIFY_REQUEST_GET_OBJECTS::ID;
  const int resp_get_objects_id = cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::ID;
  const int req_chain_id = cryptonote::NOTIFY_REQUEST_CHAIN::ID;
  const int resp_chain_id = cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::ID;
  const int new_fluffy_id = cryptonote::NOTIFY_NEW_FLUFFY_BLOCK::ID;
  const int req_fluffy_missing_id = cryptonote::NOTIFY_REQUEST_FLUFFY_MISSING_TX::ID;
  const int get_txpool_complement_id = cryptonote::NOTIFY_GET_TXPOOL_COMPLEMENT::ID;

  EXPECT_EQ(2001, new_block_id);
  EXPECT_EQ(2002, new_txs_id);
  EXPECT_EQ(2003, req_get_objects_id);
  EXPECT_EQ(2004, resp_get_objects_id);
  EXPECT_EQ(2006, req_chain_id);
  EXPECT_EQ(2007, resp_chain_id);
  EXPECT_EQ(2008, new_fluffy_id);
  EXPECT_EQ(2009, req_fluffy_missing_id);
  EXPECT_EQ(2010, get_txpool_complement_id);
}

// ---- Peerlist update preserves pruning_seed ----

TEST(P2PNetNode, PeerlistWhiteUpdatePreservesPruningSeed)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  // Add peer with pruning_seed
  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 384;
  pe1.rpc_port = 18081;
  pe1.rpc_credits_per_hash = 0;
  plm.append_with_peer_white(pe1);

  // Update with no pruning_seed (simulating older node)
  nodetool::peerlist_entry pe2;
  pe2.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe2.id = 1;
  pe2.last_seen = time(NULL) + 100;
  pe2.pruning_seed = 0;  // older node doesn't have this
  pe2.rpc_port = 0;
  pe2.rpc_credits_per_hash = 0;
  plm.append_with_peer_white(pe2);

  EXPECT_EQ(1u, plm.get_white_peers_count());

  // Verify the pruning_seed was preserved
  nodetool::peerlist_entry result;
  EXPECT_TRUE(plm.get_white_peer_by_index(result, 0));
  EXPECT_EQ(384u, result.pruning_seed);
}

TEST(P2PNetNode, PeerlistWhiteUpdatePreservesRpcPort)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  nodetool::peerlist_entry pe1;
  pe1.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe1.id = 1;
  pe1.last_seen = time(NULL);
  pe1.pruning_seed = 0;
  pe1.rpc_port = 18081;
  pe1.rpc_credits_per_hash = 0;
  plm.append_with_peer_white(pe1);

  // Update with no rpc_port
  nodetool::peerlist_entry pe2;
  pe2.adr = make_ipv4_addr(8, 8, 8, 8, 18080);
  pe2.id = 1;
  pe2.last_seen = time(NULL) + 100;
  pe2.pruning_seed = 0;
  pe2.rpc_port = 0;
  pe2.rpc_credits_per_hash = 0;
  plm.append_with_peer_white(pe2);

  nodetool::peerlist_entry result;
  EXPECT_TRUE(plm.get_white_peer_by_index(result, 0));
  EXPECT_EQ(18081, result.rpc_port);
}

// ---- Multiple peers in peerlist ----

TEST(P2PNetNode, PeerlistMultipleWhitePeers)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  const int count = 20;
  for (int i = 0; i < count; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, static_cast<uint8_t>(i + 1), 0, 1, 18080);
    pe.id = i + 1;
    pe.last_seen = time(NULL) + i;
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_white(pe);
  }

  EXPECT_EQ(static_cast<size_t>(count), plm.get_white_peers_count());
}

TEST(P2PNetNode, PeerlistMultipleGrayPeers)
{
  nodetool::peerlist_manager plm;
  plm.init(nodetool::peerlist_types{}, true);

  const int count = 20;
  for (int i = 0; i < count; ++i)
  {
    nodetool::peerlist_entry pe;
    pe.adr = make_ipv4_addr(8, static_cast<uint8_t>(i + 1), 0, 1, 18080);
    pe.id = i + 1;
    pe.last_seen = time(NULL) + i;
    pe.pruning_seed = 0;
    pe.rpc_port = 0;
    pe.rpc_credits_per_hash = 0;
    plm.append_with_peer_gray(pe);
  }

  EXPECT_EQ(static_cast<size_t>(count), plm.get_gray_peers_count());
}
