# Serialization

## Overview

The Monero serialization framework is a macro-based domain-specific language (DSL) that provides portable, format-agnostic serialization of C++ data structures. It uses compile-time type traits and SFINAE-based dispatch to route serialization through one of several archive backends (binary, JSON, debug). The framework handles all core Monero data types -- transactions, blocks, cryptographic primitives, RingCT structures -- and is the foundation for network wire format encoding, on-disk storage, and human-readable JSON output. All binary serialization uses little-endian byte order. The design is originally derived from the CryptoNote project.

## Key Files

| File | Description |
|------|-------------|
| `src/serialization/serialization.h` | Core framework: type traits (`is_blob_type`, `is_blob_forced`), dispatch function `do_serialize`, all DSL macros (`BEGIN_SERIALIZE_OBJECT`, `FIELD`, `VARINT_FIELD`, etc.), and the `serialization::serialize` / `serialization::serialize_noeof` entry points. |
| `src/serialization/binary_archive.h` | Binary archive: `binary_archive<false>` (reader from `epee::span<const uint8_t>`) and `binary_archive<true>` (writer to `std::ostream`). Little-endian, varint-encoded integers. |
| `src/serialization/json_archive.h` | JSON archive: `json_archive<true>` (write-only). Outputs human-readable JSON with optional indentation. Blobs are hex-encoded. |
| `src/serialization/debug_archive.h` | Debug archive: wraps `json_archive` and prepends variant type tags. Used for diagnostic/debug output of tagged values. |
| `src/serialization/binary_utils.h` | Convenience functions `serialization::parse_binary` and `serialization::dump_binary` for string-based binary round-tripping. |
| `src/serialization/containers.h` | STL container type trait (`serialization::is_container`) and dispatch to `do_serialize_container`. Covers `std::vector`, `std::deque`, `std::list`, `std::set`, `std::map`, `std::unordered_set`, `std::unordered_map`, and their multi-variants. |
| `src/serialization/container.h` | Generic container serialization logic: `do_serialize_container` for reading and writing, including `serialize_container_element` with automatic varint optimization for unsigned integer element types. |
| `src/serialization/pair.h` | Serialization of `std::pair<F, S>` as a two-element array, with automatic varint encoding for unsigned integer components. |
| `src/serialization/tuple.h` | Serialization of `std::tuple<Ts...>` with compile-time recursive element handling. Includes `TUPLE_COMPACT_FIELD` macros. |
| `src/serialization/string.h` | Serialization of `std::string`: varint-prefixed length followed by raw bytes. |
| `src/serialization/variant.h` | Serialization of `boost::variant<T...>`: tag-based dispatch using `variant_serialization_traits` and compile-time type list iteration via `variant_reader`. |
| `src/serialization/crypto.h` | Blob serializer declarations for all `crypto::` types and specialized `std::vector<crypto::signature>` serialization. Registers `VARIANT_TAG` entries for `debug_archive`. |
| `src/serialization/difficulty_type.h` | Serialization of `cryptonote::difficulty_type` (128-bit) as two varint-encoded `uint64_t` values (high, low). |
| `src/serialization/json_object.h` | RapidJSON-based serialization layer (`cryptonote::json::toJsonValue` / `fromJsonValue`) for all core Monero types. Separate from the archive-based DSL. |
| `src/serialization/json_object.cpp` | Implementation of all `toJsonValue` / `fromJsonValue` overloads for transactions, blocks, RingCT types, RPC structures, etc. |
| `src/serialization/CMakeLists.txt` | Build configuration. Compiles only `json_object.cpp`; everything else is header-only. Links against `cryptonote_basic`, `cryptonote_core`, `cryptonote_protocol`, and `epee`. |

## Data Structures

### Type Traits

```cpp
// Marks a type for byte-wise (blob) serialization. Default is false.
template <class T>
struct is_blob_type { typedef boost::false_type type; };

// Forces blob serialization even for non-trivially-copyable types. Use with extreme caution.
template <class T>
struct is_blob_forced: std::false_type {};
```

Types marked with `is_blob_type` are serialized by copying their raw bytes. A `static_assert` in `do_serialize` verifies that blob types are trivially copyable unless `is_blob_forced` is also specialized to `true`.

### Container Type Trait

Defined in `containers.h`:

```cpp
namespace serialization {
    template <typename T> struct is_container: std::false_type {};
    // Specialized to std::true_type for:
    //   std::deque, std::map, std::multimap, std::set,
    //   std::unordered_map, std::unordered_multimap, std::unordered_set, std::vector
}
```

