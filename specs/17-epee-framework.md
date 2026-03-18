# Epee Framework

## Overview

Epee is Monero's foundational utility library, originally authored by Andrey N. Sabelnikov. It provides the core infrastructure that nearly every other Monero component depends on: a custom binary/JSON serialization system (Portable Storage), network address abstractions, a Boost.Asio-based TCP server and HTTP server, the Levin wire protocol used for P2P communication, a category-based logging framework built on easylogging++, non-owning memory span types, string conversion utilities, byte buffer management, and macro-based serialization helpers. The library lives under `contrib/epee/` and is compiled as part of every Monero build target.

## Key Files

| File | Description |
|------|-------------|
| `contrib/epee/include/storages/portable_storage.h` | Main `portable_storage` class -- the top-level serialization API |
| `contrib/epee/include/storages/portable_storage_base.h` | Core data types: `section`, `storage_entry`, `array_entry`, type codes, varint masks |
| `contrib/epee/include/storages/portable_storage_from_bin.h` | Binary deserialization (`throwable_buffer_reader`) |
| `contrib/epee/include/storages/portable_storage_to_bin.h` | Binary serialization (varint packing, section/entry writers) |
| `contrib/epee/include/storages/portable_storage_from_json.h` | JSON deserialization (state-machine parser) |
| `contrib/epee/include/storages/portable_storage_to_json.h` | JSON serialization (visitor-based pretty-printer) |
| `contrib/epee/include/storages/portable_storage_bin_utils.h` | Endianness conversion (`CONVERT_POD` / `convert_swapper`) |
| `contrib/epee/include/storages/portable_storage_val_converters.h` | Type-safe integer/bool/string value conversion with bounds checking |
| `contrib/epee/include/storages/portable_storage_template_helper.h` | Convenience functions: `load_t_from_binary`, `store_t_to_json`, etc. |
| `contrib/epee/include/storages/levin_abstract_invoke2.h` | Levin command invoke/notify helpers using portable storage |
| `contrib/epee/include/storages/http_abstract_invoke.h` | HTTP JSON/binary RPC invoke helpers |
| `contrib/epee/src/portable_storage.cpp` | Non-template `portable_storage` method implementations |
| `contrib/epee/include/serialization/keyvalue_serialization.h` | `BEGIN_KV_SERIALIZE_MAP` / `KV_SERIALIZE` macro family |
| `contrib/epee/include/serialization/keyvalue_serialization_overloads.h` | Overloads for STL containers, blobs, etc. |
| `contrib/epee/include/string_tools.h` | String/hex/IP conversion utilities (header) |
| `contrib/epee/include/string_tools_lexical.h` | `get_xtype_from_string` / `xtype_to_string` via `boost::lexical_cast` |
| `contrib/epee/src/string_tools.cpp` | String utility implementations (IP parsing, module path, etc.) |
| `contrib/epee/include/net/http_server_impl_base.h` | Template HTTP server base class |
| `contrib/epee/include/net/http_protocol_handler.h` | HTTP connection handler / state machine |
| `contrib/epee/include/net/http_server_handlers_map2.h` | `CHAIN_HTTP_TO_MAP2` / `BEGIN_URI_MAP2` macro-based handler routing |
| `contrib/epee/include/net/abstract_tcp_server2.h` | `connection<>` and `boosted_tcp_server<>` -- Boost.Asio TCP infrastructure |
| `contrib/epee/include/net/net_utils_base.h` | Network address classes, `connection_context_base`, `i_service_endpoint` |
| `contrib/epee/src/net_utils_base.cpp` | Implementations for address comparison, zone helpers |
| `contrib/epee/include/net/enums.h` | `address_type` and `zone` enums |
| `contrib/epee/include/net/levin_base.h` | Levin protocol header structs, `message_writer`, error codes |
| `contrib/epee/include/net/levin_protocol_handler_async.h` | Async Levin protocol handler (used by P2P layer) |
| `contrib/epee/include/net/net_ssl.h` | SSL/TLS support types and options |
| `contrib/epee/include/misc_log_ex.h` | Logging macros (`MERROR`, `MINFO`, `MDEBUG`, etc.) and assertion helpers |
| `contrib/epee/src/mlog.cpp` | Logging configuration, category management, console color support |
| `contrib/epee/include/span.h` | `epee::span<T>` -- non-owning view over contiguous memory |
| `contrib/epee/include/byte_slice.h` | `byte_slice` -- immutable, ref-counted byte buffer |
| `contrib/epee/include/byte_stream.h` | `byte_stream` -- growable write buffer (replaces `std::stringstream`) |
| `contrib/epee/include/hex.h` | Hex encoding/decoding utilities |
| `contrib/epee/include/wipeable_string.h` | Securely wiped string for sensitive data |
| `contrib/epee/include/mlocker.h` | Memory-locked allocations (prevents swapping secrets) |
| `contrib/epee/include/file_io_utils.h` | File read/write helpers |
| `contrib/epee/include/int-util.h` | Integer utilities and byte-swap macros |
| `contrib/epee/include/syncobj.h` | Synchronization primitives (`critical_section`, etc.) |
| `contrib/epee/include/rolling_median.h` | Rolling median calculator |
| `contrib/epee/include/math_helper.h` | Math helper functions |
| `contrib/epee/include/profile_tools.h` | Performance profiling utilities |
| `docs/PORTABLE_STORAGE.md` | External documentation of the portable storage binary format |

