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

#include "serialization/keyvalue_serialization.h"
#include "storages/portable_storage.h"
#include "storages/portable_storage_template_helper.h"
#include "byte_slice.h"

namespace
{
  struct simple_struct
  {
    int32_t val_int;
    std::string val_str;
    uint64_t val_uint64;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val_int)
      KV_SERIALIZE(val_str)
      KV_SERIALIZE(val_uint64)
    END_KV_SERIALIZE_MAP()
  };

  struct nested_struct
  {
    simple_struct inner;
    bool flag;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(inner)
      KV_SERIALIZE(flag)
    END_KV_SERIALIZE_MAP()
  };

  struct list_struct
  {
    std::vector<int32_t> ints;
    std::vector<std::string> strings;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(ints)
      KV_SERIALIZE(strings)
    END_KV_SERIALIZE_MAP()
  };

  struct optional_struct
  {
    int32_t required_val;
    int32_t optional_val;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(required_val)
      KV_SERIALIZE_OPT(optional_val, 42)
    END_KV_SERIALIZE_MAP()
  };
}

TEST(portable_storage, json_roundtrip_simple)
{
  simple_struct s;
  s.val_int = -42;
  s.val_str = "hello world";
  s.val_uint64 = 1234567890123ULL;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(s, json));
  ASSERT_FALSE(json.empty());

  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s2, json));
  ASSERT_EQ(s2.val_int, -42);
  ASSERT_EQ(s2.val_str, "hello world");
  ASSERT_EQ(s2.val_uint64, 1234567890123ULL);
}

TEST(portable_storage, binary_roundtrip_simple)
{
  simple_struct s;
  s.val_int = 100;
  s.val_str = "test";
  s.val_uint64 = 9999;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  ASSERT_FALSE(blob.empty());

  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val_int, 100);
  ASSERT_EQ(s2.val_str, "test");
  ASSERT_EQ(s2.val_uint64, 9999u);
}

TEST(portable_storage, json_roundtrip_nested)
{
  nested_struct n;
  n.inner.val_int = 10;
  n.inner.val_str = "nested";
  n.inner.val_uint64 = 0;
  n.flag = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(n, json));

  nested_struct n2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(n2, json));
  ASSERT_EQ(n2.inner.val_int, 10);
  ASSERT_EQ(n2.inner.val_str, "nested");
  ASSERT_TRUE(n2.flag);
}

TEST(portable_storage, binary_roundtrip_nested)
{
  nested_struct n;
  n.inner.val_int = -1;
  n.inner.val_str = "deep";
  n.inner.val_uint64 = 42;
  n.flag = false;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(n, blob));

  nested_struct n2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(n2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(n2.inner.val_int, -1);
  ASSERT_EQ(n2.inner.val_str, "deep");
  ASSERT_EQ(n2.inner.val_uint64, 42u);
  ASSERT_FALSE(n2.flag);
}

TEST(portable_storage, json_roundtrip_lists)
{
  list_struct l;
  l.ints = {1, 2, 3, 4, 5};
  l.strings = {"alpha", "beta", "gamma"};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(l, json));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(l2, json));
  ASSERT_EQ(l2.ints.size(), 5u);
  ASSERT_EQ(l2.ints[0], 1);
  ASSERT_EQ(l2.ints[4], 5);
  ASSERT_EQ(l2.strings.size(), 3u);
  ASSERT_EQ(l2.strings[2], "gamma");
}

TEST(portable_storage, binary_roundtrip_lists)
{
  list_struct l;
  l.ints = {10, 20, 30};
  l.strings = {"x"};

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(l, blob));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(l2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(l2.ints.size(), 3u);
  ASSERT_EQ(l2.strings.size(), 1u);
  ASSERT_EQ(l2.strings[0], "x");
}

TEST(portable_storage, optional_field_default)
{
  // Load JSON that doesn't contain optional_val
  std::string json = "{\"required_val\": 7}";
  optional_struct o;
  ASSERT_TRUE(epee::serialization::load_t_from_json(o, json));
  ASSERT_EQ(o.required_val, 7);
  ASSERT_EQ(o.optional_val, 42); // default value
}