Any type matching `is_container<T>::value == true` is automatically dispatched through `do_serialize_container`.

### Variant Serialization Traits

Defined in `variant.h`:

```cpp
template <class Archive, class T>
struct variant_serialization_traits {};  // Must be specialized via VARIANT_TAG macro
```

Each `boost::variant` member type must have a specialization providing a `get_tag()` static method that returns the discriminant value used to identify the variant alternative during deserialization.

### Serializable Value Type Adapter

In `container.h`, the helper `serializable_value_type` converts `std::pair<const K, V>` (from map containers) to `std::pair<K, V>` to allow deserialization into non-const key types.

### Archive Base Types

| Type | Template Parameter | Purpose |
|------|--------------------|---------|
| `binary_archive_base<IsSaving>` | `bool IsSaving` | Shared base for binary archives. Defines no-op `tag()`, `begin_object()`, `end_object()`, `begin_variant()`, `end_variant()`. |
| `binary_archive<false>` | - | Binary reader. Reads from `epee::span<const uint8_t>`. |
| `binary_archive<true>` | - | Binary writer. Writes to `std::ostream`. |
| `json_archive_base<Stream, IsSaving>` | `Stream`, `bool IsSaving` | Shared base for JSON archives. Manages indentation, tag output, object delimiters. |
| `json_archive<true>` | - | JSON writer. Write-only; outputs to `std::ostream`. |
| `debug_archive<W>` | `bool W` | Inherits from `json_archive<W>`. Wraps values with their variant type tag for diagnostic output. |

## Serialization Macros

### Object / Block Macros

| Macro | Description |
|-------|-------------|
| `BEGIN_SERIALIZE()` | Opens a `member_do_serialize` method. Use for non-object serialization (no `begin_object`/`end_object` calls). Pair with `END_SERIALIZE()`. |
| `BEGIN_SERIALIZE_OBJECT()` | Opens both `member_do_serialize` (which calls `begin_object`/`end_object`) and a `do_serialize_object` method. Pair with `END_SERIALIZE()`. |
| `BEGIN_SERIALIZE_FN(stype)` | Like `BEGIN_SERIALIZE()` but as a free function taking `stype &v`. Inside, use `FIELD_F()` / `VARINT_FIELD_F()`. |
| `BEGIN_SERIALIZE_OBJECT_FN(stype)` | Like `BEGIN_SERIALIZE_OBJECT()` but as a free function. Inside, use `FIELD_F()` / `VARINT_FIELD_F()`. |
| `END_SERIALIZE()` | Closes the serialize function body. Returns `ar.good()`. |

### Field Macros

| Macro | Description |
|-------|-------------|
| `FIELD(f)` | Tags the field with its variable name (stringified via `#f`) and serializes it via `do_serialize(ar, f)`. |
| `FIELD_N(t, f)` | Tags the field with the explicit string `t` and serializes `f`. |
| `FIELD_F(f)` | Free-function variant of `FIELD`. Expands to `FIELD_N(#f, v.f)` where `v` is the free-function parameter. |
| `FIELDS(f)` | Serializes `f` without a tag. Used for untagged serialization. |
| `VARINT_FIELD(f)` | Tags with `#f` and calls `ar.serialize_varint(f)` instead of `do_serialize`. |
| `VARINT_FIELD_N(t, f)` | Tags with explicit string `t` and calls `ar.serialize_varint(f)`. |
| `VARINT_FIELD_F(f)` | Free-function variant: `VARINT_FIELD_N(#f, v.f)`. |

### Special Field Macros

| Macro | Description |
|-------|-------------|
| `MAGIC_FIELD(m)` | Declares a `std::string magic = m`, serializes it as a blob tagged `"magic"`, and verifies the value matches on deserialization. |
| `VERSION_FIELD(v)` | Declares a `uint32_t version = v`, serializes it as a varint tagged `"version"`. |
| `PREPARE_CUSTOM_VECTOR_SERIALIZATION(size, vec)` | On deserialization, resizes the vector to `size`; on serialization, does nothing. |

### Tuple Macros (from `tuple.h`)

| Macro | Description |
|-------|-------------|
| `TUPLE_COMPACT_FIELDS(v)` | Serializes a tuple without the backwards-compatible size prefix. |
| `TUPLE_COMPACT_FIELD_N(t, v)` | Tags with `t` and serializes a compact tuple. |
| `TUPLE_COMPACT_FIELD(f)` | Tags with `#f` and serializes a compact tuple. |
| `TUPLE_COMPACT_FIELD_F(f)` | Free-function variant of `TUPLE_COMPACT_FIELD`. |

### Type Registration Macros

