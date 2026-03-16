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

#include "common/password.h"
#include "wipeable_string.h"

TEST(password, construct_from_string)
{
  std::string pass_str("test_password");
  tools::password_container pc(std::move(pass_str));
  ASSERT_EQ(pc.password(), epee::wipeable_string("test_password"));
}

TEST(password, construct_from_wipeable_string)
{
  epee::wipeable_string ws("secure_pass");
  tools::password_container pc(ws);
  ASSERT_EQ(pc.password(), epee::wipeable_string("secure_pass"));
}

TEST(password, empty_password)
{
  tools::password_container pc(std::string(""));
  ASSERT_TRUE(pc.password().empty());
}

TEST(password, max_password_size_constant)
{
  ASSERT_EQ(tools::password_container::max_password_size, 1024u);
}

TEST(password, login_parse_with_colon)
{
  auto result = tools::login::parse("user:pass", false, [](bool) -> boost::optional<tools::password_container> {
    return boost::none;
  });
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->username, "user");
  ASSERT_EQ(result->password.password(), epee::wipeable_string("pass"));
}

TEST(password, login_parse_user_only)
{
  // When no colon is present and no password is prompted, result depends on prompt
  auto result = tools::login::parse("user", false, [](bool) -> boost::optional<tools::password_container> {
    return tools::password_container(std::string("prompted_pass"));
  });
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->username, "user");
  ASSERT_EQ(result->password.password(), epee::wipeable_string("prompted_pass"));
}

TEST(password, login_parse_empty_password_after_colon)
{
  auto result = tools::login::parse("user:", false, [](bool) -> boost::optional<tools::password_container> {
    return boost::none;
  });
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->username, "user");
  ASSERT_TRUE(result->password.password().empty());
}
