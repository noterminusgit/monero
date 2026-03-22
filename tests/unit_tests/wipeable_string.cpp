// Copyright (c) 2018-2024, The Monero Project

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

#include <boost/optional/optional.hpp>
#include <string.h>
#include "gtest/gtest.h"

#include "misc_log_ex.h"
#include "wipeable_string.h"
#include "hex.h"

TEST(wipeable_string, ctor)
{
  epee::wipeable_string s0;
  ASSERT_EQ(s0.size(), 0);

  epee::wipeable_string s1(std::string("foo"));
  ASSERT_EQ(s1.size(), 3);
  ASSERT_TRUE(!memcmp(s1.data(), "foo", s1.size()));

  epee::wipeable_string s2(std::string("bar"));
  ASSERT_EQ(s2.size(), 3);
  ASSERT_TRUE(!memcmp(s2.data(), "bar", s2.size()));

  epee::wipeable_string s3(std::string("quux"));
  ASSERT_EQ(s3.size(), 4);
  ASSERT_TRUE(!memcmp(s3.data(), "quux", s3.size()));
}

TEST(wipeable_string, wipe)
{
  epee::wipeable_string s0(std::string("foo"));
  ASSERT_EQ(s0.size(), 3);
  s0.wipe();
  ASSERT_EQ(s0.size(), 3);
  ASSERT_TRUE(!memcmp(s0.data(), "\0\0\0", 3));
}

TEST(wipeable_string, clear)
{
  epee::wipeable_string s0(std::string("foo"));
  ASSERT_EQ(s0.size(), 3);
  s0.clear();
  ASSERT_EQ(s0.size(), 0);
}

TEST(wipeable_string, push_back)
{
  epee::wipeable_string s0(std::string("fo"));
  ASSERT_EQ(s0.size(), 2);
  s0.push_back('o');
  ASSERT_EQ(s0.size(), 3);
  ASSERT_TRUE(!memcmp(s0.data(), "foo", s0.size()));
}

TEST(wipeable_string, append_char)
{
  epee::wipeable_string s0(std::string("fo"));
  ASSERT_EQ(s0.size(), 2);
  s0 += 'o';
  ASSERT_EQ(s0.size(), 3);
  ASSERT_TRUE(!memcmp(s0.data(), "foo", s0.size()));
}

TEST(wipeable_string, append_string)
{
  epee::wipeable_string s0(std::string("foo"));
  ASSERT_EQ(s0.size(), 3);
  s0 += "bar";
  ASSERT_EQ(s0.size(), 6);
  ASSERT_TRUE(!memcmp(s0.data(), "foobar", s0.size()));
}

TEST(wipeable_string, empty)
{
  epee::wipeable_string s0;
  ASSERT_TRUE(s0.empty());
  s0.push_back(' ');
  ASSERT_FALSE(s0.empty());
  ASSERT_EQ(s0.pop_back(), ' ');
  ASSERT_TRUE(s0.empty());
}

TEST(wipeable_string, pop_back)
{
  epee::wipeable_string s = "test";
  ASSERT_EQ(s.size(), 4);
  ASSERT_EQ(s.pop_back(), 't');
  ASSERT_EQ(s.size(), 3);
  ASSERT_TRUE(!memcmp(s.data(), "tes", s.size()));
}

TEST(wipeable_string, equal)
{
  epee::wipeable_string s0 = "foo";
  epee::wipeable_string s1 = "bar";
  epee::wipeable_string s0_2 = "foo";
  ASSERT_TRUE(s0 == s0);
  ASSERT_TRUE(s0 == s0_2);
  ASSERT_TRUE(s1 == s1);
  ASSERT_FALSE(s1 == s0);
  ASSERT_FALSE(s1 == s0_2);
}

TEST(wipeable_string, not_equal)
{
  epee::wipeable_string s0 = "foo";
  epee::wipeable_string s1 = "bar";
  epee::wipeable_string s0_2 = "foo";
  ASSERT_FALSE(s0 != s0);
  ASSERT_FALSE(s0 != s0_2);
  ASSERT_FALSE(s1 != s1);
  ASSERT_TRUE(s1 != s0);
  ASSERT_TRUE(s1 != s0_2);
}