## Components

### Portable Storage

The portable storage system is the primary serialization mechanism for Monero's P2P protocol (Levin) and RPC interfaces. It supports serialization to and from both a compact binary format and JSON. The canonical specification is documented in `docs/PORTABLE_STORAGE.md`.

#### Data Model

The core data model is defined in `contrib/epee/include/storages/portable_storage_base.h`:

- **`section`**: A map (`std::map<std::string, storage_entry>`) of named entries. This is the fundamental container, analogous to a JSON object.
- **`storage_entry`**: A `boost::variant` over all supported scalar types plus `section` and `array_entry`. The supported scalar types are: `uint64_t`, `uint32_t`, `uint16_t`, `uint8_t`, `int64_t`, `int32_t`, `int16_t`, `int8_t`, `double`, `bool`, `std::string`.
- **`array_entry`**: A recursive `boost::variant` over `array_entry_t<T>` for each supported type (including `section` for arrays of objects). Each `array_entry_t<T>` wraps a `std::vector<T>` (or `std::deque<bool>` for bools) and provides iterator-based sequential access via `get_first_val()` / `get_next_val()`.
- **Handle types**: `hsection` is `section*`; `harray` is `array_entry*`.

#### Binary Format

The binary format begins with a 9-byte header:

| Field | Type | Value |
|-------|------|-------|
| Signature A | uint32 | `0x01011101` |
| Signature B | uint32 | `0x01020101` |
| Version | uint8 | `0x01` |

This is followed by the root section, serialized as a varint entry count followed by sequentially encoded name-value entries. Each entry consists of a length-prefixed key name (1 byte length, max 255), a type byte, and the value data. Arrays set the `SERIALIZE_FLAG_ARRAY` (`0x80`) bit on the type byte and prefix the values with a varint count.

**Varint encoding**: The lowest 2 bits indicate the byte width (0=1 byte, 1=2 bytes, 2=4 bytes, 3=8 bytes). The actual value is stored in the remaining bits (shifted left by 2). All integers are little-endian. Big-endian platforms use `CONVERT_POD` to byte-swap.

**Type codes** (defined in `portable_storage_base.h`):

| Code | Type | Code | Type |
|------|------|------|------|
| 1 | int64 | 7 | uint16 |
| 2 | int32 | 8 | uint8 |
| 3 | int16 | 9 | double |
| 4 | int8 | 10 | string |
| 5 | uint64 | 11 | bool |
| 6 | uint32 | 12 | object (section) |

Strings are varint-length-prefixed byte sequences (max `MAX_STRING_LEN_POSSIBLE` = 2,000,000,000 bytes). Hashes, keys, and binary blobs in Monero are stored as type 10 (string).

#### The `portable_storage` Class

Defined in `contrib/epee/include/storages/portable_storage.h` and implemented in `contrib/epee/src/portable_storage.cpp`. Key API:

