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

// ---------- compare_no_case ----------

TEST(string_tools, compare_no_case_equal)
{
  // compare_no_case returns false when strings are equal (case-insensitive)
  // It returns !boost::iequals, so false means "equal"
  EXPECT_FALSE(epee::string_tools::compare_no_case("hello", "hello"));
  EXPECT_FALSE(epee::string_tools::compare_no_case("Hello", "hello"));
  EXPECT_FALSE(epee::string_tools::compare_no_case("HELLO", "hello"));
  EXPECT_FALSE(epee::string_tools::compare_no_case("HeLLo", "hEllO"));
}

TEST(string_tools, compare_no_case_not_equal)
{
  EXPECT_TRUE(epee::string_tools::compare_no_case("hello", "world"));
  EXPECT_TRUE(epee::string_tools::compare_no_case("abc", "abcd"));
  EXPECT_TRUE(epee::string_tools::compare_no_case("", "a"));
}

TEST(string_tools, compare_no_case_empty)
{
  EXPECT_FALSE(epee::string_tools::compare_no_case("", ""));
}

// ---------- pad_string ----------

TEST(string_tools, pad_string_append)
{
  std::string result = epee::string_tools::pad_string("hi", 5, ' ', false);
  EXPECT_EQ(result, "hi   ");
  EXPECT_EQ(result.size(), 5u);
}

TEST(string_tools, pad_string_prepend)
{
  std::string result = epee::string_tools::pad_string("hi", 5, '0', true);
  EXPECT_EQ(result, "000hi");
  EXPECT_EQ(result.size(), 5u);
}

TEST(string_tools, pad_string_already_long_enough)
{
  std::string result = epee::string_tools::pad_string("hello", 3, ' ', false);
  EXPECT_EQ(result, "hello");
}

TEST(string_tools, pad_string_exact_length)
{
  std::string result = epee::string_tools::pad_string("abc", 3, ' ', false);
  EXPECT_EQ(result, "abc");
}

TEST(string_tools, pad_string_custom_char)
{
  std::string result = epee::string_tools::pad_string("1", 8, '0', true);
  EXPECT_EQ(result, "00000001");
}

// ---------- num_to_string_fast ----------

TEST(string_tools, num_to_string_fast_positive)
{
  std::string result = epee::string_tools::num_to_string_fast(42);
  EXPECT_EQ(result, "42");
}

TEST(string_tools, num_to_string_fast_negative)
{
  std::string result = epee::string_tools::num_to_string_fast(-100);
  EXPECT_EQ(result, "-100");
}

TEST(string_tools, num_to_string_fast_zero)
{
  std::string result = epee::string_tools::num_to_string_fast(0);
  EXPECT_EQ(result, "0");
}

TEST(string_tools, num_to_string_fast_large)
{
  std::string result = epee::string_tools::num_to_string_fast(9223372036854775807LL);
  EXPECT_EQ(result, "9223372036854775807");
}

// ---------- buff_to_hex_nodelimer ----------

TEST(string_tools, buff_to_hex_nodelimer_empty)
{
  std::string hex = epee::string_tools::buff_to_hex_nodelimer("");
  EXPECT_EQ(hex, "");
}

TEST(string_tools, buff_to_hex_nodelimer_abc)
{
  std::string hex = epee::string_tools::buff_to_hex_nodelimer("abc");
  EXPECT_EQ(hex, "616263");
}

TEST(string_tools, buff_to_hex_nodelimer_binary)
{
  std::string src(4, '\0');
  src[0] = 0x00;
  src[1] = 0xff;
  src[2] = 0x0a;
  src[3] = 0xf0;
  std::string hex = epee::string_tools::buff_to_hex_nodelimer(src);
  EXPECT_EQ(hex, "00ff0af0");
}

// ---------- parse_hexstr_to_binbuff ----------

TEST(string_tools, parse_hexstr_to_binbuff_valid)
{
  std::string result;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff("616263", result));
  EXPECT_EQ(result, "abc");
}

TEST(string_tools, parse_hexstr_to_binbuff_empty)
{
  std::string result;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff("", result));
  EXPECT_EQ(result, "");
}

TEST(string_tools, parse_hexstr_to_binbuff_invalid)
{
  std::string result;
  EXPECT_FALSE(epee::string_tools::parse_hexstr_to_binbuff("xyz", result));
}

TEST(string_tools, parse_hexstr_to_binbuff_odd_length)
{
  std::string result;
  EXPECT_FALSE(epee::string_tools::parse_hexstr_to_binbuff("abc", result));
}

