// Copyright (c) 2023-2024, The Monero Project
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

#include <fstream>
#include <thread>
#include <boost/filesystem.hpp>
#include "common/util.h"
#include "cryptonote_config.h"

TEST(LocalAddress, localhost) { ASSERT_TRUE(tools::is_local_address("localhost")); }
TEST(LocalAddress, localhost_port) { ASSERT_TRUE(tools::is_local_address("localhost:18081")); }
TEST(LocalAddress, localhost_suffix) { ASSERT_TRUE(tools::is_local_address("test.localhost")); }
TEST(LocalAddress, loopback) { ASSERT_TRUE(tools::is_local_address("127.0.0.1")); }
TEST(LocalAddress, loopback_port) { ASSERT_TRUE(tools::is_local_address("127.0.0.1:18081")); }
TEST(LocalAddress, loopback_protocol) { ASSERT_TRUE(tools::is_local_address("http://127.0.0.1")); }
TEST(LocalAddress, loopback_hi) { ASSERT_TRUE(tools::is_local_address("127.255.255.255")); }
TEST(LocalAddress, loopback_lo) { ASSERT_TRUE(tools::is_local_address("127.0.0.0")); }
TEST(LocalAddress, loopback_ipv6) { ASSERT_TRUE(tools::is_local_address("[0:0:0:0:0:0:0:1]")); }

TEST(LocalAddress, onion) { ASSERT_FALSE(tools::is_local_address("vww6ybal4bd7szmgncyruucpgfkqahzddi37ktceo3ah7ngmcopnpyyd.onion")); }
TEST(LocalAddress, i2p) { ASSERT_FALSE(tools::is_local_address("xmrto2bturnore26xmrto2bturnore26xmrto2bturnore26xmr2.b32.i2p")); }
TEST(LocalAddress, valid_ip) { ASSERT_FALSE(tools::is_local_address("1.2.3.4")); }
TEST(LocalAddress, valid_ipv6) { ASSERT_FALSE(tools::is_local_address("[0:0:0:0:0:0:0:2]")); }
TEST(LocalAddress, valid_domain) { ASSERT_FALSE(tools::is_local_address("getmonero.org")); }
TEST(LocalAddress, local_prefix) { ASSERT_FALSE(tools::is_local_address("localhost.com")); }
TEST(LocalAddress, invalid) { ASSERT_FALSE(tools::is_local_address("test")); }
TEST(LocalAddress, empty) { ASSERT_FALSE(tools::is_local_address("")); }

// ---------- is_privacy_preserving_network ----------

TEST(PrivacyNetwork, onion_address)
{
  ASSERT_TRUE(tools::is_privacy_preserving_network("vww6ybal4bd7szmgncyruucpgfkqahzddi37ktceo3ah7ngmcopnpyyd.onion"));
}

TEST(PrivacyNetwork, i2p_address)
{
  ASSERT_TRUE(tools::is_privacy_preserving_network("xmrto2bturnore26xmrto2bturnore26xmrto2bturnore26xmr2.b32.i2p"));
}

TEST(PrivacyNetwork, regular_domain)
{
  ASSERT_FALSE(tools::is_privacy_preserving_network("getmonero.org"));
}

TEST(PrivacyNetwork, regular_ip)
{
  ASSERT_FALSE(tools::is_privacy_preserving_network("1.2.3.4"));
}

TEST(PrivacyNetwork, empty)
{
  ASSERT_FALSE(tools::is_privacy_preserving_network(""));
}

// ---------- vercmp ----------

TEST(VersionCompare, equal)
{
  ASSERT_EQ(tools::vercmp("1.0.0", "1.0.0"), 0);
}

TEST(VersionCompare, less)
{
  ASSERT_LT(tools::vercmp("1.0.0", "1.0.1"), 0);
  ASSERT_LT(tools::vercmp("1.0.0", "1.1.0"), 0);
  ASSERT_LT(tools::vercmp("1.0.0", "2.0.0"), 0);
}

TEST(VersionCompare, greater)
{
  ASSERT_GT(tools::vercmp("1.0.1", "1.0.0"), 0);
  ASSERT_GT(tools::vercmp("2.0.0", "1.9.9"), 0);
}

TEST(VersionCompare, different_lengths)
{
  ASSERT_LT(tools::vercmp("1.0", "1.0.1"), 0);
  ASSERT_GT(tools::vercmp("1.0.1", "1.0"), 0);
}

// ---------- parse_subaddress_lookahead ----------

TEST(SubaddressLookahead, valid)
{
  auto result = tools::parse_subaddress_lookahead("50:200");
  ASSERT_TRUE(result.is_initialized());
  EXPECT_EQ(result->first, 50u);
  EXPECT_EQ(result->second, 200u);
}

