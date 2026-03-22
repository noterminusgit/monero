// Copyright (c) 2020-2024, The Monero Project

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

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "byte_slice.h"
#include "serialization/keyvalue_serialization.h"
#include "storages/portable_storage.h"
#include "storages/portable_storage_template_helper.h"
#include "span.h"

TEST(epee_binary, two_keys)
{
  static constexpr const std::uint8_t data[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x08, 0x01, 'a',
    0x0B, 0x00, 0x01, 'b', 0x0B, 0x00
  };

  epee::serialization::portable_storage storage{};
  EXPECT_TRUE(storage.load_from_binary(data));
}

TEST(epee_binary, duplicate_key)
{
  static constexpr const std::uint8_t data[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x08, 0x01, 'a',
    0x0B, 0x00, 0x01, 'a', 0x0B, 0x00
  };

  epee::serialization::portable_storage storage{};
  EXPECT_FALSE(storage.load_from_binary(data));
}

namespace
{
struct ObjOfObjs
{
  std::vector<ObjOfObjs> x;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(x)
  END_KV_SERIALIZE_MAP()
};

struct ObjOfInts
{
  std::list<int> x;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(x)
  END_KV_SERIALIZE_MAP()
};

template<typename t_param>
struct ParentObjWithOptChild
{
  t_param     params;

  ParentObjWithOptChild(): params{} {}

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(params)
  END_KV_SERIALIZE_MAP()
};

struct ObjWithOptChild
{
  bool test_value;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE_OPT(test_value, true);
  END_KV_SERIALIZE_MAP()
};
}

TEST(epee_binary, serialize_deserialize)
{
  ParentObjWithOptChild<ObjWithOptChild> o;
  std::string o_json;
  o.params.test_value = true;

  EXPECT_TRUE(epee::serialization::store_t_to_json(o, o_json));
  EXPECT_TRUE(o.params.test_value);

  EXPECT_TRUE(epee::serialization::load_t_from_json(o, o_json));
  EXPECT_TRUE(o.params.test_value);

  ParentObjWithOptChild<ObjWithOptChild> o2;
  std::string o2_json;
  o.params.test_value = false;

  EXPECT_TRUE(epee::serialization::store_t_to_json(o2, o2_json));
  EXPECT_FALSE(o2.params.test_value);

  EXPECT_TRUE(epee::serialization::load_t_from_json(o2, o2_json));
  EXPECT_FALSE(o2.params.test_value);

  // compiler sets default value of test_value to false
  ParentObjWithOptChild<ObjWithOptChild> o3;
  std::string o3_json;

  EXPECT_TRUE(epee::serialization::store_t_to_json(o3, o3_json));
  EXPECT_FALSE(o3.params.test_value);

  EXPECT_TRUE(epee::serialization::load_t_from_json(o3, o3_json));
  EXPECT_FALSE(o3.params.test_value);

  // test optional field default initialization.
  ParentObjWithOptChild<ObjWithOptChild> o4;
  std::string o4_json = "{\"params\": {}}";

  EXPECT_TRUE(epee::serialization::load_t_from_json(o4, o4_json));
  EXPECT_TRUE(o4.params.test_value);
}

// ============================================================
// Additional epee serialization roundtrip tests
// ============================================================

namespace
{
struct MultiFieldStruct
{
  int32_t int_val;
  uint64_t uint_val;
  std::string str_val;
  bool bool_val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(int_val)
    KV_SERIALIZE(uint_val)
    KV_SERIALIZE(str_val)
    KV_SERIALIZE(bool_val)
  END_KV_SERIALIZE_MAP()
};

struct NestedStruct
{
  std::string name;
  MultiFieldStruct inner;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(name)
    KV_SERIALIZE(inner)
  END_KV_SERIALIZE_MAP()
};

struct VectorStruct
{
  std::vector<uint64_t> numbers;
  std::vector<std::string> strings;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(numbers)
    KV_SERIALIZE(strings)
  END_KV_SERIALIZE_MAP()
};

struct OptionalFieldStruct
{
  int32_t required_val;
  uint64_t opt_val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(required_val)
    KV_SERIALIZE_OPT(opt_val, (uint64_t)999)
  END_KV_SERIALIZE_MAP()
};
}

TEST(EpeeSerialization, json_roundtrip_multi_field)
{
  MultiFieldStruct orig;
  orig.int_val = -42;
  orig.uint_val = 123456789;
  orig.str_val = "test string";
  orig.bool_val = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));
  ASSERT_FALSE(json.empty());

  MultiFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.int_val, -42);
  EXPECT_EQ(restored.uint_val, 123456789u);
  EXPECT_EQ(restored.str_val, "test string");
  EXPECT_TRUE(restored.bool_val);
}

