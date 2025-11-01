# MessagePack

Glaze ships with first–class MessagePack support. You can read and write payloads using
the same reflection metadata that drives JSON, BEVE, and TOML so there is no extra boilerplate.

```cpp
#include "glaze/msgpack.hpp"

struct point
{
   double x{};
   double y{};
};

template <> struct glz::meta<point>
{
   using T = point;
   static constexpr auto value = object(&T::x, &T::y);
};

point p{.x = 4.2, .y = 1.3};
std::string buffer{};

// Write MessagePack into a std::string
auto write_error = glz::write_msgpack(p, buffer);
if (write_error) {
   const auto message = glz::format_error(write_error, buffer);
   // handle serialization failure
}

// Read the buffer back
point decoded{};
auto read_error = glz::read_msgpack(decoded, std::string_view{buffer});
if (read_error) {
   const auto message = glz::format_error(read_error, buffer);
   // handle parse problems
}
```

`glz::write_msgpack` and `glz::read_msgpack` mirror the JSON helpers:

- They work with any reflected type, STL container, or custom specialization.
- Overloads exist for output buffers (`std::string`, `std::vector<std::byte>`, `std::vector<char>`, etc.).
- The return type is an `error_ctx`. A default–constructed context means success.

## `glz::msgpack::ext`

MessagePack “ext” values are supported through `glz::msgpack::ext`. The struct stores the type code and
raw payload bytes. You can embed it inside your objects or work with it directly.

```cpp
glz::msgpack::ext payload{5, {std::byte{0xCA}, std::byte{0xFE}}};
auto encoded = glz::write_msgpack(payload);

if (encoded) {
   auto decoded = glz::read_msgpack<glz::msgpack::ext>(std::string_view{encoded.value()});
   // decoded->type == 5, decoded->data == {0xCA, 0xFE}
}
```

## Binary Buffers

Glaze automatically emits the compact MessagePack `bin*` tags for contiguous byte buffers such as
`std::vector<std::byte>`, `std::vector<unsigned char>`, or `std::span<std::byte>`. Reads follow the same path,
so you can round–trip arbitrary binary payloads without extra adapters.

```cpp
std::vector<std::byte> blob{std::byte{0x00}, std::byte{0x7F}};
auto encoded = glz::write_msgpack(blob);

std::vector<std::byte> restored;
auto ec = glz::read_msgpack(restored, std::string_view{encoded.value()});
```

## Partial Write and Partial Read

The generic options system works for MessagePack as well. You can serialize only a subset of fields by
leveraging JSON pointers, or short–circuit deserialization with `.partial_read`:

```cpp
struct user {
   std::string name;
   int64_t id{};
   bool active{};
};

template <> struct glz::meta<user>
{
   using T = user;
   static constexpr auto value = object(&T::name, &T::id, &T::active);
};

static constexpr auto partial = glz::json_ptrs("/name");

user u{.name = "Bailey", .id = 42, .active = true};
auto encoded = glz::write_msgpack<partial>(u);

user decoded{.id = 99};
auto ec = glz::read_msgpack(decoded, std::string_view{encoded.value()});
// decoded.name is populated, id remains 99, active stays default.
```

Partial reading reuses the `glz::opts` struct:

```cpp
auto ec = glz::read<glz::opts{.format = glz::MSGPACK, .partial_read = true}>(decoded, std::string_view{payload});
```

Glaze updates only the members present in the MessagePack map and stops parsing once every tracked field has been visited, which is helpful when you only need a few keys from a large document.

## File Helpers

Use `glz::write_file_msgpack` and `glz::read_file_msgpack` when you want to work with files. The helpers reuse the
same error reporting pipeline as JSON/TOML:

```cpp
std::vector<std::byte> buffer{};
glz::write_file_msgpack(u, "user.bin", buffer);

user restored{};
glz::read_file_msgpack(restored, "user.bin", buffer);
```

Because the helpers accept any contiguous buffer, you can reuse the same `std::vector<std::byte>` for both operations to avoid extra allocations.

## Low-Level API

When you need more control, the generic `glz::read`/`glz::write` templates accept `opts` with `.format = glz::MSGPACK`.
This unlocks advanced scenarios such as:

- Disabling unknown-key errors (`.error_on_unknown_keys = false`)
- Structs-as-arrays (`.structs_as_arrays = true`)
- Enabling partial reads as shown earlier

```cpp
static constexpr glz::opts permissive{.format = glz::MSGPACK, .error_on_unknown_keys = false};
user flexible{};
glz::read<permissive>(flexible, std::string_view{payload});
```

The MessagePack reader/writer plug into the same core pipeline as the JSON and BEVE backends, so hooks such as custom read/write functions or wrappers continue to work.