| Macro | Description |
|-------|-------------|
| `BLOB_SERIALIZER(T)` | Specializes `is_blob_type<T>` to `boost::true_type`. The type must be trivially copyable. |
| `BLOB_SERIALIZER_FORCED(T)` | Like `BLOB_SERIALIZER` but also specializes `is_blob_forced<T>` to `std::true_type`, bypassing the trivially-copyable check. |
| `VARIANT_TAG(Archive, Type, Tag)` | Specializes `variant_serialization_traits<Archive<W>, Type>` with a `get_tag()` returning `Tag`. Used to associate discriminant values with variant alternatives for a specific archive type. |

## Archive Types

### binary_archive (Reader: `binary_archive<false>`)

Reads from an `epee::span<const uint8_t>` byte buffer. All integers are stored in little-endian format.

**Construction:**
```cpp
binary_archive<false> ar{epee::strspan<std::uint8_t>(blob)};
```

**Key methods:**
- `serialize_int(T &v)` -- Reads `sizeof(T)` bytes, converts from little-endian to native.
- `serialize_uint(T &v)` -- Same as `serialize_int` but explicitly for unsigned types.
- `serialize_blob(void *buf, size_t len)` -- Reads `len` raw bytes into `buf`.
- `serialize_varint(T &v)` -- Reads a variable-length integer using `tools::read_varint`.
- `begin_array(size_t &s)` -- Reads the array length as a varint.
- `read_variant_tag(variant_tag_type &t)` -- Reads a `uint8_t` tag.
- `remaining_bytes()` -- Returns bytes left in the buffer.
- `getpos()` -- Returns current read position.
- `good()` / `set_fail()` / `eof()` -- Stream state management.
- `enable_varint_bug_backward_compatibility()` -- Enables backward-compatible deserialization mode for older data that serialized certain unsigned integers as non-varint values.
- `tag(const char *)`, `begin_object()`, `end_object()`, `begin_variant()`, `end_variant()` -- All no-ops in binary format.

### binary_archive (Writer: `binary_archive<true>`)

Writes to a `std::ostream`.

**Construction:**
```cpp
std::stringstream ostr;
binary_archive<true> ar(ostr);
```

**Key methods:**
- `serialize_int(T v)` -- Writes `sizeof(T)` bytes in little-endian order.
- `serialize_uint(T v)` -- Writes unsigned integer in little-endian.
- `serialize_blob(void *buf, size_t len)` -- Writes `len` raw bytes.
- `serialize_varint(T &v)` -- Writes a variable-length integer using `tools::write_varint`.
- `begin_array(size_t s)` -- Writes the array size as a varint.
- `write_variant_tag(variant_tag_type t)` -- Writes a `uint8_t` tag.
- `good()` / `set_fail()` -- Stream state management.
- `tag(const char *)`, `begin_object()`, `end_object()` -- All no-ops.

### json_archive (Writer: `json_archive<true>`)

Write-only JSON output archive. There is no `json_archive<false>` (JSON reading is handled separately by `json_object.h`).

**Construction:**
```cpp
json_archive<true> ar(ostream, indent);  // indent=false by default
```

**Key methods:**
- `tag(const char *tag)` -- Writes `"tag": ` with comma-separation between fields.
- `begin_object()` / `end_object()` -- Writes `{` and `}` with depth tracking for indentation.
- `serialize_int(T v)` -- Writes decimal integer. Uses unary `+` to promote `char`/`unsigned char` to `int`.
- `serialize_blob(void *buf, size_t len)` -- Writes hex-encoded bytes wrapped in delimiters (default: double quotes).
- `serialize_varint(T &v)` -- Writes decimal integer (same output as `serialize_int`).
- `begin_array(size_t s)` / `delimit_array()` / `end_array()` -- Writes `[ `, `, `, `]`.
- `begin_string()` / `end_string()` -- Writes string delimiters.
- `write_variant_tag(const char *t)` -- Writes a tag (variant tags are strings in JSON, not byte values).

**Variant tag type:** `const char *` (strings), unlike `binary_archive` which uses `uint8_t`.

### debug_archive

Inherits from `json_archive<W>`. Overrides `do_serialize` to wrap each value in an object with its variant type tag name, producing output like `{"public_key": ...}`. Used for diagnostic purposes.

**Construction:**
```cpp
debug_archive<true> ar(ostream);
```

Provides a `stream()` accessor for direct stream access.

## Public API

### High-Level Serialization Functions