TEST(EpeeSerialization, json_roundtrip_nested_struct)
{
  NestedStruct orig;
  orig.name = "outer";
  orig.inner.int_val = 100;
  orig.inner.uint_val = 200;
  orig.inner.str_val = "inner_string";
  orig.inner.bool_val = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  NestedStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.name, "outer");
  EXPECT_EQ(restored.inner.int_val, 100);
  EXPECT_EQ(restored.inner.uint_val, 200u);
  EXPECT_EQ(restored.inner.str_val, "inner_string");
  EXPECT_FALSE(restored.inner.bool_val);
}

TEST(EpeeSerialization, json_roundtrip_vectors)
{
  VectorStruct orig;
  orig.numbers = {1, 2, 3, 100, 999999};
  orig.strings = {"hello", "world", ""};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  VectorStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  ASSERT_EQ(restored.numbers.size(), 5u);
  EXPECT_EQ(restored.numbers[0], 1u);
  EXPECT_EQ(restored.numbers[4], 999999u);
  ASSERT_EQ(restored.strings.size(), 3u);
  EXPECT_EQ(restored.strings[0], "hello");
  EXPECT_EQ(restored.strings[1], "world");
  EXPECT_EQ(restored.strings[2], "");
}

TEST(EpeeSerialization, json_roundtrip_empty_vectors)
{
  VectorStruct orig;
  // Leave vectors empty

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  VectorStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_TRUE(restored.numbers.empty());
  EXPECT_TRUE(restored.strings.empty());
}

TEST(EpeeSerialization, json_roundtrip_optional_field_present)
{
  OptionalFieldStruct orig;
  orig.required_val = 42;
  orig.opt_val = 777;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  OptionalFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.required_val, 42);
  EXPECT_EQ(restored.opt_val, 777u);
}

TEST(EpeeSerialization, json_roundtrip_optional_field_default)
{
  // When the optional field is missing from JSON, the default value should be used
  OptionalFieldStruct restored;
  std::string json = "{\"required_val\": 10}";
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.required_val, 10);
  EXPECT_EQ(restored.opt_val, 999u); // default value
}

TEST(EpeeSerialization, json_roundtrip_special_chars_in_string)
{
  MultiFieldStruct orig;
  orig.int_val = 0;
  orig.uint_val = 0;
  orig.str_val = "line1\nline2\ttab\"quote";
  orig.bool_val = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  MultiFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.str_val, "line1\nline2\ttab\"quote");
}

TEST(EpeeSerialization, json_roundtrip_large_uint64)
{
  MultiFieldStruct orig;
  orig.int_val = 0;
  orig.uint_val = 0xFFFFFFFFFFFFFFFF;
  orig.str_val = "";
  orig.bool_val = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  MultiFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.uint_val, 0xFFFFFFFFFFFFFFFF);
}

TEST(EpeeSerialization, json_roundtrip_negative_int)
{
  MultiFieldStruct orig;
  orig.int_val = -2147483648; // INT32_MIN
  orig.uint_val = 0;
  orig.str_val = "";
  orig.bool_val = false;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  MultiFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.int_val, -2147483648);
}

TEST(EpeeSerialization, binary_roundtrip_multi_field)
{
  MultiFieldStruct orig;
  orig.int_val = 42;
  orig.uint_val = 9999;
  orig.str_val = "binary test";
  orig.bool_val = true;

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));
  ASSERT_GT(binary.size(), 0u);

  MultiFieldStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  EXPECT_EQ(restored.int_val, 42);
  EXPECT_EQ(restored.uint_val, 9999u);
  EXPECT_EQ(restored.str_val, "binary test");
  EXPECT_TRUE(restored.bool_val);
}

TEST(EpeeSerialization, binary_roundtrip_nested)
{
  NestedStruct orig;
  orig.name = "container";
  orig.inner.int_val = -100;
  orig.inner.uint_val = 5000;
  orig.inner.str_val = "nested";
  orig.inner.bool_val = true;

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  NestedStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  EXPECT_EQ(restored.name, "container");
  EXPECT_EQ(restored.inner.int_val, -100);
  EXPECT_EQ(restored.inner.uint_val, 5000u);
  EXPECT_EQ(restored.inner.str_val, "nested");
  EXPECT_TRUE(restored.inner.bool_val);
}

TEST(EpeeSerialization, binary_roundtrip_vectors)
{
  VectorStruct orig;
  orig.numbers = {10, 20, 30, 40};
  orig.strings = {"alpha", "beta"};

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  VectorStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  ASSERT_EQ(restored.numbers.size(), 4u);
  EXPECT_EQ(restored.numbers[0], 10u);
  EXPECT_EQ(restored.numbers[3], 40u);
  ASSERT_EQ(restored.strings.size(), 2u);
  EXPECT_EQ(restored.strings[0], "alpha");
  EXPECT_EQ(restored.strings[1], "beta");
}

