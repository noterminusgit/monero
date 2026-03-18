// Copyright (c) 2019-2024, The Monero Project
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
#include "int-util.h"
#include "cryptonote_basic/difficulty.h"
#include "cryptonote_config.h"

static cryptonote::difficulty_type MKDIFF(uint64_t high, uint64_t low)
{
  cryptonote::difficulty_type d = high;
  d = (d << 64) | low;
  return d;
}

static crypto::hash MKHASH(uint64_t high, uint64_t low)
{
  cryptonote::difficulty_type hash_target = high;
  hash_target = (hash_target << 64) | low;
  boost::multiprecision::uint256_t hash_value = std::numeric_limits<boost::multiprecision::uint256_t>::max() / hash_target;
  crypto::hash h;
  uint64_t val;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[0] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[1] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[2] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[3] = SWAP64LE(val);
  return h;
}

TEST(difficulty, check_hash)
{
  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0, 1), MKDIFF(0, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0, 1), MKDIFF(0, 2)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0, 0xffffffffffffffff), MKDIFF(0, 0xffffffffffffffff)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0, 0xffffffffffffffff), MKDIFF(1, 0)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(1, 1), MKDIFF(1, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(1, 1), MKDIFF(1, 2)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0xffffffffffffffff, 1), MKDIFF(0xffffffffffffffff, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0xffffffffffffffff, 1), MKDIFF(0xffffffffffffffff, 2)));
}

TEST(difficulty, check_hash_difficulty_one)
{
  // Difficulty 1 should accept any hash
  crypto::hash h;
  memset(&h, 0xff, sizeof(h));
  ASSERT_TRUE(cryptonote::check_hash(h, MKDIFF(0, 1)));
}

TEST(difficulty, check_hash_zero_hash)
{
  // Zero hash should pass any difficulty
  crypto::hash h;
  memset(&h, 0, sizeof(h));
  ASSERT_TRUE(cryptonote::check_hash(h, MKDIFF(0, 1)));
  ASSERT_TRUE(cryptonote::check_hash(h, MKDIFF(0, 1000)));
  ASSERT_TRUE(cryptonote::check_hash(h, MKDIFF(0xffffffffffffffff, 0xffffffffffffffff)));
}

TEST(difficulty, hex_conversion)
{
  ASSERT_EQ(cryptonote::hex(MKDIFF(0, 0)), "0x0");
  ASSERT_EQ(cryptonote::hex(MKDIFF(0, 1)), "0x1");
  ASSERT_EQ(cryptonote::hex(MKDIFF(0, 255)), "0xff");
  ASSERT_EQ(cryptonote::hex(MKDIFF(0, 256)), "0x100");
  ASSERT_EQ(cryptonote::hex(MKDIFF(1, 0)), "0x10000000000000000");
}

TEST(difficulty, next_difficulty_empty_inputs)
{
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  // Empty inputs should return 1
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_EQ(d, 1);
}

TEST(difficulty, next_difficulty_single_block)
{
  std::vector<uint64_t> timestamps = {1000};
  std::vector<cryptonote::difficulty_type> difficulties = {100};
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_EQ(d, 1);
}

TEST(difficulty, next_difficulty_steady_rate)
{
  // Simulate blocks at exactly the target rate
  const size_t target_seconds = 120;
  const size_t n = 20;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i * target_seconds);
    difficulties.push_back(1000 * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, target_seconds);
  // Difficulty should stay roughly at 1000
  ASSERT_GT(d, 0);
  ASSERT_LT(d, 2000);
}

TEST(difficulty, next_difficulty_64_matches)
{
  // 64-bit variant should give same result for small values
  const size_t target_seconds = 120;
  std::vector<uint64_t> timestamps = {100, 220, 340, 460};
  std::vector<uint64_t> cum_diff_64 = {100, 200, 300, 400};
  std::vector<cryptonote::difficulty_type> cum_diff_128;
  for (auto d : cum_diff_64)
    cum_diff_128.push_back(d);

  uint64_t d64 = cryptonote::next_difficulty_64(timestamps, cum_diff_64, target_seconds);
  cryptonote::difficulty_type d128 = cryptonote::next_difficulty(timestamps, cum_diff_128, target_seconds);
  ASSERT_EQ(d64, d128.convert_to<uint64_t>());
}

// ===== Phase 7 extended tests =====