- **`load_from_binary(span<const uint8_t>, limits_t*)`**: Deserializes from binary. Validates the 9-byte header, then delegates to `throwable_buffer_reader`. Optional `limits_t` constrains max objects, fields, and strings to prevent DoS.
- **`store_to_binary(byte_slice&)` / `store_to_binary(byte_stream&)`**: Serializes to binary, writing the header then recursively packing sections.
- **`load_from_json(const string&)`**: Parses JSON into the internal section tree using a hand-written state-machine parser.
- **`dump_as_json(string&, indent, insert_newlines)`**: Serializes to JSON string using boost visitor pattern.
- **`get_value<T>(name, val, section)`**: Retrieves a typed value from a section using `boost::apply_visitor` with type conversion.
- **`set_value<T>(name, val, section)`**: Sets or inserts a typed value.
- **`open_section(name, parent, create)`**: Opens or creates a named sub-section.
- **Array access**: `get_first_value` / `get_next_value`, `insert_first_value` / `insert_next_value` for sequential array element access. Similar API for section arrays (`get_first_section`, `insert_first_section`, etc.).

#### Binary Deserialization (`throwable_buffer_reader`)

Defined entirely in `contrib/epee/include/storages/portable_storage_from_bin.h`. This is a streaming reader that:

- Enforces a **recursion limit** of 100 (configurable via `EPEE_PORTABLE_STORAGE_RECURSION_LIMIT`).
- Tracks and enforces **limits** on total objects, fields, and strings via `set_limits()`.
- Performs **size sanity checks** using `ps_min_bytes<T>::strict` to ensure claimed array sizes are plausible given remaining buffer bytes.
- Validates **bool values** are 0 or 1, and rejects **duplicate keys** within a section.
- Uses a `RECURSION_LIMITATION()` guard macro in every read method.

#### JSON Parser

Defined in `contrib/epee/include/storages/portable_storage_from_json.h`. A state-machine parser with states: `match_state_lookup_for_section_start`, `match_state_lookup_for_name`, `match_state_waiting_separator`, `match_state_wonder_after_separator`, `match_state_wonder_after_value`, `match_state_wonder_array`, etc. The parser:

- Handles strings, signed/unsigned integers, doubles, booleans, null, objects, and arrays.
- Enforces a JSON recursion limit of 100 (`EPEE_JSON_RECURSION_LIMIT_INTERNAL`).
- Does not support arrays of arrays (throws: "array of array not suppoerted yet").
- Delegates to `epee::misc_utils::parse::match_string2`, `match_number2`, `match_word2` for token extraction.

#### KV Serialization Macros

Defined in `contrib/epee/include/serialization/keyvalue_serialization.h`. These macros generate `store()` and `load()` methods on structs:

- **`BEGIN_KV_SERIALIZE_MAP()`**: Opens the serialize map; generates `store`, `load`, `_load`, and a templated `serialize_map<is_store>` method.
- **`KV_SERIALIZE(var)`**: Serializes field `var` using its own name as the key.
- **`KV_SERIALIZE_N(var, name)`**: Serializes field `var` under a custom key name.
- **`KV_SERIALIZE_OPT(var, default)`**: Only serializes if the value differs from `default`; sets the default on load failure.
- **`KV_SERIALIZE_VAL_POD_AS_BLOB(var)`**: Serializes a POD type as a binary string blob (with `is_trivially_copyable` and `is_standard_layout` compile-time checks).
- **`KV_SERIALIZE_CONTAINER_POD_AS_BLOB(var)`**: Serializes a container of POD types as a blob.
- **`KV_SERIALIZE_PARENT(type)`**: Serializes parent class fields.
- **`END_KV_SERIALIZE_MAP()`**: Closes the serialize map.

#### Template Helpers

`contrib/epee/include/storages/portable_storage_template_helper.h` provides convenience functions that combine `portable_storage` with struct `store()`/`load()`:

- `load_t_from_json(out, json_string)` / `store_t_to_json(in, json_string)`
- `load_t_from_binary(out, binary_span)` / `store_t_to_binary(in, byte_slice)`
- File variants: `load_t_from_json_file`, `store_t_to_json_file`, `load_t_from_binary_file`

#### Value Converters