```cpp
namespace serialization {
    // Serialize v into/from ar, then verify stream state (including EOF check for readers)
    template <class Archive, class T>
    bool serialize(Archive &ar, T &v);

    // Same as serialize() but does NOT require EOF at end (allows trailing data)
    template <class Archive, class T>
    bool serialize_noeof(Archive &ar, T &v);

    // Parse a binary string into a value
    template <class T>
    bool parse_binary(const std::string &blob, T &v);

    // Dump a value into a binary string
    template <class T>
    bool dump_binary(T &v, std::string &blob);
}
```

### Core Dispatch Function

```cpp
// Dispatches based on type traits (checked in this priority order):
// 1. is_blob_type<T>::type::value == true  -->  ar.serialize_blob(&v, sizeof(v))
// 2. boost::is_integral<T>::value == true  -->  ar.serialize_int(v)
// 3. T has member_do_serialize(ar) method  -->  v.member_do_serialize(ar)
// 4. T is bool                             -->  ar.serialize_blob(&v, sizeof(v))
// 5. do_serialize_object(ar, v, args...)   -->  ar.begin_object() + do_serialize_object + ar.end_object()
template <class Archive, class T>
bool do_serialize(Archive &ar, T &v);
```

### Usage Pattern: Struct with DSL

```cpp
struct my_struct {
    uint64_t amount;
    crypto::hash hash;
    std::vector<uint8_t> data;

    BEGIN_SERIALIZE_OBJECT()
        VARINT_FIELD(amount)
        FIELD(hash)
        FIELD(data)
    END_SERIALIZE()
};
```

### Usage Pattern: Free Function Serialization

```cpp
BEGIN_SERIALIZE_OBJECT_FN(my_struct)
    VARINT_FIELD_F(amount)
    FIELD_F(hash)
    FIELD_F(data)
END_SERIALIZE()
```

### Usage Pattern: Blob Type Registration

```cpp
BLOB_SERIALIZER(crypto::public_key);         // trivially copyable required
BLOB_SERIALIZER_FORCED(crypto::secret_key);   // bypass trivially-copyable check
```

### Usage Pattern: Variant Tag Registration

```cpp
// Binary archive uses uint8_t tags
VARIANT_TAG(binary_archive, cryptonote::txin_gen, 0xff);
VARIANT_TAG(binary_archive, cryptonote::txin_to_key, 0x2);

// JSON archive uses string tags
VARIANT_TAG(json_archive, cryptonote::txin_gen, "gen");
VARIANT_TAG(json_archive, cryptonote::txin_to_key, "key");
```

### Usage Pattern: Binary Round-Trip

```cpp
// Serialize to binary
std::string blob;
serialization::dump_binary(my_obj, blob);

// Deserialize from binary
my_type restored;
serialization::parse_binary(blob, restored);
```

## Internal Logic

### Serialization Dispatch Mechanism

The framework uses SFINAE (`std::enable_if_t`) to select among several overloads of `do_serialize`:

1. **Blob types**: If `is_blob_type<T>::type::value` is true, the value is copied as raw bytes via `ar.serialize_blob(&v, sizeof(v))`. A static assertion checks `std::is_trivially_copyable<T>()` unless `is_blob_forced<T>()` is true.

2. **Integral types**: If `boost::is_integral<T>::value` is true, `ar.serialize_int(v)` is called, which writes/reads the value in little-endian format for binary archives or as a decimal for JSON.

3. **Member serialization**: If `T` has a `member_do_serialize` method (generated by the `BEGIN_SERIALIZE*` macros), that method is invoked.

4. **Boolean special case**: `bool` values are serialized as blobs (1 byte), taking priority over the integral path.

5. **Object serialization with extra args**: If `do_serialize_object(ar, v, args...)` is valid, the value is wrapped in `begin_object()`/`end_object()` calls.

### Container Serialization

Containers (detected via `serialization::is_container`) follow this protocol:

- **Writing**: Write the element count as a varint, then each element with `delimit_array()` between them.
- **Reading**: Read the count varint, clear the container, perform a sanity check (`remaining_bytes >= count`), reserve space, then read each element.

An optimization in `serialize_container_element` automatically uses varint encoding for unsigned integer element types with `sizeof > 1` (i.e., `uint16_t`, `uint32_t`, `uint64_t`), rather than fixed-width encoding.

### Pair Serialization

`std::pair<F, S>` is serialized as a two-element array. Each element is serialized via `serialize_pair_element`, which applies the same varint optimization as containers for unsigned integers.

### Tuple Serialization

`std::tuple<Ts...>` is serialized using compile-time recursive template instantiation (`do_serialize_tuple_nth<I, BackwardsCompat>`). The `BackwardsCompat` flag controls whether 3-tuples and 4-tuples write a size prefix (for compatibility with older serialization formats). `uint64_t` elements are automatically varint-encoded.

