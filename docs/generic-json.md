# Generic JSON

While Glaze is focused on strongly typed data, there is basic support for completely generic JSON.

If nothing is known about the JSON structure, then [glz::generic](https://github.com/stephenberry/glaze/blob/main/include/glaze/json/generic.hpp) may be helpful, but it comes at a performance cost due to the use of dynamic memory allocations. The previous `glz::json_t` name remains as an alias for backwards compatibility.

## Generic Type Variants

Glaze provides three generic JSON types with different number storage strategies:

| Type | Number Storage | Use Case |
|------|---------------|----------|
| `glz::generic` | `double` | Fast, JavaScript-compatible (default) |
| `glz::generic_i64` | `int64_t` then `double` | Signed integer precision |
| `glz::generic_u64` | `uint64_t` then `int64_t` then `double` | Full integer range |

### glz::generic (Default)

All numbers are stored as `double`. This is the fastest option and matches JavaScript's number semantics. However, integers larger than 2^53 will lose precision.

```c++
glz::generic json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[0].get<double>() == 5.0);
assert(json[1].get<std::string>() == "Hello World");
assert(json[2]["pi"].get<double>() == 3.14);
assert(json[2]["pi"].as<int>() == 3);
```

### glz::generic_i64

Integers are stored as `int64_t`, with fallback to `double` for floating-point numbers. This preserves precision for signed integers up to 9.2 quintillion (2^63-1).

```c++
glz::generic_i64 json{};
std::string buffer = R"({"id": 9007199254740993, "value": 3.14})";
glz::read_json(json, buffer);

// Large integer preserved exactly (would lose precision in double)
assert(json["id"].is_int64());
assert(json["id"].get<int64_t>() == 9007199254740993LL);

// Floating-point stored as double
assert(json["value"].is_double());
assert(json["value"].get<double>() == 3.14);

// Use as<T>() for conversions
assert(json["id"].as<double>() == 9007199254740993.0);
```

### glz::generic_u64

Provides full integer range support. Positive integers are stored as `uint64_t`, negative integers as `int64_t`, and floating-point numbers as `double`. This handles the complete unsigned 64-bit range (0 to 18.4 quintillion).

```c++
glz::generic_u64 json{};
std::string buffer = R"({"big_id": 18446744073709551615, "neg": -100, "pi": 3.14})";
glz::read_json(json, buffer);

// Maximum uint64_t value preserved exactly
assert(json["big_id"].is_uint64());
assert(json["big_id"].get<uint64_t>() == 18446744073709551615ULL);

// Negative integers stored as int64_t
assert(json["neg"].is_int64());
assert(json["neg"].get<int64_t>() == -100);

// Floating-point stored as double
assert(json["pi"].is_double());
assert(json["pi"].get<double>() == 3.14);
```

### Choosing the Right Type

- **`glz::generic`**: Use when performance matters most and you don't need large integer precision, or when interoperating with JavaScript.
- **`glz::generic_i64`**: Use when dealing with signed 64-bit IDs, timestamps, or APIs that return large signed integers.
- **`glz::generic_u64`**: Use when handling database IDs, blockchain values, or any unsigned 64-bit integers.

## Basic Usage

```c++
glz::generic json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[0].get<double>() == 5.0);
assert(json[1].get<std::string>() == "Hello World");
assert(json[2]["pi"].get<double>() == 3.14);
assert(json[2]["pi"].as<int>() == 3);
```

```c++
glz::generic json = {
   {"pi", 3.141},
   {"happy", true},
   {"name", "Stephen"},
   {"nothing", nullptr},
   {"answer", {{"everything", 42.0}}},
   {"list", {1.0, 0.0, 2.0}},
   {"object", {
      {"currency", "USD"},
      {"value", 42.99}
   }}
};
std::string buffer{};
glz::write_json(json, buffer);
expect(buffer == R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Stephen","object":{"currency":"USD","value":42.99},"pi":3.141})");
```

## get() vs as()

All generic types are variants underneath. The `get<T>()` method mimics a `std::get` call for a variant, which rejects conversions and throws if the type doesn't match. The `as<T>()` method performs conversions.

```c++
glz::generic json{};
glz::read_json(json, "3.14");

json.get<double>();  // 3.14 - exact type match
json.as<int>();      // 3 - converts double to int
```

For `generic_i64` and `generic_u64`, `as<T>()` handles conversions from integer storage:

```c++
glz::generic_i64 json{};
glz::read_json(json, "42");

json.get<int64_t>();  // 42 - stored as int64_t
json.as<double>();    // 42.0 - converts to double
json.as<int>();       // 42 - converts to int
```

## Type Checking generic

All generic types have member functions to check the JSON type:

- `.is_object()`
- `.is_array()`
- `.is_string()`
- `.is_number()` - returns true for any numeric type
- `.is_boolean()`
- `.is_null()`

There are also free functions of these, such as `glz::is_object(...)`

### Additional Type Checks for Integer Modes

`glz::generic_i64` and `glz::generic_u64` provide additional methods:

- `.is_int64()` - true if stored as `int64_t`
- `.is_double()` - true if stored as `double`
- `.is_uint64()` - true if stored as `uint64_t` (only `generic_u64`)
- `.as_number()` - returns the value as `double`, converting if necessary

```c++
glz::generic_i64 json{};
glz::read_json(json, "42");

json.is_number();  // true - it's some kind of number
json.is_int64();   // true - specifically stored as int64_t
json.is_double();  // false - not stored as double

double val = json.as_number();  // 42.0 - converts int64_t to double
```

## .empty()

Calling `.empty()` on a `generic` value will return true if it contains an empty object, array, or string, or a null value. Otherwise, returns false.

## .size()

Calling `.size()` on a `generic` value will return the number of items in an object or array, or the size of a string. Otherwise, returns zero.

## .dump()

Calling `.dump()` on a `generic` value is equivalent to calling `glz::write_json(value)`, which returns an `expected<std::string, glz::error_ctx>`.

## glz::raw_json

There are times when you want to parse JSON into a C++ string, to inspect or decode at a later point. `glz::raw_json` is a simple wrapper around a `std::string` that will decode and encode JSON without needing a concrete structure.

```c++
std::vector<glz::raw_json> v{"0", "1", "2"};
std::string s;
glz::write_json(v, s);
expect(s == R"([0,1,2])");
```

## Using `generic` As The Source

After parsing into a `generic` it is sometimes desirable to parse into a concrete struct or a portion of the `generic` into a struct. Glaze allows a `generic` value to be used as the source where a buffer would normally be passed.

```c++
auto json = glz::read_json<glz::generic>(R"({"foo":"bar"})");
expect(json->contains("foo"));
auto obj = glz::read_json<std::map<std::string, std::string>>(json.value());
// This reads the generic value into a std::map
```

**Performance Note**: When reading primitives or containers (vectors, maps, arrays, etc.) from `generic`, Glaze uses an optimized direct-traversal path that avoids JSON serialization. For complex user-defined structs, it falls back to JSON round-trip to handle reflection metadata.

Another example:

```c++
glz::generic json{};
expect(not glz::read_json(json, R"("Beautiful beginning")"));
std::string v{};
expect(not glz::read<glz::opts{}>(v, json));
expect(v == "Beautiful beginning");
```

### Optimized Conversion from `generic`

When reading from a `generic` into primitives or containers, `glz::read_json` automatically uses an optimized direct-traversal path:

```c++
glz::generic json{};
glz::read_json(json, R"([1, 2, 3, 4, 5])");

// Efficient direct conversion - no JSON serialization overhead
std::vector<int> vec;
auto ec = glz::read_json(vec, json);
if (!ec) {
  // vec now contains {1, 2, 3, 4, 5}
}
```

The optimization automatically applies to:
- **Primitives**: `bool`, `double`, `int`, and other numeric types
- **Strings**: `std::string`
- **Arrays**: `std::vector`, `std::array`, `std::deque`, `std::list`
- **Maps**: `std::map`, `std::unordered_map`
- **Nested combinations**: `std::vector<std::vector<int>>`, `std::map<std::string, std::vector<double>>`, etc.

Benefits of the optimized path:
- **No JSON round-trip**: Converts directly from the internal `generic` representation
- **Memory reuse**: Existing allocations in the target container are preserved
- **Recursive efficiency**: Nested containers are converted with zero intermediate serialization

For complex user-defined structs, `glz::read_json` automatically falls back to JSON round-trip to handle reflection metadata.

**Advanced**: If you need explicit control over the conversion process, `glz::convert_from_generic(result, source)` is available as a lower-level API that returns `error_ctx` instead of using the `expected` wrapper.

## Extracting Containers with JSON Pointers

`glz::get` can be used with JSON Pointers to extract values from a `generic` object. For primitive types (bool, double, std::string), `glz::get` returns a reference wrapper to the value stored in the `generic`:

```c++
glz::generic json{};
std::string buffer = R"({"name": "Alice", "age": 30, "active": true})";
glz::read_json(json, buffer);

// Get primitive types - returns expected<reference_wrapper<T>, error_ctx>
auto name = glz::get<std::string>(json, "/name");
if (name) {
  expect(name->get() == "Alice");
}

auto age = glz::get<double>(json, "/age");
if (age) {
  expect(age->get() == 30.0);
}
```

For container types (vectors, maps, arrays, lists, etc.), `glz::get` deserializes the value and returns a copy:

```c++
glz::generic json{};
std::string buffer = R"({
  "names": ["Alice", "Bob", "Charlie"],
  "scores": {"math": 95, "english": 87}
})";
glz::read_json(json, buffer);

// Get container types - returns expected<T, error_ctx>
auto names = glz::get<std::vector<std::string>>(json, "/names");
if (names) {
  expect(names->size() == 3);
  expect((*names)[0] == "Alice");
}

auto scores = glz::get<std::map<std::string, int>>(json, "/scores");
if (scores) {
  expect(scores->at("math") == 95);
}

// Works with other container types too
auto names_list = glz::get<std::list<std::string>>(json, "/names");
auto names_array = glz::get<std::array<std::string, 3>>(json, "/names");
```

This works because `glz::generic` stores arrays as `std::vector<glz::generic>` and objects as `std::map<std::string, glz::generic>`. When you request a specific container type, Glaze deserializes the generic representation into your desired type.

## See Also

- [JSON Patch (RFC 6902)](./json-patch.md) - Apply structured patches to `glz::generic` documents
- [JSON Merge Patch (RFC 7386)](./json-merge-patch.md) - Apply partial updates to `glz::generic` documents
- [JSON Pointer Syntax](./json-pointer-syntax.md) - Path syntax for navigating JSON documents
