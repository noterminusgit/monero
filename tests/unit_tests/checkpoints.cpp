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
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

#include "checkpoints/checkpoints.cpp"

using namespace cryptonote;


TEST(checkpoints_is_alternative_block_allowed, handles_empty_checkpoints)
{
  checkpoints cp;

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE(cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE(cp.is_alternative_block_allowed(9, 1));
}

TEST(checkpoints_is_alternative_block_allowed, handles_one_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 9));

  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 9));
}

TEST(checkpoints_is_alternative_block_allowed, handles_two_and_more_checkpoints)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(9, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 11));

  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(10, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(10, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(11, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(11, 11));
}

TEST(checkpoints, add_checkpoint_basic)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000000"));
}

TEST(checkpoints, add_duplicate_same_hash)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000000"));
  // Adding same checkpoint again with same hash should succeed
  ASSERT_TRUE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000000"));
}

TEST(checkpoints, add_duplicate_different_hash)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000000"));
  // Adding same height with different hash should fail
  ASSERT_FALSE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000001"));
}

TEST(checkpoints, check_block_at_checkpoint)
{
  checkpoints cp;
  crypto::hash null_hash = crypto::null_hash;
  ASSERT_TRUE(cp.add_checkpoint(5, epee::string_tools::pod_to_hex(null_hash)));

  bool is_a_checkpoint = false;
  // Correct hash at checkpoint height
  ASSERT_TRUE(cp.check_block(5, null_hash, is_a_checkpoint));
  ASSERT_TRUE(is_a_checkpoint);
}

TEST(checkpoints, check_block_wrong_hash)
{
  checkpoints cp;
  crypto::hash null_hash = crypto::null_hash;
  ASSERT_TRUE(cp.add_checkpoint(5, epee::string_tools::pod_to_hex(null_hash)));

  // Wrong hash at checkpoint height
  crypto::hash wrong_hash;
  memset(&wrong_hash, 0xff, sizeof(wrong_hash));
  bool is_a_checkpoint = false;
  ASSERT_FALSE(cp.check_block(5, wrong_hash, is_a_checkpoint));
}

TEST(checkpoints, check_block_not_at_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));

  // Height 3 is not a checkpoint
  crypto::hash some_hash;
  memset(&some_hash, 0xab, sizeof(some_hash));
  bool is_a_checkpoint = false;
  ASSERT_TRUE(cp.check_block(3, some_hash, is_a_checkpoint));
  ASSERT_FALSE(is_a_checkpoint);
}

TEST(checkpoints, is_in_checkpoint_zone)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(10, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_TRUE(cp.is_in_checkpoint_zone(5));
  ASSERT_TRUE(cp.is_in_checkpoint_zone(10));
  ASSERT_FALSE(cp.is_in_checkpoint_zone(11));
}

TEST(checkpoints, is_in_checkpoint_zone_empty)
{
  checkpoints cp;
  ASSERT_FALSE(cp.is_in_checkpoint_zone(0));
  ASSERT_FALSE(cp.is_in_checkpoint_zone(1));
}

