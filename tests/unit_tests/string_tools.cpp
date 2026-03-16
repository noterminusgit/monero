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

#include "string_tools.h"
#include "string_tools_lexical.h"
#include "crypto/hash.h"

// ---------- IP string <-> int conversions ----------

TEST(string_tools, ip_roundtrip_loopback)
{
  uint32_t ip = 0;
  ASSERT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "127.0.0.1"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(ip_str, "127.0.0.1");
}

TEST(string_tools, ip_roundtrip)
{
  uint32_t ip = 0;
  ASSERT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "192.168.1.100"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(ip_str, "192.168.1.100");
}

TEST(string_tools, ip_roundtrip_zeros)
{
  uint32_t ip = 0;
  ASSERT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "0.0.0.0"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(ip_str, "0.0.0.0");
}

TEST(string_tools, string_to_ip_invalid)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "not_an_ip"));
}

TEST(string_tools, string_to_ip_empty)
{
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, ""));
}

// ---------- parse_peer_from_string ----------

TEST(string_tools, parse_peer_with_port)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  ASSERT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "192.168.1.1:18080"));
  EXPECT_EQ(port, 18080);
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(ip_str, "192.168.1.1");
}

TEST(string_tools, parse_peer_without_port)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  ASSERT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "10.0.0.1"));
  EXPECT_EQ(port, 0);
}

// ---------- pod_to_hex / hex_to_pod ----------

TEST(string_tools, pod_to_hex_hash)
{
  crypto::hash h = crypto::null_hash;
  std::string hex = epee::string_tools::pod_to_hex(h);
  // null_hash should be all zeros
  EXPECT_EQ(hex, std::string(64, '0'));
}

TEST(string_tools, hex_to_pod_roundtrip)
{
  crypto::hash h;
  memset(&h, 0xab, sizeof(h));
  std::string hex = epee::string_tools::pod_to_hex(h);
  EXPECT_EQ(hex.size(), 64u);

  crypto::hash h2;
  ASSERT_TRUE(epee::string_tools::hex_to_pod(hex, h2));
  EXPECT_EQ(h, h2);
}

TEST(string_tools, hex_to_pod_invalid)
{
  crypto::hash h;
  EXPECT_FALSE(epee::string_tools::hex_to_pod("not_hex", h));
}

TEST(string_tools, hex_to_pod_wrong_length)
{
  crypto::hash h;
  EXPECT_FALSE(epee::string_tools::hex_to_pod("abcd", h)); // too short
}

// ---------- num_to_string_fast / string_to_num ----------

TEST(string_tools, num_to_string_and_back)
{
  std::string str;
  uint64_t val = 123456789ULL;
  ASSERT_TRUE(epee::string_tools::xtype_to_string(val, str));
  EXPECT_EQ(str, "123456789");

  uint64_t val2 = 0;
  ASSERT_TRUE(epee::string_tools::get_xtype_from_string(val2, str));
  EXPECT_EQ(val, val2);
}

TEST(string_tools, string_to_num_invalid)
{
  uint64_t val = 0;
  EXPECT_FALSE(epee::string_tools::get_xtype_from_string(val, "not_a_number"));
}

TEST(string_tools, string_to_num_negative_for_unsigned)
{
  uint64_t val = 0;
  EXPECT_FALSE(epee::string_tools::get_xtype_from_string(val, "-1"));
}

TEST(string_tools, string_to_num_signed)
{
  int64_t val = 0;
  ASSERT_TRUE(epee::string_tools::get_xtype_from_string(val, "-42"));
  EXPECT_EQ(val, -42);
}

// ---------- trim ----------

TEST(string_tools, trim_spaces)
{
  std::string s = "  hello  ";
  epee::string_tools::trim(s);
  EXPECT_EQ(s, "hello");
}

TEST(string_tools, trim_no_spaces)
{
  std::string s = "hello";
  epee::string_tools::trim(s);
  EXPECT_EQ(s, "hello");
}

TEST(string_tools, trim_empty)
{
  std::string s = "";
  epee::string_tools::trim(s);
  EXPECT_EQ(s, "");
}

TEST(string_tools, trim_all_spaces)
{
  std::string s = "   ";
  epee::string_tools::trim(s);
  EXPECT_EQ(s, "");
}