TEST(EpeeSerialization, portable_storage_json_to_binary_roundtrip)
{
  // Store as JSON, load into portable_storage, store as binary, load back
  MultiFieldStruct orig;
  orig.int_val = 777;
  orig.uint_val = 888;
  orig.str_val = "cross-format";
  orig.bool_val = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  // Load from JSON into a new struct, then store to binary
  MultiFieldStruct from_json;
  ASSERT_TRUE(epee::serialization::load_t_from_json(from_json, json));

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(from_json, binary));

  // Load from binary
  MultiFieldStruct from_binary;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(from_binary, epee::span<const uint8_t>(binary.data(), binary.size())));

  EXPECT_EQ(from_binary.int_val, 777);
  EXPECT_EQ(from_binary.uint_val, 888u);
  EXPECT_EQ(from_binary.str_val, "cross-format");
  EXPECT_TRUE(from_binary.bool_val);
}

TEST(EpeeSerialization, portable_storage_direct_api)
{
  epee::serialization::portable_storage ps;
  ps.set_value("my_int", int64_t(42), nullptr);
  ps.set_value("my_string", std::string("hello"), nullptr);
  ps.set_value("my_bool", true, nullptr);

  // Store to binary and load back
  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));
  ASSERT_GT(blob.size(), 0u);

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  int64_t iv = 0;
  ASSERT_TRUE(ps2.get_value("my_int", iv, nullptr));
  EXPECT_EQ(iv, 42);

  std::string sv;
  ASSERT_TRUE(ps2.get_value("my_string", sv, nullptr));
  EXPECT_EQ(sv, "hello");

  bool bv = false;
  ASSERT_TRUE(ps2.get_value("my_bool", bv, nullptr));
  EXPECT_TRUE(bv);
}

TEST(EpeeSerialization, portable_storage_missing_key)
{
  epee::serialization::portable_storage ps;
  ps.set_value("existing", int64_t(1), nullptr);

  int64_t val = 0;
  EXPECT_FALSE(ps.get_value("nonexistent", val, nullptr));
}

TEST(EpeeSerialization, json_invalid_json_fails)
{
  MultiFieldStruct s;
  EXPECT_FALSE(epee::serialization::load_t_from_json(s, "not valid json at all {{{"));
}

TEST(EpeeSerialization, json_empty_object)
{
  OptionalFieldStruct s;
  ASSERT_TRUE(epee::serialization::load_t_from_json(s, "{}"));
  // Optional field should get its default
  EXPECT_EQ(s.opt_val, 999u);
}

TEST(epee_binary, any_empty_seq)
{
  // Test that any c++ sequence type (std::vector, std::list, etc) can deserialize without error
  // from an *empty* epee binary array of any type. This property is useful for other projects who
  // maintain code which serializes epee binary but don't know the type of arrays until runtime.
  // Without any elements to actually serialize, they don't know what type code to use for arrays
  // and should be able to choose a default typecode.

  static constexpr const std::uint8_t data_empty_bool[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x04, 0x01, 'x', 0x8B /*array of bools*/, 0x00 /*length 0*/
  };

  static constexpr const std::uint8_t data_empty_double[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x04, 0x01, 'x', 0x89 /*array of doubles*/, 0x00 /*length 0*/
  };

  static constexpr const std::uint8_t data_empty_string[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x04, 0x01, 'x', 0x8A /*array of strings*/, 0x00 /*length 0*/
  };

  static constexpr const std::uint8_t data_empty_int64[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x04, 0x01, 'x', 0x81 /*array of int64s*/, 0x00 /*length 0*/
  };

  static constexpr const std::uint8_t data_empty_object[] = {
    0x01, 0x11, 0x01, 0x1, 0x01, 0x01, 0x02, 0x1, 0x1, 0x04, 0x01, 'x', 0x8C /*array of objects*/, 0x00 /*length 0*/
  };

  ObjOfObjs o;

  EXPECT_TRUE(epee::serialization::load_t_from_binary(o, epee::span<const std::uint8_t>(data_empty_bool)));
  EXPECT_EQ(0, o.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(o, epee::span<const std::uint8_t>(data_empty_double)));
  EXPECT_EQ(0, o.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(o, epee::span<const std::uint8_t>(data_empty_string)));
  EXPECT_EQ(0, o.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(o, epee::span<const std::uint8_t>(data_empty_int64)));
  EXPECT_EQ(0, o.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(o, epee::span<const std::uint8_t>(data_empty_object)));
  EXPECT_EQ(0, o.x.size());

  ObjOfInts i;

  EXPECT_TRUE(epee::serialization::load_t_from_binary(i, epee::span<const std::uint8_t>(data_empty_bool)));
  EXPECT_EQ(0, i.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(i, epee::span<const std::uint8_t>(data_empty_double)));
  EXPECT_EQ(0, i.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(i, epee::span<const std::uint8_t>(data_empty_string)));
  EXPECT_EQ(0, i.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(i, epee::span<const std::uint8_t>(data_empty_int64)));
  EXPECT_EQ(0, i.x.size());

  EXPECT_TRUE(epee::serialization::load_t_from_binary(i, epee::span<const std::uint8_t>(data_empty_object)));
  EXPECT_EQ(0, i.x.size());
}
