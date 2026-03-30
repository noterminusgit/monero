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

// ============================================================
// Extended epee serialization coverage tests
// ============================================================

namespace
{
struct Uint64Struct
{
  uint64_t val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(val)
  END_KV_SERIALIZE_MAP()
};

struct StringStruct
{
  std::string val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(val)
  END_KV_SERIALIZE_MAP()
};

struct BoolStruct
{
  bool val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(val)
  END_KV_SERIALIZE_MAP()
};

struct DeepNested
{
  std::string name;
  NestedStruct child;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(name)
    KV_SERIALIZE(child)
  END_KV_SERIALIZE_MAP()
};

struct ArrayOfObjects
{
  std::vector<MultiFieldStruct> items;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(items)
  END_KV_SERIALIZE_MAP()
};

struct MixedArrays
{
  std::vector<uint64_t> nums;
  std::vector<std::string> strs;
  std::vector<bool> flags;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(nums)
    KV_SERIALIZE(strs)
    KV_SERIALIZE(flags)
  END_KV_SERIALIZE_MAP()
};

struct BinaryBlobStruct
{
  std::string blob_data;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(blob_data)
  END_KV_SERIALIZE_MAP()
};

struct MultiOptional
{
  uint64_t a;
  uint64_t b;
  std::string c;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE_OPT(a, (uint64_t)10)
    KV_SERIALIZE_OPT(b, (uint64_t)20)
    KV_SERIALIZE_OPT(c, std::string("default"))
  END_KV_SERIALIZE_MAP()
};

struct Int8Struct
{
  int8_t val;

  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(val)
  END_KV_SERIALIZE_MAP()
};
}

// --- Basic types ---

TEST(EpeeSerialization, portable_storage_uint64_roundtrip)
{
  epee::serialization::portable_storage ps;
  ps.set_value("x", uint64_t(0xDEADBEEFCAFEBABE), nullptr);

  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  uint64_t v = 0;
  ASSERT_TRUE(ps2.get_value("x", v, nullptr));
  EXPECT_EQ(v, 0xDEADBEEFCAFEBABE);
}

TEST(EpeeSerialization, portable_storage_string_roundtrip)
{
  epee::serialization::portable_storage ps;
  ps.set_value("msg", std::string("hello portable_storage"), nullptr);

  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  std::string v;
  ASSERT_TRUE(ps2.get_value("msg", v, nullptr));
  EXPECT_EQ(v, "hello portable_storage");
}

TEST(EpeeSerialization, portable_storage_bool_roundtrip)
{
  epee::serialization::portable_storage ps;
  ps.set_value("flag_t", true, nullptr);
  ps.set_value("flag_f", false, nullptr);

  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  bool vt = false, vf = true;
  ASSERT_TRUE(ps2.get_value("flag_t", vt, nullptr));
  ASSERT_TRUE(ps2.get_value("flag_f", vf, nullptr));
  EXPECT_TRUE(vt);
  EXPECT_FALSE(vf);
}

TEST(EpeeSerialization, json_uint64_struct_roundtrip)
{
  Uint64Struct orig;
  orig.val = 12345678901234;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  Uint64Struct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.val, 12345678901234u);
}

TEST(EpeeSerialization, json_string_struct_roundtrip)
{
  StringStruct orig;
  orig.val = "test string with spaces";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  StringStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.val, "test string with spaces");
}

TEST(EpeeSerialization, json_bool_struct_roundtrip)
{
  for (bool bval : {true, false}) {
    BoolStruct orig;
    orig.val = bval;

    std::string json;
    ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

    BoolStruct restored;
    ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
    EXPECT_EQ(restored.val, bval);
  }
}

// --- Nested objects ---