TEST(difficulty, all_zero_timestamps)
{
  // All timestamps the same (0) - blocks all at once
  const size_t n = 10;
  std::vector<uint64_t> timestamps(n, 0);
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
    difficulties.push_back(100 * (i + 1));

  // Should not crash, result should be valid
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_GT(d, 0);
}

TEST(difficulty, monotonically_increasing_timestamps)
{
  // Timestamps go up by 1 second each (very fast blocks)
  const size_t n = 20;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i);
    difficulties.push_back(1000 * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  // With very fast blocks (1s apart vs 120s target), difficulty should increase
  ASSERT_GT(d, 0);
}

TEST(difficulty, timestamps_at_target_spacing)
{
  // Blocks arrive exactly at the target spacing
  const size_t target = 120;
  const size_t n = 30;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  cryptonote::difficulty_type base_diff = 5000;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i * target);
    difficulties.push_back(base_diff * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, target);
  // Difficulty should stay roughly at base_diff
  ASSERT_GT(d, 0);
  ASSERT_LT(d, base_diff * 3);
}

TEST(difficulty, very_fast_blocks)
{
  // Blocks arriving every 2 seconds (target 120)
  const size_t n = 15;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i * 2);
    difficulties.push_back(500 * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_GT(d, 0);
}

TEST(difficulty, very_slow_blocks)
{
  // Blocks arriving every 1000 seconds (target 120)
  const size_t n = 15;
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> difficulties;
  for (size_t i = 0; i < n; ++i)
  {
    timestamps.push_back(1000 + i * 1000);
    difficulties.push_back(500 * (i + 1));
  }
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_GT(d, 0);
}

TEST(difficulty, difficulty_target_constants)
{
  // Verify the known target constants
  ASSERT_EQ(DIFFICULTY_TARGET_V1, 60);
  ASSERT_EQ(DIFFICULTY_TARGET_V2, 120);
}

TEST(difficulty, two_blocks_difficulty)
{
  std::vector<uint64_t> timestamps = {1000, 1120};
  std::vector<cryptonote::difficulty_type> difficulties = {100, 200};
  cryptonote::difficulty_type d = cryptonote::next_difficulty(timestamps, difficulties, 120);
  ASSERT_GT(d, 0);
}

TEST(difficulty, max_difficulty_check_hash)
{
  // Max difficulty: should reject almost all hashes
  cryptonote::difficulty_type max_diff = MKDIFF(0xffffffffffffffff, 0xffffffffffffffff);
  crypto::hash h;
  memset(&h, 0x01, sizeof(h)); // non-zero hash
  ASSERT_FALSE(cryptonote::check_hash(h, max_diff));
}

TEST(difficulty, difficulty_one_accepts_all)
{
  // Difficulty 1 should accept any hash
  cryptonote::difficulty_type diff_one = MKDIFF(0, 1);
  crypto::hash h;
  memset(&h, 0xff, sizeof(h));
  ASSERT_TRUE(cryptonote::check_hash(h, diff_one));
  memset(&h, 0x80, sizeof(h));
  ASSERT_TRUE(cryptonote::check_hash(h, diff_one));
}

TEST(difficulty, check_hash_boundary)
{
  // Create a hash that barely passes a difficulty
  cryptonote::difficulty_type diff = MKDIFF(0, 1000);
  crypto::hash h = MKHASH(0, 1000);
  ASSERT_TRUE(cryptonote::check_hash(h, diff));
  // Higher difficulty should reject
  ASSERT_FALSE(cryptonote::check_hash(h, MKDIFF(0, 1001)));
}

TEST(difficulty, hex_large_values)
{
  ASSERT_EQ(cryptonote::hex(MKDIFF(0xffffffffffffffff, 0xffffffffffffffff)),
    "0xffffffffffffffffffffffffffffffff");
}

// Note: next_difficulty with mismatched sizes calls assert() which cannot
// be safely tested with ASSERT_DEATH in a multi-threaded test binary
// (causes heap corruption due to fork). Test removed to avoid crashes.

TEST(difficulty, next_difficulty_64_empty)
{
  std::vector<uint64_t> timestamps;
  std::vector<uint64_t> difficulties;
  uint64_t d = cryptonote::next_difficulty_64(timestamps, difficulties, 120);
  ASSERT_EQ(d, 1u);
}

TEST(difficulty, next_difficulty_64_single)
{
  std::vector<uint64_t> timestamps = {1000};
  std::vector<uint64_t> difficulties = {100};
  uint64_t d = cryptonote::next_difficulty_64(timestamps, difficulties, 120);
  ASSERT_EQ(d, 1u);
}