TEST(portable_storage, optional_field_provided)
{
  std::string json = "{\"required_val\": 7, \"optional_val\": 99}";
  optional_struct o;
  ASSERT_TRUE(epee::serialization::load_t_from_json(o, json));
  ASSERT_EQ(o.required_val, 7);
  ASSERT_EQ(o.optional_val, 99);
}

TEST(portable_storage, empty_string_field)
{
  simple_struct s;
  s.val_int = 0;
  s.val_str = "";
  s.val_uint64 = 0;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(s, json));

  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s2, json));
  ASSERT_TRUE(s2.val_str.empty());
}

TEST(portable_storage, large_values)
{
  simple_struct s;
  s.val_int = INT32_MAX;
  s.val_str = std::string(10000, 'A');
  s.val_uint64 = UINT64_MAX;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(s, json));

  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s2, json));
  ASSERT_EQ(s2.val_int, INT32_MAX);
  ASSERT_EQ(s2.val_str.size(), 10000u);
  ASSERT_EQ(s2.val_uint64, UINT64_MAX);
}

TEST(portable_storage, empty_list)
{
  list_struct l;
  l.ints = {};
  l.strings = {};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(l, json));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(l2, json));
  ASSERT_TRUE(l2.ints.empty());
  ASSERT_TRUE(l2.strings.empty());
}

TEST(portable_storage, raw_api_set_get_value)
{
  epee::serialization::portable_storage ps;
  ps.set_value("my_int", int64_t(42), nullptr);
  ps.set_value("my_str", std::string("hello"), nullptr);

  int64_t val_int = 0;
  std::string val_str;
  ASSERT_TRUE(ps.get_value("my_int", val_int, nullptr));
  ASSERT_TRUE(ps.get_value("my_str", val_str, nullptr));
  ASSERT_EQ(val_int, 42);
  ASSERT_EQ(val_str, "hello");
}

TEST(portable_storage, raw_api_section)
{
  epee::serialization::portable_storage ps;
  auto section = ps.open_section("child", nullptr, true);
  ASSERT_NE(section, nullptr);

  ps.set_value("nested_val", int64_t(99), section);

  int64_t val = 0;
  ASSERT_TRUE(ps.get_value("nested_val", val, section));
  ASSERT_EQ(val, 99);
}

TEST(portable_storage, raw_api_missing_value)
{
  epee::serialization::portable_storage ps;
  int64_t val = 0;
  ASSERT_FALSE(ps.get_value("nonexistent", val, nullptr));
}

TEST(portable_storage, invalid_json_fails)
{
  simple_struct s;
  ASSERT_FALSE(epee::serialization::load_t_from_json(s, "this is not json"));
}

TEST(portable_storage, invalid_binary_fails)
{
  simple_struct s;
  std::string bad_data = "this is not binary portable storage";
  ASSERT_FALSE(epee::serialization::load_t_from_binary(s, bad_data));
}

// --- Additional portable_storage tests ---

