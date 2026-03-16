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
#include "byte_stream.h"

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

  std::string blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(s, blob));
  ASSERT_FALSE(blob.empty());

  simple_struct s2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(s2, blob));
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

  std::string blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(n, blob));

  nested_struct n2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(n2, blob));
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

  std::string blob;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(l, blob));

  list_struct l2;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(l2, blob));
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