static epee::wipeable_string trimmed(const char *s)
{
  epee::wipeable_string str(s);
  str.trim();
  return str;
}

TEST(wipeable_string, trim)
{
  ASSERT_TRUE(trimmed("") == "");
  ASSERT_TRUE(trimmed(" ") == "");
  ASSERT_TRUE(trimmed("  ") == "");
  ASSERT_TRUE(trimmed("a") == "a");
  ASSERT_TRUE(trimmed(" a") == "a");
  ASSERT_TRUE(trimmed("  a") == "a");
  ASSERT_TRUE(trimmed("a ") == "a");
  ASSERT_TRUE(trimmed("a  ") == "a");
  ASSERT_TRUE(trimmed(" a  ") == "a");
  ASSERT_TRUE(trimmed("  a  ") == "a");
  ASSERT_TRUE(trimmed("  ab  ") == "ab");
  ASSERT_TRUE(trimmed("  a b  ") == "a b");
  ASSERT_TRUE(trimmed("  a  b  ") == "a  b");
}

static bool check_split(const char *s, const std::vector<epee::wipeable_string> &v)
{
  epee::wipeable_string str(s);
  std::vector<epee::wipeable_string> fields;
  str.split(fields);
  return v == fields;
}

TEST(wipeable_string, split)
{
  ASSERT_TRUE(check_split("", {}));
  ASSERT_TRUE(check_split("foo", {"foo"}));
  ASSERT_TRUE(check_split("  foo   ", {"foo"}));
  ASSERT_TRUE(check_split("foo bar", {"foo", "bar"}));
  ASSERT_TRUE(check_split("foo     bar", {"foo", "bar"}));
  ASSERT_TRUE(check_split("foo     bar   baz", {"foo", "bar", "baz"}));
  ASSERT_TRUE(check_split(" foo     bar   baz         ", {"foo", "bar", "baz"}));
  ASSERT_TRUE(check_split("  foo     bar   baz", {"foo", "bar", "baz"}));
  ASSERT_TRUE(check_split("foo     bar   baz ", {"foo", "bar", "baz"}));
  ASSERT_TRUE(check_split("\tfoo\n bar\r\nbaz", {"foo", "bar", "baz"}));
}

TEST(wipeable_string, parse_hexstr)
{
  boost::optional<epee::wipeable_string> s;

  ASSERT_TRUE(boost::none == epee::wipeable_string("x").parse_hexstr());
  ASSERT_TRUE(boost::none == epee::wipeable_string("x0000000000000000").parse_hexstr());
  ASSERT_TRUE(boost::none == epee::wipeable_string("0000000000000000x").parse_hexstr());
  ASSERT_TRUE(boost::none == epee::wipeable_string("0").parse_hexstr());
  ASSERT_TRUE(boost::none == epee::wipeable_string("000").parse_hexstr());

  ASSERT_TRUE((s = epee::wipeable_string("").parse_hexstr()) != boost::none);
  ASSERT_TRUE(*s == "");
  ASSERT_TRUE((s = epee::wipeable_string("00").parse_hexstr()) != boost::none);
  ASSERT_TRUE(*s == epee::wipeable_string("", 1));
  ASSERT_TRUE((s = epee::wipeable_string("41").parse_hexstr()) != boost::none);
  ASSERT_TRUE(*s == epee::wipeable_string("A"));
  ASSERT_TRUE((s = epee::wipeable_string("414243").parse_hexstr()) != boost::none);
  ASSERT_TRUE(*s == epee::wipeable_string("ABC"));
}

TEST(wipeable_string, to_hex)
{
  ASSERT_TRUE(epee::to_hex::wipeable_string(epee::span<const uint8_t>((const uint8_t*)"", 0)) == epee::wipeable_string(""));
  ASSERT_TRUE(epee::to_hex::wipeable_string(epee::span<const uint8_t>((const uint8_t*)"abc", 3)) == epee::wipeable_string("616263"));
}

TEST(wipeable_string, to_string)
{
  // Converting a wipeable_string to a string defeats the purpose of wipeable_string,
  // but nice to know this works
  std::string str;
  {
    epee::wipeable_string wipeable_str("foo");
    str = std::string(wipeable_str.data(), wipeable_str.size());
  }
  ASSERT_TRUE(str == std::string("foo"));
}