### Variant Serialization

`boost::variant<T...>` serialization works as follows:

- **Writing**: Uses `boost::apply_visitor` with `variant_write_visitor` which calls `write_variant_tag(get_tag())` for the active type, then serializes the value.
- **Reading**: Reads the tag, then uses compile-time recursive `variant_reader` to iterate through the variant's type list (`boost::mpl::begin`/`end`) until a matching tag is found, then deserializes into that type.

Tag types differ by archive:
- `binary_archive`: `uint8_t` -- numeric discriminants (e.g., `0xff` for `txin_gen`, `0x2` for `txin_to_key`)
- `json_archive`: `const char *` -- string discriminants (e.g., `"gen"`, `"key"`)
- `debug_archive`: `const char *` -- inherited from `json_archive`

### Varint Bug Backward Compatibility

A backward-compatibility mechanism exists in the binary reader (`enable_varint_bug_backward_compatibility()`) to handle data written by older code that serialized certain unsigned integer container/pair elements as fixed-width rather than varint. When enabled on the reader, types that were not previously varint-encoded (`uint16_t`) are deserialized using `do_serialize` (fixed-width) instead of `serialize_varint`. Types that were always varint (`uint64_t`, `uint32_t`) continue to use varint. This is used in wallet deserialization (`wallet2.cpp`).

### String Serialization

Strings are serialized as a varint length prefix followed by the raw character data. On deserialization, a sanity check verifies `remaining_bytes >= size` before allocating.

### Difficulty Type Serialization

The 128-bit `cryptonote::difficulty_type` is split into high and low 64-bit halves, each serialized as a varint.

### Stream State Checking

`serialization::detail::do_check_stream_state` has two overloads:
- For saving (writing): simply checks `ar.good()`.
- For loading (reading): checks `ar.good()` and optionally checks for EOF (controlled by the `noeof` parameter).

### JSON Object Layer (`json_object.h` / `json_object.cpp`)

A separate, parallel JSON serialization system using RapidJSON exists alongside the archive-based DSL. It provides `toJsonValue`/`fromJsonValue` function pairs for all core Monero types (transactions, blocks, RingCT structures, RPC data structures). This layer:
- Uses `rapidjson::Writer<epee::byte_stream>` for writing.
- Uses `rapidjson::Value` for reading.
- Supports POD types via hex encoding (`is_to_hex` trait).
- Supports maps (string or hex-POD keys) and vectors via SFINAE-based template overloads.
- Defines custom exception types: `MISSING_KEY`, `WRONG_TYPE`, `BAD_INPUT`, `PARSE_FAIL`.
- Provides macros: `INSERT_INTO_JSON_OBJECT`, `GET_FROM_JSON_OBJECT`, `OBJECT_HAS_MEMBER_OR_THROW`.

## Dependencies

### This Module Depends On

| Dependency | Usage |
|------------|-------|
| `common/varint.h` (`tools::read_varint`, `tools::write_varint`) | Variable-length integer encoding/decoding in `binary_archive`. |
| `common/va_args.h` | `__VA_OPT__` support detection macro. |
| `epee` (`span.h`, `byte_stream.h`, `hex.h`) | Byte span for binary reader input; byte stream for RapidJSON writer output; hex encoding. |
| `boost::mpl` | `bool_<>` for `is_saving` trait; `begin`/`end`/`front`/`pop_front`/`deref` for variant type list iteration. |
| `boost::endian` | `little_to_native_inplace` for portable binary deserialization. |
| `boost::variant` | Variant type support and `apply_visitor`. |
| `boost::type_traits` | `is_integral`, `integral_constant`. |
| `rapidjson` | JSON DOM and writer for the `json_object` layer. |
| `crypto/` | Type definitions for `crypto::hash`, `crypto::public_key`, etc. (used in `crypto.h`). |
| `cryptonote_basic/` | Core type definitions for blocks, transactions, difficulty (used in `json_object` and `difficulty_type`). |
| `ringct/` | RingCT type definitions (used in `json_object`). |
| `rpc/message_data_structs.h` | RPC data structures (used in `json_object`). |
| `common/sfinae_helpers.h` | `is_map_like`, `is_vector_like` traits (used in `json_object`). |

### Modules That Depend On This

