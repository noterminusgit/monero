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

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>
#include <boost/foreach.hpp>
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "serialization/binary_archive.h"
#include "serialization/json_archive.h"
#include "serialization/debug_archive.h"
#include "serialization/variant.h"
#include "serialization/containers.h"
#include "serialization/binary_utils.h"
#include "gtest/gtest.h"
using namespace std;

TEST(varint, equal)
{
  for (uint64_t idx = 0; idx < 65537; ++idx)
  {
    char buf[12];
    char *bufptr = buf;
    tools::write_varint(bufptr, idx);
    uint64_t bytes = bufptr - buf;
    ASSERT_TRUE (bytes > 0 && bytes <= sizeof(buf));

    uint64_t idx2;
    std::string s(buf, bytes);
    int read = tools::read_varint(s.begin(), s.end(), idx2);
    ASSERT_EQ (read, bytes);
    ASSERT_TRUE(idx2 == idx);
  }
}

TEST(varint, max_uint64_t_bytes)
{
  unsigned char bytes[100]{};
  unsigned char *p = bytes;
  tools::write_varint(p, std::numeric_limits<std::uint64_t>::max());
  std::size_t i = 0;
  while (bytes[i]) ++i;
  ASSERT_EQ(10, i);        // tests size of max 64-bit uint
  ASSERT_EQ(bytes + i, p); // tests iterator is modified in-place
}

template <typename T>
static void subtest_varint_byte_size_for_single_value(const std::size_t expected_bytes, const T val)
{
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
  static constexpr int DIGITS = std::numeric_limits<T>::digits;
  static_assert(DIGITS == CHAR_BIT * sizeof(T));
  static_assert(sizeof(T));
  static_assert(sizeof(T) <= 8); // 64-bit or less

  unsigned char bytes[100]{};
  unsigned char *p = bytes;
  tools::write_varint(p, val);
  ASSERT_EQ(expected_bytes, p - bytes);
  ASSERT_EQ(expected_bytes, tools::get_varint_byte_size(val));
}

template <typename T>
static void subtest_varint_byte_size_for_type()
{
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
  static constexpr int DIGITS = std::numeric_limits<T>::digits;
  static_assert(DIGITS == CHAR_BIT * sizeof(T));
  static_assert(sizeof(T));

  for (T n = 0; n < 128; ++n)
  {
    subtest_varint_byte_size_for_single_value(1, n);
  }

  for (T n = 128; n < 255; ++n)
  {
    subtest_varint_byte_size_for_single_value(2, n);
  }

  subtest_varint_byte_size_for_single_value(2, T{255});

  if constexpr (DIGITS > 8)
  {
    static_assert(DIGITS >= 16);
    for (T n = 256; n < 16384; ++n)
    {
      subtest_varint_byte_size_for_single_value(2, n);
    }

    for (T n = 16384; n < 65535; ++n)
    {
      subtest_varint_byte_size_for_single_value(3, n);
    }

    subtest_varint_byte_size_for_single_value(3, T{65535});

    if constexpr (DIGITS > 16)
    {
      static_assert(DIGITS >= 32);
      subtest_varint_byte_size_for_single_value(3, T{2097151});
      subtest_varint_byte_size_for_single_value(4, T{2097152});
      subtest_varint_byte_size_for_single_value(4, T{268435455});
      subtest_varint_byte_size_for_single_value(5, T{268435456});
      subtest_varint_byte_size_for_single_value(5, T{4294967295});

      if constexpr (DIGITS > 32)
      {
        static_assert(DIGITS >= 64);
        subtest_varint_byte_size_for_single_value(5, T{34359738367});
        subtest_varint_byte_size_for_single_value(6, T{34359738368});
        subtest_varint_byte_size_for_single_value(6, T{4398046511103});
        subtest_varint_byte_size_for_single_value(7, T{4398046511104});
        subtest_varint_byte_size_for_single_value(7, T{562949953421311});
        subtest_varint_byte_size_for_single_value(8, T{562949953421312});
        subtest_varint_byte_size_for_single_value(8, T{72057594037927935});
        subtest_varint_byte_size_for_single_value(9, T{72057594037927936});
        subtest_varint_byte_size_for_single_value(9, T{9223372036854775807});
        subtest_varint_byte_size_for_single_value(10, T{9223372036854775808ull});
        subtest_varint_byte_size_for_single_value(10, T{18446744073709551615ull});
      }
    }
  }
};

