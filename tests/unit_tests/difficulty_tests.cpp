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
#include "cryptonote_basic/difficulty.h"
#include "cryptonote_config.h"

// ---- check_hash tests ----

TEST(DifficultyTests, check_hash_zero_difficulty)
{
  // Difficulty 0 should always pass (any hash meets difficulty 0)
  crypto::hash h;
  memset(&h, 0xff, sizeof(h));
  cryptonote::difficulty_type zero_diff = 0;
  // check_hash with difficulty 0: hash * 0 = 0 which is <= max256, so should pass
  EXPECT_TRUE(cryptonote::check_hash(h, zero_diff));
}

TEST(DifficultyTests, check_hash_one_difficulty)
{
  // Difficulty 1 should always pass (any hash works)
  crypto::hash h;
  memset(&h, 0xff, sizeof(h));
  cryptonote::difficulty_type one_diff = 1;
  EXPECT_TRUE(cryptonote::check_hash(h, one_diff));

  // Also check with a zero hash
  memset(&h, 0x00, sizeof(h));
  EXPECT_TRUE(cryptonote::check_hash(h, one_diff));

  // And a mid-range hash
  memset(&h, 0x80, sizeof(h));
  EXPECT_TRUE(cryptonote::check_hash(h, one_diff));
}

TEST(DifficultyTests, check_hash_max_difficulty)
{
  // Very high difficulty: most hashes should fail
  cryptonote::difficulty_type max_diff;
  max_diff = std::numeric_limits<boost::multiprecision::uint128_t>::max();

  // A hash with all bytes set should fail max difficulty
  crypto::hash h;
  memset(&h, 0xff, sizeof(h));
  EXPECT_FALSE(cryptonote::check_hash(h, max_diff));

  // A hash with high-order bits set should also fail
  memset(&h, 0x00, sizeof(h));
  // Set the most significant byte (last in the 256-bit LE layout)
  ((unsigned char*)&h)[31] = 0x01;
  EXPECT_FALSE(cryptonote::check_hash(h, max_diff));

  // A zero hash should still pass any difficulty
  memset(&h, 0x00, sizeof(h));
  EXPECT_TRUE(cryptonote::check_hash(h, max_diff));
}

// ---- next_difficulty tests ----

TEST(DifficultyTests, next_difficulty_empty)
{
  // Empty timestamps and difficulties should return 1
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, DIFFICULTY_TARGET_V2);
  EXPECT_EQ(d, 1);
}

TEST(DifficultyTests, next_difficulty_single)
{
  // Single entry: length <= 1 returns 1
  std::vector<uint64_t> timestamps = {1000};
  std::vector<cryptonote::difficulty_type> difficulties = {100};
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, DIFFICULTY_TARGET_V2);
  EXPECT_EQ(d, 1);
}

TEST(DifficultyTests, next_difficulty_constant)
{
  // All timestamps equally spaced at target interval, constant difficulty increments
  const size_t target_seconds = DIFFICULTY_TARGET_V2;
  const size_t n = 20;
  const cryptonote::difficulty_type base_diff = 1000;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i * target_seconds);
    difficulties.push_back(base_diff * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, target_seconds);
  // With blocks arriving exactly on schedule, difficulty should stay near base_diff
  EXPECT_GT(d, 0);
  EXPECT_LT(d, base_diff * 3);
}