namespace
{
  struct int8_struct
  {
    int8_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct int16_struct
  {
    int16_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct int32_only_struct
  {
    int32_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct int64_only_struct
  {
    int64_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct uint8_struct
  {
    uint8_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct uint16_struct
  {
    uint16_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct uint32_struct
  {
    uint32_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct uint64_only_struct
  {
    uint64_t val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct bool_struct
  {
    bool val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct double_struct
  {
    double val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(val)
    END_KV_SERIALIZE_MAP()
  };

  struct deep_inner
  {
    int32_t leaf_val;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(leaf_val)
    END_KV_SERIALIZE_MAP()
  };

  struct mid_level
  {
    deep_inner inner;
    std::string mid_str;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(inner)
      KV_SERIALIZE(mid_str)
    END_KV_SERIALIZE_MAP()
  };

  struct top_level
  {
    mid_level mid;
    bool top_flag;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(mid)
      KV_SERIALIZE(top_flag)
    END_KV_SERIALIZE_MAP()
  };

  struct nested_list_struct
  {
    std::vector<int32_t> nums;
    nested_struct sub;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(nums)
      KV_SERIALIZE(sub)
    END_KV_SERIALIZE_MAP()
  };
}

TEST(portable_storage, int8_roundtrip)
{
  int8_struct s;
  s.val = -128;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  int8_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, -128);
}

TEST(portable_storage, int8_max_roundtrip)
{
  int8_struct s;
  s.val = 127;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  int8_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, 127);
}

TEST(portable_storage, int16_roundtrip)
{
  int16_struct s;
  s.val = -32768;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  int16_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, -32768);
}

TEST(portable_storage, int32_roundtrip)
{
  int32_only_struct s;
  s.val = INT32_MIN;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  int32_only_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, INT32_MIN);
}

TEST(portable_storage, int64_roundtrip)
{
  int64_only_struct s;
  s.val = INT64_MIN;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  int64_only_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, INT64_MIN);
}

TEST(portable_storage, uint8_roundtrip)
{
  uint8_struct s;
  s.val = 255;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  uint8_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, 255);
}

TEST(portable_storage, uint16_roundtrip)
{
  uint16_struct s;
  s.val = 65535;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  uint16_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, 65535);
}

TEST(portable_storage, uint32_roundtrip)
{
  uint32_struct s;
  s.val = UINT32_MAX;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  uint32_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, UINT32_MAX);
}

TEST(portable_storage, uint64_roundtrip)
{
  uint64_only_struct s;
  s.val = UINT64_MAX;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  uint64_only_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val, UINT64_MAX);
}

TEST(portable_storage, bool_true_roundtrip)
{
  bool_struct s;
  s.val = true;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  bool_struct s2;
  s2.val = false;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_TRUE(s2.val);
}

TEST(portable_storage, bool_false_roundtrip)
{
  bool_struct s;
  s.val = false;
  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  bool_struct s2;
  s2.val = true;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_FALSE(s2.val);
}

TEST(portable_storage, bool_json_roundtrip)
{
  bool_struct s;
  s.val = true;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(s, json));
  bool_struct s2;
  s2.val = false;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s2, json));
  ASSERT_TRUE(s2.val);
}

TEST(portable_storage, string_with_newlines)
{
  simple_struct s;
  s.val_int = 0;
  s.val_str = "line1\nline2\nline3";
  s.val_uint64 = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val_str, "line1\nline2\nline3");
}

TEST(portable_storage, string_with_quotes)
{
  simple_struct s;
  s.val_int = 0;
  s.val_str = "he said \"hello\"";
  s.val_uint64 = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val_str, "he said \"hello\"");
}

TEST(portable_storage, string_with_null_bytes)
{
  simple_struct s;
  s.val_int = 0;
  s.val_str = std::string("abc\0def", 7);
  s.val_uint64 = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val_str.size(), 7u);
  ASSERT_EQ(s2.val_str, std::string("abc\0def", 7));
}

TEST(portable_storage, string_with_tabs_and_backslash)
{
  simple_struct s;
  s.val_int = 0;
  s.val_str = "tab\there\\backslash";
  s.val_uint64 = 0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(s2.val_str, "tab\there\\backslash");
}

TEST(portable_storage, deeply_nested_three_levels_json)
{
  top_level t;
  t.mid.inner.leaf_val = 42;
  t.mid.mid_str = "middle";
  t.top_flag = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(t, json));
  ASSERT_FALSE(json.empty());

  top_level t2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(t2, json));
  ASSERT_EQ(t2.mid.inner.leaf_val, 42);
  ASSERT_EQ(t2.mid.mid_str, "middle");
  ASSERT_TRUE(t2.top_flag);
}