| Module | Usage |
|--------|-------|
| `cryptonote_basic/` | `cryptonote_basic.h`, `tx_extra.h`, `subaddress_index.h` -- all core types use `BEGIN_SERIALIZE_OBJECT`, `BLOB_SERIALIZER`, `VARIANT_TAG`. |
| `cryptonote_core/` | `blockchain.h`/`.cpp` -- block/transaction serialization for storage and validation. |
| `cryptonote_protocol/` | `cryptonote_protocol_defs.h` -- protocol message serialization. |
| `ringct/` | `rctTypes.h`, `rctSigs.cpp` -- RingCT type serialization with blob and variant tags. |
| `wallet/` | `wallet2.cpp`/`.h` -- wallet file serialization, uses `enable_varint_bug_backward_compatibility()`. |
| `p2p/` | `net_peerlist_boost_serialization.h`, `p2p_protocol_defs.h` -- peer list data serialization. |
| `rpc/` | `daemon_messages.cpp`, `message.cpp`, `zmq_pub.cpp`, `rpc_payment.h` -- RPC request/response serialization. |
| `blockchain_utilities/` | `bootstrap_file.cpp`, `bootstrap_serialization.h` -- blockchain import/export. |
| `multisig/` | `multisig_kex_msg_serialization.h` -- key exchange message serialization. |
| `net/` | `tor_address.cpp`, `i2p_address.cpp` -- network address serialization. |
| `checkpoints/` | `checkpoints.cpp` -- checkpoint data serialization. |

## Transaction Input/Output Variant Tags (Binary)

Source: `src/cryptonote_basic/cryptonote_basic.h` VARIANT_TAG definitions.

### Binary Archive Tags

The following `uint8_t` tag values identify transaction input and output types in the binary wire format:

| Type | Tag Value | Description |
|------|-----------|-------------|
| `txin_gen` | `0xff` | Coinbase (miner) transaction input |
| `txin_to_key` | `0x02` | Standard transaction input (key image + ring members) |
| `txout_to_key` | `0x02` | Standard transaction output (pre-view-tag, HF < 16) |
| `txout_to_tagged_key` | `0x03` | Tagged transaction output (with view tag, HF ≥ 15) |

### JSON Archive Tags

| Type | Tag String | Description |
|------|------------|-------------|
| `txin_gen` | `"gen"` | Coinbase input |
| `txin_to_key` | `"key"` | Standard input |
| `txout_to_key` | `"key"` | Standard output |
| `txout_to_tagged_key` | `"tagged_key"` | Tagged output |

### Important Notes

- `txout_to_key` and `txin_to_key` share the same binary tag `0x02`, but they appear in different contexts (output list vs input list) so there is no ambiguity.
- From HF v16, only `txout_to_tagged_key` (tag `0x03`) is valid for transaction outputs. The `txout_to_key` type is rejected.
- From HF v15, outputs may be either `txout_to_key` or `txout_to_tagged_key`, but all outputs in a transaction must use the same type.

## Varint Backward Compatibility

Source: `wallet2.cpp`, `serialization/binary_archive.h`.

### The Varint Bug

Older versions of the wallet serialization code wrote certain unsigned integer fields as fixed-width values rather than variable-length integers (varints). When the serialization framework was updated to use varints for all unsigned integers in containers and pairs, a backward-compatibility mechanism was needed.

### Compatibility Mechanism

`binary_archive<false>::enable_varint_bug_backward_compatibility()`:
- When enabled on a binary reader, types that were historically NOT varint-encoded (`uint16_t`) are deserialized using fixed-width reads (`do_serialize`) instead of `serialize_varint`.
- Types that were always varint-encoded (`uint32_t`, `uint64_t`) continue to use varint.
- This mode is ONLY used during wallet cache deserialization — it does NOT affect the wire protocol or blockchain serialization.

### Scope

- **Affected:** Wallet file deserialization (loading `.keys` and cache files from older wallet versions).
- **Not affected:** Network wire protocol, block/transaction binary serialization, RPC.
- Controlled by `binary_archive::m_varint_bug_backward_compatibility` flag, which is `false` by default.

## Blob Hashing Contracts

Source: `src/cryptonote_basic/cryptonote_format_utils.cpp`.

### Hash Functions

| Function | Input | Hash Algorithm | Purpose |
|----------|-------|----------------|---------|
| `get_transaction_hash(tx)` | Full serialized tx blob (prefix + signatures/RCT) | Keccak-256 | Primary transaction identifier; used in tx pool, block tx lists, key image mapping |
| `get_transaction_prefix_hash(tx)` | Serialized `transaction_prefix` only (version, unlock_time, inputs, outputs, extra) | Keccak-256 | Signing hash — what gets signed by ring signatures |
| `get_blob_hash(blob)` | Raw byte array | Keccak-256 | General-purpose hash of arbitrary data |
| `get_block_hashing_blob(block)` | Block header fields + Merkle tree root of transaction hashes | Keccak-256 | PoW input — what miners hash to find valid nonces |
| `get_block_hash(block)` | Serialized block header + Merkle root + tx count varint | Keccak-256 | Block identifier for chain storage and P2P |