TEST(string_tools, buff_to_hex_roundtrip)
{
  std::string original = "Hello, Monero!";
  std::string hex = epee::string_tools::buff_to_hex_nodelimer(original);
  std::string decoded;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(hex, decoded));
  EXPECT_EQ(decoded, original);
}

// ---------- to_string_hex ----------

TEST(string_tools, to_string_hex_int)
{
  std::string hex = epee::string_tools::to_string_hex(255);
  EXPECT_EQ(hex, "ff");
}

TEST(string_tools, to_string_hex_zero)
{
  std::string hex = epee::string_tools::to_string_hex(0);
  EXPECT_EQ(hex, "0");
}

TEST(string_tools, to_string_hex_large)
{
  std::string hex = epee::string_tools::to_string_hex(0xDEADBEEF);
  EXPECT_EQ(hex, "deadbeef");
}

// ---------- trim const overload ----------

TEST(string_tools, trim_const_overload)
{
  std::string result = epee::string_tools::trim("  hello  ");
  EXPECT_EQ(result, "hello");
}

TEST(string_tools, trim_const_tabs)
{
  std::string result = epee::string_tools::trim("\t\thello\t\t");
  EXPECT_EQ(result, "hello");
}

// ---------- IP edge cases ----------

TEST(string_tools, ip_broadcast_rejected)
{
  // Monero's IP parser rejects 255.255.255.255 (broadcast address)
  uint32_t ip = 0;
  EXPECT_FALSE(epee::string_tools::get_ip_int32_from_string(ip, "255.255.255.255"));
}

TEST(string_tools, ip_class_a)
{
  uint32_t ip = 0;
  ASSERT_TRUE(epee::string_tools::get_ip_int32_from_string(ip, "10.0.0.1"));
  std::string ip_str = epee::string_tools::get_ip_string_from_int32(ip);
  EXPECT_EQ(ip_str, "10.0.0.1");
}

// ---------- get_extension / cut_off_extension ----------

TEST(string_tools, get_extension_basic)
{
  std::string ext = epee::string_tools::get_extension("file.txt");
  EXPECT_EQ(ext, "txt");
}

TEST(string_tools, get_extension_no_ext)
{
  std::string ext = epee::string_tools::get_extension("file");
  EXPECT_EQ(ext, "");
}

TEST(string_tools, get_extension_double)
{
  std::string ext = epee::string_tools::get_extension("archive.tar.gz");
  EXPECT_EQ(ext, "gz");
}

TEST(string_tools, cut_off_extension_basic)
{
  std::string base = epee::string_tools::cut_off_extension("file.txt");
  EXPECT_EQ(base, "file");
}

TEST(string_tools, cut_off_extension_no_ext)
{
  std::string base = epee::string_tools::cut_off_extension("file");
  EXPECT_EQ(base, "file");
}

TEST(string_tools, cut_off_extension_double)
{
  std::string base = epee::string_tools::cut_off_extension("archive.tar.gz");
  EXPECT_EQ(base, "archive.tar");
}

// ---------- parse_peer edge cases ----------

TEST(string_tools, parse_peer_high_port)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  ASSERT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "127.0.0.1:65535"));
  EXPECT_EQ(port, 65535);
}

TEST(string_tools, parse_peer_port_zero)
{
  uint32_t ip = 0;
  uint16_t port = 99;
  ASSERT_TRUE(epee::string_tools::parse_peer_from_string(ip, port, "127.0.0.1:0"));
  EXPECT_EQ(port, 0);
}

// ---------- num conversions edge cases ----------

TEST(string_tools, string_to_num_zero)
{
  uint64_t val = 99;
  ASSERT_TRUE(epee::string_tools::get_xtype_from_string(val, "0"));
  EXPECT_EQ(val, 0u);
}

TEST(string_tools, string_to_num_max_uint64)
{
  uint64_t val = 0;
  ASSERT_TRUE(epee::string_tools::get_xtype_from_string(val, "18446744073709551615"));
  EXPECT_EQ(val, UINT64_MAX);
}

TEST(string_tools, string_to_num_overflow_uint64)
{
  uint64_t val = 0;
  // One more than max uint64
  EXPECT_FALSE(epee::string_tools::get_xtype_from_string(val, "18446744073709551616"));
}

TEST(string_tools, string_to_num_with_whitespace_rejected)
{
  uint64_t val = 0;
  // Leading whitespace is rejected by the Monero parser
  EXPECT_FALSE(epee::string_tools::get_xtype_from_string(val, " 42"));
}