TEST(EpeeSerialization, json_deep_nested_roundtrip)
{
  DeepNested orig;
  orig.name = "root";
  orig.child.name = "middle";
  orig.child.inner.int_val = -999;
  orig.child.inner.uint_val = 888;
  orig.child.inner.str_val = "leaf";
  orig.child.inner.bool_val = true;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  DeepNested restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.name, "root");
  EXPECT_EQ(restored.child.name, "middle");
  EXPECT_EQ(restored.child.inner.int_val, -999);
  EXPECT_EQ(restored.child.inner.uint_val, 888u);
  EXPECT_EQ(restored.child.inner.str_val, "leaf");
  EXPECT_TRUE(restored.child.inner.bool_val);
}

TEST(EpeeSerialization, binary_deep_nested_roundtrip)
{
  DeepNested orig;
  orig.name = "binary_root";
  orig.child.name = "binary_mid";
  orig.child.inner.int_val = 42;
  orig.child.inner.uint_val = 0;
  orig.child.inner.str_val = "";
  orig.child.inner.bool_val = false;

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  DeepNested restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  EXPECT_EQ(restored.name, "binary_root");
  EXPECT_EQ(restored.child.name, "binary_mid");
  EXPECT_EQ(restored.child.inner.int_val, 42);
}

// --- Array serialization ---

TEST(EpeeSerialization, json_array_of_objects_roundtrip)
{
  ArrayOfObjects orig;
  for (int i = 0; i < 5; ++i) {
    MultiFieldStruct item;
    item.int_val = i * 10;
    item.uint_val = i * 100;
    item.str_val = "item_" + std::to_string(i);
    item.bool_val = (i % 2 == 0);
    orig.items.push_back(item);
  }

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  ArrayOfObjects restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  ASSERT_EQ(restored.items.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(restored.items[i].int_val, i * 10);
    EXPECT_EQ(restored.items[i].uint_val, (uint64_t)(i * 100));
    EXPECT_EQ(restored.items[i].str_val, "item_" + std::to_string(i));
    EXPECT_EQ(restored.items[i].bool_val, (i % 2 == 0));
  }
}

TEST(EpeeSerialization, json_empty_array_of_objects)
{
  ArrayOfObjects orig;
  // items is empty

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  ArrayOfObjects restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_TRUE(restored.items.empty());
}

TEST(EpeeSerialization, json_mixed_arrays_roundtrip)
{
  MixedArrays orig;
  orig.nums = {0, 1, 999, 0xFFFFFFFFFFFFFFFF};
  orig.strs = {"alpha", "beta", "", "gamma"};
  orig.flags = {true, false, true, true, false};

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  MixedArrays restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  ASSERT_EQ(restored.nums.size(), 4u);
  EXPECT_EQ(restored.nums[0], 0u);
  EXPECT_EQ(restored.nums[3], 0xFFFFFFFFFFFFFFFF);
  ASSERT_EQ(restored.strs.size(), 4u);
  EXPECT_EQ(restored.strs[2], "");
  ASSERT_EQ(restored.flags.size(), 5u);
  EXPECT_TRUE(restored.flags[0]);
  EXPECT_FALSE(restored.flags[1]);
}

TEST(EpeeSerialization, binary_mixed_arrays_roundtrip)
{
  MixedArrays orig;
  orig.nums = {10, 20, 30};
  orig.strs = {"x", "y"};
  orig.flags = {false, true};

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  MixedArrays restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  ASSERT_EQ(restored.nums.size(), 3u);
  EXPECT_EQ(restored.nums[0], 10u);
  ASSERT_EQ(restored.strs.size(), 2u);
  EXPECT_EQ(restored.strs[0], "x");
  ASSERT_EQ(restored.flags.size(), 2u);
  EXPECT_FALSE(restored.flags[0]);
  EXPECT_TRUE(restored.flags[1]);
}

// --- Binary blob ---

TEST(EpeeSerialization, binary_blob_data_roundtrip)
{
  BinaryBlobStruct orig;
  // Create binary-looking data
  orig.blob_data.resize(256);
  for (int i = 0; i < 256; ++i)
    orig.blob_data[i] = static_cast<char>(i);

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  BinaryBlobStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  ASSERT_EQ(restored.blob_data.size(), 256u);
  for (int i = 0; i < 256; ++i)
    EXPECT_EQ(static_cast<uint8_t>(restored.blob_data[i]), static_cast<uint8_t>(i));
}