`contrib/epee/include/storages/portable_storage_val_converters.h` provides safe type conversion between storage entry types. It handles signed-to-unsigned, unsigned-to-signed, and cross-width integer conversions with bounds checking. A special `std::string` to `uint64_t` converter supports MyMonero/OpenMonero compatibility (parsing numeric strings and ISO 8601 timestamps).

#### Levin Integration

`contrib/epee/include/storages/levin_abstract_invoke2.h` provides `async_invoke_remote_command2` and `invoke_remote_command2` that serialize structs via portable storage, wrap them in Levin messages, and send/receive them over the P2P transport. Default Levin limits: 8192 objects, 16384 fields, 16384 strings.

#### HTTP Integration

`contrib/epee/include/storages/http_abstract_invoke.h` provides `invoke_http_json` and `invoke_http_bin` that use portable storage to serialize/deserialize RPC request/response structs over HTTP.

### String Tools

String tools are split across three files:

#### `string_tools.h` (Header)

Provides inline and declared utility functions in `epee::string_tools`:

- **Hex conversion**: `buff_to_hex_nodelimer(string)` converts a binary string to hex. `parse_hexstr_to_binbuff(string_ref, string&)` does the reverse. Both delegate to `epee::to_hex` / `epee::from_hex`.
- **POD-to-hex**: `pod_to_hex<T>(s)` converts any standard-layout POD to a hex string. `hex_to_pod<T>(hex, s)` does the reverse. Both enforce `is_standard_layout` and `has_unique_object_representations` at compile time. Overloads exist for `tools::scrubbed<T>` and `epee::mlocked<T>`.
- **Arithmetic-to-hex**: `to_string_hex<T>(val)` converts an arithmetic type to hex via `std::stringstream`.
- **IP address tools**: `get_ip_string_from_int32(uint32_t)` converts a network-order IP to dotted notation using `inet_ntoa`. `get_ip_int32_from_string(uint32_t&, string)` does the reverse via `inet_addr`. `parse_peer_from_string(ip, port, address)` splits "ip:port" strings.
- **String manipulation**: `trim(string)` delegates to `boost::trim`. `pad_string(s, n, c, prepend)` pads a string to width `n`. `compare_no_case(s1, s2)` is a case-insensitive comparison via `boost::iequals` (note: returns `true` if they differ).
- **File path tools**: `get_extension(str)`, `cut_off_extension(str)` using `boost::filesystem`.
- **Module info**: `get_current_module_name()`, `get_current_module_folder()`, `set_module_name_and_folder(path)` track the running process path.
- **Windows UTF**: `utf8_to_utf16(string)` / `utf16_to_utf8(wstring)` (Windows only).

#### `string_tools_lexical.h`

- **`get_xtype_from_string<T>(val, str)`**: Converts a string to any type `T` via `boost::lexical_cast`. For unsigned integral types, pre-validates that all characters are digits.
- **`xtype_to_string<T>(val, str)`**: Converts any type to string via `boost::lexical_cast`.

#### `string_tools.cpp` (Implementation)

Provides the non-inline implementations of IP parsing, module path management, string padding, file extension extraction, and Windows UTF conversion.

### HTTP Server

The HTTP server infrastructure is built on top of the TCP server layer.

#### `http_server_impl_base<t_child_class, t_connection_context>`

Defined in `contrib/epee/include/net/http_server_impl_base.h`. This is a CRTP base class that:

- Wraps a `boosted_tcp_server` configured with `http_custom_handler` and `e_connection_type_RPC`.
- **`init()`** configures binding (IPv4 and optional IPv6), access control origins, HTTP authentication (`boost::optional<login>`), SSL options, and connection limits:
  - `max_public_ip_connections` (default: `DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP`)
  - `max_private_ip_connections` (default: `DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP`)
  - `max_connections` (default: `DEFAULT_RPC_MAX_CONNECTIONS`)
  - `response_soft_limit` (default: `DEFAULT_RPC_SOFT_LIMIT_SIZE`)
- **`run(threads_count, wait)`** starts the io_context worker threads.
- **`is_host_limit(network_address)`** implements per-host connection limiting, distinguishing private (loopback/local) and public addresses.
- **`deinit()`**, **`send_stop_signal()`**, **`timed_wait_server_stop()`** provide lifecycle management.