TEST(portable_storage, deeply_nested_three_levels_binary)
{
  top_level t;
  t.mid.inner.leaf_val = -99;
  t.mid.mid_str = "deep_binary";
  t.top_flag = false;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(t, blob));

  top_level t2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(t2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(t2.mid.inner.leaf_val, -99);
  ASSERT_EQ(t2.mid.mid_str, "deep_binary");
  ASSERT_FALSE(t2.top_flag);
}

TEST(portable_storage, large_array_1000_elements)
{
  list_struct l;
  for (int i = 0; i < 1000; ++i)
    l.ints.push_back(i);
  l.strings = {"only_one"};

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(l, blob));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(l2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(l2.ints.size(), 1000u);
  ASSERT_EQ(l2.ints[0], 0);
  ASSERT_EQ(l2.ints[999], 999);
}

TEST(portable_storage, large_array_json)
{
  list_struct l;
  for (int i = 0; i < 1000; ++i)
    l.ints.push_back(i * 2);
  l.strings = {};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(l, json));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(l2, json));
  ASSERT_EQ(l2.ints.size(), 1000u);
  ASSERT_EQ(l2.ints[500], 1000);
}

TEST(portable_storage, mixed_nested_and_array)
{
  nested_list_struct nl;
  nl.nums = {10, 20, 30};
  nl.sub.inner.val_int = 7;
  nl.sub.inner.val_str = "mixed";
  nl.sub.inner.val_uint64 = 999;
  nl.sub.flag = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(nl, json));

  nested_list_struct nl2;
  ASSERT_TRUE(epee::serialization::load_t_from_json(nl2, json));
  ASSERT_EQ(nl2.nums.size(), 3u);
  ASSERT_EQ(nl2.nums[2], 30);
  ASSERT_EQ(nl2.sub.inner.val_int, 7);
  ASSERT_TRUE(nl2.sub.flag);
}

TEST(portable_storage, mixed_nested_and_array_binary)
{
  nested_list_struct nl;
  nl.nums = {-1, 0, 1};
  nl.sub.inner.val_int = -5;
  nl.sub.inner.val_str = "bin_mixed";
  nl.sub.inner.val_uint64 = 12345;
  nl.sub.flag = false;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(nl, blob));

  nested_list_struct nl2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(nl2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_EQ(nl2.nums.size(), 3u);
  ASSERT_EQ(nl2.sub.inner.val_str, "bin_mixed");
}

TEST(portable_storage, binary_magic_bytes)
{
  simple_struct s;
  s.val_int = 1;
  s.val_str = "magic";
  s.val_uint64 = 2;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  ASSERT_GE(blob.size(), 9u); // at least header size

  // Verify magic signature bytes (little endian)
  const uint8_t* data = blob.data();
  uint32_t sig_a = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  uint32_t sig_b = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
  ASSERT_EQ(sig_a, 0x01011101u);
  ASSERT_EQ(sig_b, 0x01020101u);
}

// delete_entry is declared in the header but not implemented in source,
// so we cannot test it. Skipping delete_entry tests.

TEST(portable_storage, raw_api_open_section_nonexistent_no_create)
{
  epee::serialization::portable_storage ps;
  auto section = ps.open_section("nonexistent", nullptr, false);
  ASSERT_EQ(section, nullptr);
}

TEST(portable_storage, raw_api_open_section_create_if_not_exist)
{
  epee::serialization::portable_storage ps;
  auto section = ps.open_section("new_section", nullptr, true);
  ASSERT_NE(section, nullptr);

  // Verify we can get it again without create flag
  auto section2 = ps.open_section("new_section", nullptr, false);
  ASSERT_NE(section2, nullptr);
}

TEST(portable_storage, raw_api_multiple_sections)
{
  epee::serialization::portable_storage ps;
  auto s1 = ps.open_section("section1", nullptr, true);
  auto s2 = ps.open_section("section2", nullptr, true);
  ASSERT_NE(s1, nullptr);
  ASSERT_NE(s2, nullptr);
  ASSERT_NE(s1, s2);

  ps.set_value("val", int64_t(10), s1);
  ps.set_value("val", int64_t(20), s2);

  int64_t v1 = 0, v2 = 0;
  ASSERT_TRUE(ps.get_value("val", v1, s1));
  ASSERT_TRUE(ps.get_value("val", v2, s2));
  ASSERT_EQ(v1, 10);
  ASSERT_EQ(v2, 20);
}