TEST(EpeeSerialization, empty_binary_blob_roundtrip)
{
  BinaryBlobStruct orig;
  orig.blob_data = "";

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  BinaryBlobStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  EXPECT_TRUE(restored.blob_data.empty());
}

// --- Optional fields / defaults ---

TEST(EpeeSerialization, json_multiple_optional_defaults)
{
  MultiOptional restored;
  std::string json = "{}";
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.a, 10u);
  EXPECT_EQ(restored.b, 20u);
  EXPECT_EQ(restored.c, "default");
}

TEST(EpeeSerialization, json_multiple_optional_partial)
{
  MultiOptional restored;
  std::string json = "{\"a\": 77}";
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.a, 77u);
  EXPECT_EQ(restored.b, 20u);  // default
  EXPECT_EQ(restored.c, "default");  // default
}

TEST(EpeeSerialization, json_multiple_optional_all_set)
{
  MultiOptional orig;
  orig.a = 1;
  orig.b = 2;
  orig.c = "custom";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  MultiOptional restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.a, 1u);
  EXPECT_EQ(restored.b, 2u);
  EXPECT_EQ(restored.c, "custom");
}

// --- Error handling for malformed data ---

TEST(EpeeSerialization, binary_empty_data_rejected)
{
  MultiFieldStruct s;
  epee::span<const uint8_t> empty_span{};
  EXPECT_FALSE(epee::serialization::load_t_from_binary(s, empty_span));
}

TEST(EpeeSerialization, binary_truncated_header_rejected)
{
  const uint8_t truncated[] = {0x01, 0x11};
  MultiFieldStruct s;
  EXPECT_FALSE(epee::serialization::load_t_from_binary(s, epee::span<const uint8_t>(truncated, sizeof(truncated))));
}

TEST(EpeeSerialization, json_empty_string_accepted)
{
  // Empty string is treated as empty object by the JSON parser
  MultiFieldStruct s;
  EXPECT_TRUE(epee::serialization::load_t_from_json(s, ""));
}

TEST(EpeeSerialization, json_null_literal_accepted_as_empty)
{
  // "null" is valid JSON but not an object
  MultiFieldStruct s;
  EXPECT_FALSE(epee::serialization::load_t_from_json(s, "null"));
}

TEST(EpeeSerialization, json_array_instead_of_object_rejected)
{
  MultiFieldStruct s;
  EXPECT_FALSE(epee::serialization::load_t_from_json(s, "[1,2,3]"));
}

TEST(EpeeSerialization, json_deeply_nested_braces)
{
  // Malformed deeply nested braces
  MultiFieldStruct s;
  EXPECT_FALSE(epee::serialization::load_t_from_json(s, "{{{{{{"));
}

// --- Cross-format roundtrips ---

TEST(EpeeSerialization, json_to_binary_to_json_nested)
{
  NestedStruct orig;
  orig.name = "cross";
  orig.inner.int_val = -50;
  orig.inner.uint_val = 50;
  orig.inner.str_val = "format";
  orig.inner.bool_val = true;

  // JSON -> struct
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  NestedStruct from_json;
  ASSERT_TRUE(epee::serialization::load_t_from_json(from_json, json));

  // struct -> binary
  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(from_json, binary));

  // binary -> struct
  NestedStruct from_binary;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(from_binary, epee::span<const uint8_t>(binary.data(), binary.size())));

  // struct -> JSON again
  std::string json2;
  ASSERT_TRUE(epee::serialization::store_t_to_json(from_binary, json2));

  NestedStruct final_restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(final_restored, json2));

  EXPECT_EQ(final_restored.name, "cross");
  EXPECT_EQ(final_restored.inner.int_val, -50);
  EXPECT_EQ(final_restored.inner.uint_val, 50u);
  EXPECT_EQ(final_restored.inner.str_val, "format");
  EXPECT_TRUE(final_restored.inner.bool_val);
}

