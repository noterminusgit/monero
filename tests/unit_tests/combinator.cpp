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

#include "common/combinator.h"
#include <set>
#include <string>

TEST(combinator, combinations_count_basic)
{
  ASSERT_EQ(tools::combinations_count(0, 0), 1u);
  ASSERT_EQ(tools::combinations_count(0, 5), 1u);
  ASSERT_EQ(tools::combinations_count(1, 5), 5u);
  ASSERT_EQ(tools::combinations_count(2, 5), 10u);
  ASSERT_EQ(tools::combinations_count(3, 5), 10u);
  ASSERT_EQ(tools::combinations_count(4, 5), 5u);
  ASSERT_EQ(tools::combinations_count(5, 5), 1u);
}

TEST(combinator, combinations_count_k_greater_than_n_throws)
{
  ASSERT_THROW(tools::combinations_count(6, 5), std::runtime_error);
  ASSERT_THROW(tools::combinations_count(1, 0), std::runtime_error);
}

TEST(combinator, combinations_count_symmetry)
{
  // C(k, n) == C(n-k, n)
  for (uint32_t n = 0; n <= 10; ++n)
    for (uint32_t k = 0; k <= n; ++k)
      ASSERT_EQ(tools::combinations_count(k, n), tools::combinations_count(n - k, n));
}

TEST(combinator, combine_k_equals_1)
{
  std::vector<int> v{10, 20, 30};
  tools::Combinator<int> c(v);
  auto result = c.combine(1);
  ASSERT_EQ(result.size(), 3u);
  ASSERT_EQ(result[0].size(), 1u);
  ASSERT_EQ(result[0][0], 10);
  ASSERT_EQ(result[1][0], 20);
  ASSERT_EQ(result[2][0], 30);
}

TEST(combinator, combine_k_equals_n)
{
  std::vector<int> v{1, 2, 3};
  tools::Combinator<int> c(v);
  auto result = c.combine(3);
  ASSERT_EQ(result.size(), 1u);
  ASSERT_EQ(result[0].size(), 3u);
  ASSERT_EQ(result[0][0], 1);
  ASSERT_EQ(result[0][1], 2);
  ASSERT_EQ(result[0][2], 3);
}

TEST(combinator, combine_2_of_4)
{
  std::vector<int> v{1, 2, 3, 4};
  tools::Combinator<int> c(v);
  auto result = c.combine(2);
  ASSERT_EQ(result.size(), 6u); // C(2,4) = 6
}

TEST(combinator, combine_3_of_5)
{
  std::vector<std::string> v{"a", "b", "c", "d", "e"};
  tools::Combinator<std::string> c(v);
  auto result = c.combine(3);
  ASSERT_EQ(result.size(), 10u); // C(3,5) = 10
  // Each combination should have 3 elements
  for (const auto& combo : result)
    ASSERT_EQ(combo.size(), 3u);
}

TEST(combinator, combine_k_zero_throws)
{
  std::vector<int> v{1, 2, 3};
  tools::Combinator<int> c(v);
  ASSERT_THROW(c.combine(0), std::runtime_error);
}

TEST(combinator, combine_k_greater_than_n_throws)
{
  std::vector<int> v{1, 2};
  tools::Combinator<int> c(v);
  ASSERT_THROW(c.combine(3), std::runtime_error);
}

TEST(combinator, combine_count_matches_function)
{
  std::vector<int> v{1, 2, 3, 4, 5, 6};
  tools::Combinator<int> c(v);
  for (size_t k = 1; k <= v.size(); ++k)
  {
    auto result = c.combine(k);
    ASSERT_EQ(result.size(), tools::combinations_count(k, v.size()));
  }
}

TEST(combinator, combinations_count_larger_values)
{
  ASSERT_EQ(tools::combinations_count(3, 10), 120u);
  ASSERT_EQ(tools::combinations_count(4, 10), 210u);
  ASSERT_EQ(tools::combinations_count(5, 10), 252u);
  ASSERT_EQ(tools::combinations_count(2, 20), 190u);
  ASSERT_EQ(tools::combinations_count(3, 20), 1140u);
}

TEST(combinator, combinations_count_pascal_triangle)
{
  // C(k, n) = C(k-1, n-1) + C(k, n-1) for k > 0 and n > 0
  for (uint32_t n = 2; n <= 12; ++n)
    for (uint32_t k = 1; k < n; ++k)
      ASSERT_EQ(tools::combinations_count(k, n),
                tools::combinations_count(k - 1, n - 1) + tools::combinations_count(k, n - 1));
}

TEST(combinator, combine_uniqueness)
{
  // All combinations should be unique
  std::vector<int> v{1, 2, 3, 4, 5};
  tools::Combinator<int> c(v);
  auto result = c.combine(3);
  ASSERT_EQ(result.size(), 10u);

  // Convert each combination to a set to ensure elements are unique within each combination
  for (const auto& combo : result)
  {
    std::set<int> s(combo.begin(), combo.end());
    ASSERT_EQ(s.size(), combo.size());
  }

  // Ensure all combinations are unique
  std::set<std::vector<int>> unique_combos(result.begin(), result.end());
  ASSERT_EQ(unique_combos.size(), result.size());
}

TEST(combinator, combine_elements_from_source)
{
  // Each element in each combination must come from the original set
  std::vector<int> v{10, 20, 30, 40};
  std::set<int> source_set(v.begin(), v.end());
  tools::Combinator<int> c(v);
  auto result = c.combine(2);
  for (const auto& combo : result)
    for (int elem : combo)
      ASSERT_TRUE(source_set.count(elem) > 0);
}

TEST(combinator, combine_string_type)
{
  // Test with string types to verify template works with non-trivial types
  std::vector<std::string> v{"alpha", "beta", "gamma", "delta"};
  tools::Combinator<std::string> c(v);
  auto result = c.combine(2);
  ASSERT_EQ(result.size(), 6u); // C(2,4) = 6
  for (const auto& combo : result)
  {
    ASSERT_EQ(combo.size(), 2u);
    ASSERT_NE(combo[0], combo[1]); // No duplicates within a combination
  }
}

TEST(combinator, combine_single_element_vector)
{
  std::vector<int> v{42};
  tools::Combinator<int> c(v);
  auto result = c.combine(1);
  ASSERT_EQ(result.size(), 1u);
  ASSERT_EQ(result[0].size(), 1u);
  ASSERT_EQ(result[0][0], 42);
}

TEST(combinator, combinations_count_n_equals_n)
{
  // C(n,n) = 1 for all n
  for (uint32_t n = 0; n <= 20; ++n)
    ASSERT_EQ(tools::combinations_count(n, n), 1u);
}

TEST(combinator, combinations_count_k_equals_1)
{
  // C(1,n) = n for all n >= 1
  for (uint32_t n = 1; n <= 20; ++n)
    ASSERT_EQ(tools::combinations_count(1, n), static_cast<uint64_t>(n));
}