#### HTTP Handler Map

`contrib/epee/include/net/http_server_handlers_map2.h` defines macros for routing HTTP requests:

- **`CHAIN_HTTP_TO_MAP2(context_type)`**: Generates a `handle_http_request` method that logs the request and delegates to `handle_http_request_map`.
- **`BEGIN_URI_MAP2()`** / **`MAP_URI2(uri, handler)`** / **`END_URI_MAP2()`**: Define a URI-to-handler dispatch table.

#### HTTP Protocol Handler

`contrib/epee/include/net/http_protocol_handler.h` defines `simple_http_connection_handler` which implements the HTTP state machine (request parsing, response generation) and uses `http_server_config` for per-server settings (folder, CORS, auth, content-length limits, per-IP connection counts).

### Net Utils

#### Network Address Hierarchy

Defined in `contrib/epee/include/net/net_utils_base.h` and `contrib/epee/include/net/enums.h`:

- **`address_type`** enum: `invalid` (0), `ipv4` (1), `ipv6` (2), `i2p` (3), `tor` (4). Values are stable for serialization.
- **`zone`** enum: `invalid` (0), `public_` (1), `i2p` (2), `tor` (3). Order from `i2p` onward determines priority for origin TX selection.

**Concrete address classes**:

- **`ipv4_network_address`**: Stores `uint32_t` IP (network byte order) and `uint16_t` port. Provides `equal()`, `less()`, `is_same_host()`, `str()`, `host_str()`, `is_loopback()`, `is_local()`. KV-serializable with endianness handling (`SWAP32LE`).
- **`ipv4_network_subnet`**: Stores `uint32_t` IP and `uint8_t` mask. Provides `subnet()` via bitmask, `matches(ipv4_network_address)` for membership testing.
- **`ipv6_network_address`**: Wraps `boost::asio::ip::address_v6` and `uint16_t` port. KV-serializes the address bytes as a blob.

**Type-erased wrapper**:

- **`network_address`**: Uses a `shared_ptr<interface>` with a template `implementation<T>` pattern to type-erase any address type. Provides a uniform API (`str()`, `host_str()`, `is_loopback()`, `is_local()`, `get_type_id()`, `get_zone()`, `is_blockable()`, `port()`). The `as<T>()` method downcasts with type checking. KV serialization dispatches on the `type` byte to the appropriate concrete address deserializer. Cross-type `is_same_host()` handles IPv4-mapped IPv6 addresses.

#### Connection Context

**`connection_context_base`** tracks metadata for each connection:

- Const members: `m_connection_id` (UUID), `m_remote_address`, `m_is_income`, `m_started` (timestamp), `m_ssl`.
- Mutable members: `m_last_recv`, `m_last_send`, `m_recv_cnt`, `m_send_cnt`, `m_current_speed_down/up`, `m_max_speed_down/up`.
- Copy uses placement-new via a private `set_details()` method (friend of `connection<>`).

#### Service Endpoint Interface

**`i_service_endpoint`** is the abstract interface for sending data over a connection:

- `do_send(byte_slice)`, `close()`, `send_done()`, `call_run_once_service_io()`, `request_callback()`, `get_io_context()`, `add_ref()`, `release()`.

#### TCP Server (`boosted_tcp_server`)

Defined in `contrib/epee/include/net/abstract_tcp_server2.h`. A template class parameterized on a protocol handler type:

- **`connection<t_protocol_handler>`**: Represents a single TCP connection. Uses `boost::asio::io_context::strand` for thread safety. Manages connection lifecycle through states: `TERMINATED`, `RUNNING`, `INTERRUPTED`, `TERMINATING`, `WASTED`. Internally tracks socket status, SSL state, timer state, protocol state, read/write buffers, and per-connection throttling.
- **`boosted_tcp_server<t_protocol_handler>`**: The server that accepts connections. Supports dual-stack (IPv4 + IPv6), SSL/TLS, connection filtering (`i_connection_filter`), per-host connection limiting (`i_connection_limit`), configurable send queue limits (`ABSTRACT_SERVER_SEND_QUE_MAX_COUNT` = 1000), idle timer callbacks, and async connection via `connect_async()`.

#### Connection Context Logging Macros