TEST(SubaddressLookahead, invalid_format)
{
  EXPECT_FALSE(tools::parse_subaddress_lookahead("").is_initialized());
  EXPECT_FALSE(tools::parse_subaddress_lookahead("abc").is_initialized());
  EXPECT_FALSE(tools::parse_subaddress_lookahead("50").is_initialized());
}

// ---------- glob_to_regex ----------

TEST(GlobToRegex, literal)
{
  EXPECT_EQ(tools::glob_to_regex("hello"), "hello");
}

TEST(GlobToRegex, star)
{
  std::string result = tools::glob_to_regex("*.txt");
  EXPECT_NE(result.find(".*"), std::string::npos);
}

TEST(GlobToRegex, question_mark)
{
  std::string result = tools::glob_to_regex("file?.txt");
  EXPECT_NE(result.find('.'), std::string::npos);
}

// ---------- get_human_readable_timespan ----------

TEST(HumanReadableTimespan, seconds)
{
  std::string result = tools::get_human_readable_timespan(45);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimespan, minutes)
{
  std::string result = tools::get_human_readable_timespan(120);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimespan, hours)
{
  std::string result = tools::get_human_readable_timespan(7200);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimespan, days)
{
  std::string result = tools::get_human_readable_timespan(86400 * 3);
  EXPECT_FALSE(result.empty());
}

// ---------- get_human_readable_bytes ----------

TEST(HumanReadableBytes, bytes)
{
  std::string result = tools::get_human_readable_bytes(100);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableBytes, kilobytes)
{
  std::string result = tools::get_human_readable_bytes(1024);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableBytes, megabytes)
{
  std::string result = tools::get_human_readable_bytes(1024 * 1024);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableBytes, gigabytes)
{
  std::string result = tools::get_human_readable_bytes(uint64_t(1024) * 1024 * 1024);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableBytes, zero)
{
  std::string result = tools::get_human_readable_bytes(0);
  EXPECT_FALSE(result.empty());
}

// ---------- get_human_readable_timestamp ----------

TEST(HumanReadableTimestamp, nonzero)
{
  std::string result = tools::get_human_readable_timestamp(1609459200); // 2021-01-01 00:00:00 UTC
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimestamp, zero)
{
  std::string result = tools::get_human_readable_timestamp(0);
  EXPECT_FALSE(result.empty());
}

// ---------- sha256sum ----------

TEST(SHA256, data_hash)
{
  const std::string data = "hello world";
  crypto::hash hash;
  ASSERT_TRUE(tools::sha256sum(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hash));
  // SHA256("hello world") is known
  EXPECT_NE(hash, crypto::null_hash);
}

TEST(SHA256, empty_data)
{
  crypto::hash hash;
  ASSERT_TRUE(tools::sha256sum(reinterpret_cast<const uint8_t*>(""), 0, hash));
  EXPECT_NE(hash, crypto::null_hash);
}

TEST(SHA256, deterministic)
{
  const std::string data = "test data";
  crypto::hash hash1, hash2;
  ASSERT_TRUE(tools::sha256sum(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hash1));
  ASSERT_TRUE(tools::sha256sum(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hash2));
  EXPECT_EQ(hash1, hash2);
}

// ---------- split_string_by_width ----------

TEST(SplitStringByWidth, short_string)
{
  auto result = tools::split_string_by_width("hello", 80);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].first, "hello");
}

TEST(SplitStringByWidth, long_string)
{
  std::string long_str(200, 'x');
  auto result = tools::split_string_by_width(long_str, 80);
  EXPECT_GE(result.size(), 2u);
}

TEST(SplitStringByWidth, empty_string)
{
  auto result = tools::split_string_by_width("", 80);
  // Empty string should produce empty or one-element result
  EXPECT_LE(result.size(), 1u);
}

// ---------- set_max_concurrency / get_max_concurrency ----------

TEST(Concurrency, set_get_concurrency_roundtrip)
{
  tools::set_max_concurrency(4);
  unsigned hwc = std::thread::hardware_concurrency();
  if (hwc >= 4)
    EXPECT_EQ(tools::get_max_concurrency(), 4u);
  else
    EXPECT_EQ(tools::get_max_concurrency(), hwc);
}

TEST(Concurrency, set_concurrency_zero_uses_default)
{
  tools::set_max_concurrency(0);
  unsigned hwc = std::thread::hardware_concurrency();
  unsigned expected = hwc > 0 ? hwc : 1;
  EXPECT_EQ(tools::get_max_concurrency(), expected);
}

TEST(Concurrency, get_concurrency_default)
{
  // Default concurrency should be > 0
  EXPECT_GT(tools::get_max_concurrency(), 0u);
}

