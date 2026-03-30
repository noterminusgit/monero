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
#include "daemon/rpc_helpers.h"

using namespace daemonize::rpc_helpers;

// ============================================================================
// get_address_type_name tests
// ============================================================================

TEST(rpc_helpers, get_address_type_name_invalid)
{
  EXPECT_STREQ("invalid", get_address_type_name(epee::net_utils::address_type::invalid));
}

TEST(rpc_helpers, get_address_type_name_ipv4)
{
  EXPECT_STREQ("IPv4", get_address_type_name(epee::net_utils::address_type::ipv4));
}

TEST(rpc_helpers, get_address_type_name_ipv6)
{
  EXPECT_STREQ("IPv6", get_address_type_name(epee::net_utils::address_type::ipv6));
}

TEST(rpc_helpers, get_address_type_name_i2p)
{
  EXPECT_STREQ("I2P", get_address_type_name(epee::net_utils::address_type::i2p));
}

TEST(rpc_helpers, get_address_type_name_tor)
{
  EXPECT_STREQ("Tor", get_address_type_name(epee::net_utils::address_type::tor));
}

// ============================================================================
// print_float tests
// ============================================================================

TEST(rpc_helpers, print_float_zero)
{
  EXPECT_EQ("0.00", print_float(0.0f, 2));
}

TEST(rpc_helpers, print_float_positive)
{
  std::string result = print_float(3.14159f, 2);
  EXPECT_EQ("3.14", result);
}

TEST(rpc_helpers, print_float_precision_1)
{
  EXPECT_EQ("3.1", print_float(3.14159f, 1));
}

TEST(rpc_helpers, print_float_precision_4)
{
  std::string result = print_float(1.23456f, 4);
  EXPECT_TRUE(result.find("1.2345") != std::string::npos || result.find("1.2346") != std::string::npos);
}

TEST(rpc_helpers, print_float_negative)
{
  std::string result = print_float(-1.5f, 2);
  EXPECT_EQ("-1.50", result);
}

TEST(rpc_helpers, print_float_large)
{
  std::string result = print_float(1000.0f, 0);
  EXPECT_EQ("1000", result);
}

// ============================================================================
// get_human_time_ago tests
// ============================================================================

TEST(rpc_helpers, get_human_time_ago_now)
{
  time_t t = 1000;
  EXPECT_EQ("now", get_human_time_ago(t, t));
}

TEST(rpc_helpers, get_human_time_ago_seconds)
{
  time_t now = 1000;
  EXPECT_EQ("30 seconds ago", get_human_time_ago(now - 30, now));
}

TEST(rpc_helpers, get_human_time_ago_1_second)
{
  time_t now = 1000;
  EXPECT_EQ("1 seconds ago", get_human_time_ago(now - 1, now));
}

TEST(rpc_helpers, get_human_time_ago_89_seconds)
{
  time_t now = 1000;
  EXPECT_EQ("89 seconds ago", get_human_time_ago(now - 89, now));
}

TEST(rpc_helpers, get_human_time_ago_90_seconds_becomes_minutes)
{
  time_t now = 10000;
  EXPECT_EQ("1 minutes ago", get_human_time_ago(now - 90, now));
}

TEST(rpc_helpers, get_human_time_ago_minutes)
{
  time_t now = 10000;
  EXPECT_EQ("5 minutes ago", get_human_time_ago(now - 300, now));
}

TEST(rpc_helpers, get_human_time_ago_89_minutes)
{
  time_t now = 100000;
  // 89 * 60 = 5340, which is < 90*60 = 5400, so still minutes
  EXPECT_EQ("89 minutes ago", get_human_time_ago(now - 5340, now));
}

TEST(rpc_helpers, get_human_time_ago_90_minutes_becomes_hours)
{
  time_t now = 100000;
  EXPECT_EQ("1 hours ago", get_human_time_ago(now - 5400, now));
}

TEST(rpc_helpers, get_human_time_ago_hours)
{
  time_t now = 100000;
  EXPECT_EQ("2 hours ago", get_human_time_ago(now - 7200, now));
}

TEST(rpc_helpers, get_human_time_ago_35_hours)
{
  time_t now = 200000;
  // 35 * 3600 = 126000, < 36 * 3600 = 129600, so still hours
  EXPECT_EQ("35 hours ago", get_human_time_ago(now - 126000, now));
}

TEST(rpc_helpers, get_human_time_ago_36_hours_becomes_days)
{
  time_t now = 200000;
  EXPECT_EQ("1 days ago", get_human_time_ago(now - 129600, now));
}

TEST(rpc_helpers, get_human_time_ago_days)
{
  time_t now = 1000000;
  // 7 * 86400 = 604800
  EXPECT_EQ("7 days ago", get_human_time_ago(now - 604800, now));
}

TEST(rpc_helpers, get_human_time_ago_future_seconds)
{
  time_t now = 1000;
  EXPECT_EQ("30 seconds in the future", get_human_time_ago(now + 30, now));
}

TEST(rpc_helpers, get_human_time_ago_future_minutes)
{
  time_t now = 10000;
  EXPECT_EQ("5 minutes in the future", get_human_time_ago(now + 300, now));
}

TEST(rpc_helpers, get_human_time_ago_future_hours)
{
  time_t now = 100000;
  EXPECT_EQ("2 hours in the future", get_human_time_ago(now + 7200, now));
}

TEST(rpc_helpers, get_human_time_ago_future_days)
{
  time_t now = 1000000;
  EXPECT_EQ("7 days in the future", get_human_time_ago(now + 604800, now));
}

// ============================================================================
// get_time_hms tests
// ============================================================================

TEST(rpc_helpers, get_time_hms_zero)
{
  EXPECT_EQ("00:00:00", get_time_hms(0));
}

TEST(rpc_helpers, get_time_hms_seconds_only)
{
  EXPECT_EQ("00:00:45", get_time_hms(45));
}

TEST(rpc_helpers, get_time_hms_minutes_and_seconds)
{
  EXPECT_EQ("00:05:30", get_time_hms(330));
}

TEST(rpc_helpers, get_time_hms_hours_minutes_seconds)
{
  EXPECT_EQ("02:30:15", get_time_hms(9015));
}

TEST(rpc_helpers, get_time_hms_one_hour)
{
  EXPECT_EQ("01:00:00", get_time_hms(3600));
}

TEST(rpc_helpers, get_time_hms_large_hours)
{
  // 100 hours = 360000 seconds
  EXPECT_EQ("100:00:00", get_time_hms(360000));
}

TEST(rpc_helpers, get_time_hms_max_seconds)
{
  EXPECT_EQ("00:00:59", get_time_hms(59));
}

TEST(rpc_helpers, get_time_hms_max_minutes)
{
  EXPECT_EQ("00:59:59", get_time_hms(3599));
}

// ============================================================================
// make_error tests
// ============================================================================

TEST(rpc_helpers, make_error_ok_status)
{
  EXPECT_EQ("base message", make_error("base message", CORE_RPC_STATUS_OK));
}

TEST(rpc_helpers, make_error_error_status)
{
  EXPECT_EQ("base message -- BUSY", make_error("base message", "BUSY"));
}

TEST(rpc_helpers, make_error_empty_base)
{
  EXPECT_EQ("", make_error("", CORE_RPC_STATUS_OK));
}

TEST(rpc_helpers, make_error_empty_base_with_error)
{
  EXPECT_EQ(" -- error", make_error("", "error"));
}

TEST(rpc_helpers, make_error_custom_status)
{
  EXPECT_EQ("failed to get info -- Connection refused", make_error("failed to get info", "Connection refused"));
}