// --- Portable storage section API ---

TEST(EpeeSerialization, portable_storage_sections)
{
  epee::serialization::portable_storage ps;

  // Create a sub-section
  auto* section = ps.open_section("sub", nullptr, true);
  ASSERT_NE(section, nullptr);
  ps.set_value("inner_int", int64_t(99), section);
  ps.set_value("inner_str", std::string("nested_val"), section);

  // Store and reload
  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  auto* section2 = ps2.open_section("sub", nullptr, false);
  ASSERT_NE(section2, nullptr);

  int64_t iv = 0;
  ASSERT_TRUE(ps2.get_value("inner_int", iv, section2));
  EXPECT_EQ(iv, 99);

  std::string sv;
  ASSERT_TRUE(ps2.get_value("inner_str", sv, section2));
  EXPECT_EQ(sv, "nested_val");
}

TEST(EpeeSerialization, portable_storage_multiple_types)
{
  epee::serialization::portable_storage ps;
  ps.set_value("u64", uint64_t(42), nullptr);
  ps.set_value("i64", int64_t(-42), nullptr);
  ps.set_value("str", std::string("test"), nullptr);
  ps.set_value("bl", true, nullptr);

  epee::byte_slice blob;
  ASSERT_TRUE(ps.store_to_binary(blob));

  epee::serialization::portable_storage ps2;
  ASSERT_TRUE(ps2.load_from_binary(epee::to_span(blob)));

  uint64_t u = 0;
  int64_t i = 0;
  std::string s;
  bool b = false;

  ASSERT_TRUE(ps2.get_value("u64", u, nullptr));
  EXPECT_EQ(u, 42u);
  ASSERT_TRUE(ps2.get_value("i64", i, nullptr));
  EXPECT_EQ(i, -42);
  ASSERT_TRUE(ps2.get_value("str", s, nullptr));
  EXPECT_EQ(s, "test");
  ASSERT_TRUE(ps2.get_value("bl", b, nullptr));
  EXPECT_TRUE(b);
}

TEST(EpeeSerialization, portable_storage_overwrite_value)
{
  epee::serialization::portable_storage ps;
  ps.set_value("key", int64_t(1), nullptr);
  ps.set_value("key", int64_t(2), nullptr);

  int64_t v = 0;
  ASSERT_TRUE(ps.get_value("key", v, nullptr));
  // Should have the last written value
  EXPECT_EQ(v, 2);
}

TEST(EpeeSerialization, json_uint64_max_struct)
{
  Uint64Struct orig;
  orig.val = 0xFFFFFFFFFFFFFFFF;

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  Uint64Struct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(restored.val, 0xFFFFFFFFFFFFFFFF);
}

TEST(EpeeSerialization, json_empty_string_field)
{
  StringStruct orig;
  orig.val = "";

  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(orig, json));

  StringStruct restored;
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_TRUE(restored.val.empty());
}

TEST(EpeeSerialization, binary_array_of_objects_roundtrip)
{
  ArrayOfObjects orig;
  for (int i = 0; i < 3; ++i) {
    MultiFieldStruct item;
    item.int_val = i;
    item.uint_val = i * 1000;
    item.str_val = std::to_string(i);
    item.bool_val = (i == 1);
    orig.items.push_back(item);
  }

  epee::byte_slice binary;
  ASSERT_TRUE(epee::serialization::store_t_to_binary(orig, binary));

  ArrayOfObjects restored;
  ASSERT_TRUE(epee::serialization::load_t_from_binary(restored, epee::span<const uint8_t>(binary.data(), binary.size())));
  ASSERT_EQ(restored.items.size(), 3u);
  EXPECT_EQ(restored.items[0].int_val, 0);
  EXPECT_EQ(restored.items[1].uint_val, 1000u);
  EXPECT_EQ(restored.items[2].str_val, "2");
  EXPECT_TRUE(restored.items[1].bool_val);
  EXPECT_FALSE(restored.items[0].bool_val);
}
