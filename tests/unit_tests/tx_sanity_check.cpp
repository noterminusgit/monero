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

#include <set>
#include <cstdint>
#include "cryptonote_core/tx_sanity_check.h"

// Test the set-based overload directly (no blob parsing needed)

TEST(tx_sanity_check, few_indices_always_passes)
{
  // n_indices <= 10 should always pass regardless of other params
  std::set<uint64_t> indices;
  for (uint64_t i = 0; i < 10; ++i)
    indices.insert(i);
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 10, 100000));
}

TEST(tx_sanity_check, few_rct_outs_always_passes)
{
  // rct_outs_available < 10000 should always pass
  std::set<uint64_t> indices;
  for (uint64_t i = 0; i < 20; ++i)
    indices.insert(i);
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 20, 9999));
}

TEST(tx_sanity_check, too_few_unique_indices_fails)
{
  // rct_indices.size() < n_indices * 8 / 10 should fail
  // n_indices = 100, unique count needs to be >= 80
  std::set<uint64_t> indices;
  for (uint64_t i = 0; i < 79; ++i)
    indices.insert(i + 50000); // high offsets so median check passes
  // 79 unique out of 100 total -> 79 < 80 -> fail
  ASSERT_FALSE(cryptonote::tx_sanity_check(indices, 100, 100000));
}

TEST(tx_sanity_check, enough_unique_indices_passes)
{
  // rct_indices.size() >= n_indices * 8 / 10 should pass (if median is ok)
  std::set<uint64_t> indices;
  uint64_t rct_outs = 100000;
  // Put all indices in the high range so median > 60% of rct_outs
  for (uint64_t i = 0; i < 80; ++i)
    indices.insert(rct_outs - 100 + i);
  // 80 unique out of 100 total -> 80 >= 80 -> passes uniqueness check
  // median is near rct_outs -> passes median check
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 100, rct_outs));
}

TEST(tx_sanity_check, low_median_fails)
{
  // median < rct_outs_available * 6 / 10 should fail
  uint64_t rct_outs = 100000;
  std::set<uint64_t> indices;
  // All indices in the low range
  for (uint64_t i = 0; i < 100; ++i)
    indices.insert(i);
  // median ~50, threshold is 60000 -> fail
  ASSERT_FALSE(cryptonote::tx_sanity_check(indices, 100, rct_outs));
}

TEST(tx_sanity_check, high_median_passes)
{
  uint64_t rct_outs = 100000;
  std::set<uint64_t> indices;
  // All indices in the high range (above 60% threshold)
  for (uint64_t i = 0; i < 100; ++i)
    indices.insert(rct_outs - 200 + i);
  // median ~99900, threshold is 60000 -> pass
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 100, rct_outs));
}

TEST(tx_sanity_check, zero_indices_passes)
{
  std::set<uint64_t> indices;
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 0, 100000));
}

TEST(tx_sanity_check, borderline_unique_ratio)
{
  // Exactly at the 80% boundary
  uint64_t rct_outs = 100000;
  std::set<uint64_t> indices;
  // n_indices = 50, need >= 40 unique
  for (uint64_t i = 0; i < 40; ++i)
    indices.insert(rct_outs - 50 + i);
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 50, rct_outs));
}

TEST(tx_sanity_check, borderline_median)
{
  // Median exactly at the 60% boundary
  uint64_t rct_outs = 100000;
  uint64_t threshold = rct_outs * 6 / 10; // 60000
  std::set<uint64_t> indices;
  // Create indices centered around threshold
  for (uint64_t i = 0; i < 100; ++i)
    indices.insert(threshold - 50 + i);
  // median should be around threshold
  ASSERT_TRUE(cryptonote::tx_sanity_check(indices, 100, rct_outs));
}
