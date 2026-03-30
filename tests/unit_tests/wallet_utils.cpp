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
#include "wallet/wallet_utils.h"

using namespace tools::wallet_utils;

// ============================================================================
// add_reason tests
// ============================================================================

TEST(wallet_utils, add_reason_empty)
{
  std::string reasons;
  add_reason(reasons, "first");
  EXPECT_EQ("first", reasons);
}

TEST(wallet_utils, add_reason_appends)
{
  std::string reasons = "first";
  add_reason(reasons, "second");
  EXPECT_EQ("first, second", reasons);
}

TEST(wallet_utils, add_reason_multiple)
{
  std::string reasons;
  add_reason(reasons, "a");
  add_reason(reasons, "b");
  add_reason(reasons, "c");
  EXPECT_EQ("a, b, c", reasons);
}

// ============================================================================
// get_text_reason tests
// ============================================================================

TEST(wallet_utils, get_text_reason_empty_response)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  EXPECT_EQ("", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_low_mixin)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.low_mixin = true;
  EXPECT_EQ("bad ring size", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_double_spend)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.double_spend = true;
  EXPECT_EQ("double spend", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_invalid_input)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.invalid_input = true;
  EXPECT_EQ("invalid input", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_invalid_output)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.invalid_output = true;
  EXPECT_EQ("invalid output", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_too_few_outputs)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.too_few_outputs = true;
  EXPECT_EQ("too few outputs", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_too_big)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.too_big = true;
  EXPECT_EQ("too big", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_overspend)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.overspend = true;
  EXPECT_EQ("overspend", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_fee_too_low)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.fee_too_low = true;
  EXPECT_EQ("fee too low", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_sanity_check_failed)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.sanity_check_failed = true;
  EXPECT_EQ("tx sanity check failed", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_not_relayed)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.not_relayed = true;
  EXPECT_EQ("tx was not relayed", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_multiple)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.double_spend = true;
  res.fee_too_low = true;
  EXPECT_EQ("double spend, fee too low", get_text_reason(res));
}

TEST(wallet_utils, get_text_reason_all_flags)
{
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response res{};
  res.low_mixin = true;
  res.double_spend = true;
  res.invalid_input = true;
  res.invalid_output = true;
  res.too_few_outputs = true;
  res.too_big = true;
  res.overspend = true;
  res.fee_too_low = true;
  res.sanity_check_failed = true;
  res.not_relayed = true;
  std::string reason = get_text_reason(res);
  EXPECT_NE(std::string::npos, reason.find("bad ring size"));
  EXPECT_NE(std::string::npos, reason.find("double spend"));
  EXPECT_NE(std::string::npos, reason.find("invalid input"));
  EXPECT_NE(std::string::npos, reason.find("invalid output"));
  EXPECT_NE(std::string::npos, reason.find("too few outputs"));
  EXPECT_NE(std::string::npos, reason.find("too big"));
  EXPECT_NE(std::string::npos, reason.find("overspend"));
  EXPECT_NE(std::string::npos, reason.find("fee too low"));
  EXPECT_NE(std::string::npos, reason.find("tx sanity check failed"));
  EXPECT_NE(std::string::npos, reason.find("tx was not relayed"));
}

// ============================================================================
// keys_intersect tests
// ============================================================================

TEST(wallet_utils, keys_intersect_both_empty)
{
  std::unordered_set<crypto::public_key> s1, s2;
  EXPECT_FALSE(keys_intersect(s1, s2));
}

TEST(wallet_utils, keys_intersect_one_empty)
{
  std::unordered_set<crypto::public_key> s1, s2;
  crypto::public_key pk;
  memset(&pk, 1, sizeof(pk));
  s1.insert(pk);
  EXPECT_FALSE(keys_intersect(s1, s2));
  EXPECT_FALSE(keys_intersect(s2, s1));
}

TEST(wallet_utils, keys_intersect_no_overlap)
{
  std::unordered_set<crypto::public_key> s1, s2;
  crypto::public_key pk1, pk2;
  memset(&pk1, 1, sizeof(pk1));
  memset(&pk2, 2, sizeof(pk2));
  s1.insert(pk1);
  s2.insert(pk2);
  EXPECT_FALSE(keys_intersect(s1, s2));
}

TEST(wallet_utils, keys_intersect_overlap)
{
  std::unordered_set<crypto::public_key> s1, s2;
  crypto::public_key pk1, pk2;
  memset(&pk1, 1, sizeof(pk1));
  memset(&pk2, 2, sizeof(pk2));
  s1.insert(pk1);
  s1.insert(pk2);
  s2.insert(pk2);
  EXPECT_TRUE(keys_intersect(s1, s2));
}

TEST(wallet_utils, keys_intersect_same_set)
{
  std::unordered_set<crypto::public_key> s1;
  crypto::public_key pk;
  memset(&pk, 1, sizeof(pk));
  s1.insert(pk);
  EXPECT_TRUE(keys_intersect(s1, s1));
}

// ============================================================================
// parse_bool tests
// ============================================================================

TEST(wallet_utils, parse_bool_1)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("1", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_0)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("0", result));
  EXPECT_FALSE(result);
}

TEST(wallet_utils, parse_bool_yes)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("yes", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_no)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("no", result));
  EXPECT_FALSE(result);
}

TEST(wallet_utils, parse_bool_true_lowercase)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("true", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_false_lowercase)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("false", result));
  EXPECT_FALSE(result);
}

TEST(wallet_utils, parse_bool_TRUE_uppercase)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("TRUE", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_FALSE_uppercase)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("FALSE", result));
  EXPECT_FALSE(result);
}

TEST(wallet_utils, parse_bool_True_mixed_case)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("True", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_False_mixed_case)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("False", result));
  EXPECT_FALSE(result);
}

