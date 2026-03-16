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

#include "common/download.h"
#include <boost/filesystem.hpp>

TEST(download, invalid_url_fails)
{
  boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  bool result = tools::download(temp.string(), "http://invalid.invalid.invalid/nonexistent");
  ASSERT_FALSE(result);
  // Clean up if file was created
  boost::filesystem::remove(temp);
}

TEST(download, empty_url_fails)
{
  boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  bool result = tools::download(temp.string(), "");
  ASSERT_FALSE(result);
  boost::filesystem::remove(temp);
}

TEST(download, async_cancel)
{
  boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  auto handle = tools::download_async(temp.string(), "http://invalid.invalid.invalid/nonexistent",
    [](const std::string&, const std::string&, bool) {},
    nullptr);
  // Should be able to cancel immediately
  if (handle)
  {
    tools::download_cancel(handle);
    // After cancel, should be finished
    ASSERT_TRUE(tools::download_finished(handle) || tools::download_error(handle));
  }
  boost::filesystem::remove(temp);
}