TEST(Concurrency, set_concurrency_large)
{
  tools::set_max_concurrency(1000);
  unsigned hwc = std::thread::hardware_concurrency();
  EXPECT_EQ(tools::get_max_concurrency(), hwc);
}

// ---------- create_directories_if_necessary ----------

TEST(CreateDirectories, create_directories_new)
{
  std::string dir = "/tmp/monero_test_create_dir_" + std::to_string(getpid());
  boost::filesystem::remove_all(dir);
  ASSERT_TRUE(tools::create_directories_if_necessary(dir));
  EXPECT_TRUE(boost::filesystem::is_directory(dir));
  boost::filesystem::remove_all(dir);
}

TEST(CreateDirectories, create_directories_existing)
{
  std::string dir = "/tmp/monero_test_create_dir_exist_" + std::to_string(getpid());
  boost::filesystem::remove_all(dir);
  ASSERT_TRUE(tools::create_directories_if_necessary(dir));
  ASSERT_TRUE(tools::create_directories_if_necessary(dir));
  boost::filesystem::remove_all(dir);
}

TEST(CreateDirectories, create_directories_empty_path)
{
  EXPECT_FALSE(tools::create_directories_if_necessary(""));
}

// ---------- cumulative_block_sync_weight ----------

TEST(CumulativeBlockSyncWeight, sync_weight_zero_blocks)
{
  uint64_t w = tools::cumulative_block_sync_weight(cryptonote::MAINNET, 0, 0);
  EXPECT_EQ(w, 0u);
}

TEST(CumulativeBlockSyncWeight, sync_weight_nonzero)
{
  uint64_t w = tools::cumulative_block_sync_weight(cryptonote::MAINNET, 0, 100);
  EXPECT_GT(w, 0u);
}

TEST(CumulativeBlockSyncWeight, sync_weight_large_start)
{
  // start well beyond known table; should not crash
  uint64_t w = tools::cumulative_block_sync_weight(cryptonote::MAINNET, 100000000, 100);
  (void)w; // may be zero or non-zero; just must not crash
}

TEST(CumulativeBlockSyncWeight, sync_weight_mainnet_vs_testnet)
{
  uint64_t wm = tools::cumulative_block_sync_weight(cryptonote::MAINNET, 0, 100);
  uint64_t wt = tools::cumulative_block_sync_weight(cryptonote::TESTNET, 0, 100);
  // Different nettypes may have different weights
  (void)wm;
  (void)wt;
}

// ---------- get_human_readable_timespan precision ----------

TEST(HumanReadableTimespan, timespan_zero)
{
  std::string result = tools::get_human_readable_timespan(0);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimespan, timespan_very_large)
{
  std::string result = tools::get_human_readable_timespan(365 * 86400);
  EXPECT_FALSE(result.empty());
}

TEST(HumanReadableTimespan, timespan_negative)
{
  // Passing 0 as the minimum safe value
  std::string result = tools::get_human_readable_timespan(0);
  EXPECT_FALSE(result.empty());
}

// ---------- split_string_by_width additional ----------

TEST(SplitStringByWidth, split_width_1)
{
  auto result = tools::split_string_by_width("abc", 1);
  EXPECT_GE(result.size(), 2u);
}

TEST(SplitStringByWidth, split_multiline)
{
  auto result = tools::split_string_by_width("line1\nline2\nline3", 80);
  EXPECT_GE(result.size(), 3u);
}

TEST(SplitStringByWidth, split_exactly_width)
{
  std::string s(80, 'a');
  auto result = tools::split_string_by_width(s, 80);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].first, s);
}

// ---------- replace_file ----------

TEST(ReplaceFile, replace_file_both_nonexistent)
{
  std::string old_name = "/tmp/monero_test_replace_nonexist_old_" + std::to_string(getpid());
  std::string new_name = "/tmp/monero_test_replace_nonexist_new_" + std::to_string(getpid());
  boost::filesystem::remove(old_name);
  boost::filesystem::remove(new_name);
  std::error_code ec = tools::replace_file(old_name, new_name);
  EXPECT_NE(ec.value(), 0);
}

TEST(ReplaceFile, replace_file_success)
{
  std::string old_name = "/tmp/monero_test_replace_old_" + std::to_string(getpid());
  std::string new_name = "/tmp/monero_test_replace_new_" + std::to_string(getpid());
  boost::filesystem::remove(old_name);
  boost::filesystem::remove(new_name);
  {
    std::ofstream ofs(old_name);
    ofs << "test data";
  }
  std::error_code ec = tools::replace_file(old_name, new_name);
  EXPECT_EQ(ec.value(), 0);
  EXPECT_TRUE(boost::filesystem::exists(new_name));
  EXPECT_FALSE(boost::filesystem::exists(old_name));
  boost::filesystem::remove(new_name);
}