// ============================================================
// Additional wipeable_string coverage tests
// ============================================================

TEST(wipeable_string, copy_constructor)
{
  epee::wipeable_string s1("hello");
  epee::wipeable_string s2(s1);
  ASSERT_EQ(s2.size(), 5);
  ASSERT_TRUE(!memcmp(s2.data(), "hello", 5));
  // Modifying s2 should not affect s1
  s2.push_back('!');
  ASSERT_EQ(s1.size(), 5);
  ASSERT_EQ(s2.size(), 6);
}

TEST(wipeable_string, move_constructor)
{
  epee::wipeable_string s1("world");
  epee::wipeable_string s2(std::move(s1));
  ASSERT_EQ(s2.size(), 5);
  ASSERT_TRUE(!memcmp(s2.data(), "world", 5));
}

TEST(wipeable_string, copy_assignment)
{
  epee::wipeable_string s1("abc");
  epee::wipeable_string s2("xyz");
  s2 = s1;
  ASSERT_EQ(s2.size(), 3);
  ASSERT_TRUE(!memcmp(s2.data(), "abc", 3));
  ASSERT_TRUE(s1 == s2);
}

TEST(wipeable_string, move_assignment)
{
  epee::wipeable_string s1("abc");
  epee::wipeable_string s2("xyz");
  s2 = std::move(s1);
  ASSERT_EQ(s2.size(), 3);
  ASSERT_TRUE(!memcmp(s2.data(), "abc", 3));
}

TEST(wipeable_string, construct_from_std_string_move)
{
  std::string src = "moveable";
  epee::wipeable_string s(std::move(src));
  ASSERT_EQ(s.size(), 8);
  ASSERT_TRUE(!memcmp(s.data(), "moveable", 8));
}

TEST(wipeable_string, construct_from_char_ptr_with_len)
{
  const char *data = "hello world";
  epee::wipeable_string s(data, 5);
  ASSERT_EQ(s.size(), 5);
  ASSERT_TRUE(!memcmp(s.data(), "hello", 5));
}

TEST(wipeable_string, resize_grow)
{
  epee::wipeable_string s("hi");
  ASSERT_EQ(s.size(), 2);
  s.resize(10);
  ASSERT_EQ(s.size(), 10);
  // First two bytes should be preserved
  ASSERT_EQ(s.data()[0], 'h');
  ASSERT_EQ(s.data()[1], 'i');
}

TEST(wipeable_string, resize_shrink)
{
  epee::wipeable_string s("hello world");
  ASSERT_EQ(s.size(), 11);
  s.resize(5);
  ASSERT_EQ(s.size(), 5);
  ASSERT_TRUE(!memcmp(s.data(), "hello", 5));
}

TEST(wipeable_string, reserve_does_not_change_size)
{
  epee::wipeable_string s("hi");
  ASSERT_EQ(s.size(), 2);
  s.reserve(100);
  ASSERT_EQ(s.size(), 2);
  ASSERT_TRUE(!memcmp(s.data(), "hi", 2));
}

TEST(wipeable_string, append_data_ptr)
{
  epee::wipeable_string s("hello");
  s.append(" world", 6);
  ASSERT_EQ(s.size(), 11);
  ASSERT_TRUE(!memcmp(s.data(), "hello world", 11));
}

TEST(wipeable_string, append_wipeable_string)
{
  epee::wipeable_string s1("foo");
  epee::wipeable_string s2("bar");
  s1 += s2;
  ASSERT_EQ(s1.size(), 6);
  ASSERT_TRUE(!memcmp(s1.data(), "foobar", 6));
}

TEST(wipeable_string, length_equals_size)
{
  epee::wipeable_string s("test");
  ASSERT_EQ(s.length(), s.size());
  ASSERT_EQ(s.length(), 4u);
}

TEST(wipeable_string, data_mutable)
{
  epee::wipeable_string s("abc");
  // Write through mutable data pointer
  s.data()[0] = 'x';
  ASSERT_EQ(s.data()[0], 'x');
  ASSERT_TRUE(!memcmp(s.data(), "xbc", 3));
}