TEST(wallet_utils, parse_bool_invalid)
{
  bool result = true;
  EXPECT_FALSE(parse_bool("maybe", result));
}

TEST(wallet_utils, parse_bool_empty)
{
  bool result = true;
  EXPECT_FALSE(parse_bool("", result));
}

TEST(wallet_utils, parse_bool_Y)
{
  bool result = false;
  EXPECT_TRUE(parse_bool("Y", result));
  EXPECT_TRUE(result);
}

TEST(wallet_utils, parse_bool_N)
{
  bool result = true;
  EXPECT_TRUE(parse_bool("N", result));
  EXPECT_FALSE(result);
}

// ============================================================================
// datestr_to_int tests
// ============================================================================

TEST(wallet_utils, datestr_to_int_valid)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_TRUE(datestr_to_int("2024-03-15", year, month, day));
  EXPECT_EQ(2024, year);
  EXPECT_EQ(3, month);
  EXPECT_EQ(15, day);
}

TEST(wallet_utils, datestr_to_int_jan_first)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_TRUE(datestr_to_int("2014-01-01", year, month, day));
  EXPECT_EQ(2014, year);
  EXPECT_EQ(1, month);
  EXPECT_EQ(1, day);
}

TEST(wallet_utils, datestr_to_int_dec_last)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_TRUE(datestr_to_int("2024-12-31", year, month, day));
  EXPECT_EQ(2024, year);
  EXPECT_EQ(12, month);
  EXPECT_EQ(31, day);
}

TEST(wallet_utils, datestr_to_int_wrong_length)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_FALSE(datestr_to_int("2024-3-15", year, month, day));
}

TEST(wallet_utils, datestr_to_int_wrong_separator)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_FALSE(datestr_to_int("2024/03/15", year, month, day));
}

TEST(wallet_utils, datestr_to_int_not_a_date)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_FALSE(datestr_to_int("abcd-ef-gh", year, month, day));
}

TEST(wallet_utils, datestr_to_int_empty)
{
  uint16_t year;
  uint8_t month, day;
  EXPECT_FALSE(datestr_to_int("", year, month, day));
}

// ============================================================================
// pop_index tests
// ============================================================================

TEST(wallet_utils, pop_index_first)
{
  std::vector<int> v = {10, 20, 30};
  int val = pop_index(v, 0);
  EXPECT_EQ(10, val);
  EXPECT_EQ(2u, v.size());
}

TEST(wallet_utils, pop_index_last)
{
  std::vector<int> v = {10, 20, 30};
  int val = pop_index(v, 2);
  EXPECT_EQ(30, val);
  EXPECT_EQ(2u, v.size());
  EXPECT_EQ(10, v[0]);
  EXPECT_EQ(20, v[1]);
}

TEST(wallet_utils, pop_index_middle)
{
  std::vector<int> v = {10, 20, 30};
  int val = pop_index(v, 1);
  EXPECT_EQ(20, val);
  EXPECT_EQ(2u, v.size());
  // The last element replaces the removed one
  EXPECT_EQ(10, v[0]);
  EXPECT_EQ(30, v[1]);
}

TEST(wallet_utils, pop_index_single)
{
  std::vector<int> v = {42};
  int val = pop_index(v, 0);
  EXPECT_EQ(42, val);
  EXPECT_TRUE(v.empty());
}

// ============================================================================
// pop_back tests
// ============================================================================

TEST(wallet_utils, pop_back_normal)
{
  std::vector<int> v = {10, 20, 30};
  int val = pop_back(v);
  EXPECT_EQ(30, val);
  EXPECT_EQ(2u, v.size());
  EXPECT_EQ(10, v[0]);
  EXPECT_EQ(20, v[1]);
}

TEST(wallet_utils, pop_back_single)
{
  std::vector<int> v = {42};
  int val = pop_back(v);
  EXPECT_EQ(42, val);
  EXPECT_TRUE(v.empty());
}

// ============================================================================
// pop_if_present tests
// ============================================================================

TEST(wallet_utils, pop_if_present_found)
{
  std::vector<int> v = {10, 20, 30};
  pop_if_present(v, 20);
  EXPECT_EQ(2u, v.size());
}

TEST(wallet_utils, pop_if_present_not_found)
{
  std::vector<int> v = {10, 20, 30};
  pop_if_present(v, 99);
  EXPECT_EQ(3u, v.size());
}

TEST(wallet_utils, pop_if_present_first)
{
  std::vector<int> v = {10, 20, 30};
  pop_if_present(v, 10);
  EXPECT_EQ(2u, v.size());
}

TEST(wallet_utils, pop_if_present_last)
{
  std::vector<int> v = {10, 20, 30};
  pop_if_present(v, 30);
  EXPECT_EQ(2u, v.size());
}

TEST(wallet_utils, pop_if_present_empty_vector)
{
  std::vector<int> v;
  pop_if_present(v, 10);
  EXPECT_TRUE(v.empty());
}

// ============================================================================
// pop_random_value tests
// ============================================================================

TEST(wallet_utils, pop_random_value_reduces_size)
{
  std::vector<int> v = {10, 20, 30, 40, 50};
  pop_random_value(v);
  EXPECT_EQ(4u, v.size());
}

TEST(wallet_utils, pop_random_value_single)
{
  std::vector<int> v = {42};
  int val = pop_random_value(v);
  EXPECT_EQ(42, val);
  EXPECT_TRUE(v.empty());
}

TEST(wallet_utils, pop_random_value_returns_element_from_vec)
{
  std::vector<int> original = {10, 20, 30, 40, 50};
  std::vector<int> v = original;
  int val = pop_random_value(v);
  EXPECT_NE(std::find(original.begin(), original.end(), val), original.end());
}