### Merkle Tree Construction

For block hashing, transactions are Merkle-hashed:
1. If 0 transactions: root = null_hash.
2. If 1 transaction: root = hash of that transaction.
3. If N > 1 transactions: Standard binary Merkle tree with Keccak-256 at each level. Tree is padded to the next power of 2 by repeating the last hash.

### Prunable Hash

For v2+ transactions, `get_transaction_prunable_hash(tx)` hashes only the prunable portion (RCT signatures, bulletproofs). This hash is stored separately in the `txs_prunable_hash` LMDB table to allow verification even after the prunable data has been deleted.

## Standard Varint (Binary Archive)

Source: `src/common/varint.h`

The standard varint format is used by `binary_archive` for all `VARINT_FIELD` serialization, container lengths, and array sizes throughout the consensus-critical binary wire format.

**Encoding** (7-bit, MSB continuation, little-endian):

```
while value >= 0x80:
    emit byte: (value & 0x7F) | 0x80    // low 7 bits + continuation flag
    value >>= 7
emit byte: value                          // final byte, MSB = 0
```

**Decoding:**

```
write = 0
for shift = 0, 7, 14, ...:
    byte = read_next_byte()
    if shift + 7 >= max_bits AND byte >= (1 << (max_bits - shift)):
        return EVARINT_OVERFLOW
    if byte == 0 AND shift != 0:
        return EVARINT_REPRESENT         // non-canonical trailing zero
    write |= (byte & 0x7F) << shift
    if (byte & 0x80) == 0:
        break
```

**Error codes:**
- `EVARINT_OVERFLOW = -1`: Value would exceed the target type's bit width
- `EVARINT_REPRESENT = -2`: Non-canonical encoding (trailing zero byte after the first)

**Size:** 1 byte for 0-127, 2 bytes for 128-16383, up to 10 bytes for `uint64_t` max.

**Example:** Value `300` (0x12C):
- Byte 0: `0x2C | 0x80` = `0xAC` (low 7 bits = 0x2C, continuation set)
- Byte 1: `0x02` (remaining bits, MSB = 0, done)
- Wire bytes: `[0xAC, 0x02]`

Source: `src/common/varint.h:66-79` (write), `src/common/varint.h:92-119` (read)

## Portable Storage Varint

Source: `contrib/epee/include/storages/portable_storage_base.h:41-45`

The Portable Storage (PS) varint is a **completely different format** from the standard varint. It is used within epee's portable storage binary format for encoding field counts, array lengths, and string lengths in P2P messages.

**Encoding** (2-bit size prefix, little-endian integer):

The 2 lowest bits of the first byte encode the total byte width:

| Size Mask (bits 0-1) | Width | Value Range | Constant |
|----------------------:|------:|-------------|----------|
| `00` | 1 byte | 0 – 63 | `PORTABLE_RAW_SIZE_MARK_BYTE` |
| `01` | 2 bytes | 0 – 16,383 | `PORTABLE_RAW_SIZE_MARK_WORD` |
| `10` | 4 bytes | 0 – 1,073,741,823 | `PORTABLE_RAW_SIZE_MARK_DWORD` |
| `11` | 8 bytes | 0 – 4,611,686,018,427,387,903 (2^62 - 1) | `PORTABLE_RAW_SIZE_MARK_INT64` |

**Encoding algorithm:**
```
encoded = (value << 2) | size_mask
write encoded as little-endian integer of the appropriate width
```

**Decoding algorithm:**
```
size_mask = first_byte & 0x03
read LE integer of width (1, 2, 4, or 8 bytes depending on mask)
value = integer >> 2
```

**Side-by-side comparison** for value `300`:

| Format | Encoding | Wire Bytes |
|--------|----------|------------|
| Standard varint | 7-bit MSB continuation | `[0xAC, 0x02]` (2 bytes) |
| PS varint | `(300 << 2) \| 0x01` = `0x04B1`, LE uint16 | `[0xB1, 0x04]` (2 bytes) |

**Side-by-side comparison** for value `42`:

| Format | Encoding | Wire Bytes |
|--------|----------|------------|
| Standard varint | `0x2A` (fits in 7 bits) | `[0x2A]` (1 byte) |
| PS varint | `(42 << 2) \| 0x00` = `0xA8`, uint8 | `[0xA8]` (1 byte) |