TEST(checkpoints, get_max_height)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(10, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(3, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_EQ(cp.get_max_height(), 10u);
}

TEST(checkpoints, get_max_height_empty)
{
  checkpoints cp;
  ASSERT_EQ(cp.get_max_height(), 0u);
}

TEST(checkpoints, get_max_height_single)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(42, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_EQ(cp.get_max_height(), 42u);
}

TEST(checkpoints, multiple_checkpoints_check_each)
{
  checkpoints cp;
  crypto::hash null_hash = crypto::null_hash;
  std::string null_hex = epee::string_tools::pod_to_hex(null_hash);

  ASSERT_TRUE(cp.add_checkpoint(10, null_hex));
  ASSERT_TRUE(cp.add_checkpoint(20, null_hex));
  ASSERT_TRUE(cp.add_checkpoint(30, null_hex));

  bool is_checkpoint = false;
  ASSERT_TRUE(cp.check_block(10, null_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);
  ASSERT_TRUE(cp.check_block(20, null_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);
  ASSERT_TRUE(cp.check_block(30, null_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);
}

TEST(checkpoints, check_block_zero_height)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(0, "0000000000000000000000000000000000000000000000000000000000000000"));

  crypto::hash null_hash = crypto::null_hash;
  bool is_checkpoint = false;
  ASSERT_TRUE(cp.check_block(0, null_hash, is_checkpoint));
  ASSERT_TRUE(is_checkpoint);
}

TEST(checkpoints, is_in_checkpoint_zone_at_zero)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(0, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.is_in_checkpoint_zone(0));
}

TEST(checkpoints, add_multiple_sequential)
{
  checkpoints cp;
  for (uint64_t h = 0; h < 100; h += 10)
  {
    ASSERT_TRUE(cp.add_checkpoint(h, "0000000000000000000000000000000000000000000000000000000000000000"));
  }
  ASSERT_EQ(cp.get_max_height(), 90u);
}

TEST(checkpoints, add_invalid_hash_format)
{
  checkpoints cp;
  ASSERT_FALSE(cp.add_checkpoint(1, "not_a_valid_hex_hash"));
}

TEST(checkpoints, add_short_hash_fails)
{
  checkpoints cp;
  ASSERT_FALSE(cp.add_checkpoint(1, "0000"));
}

TEST(checkpoints, large_height)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(1000000, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_EQ(cp.get_max_height(), 1000000u);
  ASSERT_TRUE(cp.is_in_checkpoint_zone(500000));
  ASSERT_FALSE(cp.is_in_checkpoint_zone(1000001));
}

// ============================================================================
// Bug #12 regression: is_alternative_block_allowed policy verification
// The function allows alt blocks only above the highest checkpoint at or
// below blockchain_height. These tests verify edge cases and consistency.
// ============================================================================

TEST(checkpoints_is_alternative_block_allowed, multiple_checkpoints_various_heights)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(10, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(50, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(100, "0000000000000000000000000000000000000000000000000000000000000000"));

  // Before any checkpoint: all alt blocks allowed (except height 0)
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 0));
  ASSERT_TRUE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(5, 50));
  ASSERT_TRUE(cp.is_alternative_block_allowed(5, 100));

  // At checkpoint 10: blocks above 10 allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 10));
  ASSERT_TRUE(cp.is_alternative_block_allowed(10, 11));
  ASSERT_TRUE(cp.is_alternative_block_allowed(10, 50));

  // Between checkpoints 10 and 50 (e.g., at 30): highest cp <= 30 is 10
  ASSERT_FALSE(cp.is_alternative_block_allowed(30, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(30, 10));
  ASSERT_TRUE(cp.is_alternative_block_allowed(30, 11));
  ASSERT_TRUE(cp.is_alternative_block_allowed(30, 30));

  // At checkpoint 50: blocks above 50 allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(50, 10));
  ASSERT_FALSE(cp.is_alternative_block_allowed(50, 50));
  ASSERT_TRUE(cp.is_alternative_block_allowed(50, 51));
  ASSERT_TRUE(cp.is_alternative_block_allowed(50, 100));

  // Beyond all checkpoints (at 200): highest cp <= 200 is 100
  ASSERT_FALSE(cp.is_alternative_block_allowed(200, 50));
  ASSERT_FALSE(cp.is_alternative_block_allowed(200, 100));
  ASSERT_TRUE(cp.is_alternative_block_allowed(200, 101));
  ASSERT_TRUE(cp.is_alternative_block_allowed(200, 200));
}

TEST(checkpoints_is_alternative_block_allowed, block_height_exactly_at_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(20, "0000000000000000000000000000000000000000000000000000000000000000"));

  // When blockchain_height >= checkpoint and block_height == checkpoint,
  // the block should NOT be allowed (checkpoint_height < block_height is false)
  ASSERT_FALSE(cp.is_alternative_block_allowed(20, 20));
  ASSERT_FALSE(cp.is_alternative_block_allowed(25, 20));
  ASSERT_FALSE(cp.is_alternative_block_allowed(100, 20));

  // block_height one above checkpoint IS allowed
  ASSERT_TRUE(cp.is_alternative_block_allowed(20, 21));
  ASSERT_TRUE(cp.is_alternative_block_allowed(25, 21));
}