`net_utils_base.h` defines convenience macros for logging with connection context:

- `LOG_ERROR_CC(ct, message)`, `LOG_WARNING_CC(ct, message)`, etc.
- `LOG_PRINT_CCONTEXT_L0(message)` through `LOG_PRINT_CCONTEXT_L3(message)` (use implicit `context` variable).

#### Levin Protocol

Defined in `contrib/epee/include/net/levin_base.h`:

- **Signature**: `0x0101010101012101` ("Bender's nightmare").
- **`bucket_head2`** (packed): `m_signature` (8), `m_cb` (8, payload size), `m_have_to_return_data` (1), `m_command` (4), `m_return_code` (4), `m_flags` (4), `m_protocol_version` (4) = 33 bytes total.
- **Flags**: `LEVIN_PACKET_REQUEST` (0x01), `LEVIN_PACKET_RESPONSE` (0x02), `LEVIN_PACKET_BEGIN` (0x04), `LEVIN_PACKET_END` (0x08).
- **Size limits**: `LEVIN_INITIAL_MAX_PACKET_SIZE` = 256 KiB (before handshake), `LEVIN_DEFAULT_MAX_PACKET_SIZE` = 100 MB (after handshake).
- **`message_writer`**: Reserves space for the Levin header, allowing payload to be written directly into a `byte_stream` and finalized without copying.
- **`levin_commands_handler<t_connection_context>`**: Abstract interface with `invoke(command, in_buff, out_buff, context)` and `notify(command, in_buff, context)`.
- **`make_noise_notify(size)`**: Generates dummy Levin messages for traffic obfuscation.
- **`make_fragmented_notify(noise_size, command, message)`**: Splits a message into fixed-size fragments matching a noise pattern.

### Logging