TEST(portable_storage, raw_api_overwrite_value)
{
  epee::serialization::portable_storage ps;
  ps.set_value("key", int64_t(1), nullptr);

  int64_t val = 0;
  ASSERT_TRUE(ps.get_value("key", val, nullptr));
  ASSERT_EQ(val, 1);

  ps.set_value("key", int64_t(2), nullptr);
  ASSERT_TRUE(ps.get_value("key", val, nullptr));
  ASSERT_EQ(val, 2);
}

TEST(portable_storage, json_extra_unknown_fields)
{
  // JSON with extra fields not in the struct should be tolerated
  std::string json = "{\"val_int\": 5, \"val_str\": \"test\", \"val_uint64\": 10, \"unknown_field\": 999}";
  simple_struct s;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s, json));
  ASSERT_EQ(s.val_int, 5);
  ASSERT_EQ(s.val_str, "test");
  ASSERT_EQ(s.val_uint64, 10u);
}

TEST(portable_storage, empty_json_object)
{
  optional_struct o;
  o.required_val = 0;
  o.optional_val = 0;
  // An empty JSON object - optional fields get defaults, required fields get zero
  std::string json = "{}";
  ASSERT_TRUE(epee::serialization::load_t_from_json(o, json));
  ASSERT_EQ(o.optional_val, 42); // default
}

TEST(portable_storage, binary_truncated_data)
{
  simple_struct s;
  s.val_int = 1;
  s.val_str = "data";
  s.val_uint64 = 2;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));

  // Truncate the binary data
  std::string truncated(reinterpret_cast<const char*>(blob.data()), blob.size() / 2);
  simple_struct s2;
  ASSERT_FALSE(epee::serialization::load_t_from_binary(s2, truncated));
}

TEST(portable_storage, binary_just_header)
{
  // Only the 9-byte header, no actual data
  uint8_t header[9] = {0x01, 0x11, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01};
  simple_struct s;
  ASSERT_FALSE(epee::serialization::load_t_from_binary(s,
    epee::span<const uint8_t>(header, sizeof(header))));
}

TEST(portable_storage, double_value_roundtrip)
{
  double_struct s;
  s.val = 3.14159265358979;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));

  double_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_DOUBLE_EQ(s2.val, 3.14159265358979);
}

TEST(portable_storage, double_negative_roundtrip)
{
  double_struct s;
  s.val = -1.23e10;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));

  double_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_DOUBLE_EQ(s2.val, -1.23e10);
}

TEST(portable_storage, double_zero_roundtrip)
{
  double_struct s;
  s.val = 0.0;

  epee::byte_slice blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));

  double_struct s2;
  s2.val = 999.0;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, epee::span<const uint8_t>(blob.data(), blob.size())));
  ASSERT_DOUBLE_EQ(s2.val, 0.0);
}

TEST(portable_storage, raw_api_set_get_double)
{
  epee::serialization::portable_storage ps;
  ps.set_value("pi", double(3.14), nullptr);

  double val = 0;
  ASSERT_TRUE(ps.get_value("pi", val, nullptr));
  ASSERT_DOUBLE_EQ(val, 3.14);
}

TEST(portable_storage, raw_api_set_get_bool)
{
  epee::serialization::portable_storage ps;
  ps.set_value("flag", bool(true), nullptr);

  bool val = false;
  ASSERT_TRUE(ps.get_value("flag", val, nullptr));
  ASSERT_TRUE(val);
}

TEST(portable_storage, raw_api_set_get_uint64)
{
  epee::serialization::portable_storage ps;
  ps.set_value("big", uint64_t(UINT64_MAX), nullptr);

  uint64_t val = 0;
  ASSERT_TRUE(ps.get_value("big", val, nullptr));
  ASSERT_EQ(val, UINT64_MAX);
}