TEST(checkpoints_is_alternative_block_allowed, blockchain_height_exactly_at_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(15, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(30, "0000000000000000000000000000000000000000000000000000000000000000"));

  // blockchain_height at first checkpoint (15): highest cp <= 15 is 15
  ASSERT_FALSE(cp.is_alternative_block_allowed(15, 10));
  ASSERT_FALSE(cp.is_alternative_block_allowed(15, 15));
  ASSERT_TRUE(cp.is_alternative_block_allowed(15, 16));

  // blockchain_height at second checkpoint (30): highest cp <= 30 is 30
  ASSERT_FALSE(cp.is_alternative_block_allowed(30, 15));
  ASSERT_FALSE(cp.is_alternative_block_allowed(30, 20));
  ASSERT_FALSE(cp.is_alternative_block_allowed(30, 30));
  ASSERT_TRUE(cp.is_alternative_block_allowed(30, 31));
}

TEST(checkpoints_is_alternative_block_allowed, many_checkpoints_consistency)
{
  checkpoints cp;
  // Add checkpoints every 10 heights from 10 to 100
  for (uint64_t h = 10; h <= 100; h += 10)
  {
    ASSERT_TRUE(cp.add_checkpoint(h, "0000000000000000000000000000000000000000000000000000000000000000"));
  }

  // For any blockchain_height past all checkpoints, alt blocks must be
  // above the highest checkpoint (100)
  for (uint64_t bh = 100; bh <= 150; ++bh)
  {
    ASSERT_FALSE(cp.is_alternative_block_allowed(bh, 50));
    ASSERT_FALSE(cp.is_alternative_block_allowed(bh, 100));
    ASSERT_TRUE(cp.is_alternative_block_allowed(bh, 101));
  }

  // For blockchain_height between checkpoints 50 and 60:
  // highest cp <= 55 is 50, so alt blocks above 50 are allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(55, 30));
  ASSERT_FALSE(cp.is_alternative_block_allowed(55, 50));
  ASSERT_TRUE(cp.is_alternative_block_allowed(55, 51));
  ASSERT_TRUE(cp.is_alternative_block_allowed(55, 55));
}

TEST(checkpoints_is_alternative_block_allowed, large_height_values)
{
  checkpoints cp;
  // Use large but not max values to avoid any overflow
  uint64_t large_cp = 1000000000ULL;
  ASSERT_TRUE(cp.add_checkpoint(large_cp, "0000000000000000000000000000000000000000000000000000000000000000"));

  // Below checkpoint height in blockchain: all alt blocks allowed
  ASSERT_TRUE(cp.is_alternative_block_allowed(500000000ULL, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(500000000ULL, 999999999ULL));

  // At checkpoint height: only above checkpoint allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(large_cp, large_cp));
  ASSERT_TRUE(cp.is_alternative_block_allowed(large_cp, large_cp + 1));

  // Above checkpoint: only above checkpoint allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(large_cp + 1000, large_cp));
  ASSERT_TRUE(cp.is_alternative_block_allowed(large_cp + 1000, large_cp + 1));
}

TEST(checkpoints_is_alternative_block_allowed, block_height_zero_always_disallowed)
{
  // Block height 0 (genesis) should never be allowed as an alt block
  checkpoints cp_empty;
  ASSERT_FALSE(cp_empty.is_alternative_block_allowed(0, 0));
  ASSERT_FALSE(cp_empty.is_alternative_block_allowed(100, 0));

  checkpoints cp_with;
  ASSERT_TRUE(cp_with.add_checkpoint(10, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_FALSE(cp_with.is_alternative_block_allowed(0, 0));
  ASSERT_FALSE(cp_with.is_alternative_block_allowed(5, 0));
  ASSERT_FALSE(cp_with.is_alternative_block_allowed(15, 0));
}

TEST(checkpoints_is_alternative_block_allowed, single_checkpoint_at_one)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(1, "0000000000000000000000000000000000000000000000000000000000000000"));

  // blockchain_height 0 (before checkpoint): all alt blocks allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));  // height 0 always false
  ASSERT_TRUE(cp.is_alternative_block_allowed(0, 1));

  // At and after checkpoint 1: only above 1 allowed
  ASSERT_FALSE(cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(1, 2));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(5, 2));
}
