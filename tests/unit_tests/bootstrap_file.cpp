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

#include "blockchain_utilities/bootstrap_file.h"
#include <boost/filesystem.hpp>
#include <fstream>

TEST(bootstrap_file, count_blocks_nonexistent_dir)
{
  BootstrapFile bf;
  // Non-existent directory should return 0 or handle gracefully
  uint64_t count = bf.count_blocks("/nonexistent/path/to/bootstrap");
  ASSERT_EQ(count, 0u);
}

TEST(bootstrap_file, count_blocks_empty_dir)
{
  boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  boost::filesystem::create_directories(temp);

  BootstrapFile bf;
  uint64_t count = bf.count_blocks(temp.string());
  ASSERT_EQ(count, 0u);

  boost::filesystem::remove_all(temp);
}

TEST(bootstrap_file, count_bytes_empty_file)
{
  boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  {
    std::ofstream f(temp.string(), std::ios::binary);
    // Write empty file
  }

  std::ifstream import_file(temp.string(), std::ios::binary);
  BootstrapFile bf;
  uint64_t h = 0;
  bool quit = false;
  uint64_t bytes = bf.count_bytes(import_file, 10, h, quit);
  ASSERT_EQ(bytes, 0u);

  import_file.close();
  boost::filesystem::remove(temp);
}
