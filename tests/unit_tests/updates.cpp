// Copyright (c) 2017-2024, The Monero Project
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

#include "common/updates.h"

// Tests for get_update_url (does not require DNS)

TEST(updates, get_update_url_user_linux)
{
  std::string url = tools::get_update_url("monero", "", "linux-x64", "0.18.0.0", true);
  ASSERT_EQ(url, "https://downloads.getmonero.org/monero-linux-x64-v0.18.0.0.tar.bz2");
}

TEST(updates, get_update_url_system_linux)
{
  std::string url = tools::get_update_url("monero", "", "linux-x64", "0.18.0.0", false);
  ASSERT_EQ(url, "https://updates.getmonero.org/monero-linux-x64-v0.18.0.0.tar.bz2");
}

TEST(updates, get_update_url_with_subdir)
{
  std::string url = tools::get_update_url("monero", "cli", "linux-x64", "0.18.1.0", true);
  ASSERT_EQ(url, "https://downloads.getmonero.org/cli/monero-linux-x64-v0.18.1.0.tar.bz2");
}

TEST(updates, get_update_url_empty_subdir)
{
  std::string url = tools::get_update_url("monero-gui", "", "linux-x64", "0.18.2.0", true);
  ASSERT_EQ(url, "https://downloads.getmonero.org/monero-gui-linux-x64-v0.18.2.0.tar.bz2");
}

TEST(updates, get_update_url_source_buildtag)
{
  std::string url = tools::get_update_url("monero", "", "source", "0.18.0.0", true);
  ASSERT_EQ(url, "https://downloads.getmonero.org/monero-source-v0.18.0.0.tar.bz2");
}