The logging system wraps [easylogging++](https://github.com/amrayn/easyloggingpp) with Monero-specific macros and configuration. Defined in `contrib/epee/include/misc_log_ex.h` (macros) and `contrib/epee/src/mlog.cpp` (configuration).

#### Log Levels and Categories

Logging uses easylogging++ levels (`Fatal`, `Error`, `Warning`, `Info`, `Debug`, `Trace`) combined with a category string. Each translation unit defines its own default category via:

```cpp
#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "net"
```

#### Macro Hierarchy

Two tiers of macros exist:

- **Category-aware** (MC prefix): `MCFATAL(cat, x)`, `MCERROR(cat, x)`, `MCWARNING(cat, x)`, `MCINFO(cat, x)`, `MCDEBUG(cat, x)`, `MCTRACE(cat, x)`.
- **Default-category** (M prefix): `MFATAL(x)`, `MERROR(x)`, `MWARNING(x)`, `MINFO(x)`, `MDEBUG(x)`, `MTRACE(x)` -- these use `MONERO_DEFAULT_LOG_CATEGORY`.
- **Global info**: `MGINFO(x)` logs at Info level to the "global" category.
- **Colored variants**: `MLOG_RED(level, x)`, `MLOG_GREEN(level, x)`, `MLOG_YELLOW(level, x)`, `MLOG_BLUE(level, x)`, `MLOG_MAGENTA(level, x)`, `MLOG_CYAN(level, x)`.
- **Legacy aliases**: `LOG_ERROR(x)` = `MERROR`, `LOG_PRINT_L0(x)` = `MWARNING`, `LOG_PRINT_L1(x)` = `MINFO`, `LOG_PRINT_L2(x)` = `MDEBUG`, `LOG_PRINT_L3/L4(x)` = `MTRACE`.

All macros check `el::Loggers::allowed(level, cat)` before evaluating the message expression, avoiding evaluation overhead for disabled levels.

#### Configuration

`mlog_configure(filename_base, console, max_log_file_size, max_log_files)` sets up:

- File logging with rotation (default max size 100 MB, default 10 files).
- Log format: `%datetime{%Y-%M-%d %H:%m:%s.%g}\t%thread\t%level\t%logger\t%loc\t%msg` (overridable via `MONERO_LOG_FORMAT` env var).
- Category-based filtering initialized from `MONERO_LOGS` env var or numeric level 0-4.

**Numeric log levels** map to category strings:

| Level | Categories |
|-------|-----------|
| 0 | `*:WARNING`, most net/verify/serialization at `FATAL`, `global:INFO`, `stacktrace:INFO` |
| 1 | `*:INFO`, `perf.*:DEBUG` |
| 2 | `*:DEBUG` |
| 3 | `*:TRACE`, `*.dump:DEBUG` |
| 4 | `*:TRACE` |

`mlog_set_categories(categories)` supports `+` prefix to append and `-` prefix to remove from current categories.

#### Assertion and Error Handling Macros

Also defined in `misc_log_ex.h`:

- **`TRY_ENTRY()` / `CATCH_ENTRY(location, return_val)`**: Exception-safe wrapper that catches `std::exception` and generic exceptions, logs them, and returns a failure value.
- **`CHECK_AND_ASSERT(expr, fail_ret_val)`**: Returns `fail_ret_val` if `expr` is false.
- **`CHECK_AND_ASSERT_MES(expr, fail_ret_val, message)`**: Same, but logs `message` on failure.
- **`CHECK_AND_ASSERT_THROW_MES(expr, message)`**: Throws `std::runtime_error` on failure.
- **`ASSERT_MES_AND_THROW(message)`**: Unconditionally logs and throws.
- **`CHECK_AND_NO_ASSERT_MES(expr, fail_ret_val, message)`**: Logs without asserting.

#### C API

Five C-callable logging functions are provided: `merror()`, `mwarning()`, `minfo()`, `mdebug()`, `mtrace()` -- each taking a category, printf-format string, and variadic args. These are used for C code that needs to participate in the logging system.

#### Console Color

`set_console_color(color, bright)` / `reset_console_color()` support 8 console colors on both Windows (via `SetConsoleTextAttribute`) and Unix (via ANSI escape codes). Colors are suppressed when stdout is not a TTY or when `NO_COLOR` env var is set.

### Span

Defined in `contrib/epee/include/span.h`. `epee::span<T>` is a non-owning view over contiguous memory, inspired by `gsl::span`.

#### Class Template `span<T>`

- Constructors: default (null), from `nullptr`, from `(T*, size_t)`, from C-arrays `T(&)[N]`.
- **Safety**: Template SFINAE prevents derived-to-base pointer conversions. Only allows exact type matches or `T*` to `const T*`.
- Methods: `begin()`, `end()`, `cbegin()`, `cend()`, `empty()`, `data()`, `size()`, `size_bytes()`, `operator[]`, `remove_prefix(amount)`.

#### Free Functions

- **`to_span(container)`**: Creates `span<const T::value_type>` from any STL-compatible container.
- **`to_mut_span(container)`**: Creates mutable span.
- **`to_byte_span(span<const T>)`**: Reinterprets a typed span as `span<const uint8_t>`, with `is_standard_layout` and `has_unique_object_representations` compile-time checks.
- **`to_mut_byte_span(container)`**: Mutable byte span from a container.
- **`as_byte_span(T&)`**: Gets the byte representation of a single object.
- **`as_mut_byte_span(T&)`**: Mutable byte view of a single object.
- **`strspan<T>(string)`**: Creates `span<const T>` from a `std::string`, for bridging string data to byte spans.

### Byte Slice and Byte Stream

#### `byte_slice`

Defined in `contrib/epee/include/byte_slice.h`. An immutable, reference-counted byte buffer inspired by Go slices:

- Storage is a custom ref-counted `byte_slice_data` (cheaper than `shared_ptr`).
- Constructible from `std::vector<uint8_t>&&`, `std::string&&`, `byte_stream&&`, or scatter-gather `initializer_list<span<const uint8_t>>`.
- **`clone()`**: Cheap shallow copy (increments ref count).
- **`remove_prefix(max_bytes)`**: Drops bytes from the front.
- **`take_slice(max_bytes)`**: Splits off a prefix as a new slice.
- **`get_slice(begin, end)`**: Returns a sub-slice.
- Move-only (no copy constructor).
- Compatible with ZMQ (`release_byte_slice::call` for `zmq_message_init_data`).

#### `byte_stream`

Defined in `contrib/epee/include/byte_stream.h`. A growable write buffer that replaces `std::stringstream`:

- No global `std::locale` acquisition (faster construction than `stringstream`).
- API: `write(ptr, len)`, `put(ch)`, `reserve(more)`, `clear()`, `size()`, `capacity()`, `data()`.
- RapidJSON compatible: `Put()`, `Flush()`, `PutReserve()`, `PutUnsafe()`, `PutN()`.
- **`take_buffer()`**: Transfers ownership of the internal buffer for zero-copy conversion to `byte_slice`.

## Dependencies

Epee is the foundation layer of the Monero codebase. Nearly every component depends on it:

- **`src/cryptonote_core/`**: Uses KV serialization macros, portable storage, logging, spans.
- **`src/cryptonote_protocol/`**: Uses Levin invoke helpers, portable storage, connection contexts.
- **`src/p2p/`**: Uses `boosted_tcp_server`, Levin protocol handler, network address types, connection filtering.
- **`src/rpc/`**: Uses `http_server_impl_base`, HTTP handler maps, JSON/binary serialization helpers.
- **`src/wallet/`**: Uses portable storage, string tools, hex conversion, logging.
- **`src/crypto/`** (indirectly): Uses `span`, `hex`, `int-util`.
- **`src/common/`**: Uses logging, string tools, file I/O.
- **`src/simplewallet/`, `src/daemon/`**: Use HTTP client, logging, console colors.

External dependencies of epee itself:

- **Boost**: `asio`, `thread`, `filesystem`, `algorithm`, `lexical_cast`, `variant`, `uuid`, `regex`, `optional`.
- **easylogging++**: The underlying logging engine (vendored in `external/easylogging++/`).
- **OpenSSL**: For SSL/TLS support in network connections.

## Known Issues

The following TODO, FIXME, HACK, and XXX comments were found in the epee source tree:

| File | Line | Comment |
|------|------|---------|
| `include/storages/portable_storage.h` | 124 | `TODO: don't think i ever again will use xml - ambiguous and "overtagged" format` (XML dump always returns false) |
| `include/storages/portable_storage.h` | 229 | `TODO: optimize code here: work without get_next_val function` |
| `include/storages/portable_storage_from_bin.h` | 205 | `TODO: add some optimization here later` (in `read_ae` array deserialization loop) |
| `include/storages/portable_storage_template_helper.h` | 32 | `TODO: (mj-xmr) This will be reduced in an another PR` (unnecessary `parserse_base_utils.h` include) |
| `include/storages/levin_abstract_invoke2.h` | 64 | `TODO: add true const support to searilzation` (const_cast used to call `store()`) |
| `contrib/epee/src/portable_storage.cpp` | 113 | `TODO:` (bare comment after `return true` in `load_from_binary`) |
| `include/net/abstract_tcp_server2.h` | 533 | `TODO: change to enum server_type, now used` (m_thread_name_prefix is a string) |
| `include/net/abstract_tcp_server2.inl` | 42-43 | `TODO` (comments on `boost::date_time` and `boost::condition_variable` includes) |
| `include/net/network_throttle-detail.hpp` | 62 | `TODO: now hardcoded for 1 second` (slot size in throttle) |
| `include/net/network_throttle-detail.hpp` | 63 | `TODO for big window size, for performance better the substract on change` |
| `include/net/network_throttle-detail.hpp` | 95 | `TODO` (safety note on `get_sleep_time`) |
| `include/net/network_throttle.hpp` | 74 | `TODO later it will be enforced that casts to other numericals are only explicit` |
| `include/net/network_throttle.hpp` | 104 | `XXX` (public access specifier without explanation) |
| `include/net/levin_protocol_handler_async.h` | 700 | `TODO or better just keep removing random elements (performance)` |
| `contrib/epee/src/network_throttle-detail.cpp` | 59 | `TODO:` (bare comment) |
| `contrib/epee/src/network_throttle-detail.cpp` | 168 | `TODO optimize when moving few slots at once` |
| `contrib/epee/src/network_throttle-detail.cpp` | 351 | `TODO 70 => 20` (tuning comment on throttle weights) |
| `contrib/epee/src/connection_basic.cpp` | 46 | `TODO:` (bare comment) |
| `contrib/epee/src/connection_basic.cpp` | 252 | `XXX LATER XXX` |