TEST(varint, get_varint_byte_size)
{
  subtest_varint_byte_size_for_type<std::uint8_t>();
  subtest_varint_byte_size_for_type<std::uint16_t>();
  subtest_varint_byte_size_for_type<std::uint32_t>();
  subtest_varint_byte_size_for_type<std::uint64_t>();
}

TEST(varint, roundtrip_zero)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(0));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 1u);

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 1);
  ASSERT_EQ(val, 0u);
}

TEST(varint, roundtrip_one)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(1));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 1u);

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 1);
  ASSERT_EQ(val, 1u);
}

TEST(varint, roundtrip_127)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(127));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 1u); // 127 fits in 1 byte (7 bits)

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 1);
  ASSERT_EQ(val, 127u);
}

TEST(varint, roundtrip_128)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(128));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 2u); // 128 needs 2 bytes

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 2);
  ASSERT_EQ(val, 128u);
}

TEST(varint, roundtrip_16383)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(16383));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 2u); // 16383 = 0x3FFF, fits in 14 bits (2 varint bytes)

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 2);
  ASSERT_EQ(val, 16383u);
}

TEST(varint, roundtrip_16384)
{
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(16384));
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 3u); // 16384 = 0x4000, needs 3 varint bytes

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 3);
  ASSERT_EQ(val, 16384u);
}

TEST(varint, roundtrip_uint64_max)
{
  uint64_t max_val = std::numeric_limits<uint64_t>::max();
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, max_val);
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 10u); // max uint64 needs 10 varint bytes

  uint64_t val;
  std::string s(buf, bytes);
  int read = tools::read_varint(s.begin(), s.end(), val);
  ASSERT_EQ(read, 10);
  ASSERT_EQ(val, max_val);
}

TEST(varint, truncated_input_returns_partial_read)
{
  // Write a value that needs 2 bytes, then only provide 1 byte
  char buf[12];
  char *bufptr = buf;
  tools::write_varint(bufptr, uint64_t(128)); // needs 2 bytes
  uint64_t bytes = bufptr - buf;
  ASSERT_EQ(bytes, 2u);

  // Only provide 1 byte (truncated)
  uint64_t val = 0;
  std::string s(buf, 1);
  int read = tools::read_varint(s.begin(), s.end(), val);
  // When input is exhausted in the middle, read returns how many bytes were read
  // The first byte has MSB set (continuation), so we read 1 byte but never complete
  ASSERT_EQ(read, 1);
}

TEST(varint, get_varint_data)
{
  // Test the get_varint_data helper
  std::string data = tools::get_varint_data(uint64_t(0));
  ASSERT_EQ(data.size(), 1u);

  data = tools::get_varint_data(uint64_t(127));
  ASSERT_EQ(data.size(), 1u);

  data = tools::get_varint_data(uint64_t(128));
  ASSERT_EQ(data.size(), 2u);

  data = tools::get_varint_data(std::numeric_limits<uint64_t>::max());
  ASSERT_EQ(data.size(), 10u);
}

TEST(varint, roundtrip_powers_of_two)
{
  for (int i = 0; i < 63; ++i)
  {
    uint64_t val = uint64_t(1) << i;
    char buf[12];
    char *bufptr = buf;
    tools::write_varint(bufptr, val);
    uint64_t bytes = bufptr - buf;
    ASSERT_GT(bytes, 0u);

    uint64_t result;
    std::string s(buf, bytes);
    int read = tools::read_varint(s.begin(), s.end(), result);
    ASSERT_EQ(static_cast<uint64_t>(read), bytes);
    ASSERT_EQ(result, val);
  }
}

TEST(varint, get_varint_byte_size_matches_write)
{
  // Verify get_varint_byte_size matches actual written size for various values
  std::vector<uint64_t> test_vals = {0, 1, 127, 128, 255, 256, 16383, 16384,
    65535, 65536, 1000000, 0xFFFFFFFF, 0x100000000ULL, 0xFFFFFFFFFFFFFFFFULL};
  for (uint64_t val : test_vals)
  {
    char buf[12];
    char *bufptr = buf;
    tools::write_varint(bufptr, val);
    uint64_t written = bufptr - buf;
    ASSERT_EQ(written, tools::get_varint_byte_size(val));
  }
}
