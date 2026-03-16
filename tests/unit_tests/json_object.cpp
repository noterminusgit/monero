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

#include "serialization/json_object.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "byte_stream.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

// Helper: serialize value to JSON string, parse back to rapidjson::Value, then deserialize
template<typename T>
void json_roundtrip(const T& input, T& output)
{
  // Serialize to JSON
  epee::byte_stream bs;
  rapidjson::Writer<epee::byte_stream> writer(bs);
  cryptonote::json::toJsonValue(writer, input);

  // Parse the JSON back
  rapidjson::Document doc;
  doc.Parse(reinterpret_cast<const char*>(bs.data()), bs.size());
  ASSERT_FALSE(doc.HasParseError());

  // Deserialize
  cryptonote::json::fromJsonValue(doc, output);
}

TEST(json_object, int_roundtrip)
{
  int input = 42;
  int output = 0;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, negative_int_roundtrip)
{
  int input = -12345;
  int output = 0;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, uint64_roundtrip)
{
  uint64_t input = 0xFFFFFFFFFFFFFFFF;
  uint64_t output = 0;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, int64_roundtrip)
{
  int64_t input = -9223372036854775807LL;
  int64_t output = 0;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, bool_roundtrip)
{
  bool input = true;
  bool output = false;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);

  input = false;
  output = true;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, string_roundtrip)
{
  std::string input = "hello world";
  std::string output;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, empty_string_roundtrip)
{
  std::string input = "";
  std::string output = "non-empty";
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, zero_roundtrip)
{
  uint64_t input = 0;
  uint64_t output = 42;
  json_roundtrip(input, output);
  ASSERT_EQ(input, output);
}

TEST(json_object, wrong_type_int_from_string_throws)
{
  rapidjson::Document doc;
  doc.Parse("\"not_a_number\"");
  ASSERT_FALSE(doc.HasParseError());

  int output;
  ASSERT_THROW(cryptonote::json::fromJsonValue(doc, output), cryptonote::json::WRONG_TYPE);
}

TEST(json_object, wrong_type_bool_from_int_throws)
{
  rapidjson::Document doc;
  doc.Parse("42");
  ASSERT_FALSE(doc.HasParseError());

  bool output;
  ASSERT_THROW(cryptonote::json::fromJsonValue(doc, output), cryptonote::json::WRONG_TYPE);
}

TEST(json_object, wrong_type_string_from_int_throws)
{
  rapidjson::Document doc;
  doc.Parse("42");
  ASSERT_FALSE(doc.HasParseError());

  std::string output;
  ASSERT_THROW(cryptonote::json::fromJsonValue(doc, output), cryptonote::json::WRONG_TYPE);
}
