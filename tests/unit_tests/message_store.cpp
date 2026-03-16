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

#include "gtest/gtest.h"

#include "wallet/message_store.h"

TEST(message_store, message_type_to_string)
{
  // Verify enum-to-string conversion does not crash
  const char *s;
  s = mms::message_store::message_type_to_string(mms::message_type::key_set);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::additional_key_set);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::multisig_sync_data);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::partially_signed_tx);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::fully_signed_tx);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::note);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::signer_config);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_type_to_string(mms::message_type::auto_config_data);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, message_direction_to_string)
{
  const char *s;
  s = mms::message_store::message_direction_to_string(mms::message_direction::in);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_direction_to_string(mms::message_direction::out);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, message_state_to_string)
{
  const char *s;
  s = mms::message_store::message_state_to_string(mms::message_state::ready_to_send);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::sent);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::waiting);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::processed);
  ASSERT_NE(s, nullptr);
  s = mms::message_store::message_state_to_string(mms::message_state::cancelled);
  ASSERT_NE(s, nullptr);
}

TEST(message_store, get_sanitized_text_normal)
{
  std::string result = mms::message_store::get_sanitized_text("hello world", 100);
  ASSERT_EQ(result, "hello world");
}

TEST(message_store, get_sanitized_text_truncated)
{
  std::string result = mms::message_store::get_sanitized_text("hello world", 5);
  ASSERT_EQ(result.size(), 5u);
}

TEST(message_store, get_sanitized_text_empty)
{
  std::string result = mms::message_store::get_sanitized_text("", 100);
  ASSERT_TRUE(result.empty());
}

TEST(message_store, get_sanitized_text_nonprintable)
{
  std::string input = "hello\x01\x02world";
  std::string result = mms::message_store::get_sanitized_text(input, 100);
  // Non-printable chars should be removed or replaced
  ASSERT_NE(result.find("hello"), std::string::npos);
  ASSERT_NE(result.find("world"), std::string::npos);
}
