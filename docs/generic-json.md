# Generic JSON

While Glaze is focused on strongly typed data, there is basic support for completely generic JSON.

If absolutely nothing is known about the JSON structure, then [glz::generic](https://github.com/stephenberry/glaze/blob/main/include/glaze/json/generic.hpp) may be helpful, but it comes at a performance cost due to the use of dynamic memory allocations. The previous `glz::json_t` name remains as an alias for backwards compatibility (`glaze_v6_0_0_generic_header` is defined when this deprecation is available).

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

`glz::generic` is a variant underneath that stores all numbers in `double`. The `get()` method mimics a `std::get` call for a variant, which rejects conversions. If you want to access a number as an `int`, then call `json.as<int>()`, which will cast the `double` to an `int`.

## Type Checking generic

`glz::generic` has member functions to check the JSON type:

- `.is_object()`
- `.is_array()`
- `.is_string()`
- `.is_number()`
- `.is_null()`

There are also free functions of these, such as `glz::is_object(...)`

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