TEST(wipeable_string, wipe_then_clear)
{
  epee::wipeable_string s("secret");
  s.wipe();
  ASSERT_EQ(s.size(), 6);
  // All bytes should be zero
  for (size_t i = 0; i < s.size(); ++i)
    ASSERT_EQ(s.data()[i], '\0');
  s.clear();
  ASSERT_EQ(s.size(), 0);
  ASSERT_TRUE(s.empty());
}

TEST(wipeable_string, hash_consistency)
{
  epee::wipeable_string s1("same_string");
  epee::wipeable_string s2("same_string");
  epee::wipeable_string s3("different");

  std::hash<epee::wipeable_string> hasher;
  ASSERT_EQ(hasher(s1), hasher(s2));
  // Different strings should (very likely) have different hashes
  ASSERT_NE(hasher(s1), hasher(s3));
}

TEST(wipeable_string, hex_to_pod_valid)
{
  uint32_t val = 0;
  epee::wipeable_string hex("deadbeef");
  ASSERT_TRUE(hex.hex_to_pod(val));
  // 0xdeadbeef in little-endian on x86
  ASSERT_EQ(val, 0xefbeadde);
}

TEST(wipeable_string, hex_to_pod_wrong_size)
{
  uint32_t val = 0;
  epee::wipeable_string hex("abcd");  // Only 2 bytes, need 4
  ASSERT_FALSE(hex.hex_to_pod(val));
}

TEST(wipeable_string, hex_to_pod_invalid_chars)
{
  uint32_t val = 0;
  epee::wipeable_string hex("zzzzzzzz");  // Invalid hex
  ASSERT_FALSE(hex.hex_to_pod(val));
}

TEST(wipeable_string, multiple_push_pop)
{
  epee::wipeable_string s;
  for (char c = 'a'; c <= 'z'; ++c)
    s.push_back(c);
  ASSERT_EQ(s.size(), 26u);
  for (int i = 25; i >= 0; --i)
  {
    char c = s.pop_back();
    ASSERT_EQ(c, 'a' + i);
  }
  ASSERT_TRUE(s.empty());
}

TEST(wipeable_string, trim_only_spaces)
{
  // wipeable_string::trim only trims spaces, not tabs/newlines
  epee::wipeable_string s("   hello   ");
  s.trim();
  ASSERT_EQ(s.size(), 5u);
  ASSERT_TRUE(s == epee::wipeable_string("hello"));
}

TEST(wipeable_string, split_single_word)
{
  epee::wipeable_string s("word");
  std::vector<epee::wipeable_string> fields;
  s.split(fields);
  ASSERT_EQ(fields.size(), 1u);
  ASSERT_TRUE(fields[0] == epee::wipeable_string("word"));
}

TEST(wipeable_string, split_empty_string)
{
  epee::wipeable_string s("");
  std::vector<epee::wipeable_string> fields;
  s.split(fields);
  ASSERT_EQ(fields.size(), 0u);
}

TEST(wipeable_string, parse_hexstr_uppercase)
{
  // Uppercase hex should work too
  boost::optional<epee::wipeable_string> s = epee::wipeable_string("4142").parse_hexstr();
  ASSERT_TRUE(s != boost::none);
  ASSERT_TRUE(*s == epee::wipeable_string("AB"));
}

TEST(wipeable_string, equality_different_lengths)
{
  epee::wipeable_string s1("abc");
  epee::wipeable_string s2("abcd");
  ASSERT_FALSE(s1 == s2);
  ASSERT_TRUE(s1 != s2);
}

TEST(wipeable_string, construct_from_char_ptr)
{
  epee::wipeable_string s("test string");
  ASSERT_EQ(s.size(), 11u);
  ASSERT_TRUE(!memcmp(s.data(), "test string", 11));
}

TEST(wipeable_string, append_char_operator_plus_eq)
{
  epee::wipeable_string s;
  s += 'a';
  s += 'b';
  s += 'c';
  ASSERT_EQ(s.size(), 3u);
  ASSERT_TRUE(s == epee::wipeable_string("abc"));
}

TEST(wipeable_string, append_std_string_operator_plus_eq)
{
  epee::wipeable_string s("hello");
  s += std::string(" world");
  ASSERT_EQ(s.size(), 11u);
  ASSERT_TRUE(!memcmp(s.data(), "hello world", 11));
}