**Rust port note:** These two varint formats are NOT interchangeable. Standard varints appear in transaction/block binary serialization. PS varints appear only in epee portable storage messages (P2P handshake, command payloads). Mixing them up will cause deserialization failures.

## Levin Protocol Header

Source: `contrib/epee/include/net/levin_base.h:39-83`

The Levin protocol header (`bucket_head2`) is a 33-byte `#pragma pack(1)` structure prefixing every P2P message. All multi-byte fields are little-endian.

**Structure layout:**

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 8 | `m_signature` | `uint64_t` | Magic: `0x0101010101012101` (`LEVIN_SIGNATURE`) |
| 8 | 8 | `m_cb` | `uint64_t` | Payload length in bytes |
| 16 | 1 | `m_have_to_return_data` | `uint8_t` | 1 if sender expects a response, 0 otherwise |
| 17 | 4 | `m_command` | `uint32_t` | Command ID (e.g., 1001 = handshake, 1003 = ping) |
| 21 | 4 | `m_return_code` | `int32_t` | Response status code (0 = success) |
| 25 | 4 | `m_flags` | `uint32_t` | Packet type flags (bitmask, see below) |
| 29 | 4 | `m_protocol_version` | `uint32_t` | Protocol version (currently `LEVIN_PROTOCOL_VER_1` = 1) |

**Total size:** 33 bytes

**Flags** (from `levin_base.h:80-83`):

| Flag | Value | Meaning |
|------|------:|---------|
| `LEVIN_PACKET_REQUEST` | `0x00000001` | This is a request (invoke or notify) |
| `LEVIN_PACKET_RESPONSE` | `0x00000002` | This is a response |
| `LEVIN_PACKET_BEGIN` | `0x00000004` | First fragment of a multi-part message |
| `LEVIN_PACKET_END` | `0x00000008` | Last fragment of a multi-part message |

**Size limits:**
- Before handshake: `LEVIN_INITIAL_MAX_PACKET_SIZE` = 256 KB (262,144 bytes)
- After handshake: `LEVIN_DEFAULT_MAX_PACKET_SIZE` = 100 MB (100,000,000 bytes)

**Legacy header:** `bucket_head` (the v0 header) has the same first 5 fields but uses `m_reservedA` and `m_reservedB` instead of `m_flags` and `m_protocol_version`. It is defined but not used in current Monero.

## Portable Storage Header

Source: `contrib/epee/include/storages/portable_storage_base.h:37-39`

Every portable storage binary blob begins with a 9-byte header:

| Offset | Size | Value | Constant |
|-------:|-----:|-------|----------|
| 0 | 4 | `0x01011101` | `PORTABLE_STORAGE_SIGNATUREA` |
| 4 | 4 | `0x01020101` | `PORTABLE_STORAGE_SIGNATUREB` |
| 8 | 1 | `0x01` | `PORTABLE_STORAGE_FORMAT_VER` |

**Serialization type constants** (from `portable_storage_base.h:52-66`):

| Constant | Value | C++ Type |
|----------|------:|----------|
| `SERIALIZE_TYPE_INT64` | 1 | `int64_t` |
| `SERIALIZE_TYPE_INT32` | 2 | `int32_t` |
| `SERIALIZE_TYPE_INT16` | 3 | `int16_t` |
| `SERIALIZE_TYPE_INT8` | 4 | `int8_t` |
| `SERIALIZE_TYPE_UINT64` | 5 | `uint64_t` |
| `SERIALIZE_TYPE_UINT32` | 6 | `uint32_t` |
| `SERIALIZE_TYPE_UINT16` | 7 | `uint16_t` |
| `SERIALIZE_TYPE_UINT8` | 8 | `uint8_t` |
| `SERIALIZE_TYPE_DOUBLE` | 9 | `double` |
| `SERIALIZE_TYPE_STRING` | 10 | `std::string` |
| `SERIALIZE_TYPE_BOOL` | 11 | `bool` |
| `SERIALIZE_TYPE_OBJECT` | 12 | `section` (nested object) |
| `SERIALIZE_TYPE_ARRAY` | 13 | Array of homogeneous type |

**Array flag:** `SERIALIZE_FLAG_ARRAY = 0x80`. When OR'd with a type byte, indicates an array of that type (e.g., `0x80 | 5 = 0x85` = array of `uint64_t`).

## Known Issues

| File | Line | Comment |
|------|------|---------|
| `src/serialization/binary_archive.h` | 52 | `//TODO: fix size_t warning in x32 platform` |
