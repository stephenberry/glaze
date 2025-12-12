# JSON Pointer Operations for Structs

Glaze provides JSON Pointer (RFC 6901) operations that work directly with C++ structs and typed data. These operations provide patch-like functionality at compile time with full type safety.

> For runtime JSON manipulation with full RFC 6902 JSON Patch support, see [JSON Patch](./json-patch.md) which works with `glz::generic`.

## Overview

| Operation | Description | Struct Support |
|-----------|-------------|----------------|
| `glz::get` | Read a value at a path | Yes |
| `glz::set` | Write a value at a path | Yes |
| `glz::seek` | Call a function on a value at a path | Yes |
| `glz::read_as_json` | Parse JSON into a specific path | Yes |
| `glz::write_at` | Write raw JSON at a specific path | Yes |

## get - Read Values

### get (reference)

Returns a reference wrapper to the value at the specified path:

```c++
struct Person {
   std::string name;
   int age;
};

Person p{"Alice", 30};

auto name = glz::get<std::string>(p, "/name");
if (name) {
   // name->get() == "Alice"
   name->get() = "Bob";  // Can modify through reference
}

auto age = glz::get<int>(p, "/age");
if (age) {
   // age->get() == 30
}
```

### get_if (pointer)

Returns a pointer to the value, or `nullptr` if not found or wrong type:

```c++
Person p{"Alice", 30};

if (auto* name = glz::get_if<std::string>(p, "/name")) {
   // *name == "Alice"
}

// Returns nullptr for wrong type
auto* wrong = glz::get_if<int>(p, "/name");  // nullptr
```

### get_value (copy)

Returns a copy of the value:

```c++
Person p{"Alice", 30};

auto age = glz::get_value<int>(p, "/age");
if (age) {
   // *age == 30 (copy, not reference)
}
```

## set - Write Values

Assigns a value at the specified path:

```c++
struct Config {
   int timeout;
   std::string host;
   struct {
      int port;
      bool ssl;
   } server;
};

Config cfg{30, "localhost", {8080, false}};

glz::set(cfg, "/timeout", 60);
glz::set(cfg, "/host", std::string("example.com"));
glz::set(cfg, "/server/port", 443);
glz::set(cfg, "/server/ssl", true);
```

`set` returns `true` if the assignment succeeded, `false` if the path doesn't exist or types are incompatible.

## seek - Generic Access

Calls a lambda on the value at the specified path. Useful when you don't know the type at compile time:

```c++
struct Data {
   int x;
   double y;
   std::string z;
};

Data d{1, 2.5, "hello"};

std::any result;
glz::seek([&](auto& value) {
   result = value;
}, d, "/y");
// result contains 2.5
```

## Nested Structures

All operations work with arbitrarily nested structures:

```c++
struct Inner {
   int value;
};

struct Middle {
   Inner inner;
   std::vector<int> items;
};

struct Outer {
   Middle middle;
   std::map<std::string, int> scores;
};

Outer obj{{{{42}, {1, 2, 3}}, {{"math", 95}}}};

// Access nested struct
auto val = glz::get<int>(obj, "/middle/inner/value");  // 42

// Access vector element
auto item = glz::get<int>(obj, "/middle/items/1");  // 2

// Access map value
auto score = glz::get<int>(obj, "/scores/math");  // 95

// Set nested values
glz::set(obj, "/middle/inner/value", 100);
glz::set(obj, "/middle/items/0", 10);
```

## Working with JSON Buffers

### read_as_json

Parse JSON directly into a specific path within a struct:

```c++
struct Scene {
   struct {
      double x, y, z;
   } position;
   std::string name;
};

Scene scene{};

// Parse JSON into a specific location
glz::read_as_json(scene, "/position", R"({"x": 1.0, "y": 2.0, "z": 3.0})");
glz::read_as_json(scene, "/name", R"("Main Camera")");
```

### get_as_json

Extract a typed value from a JSON buffer at a specific path:

```c++
std::string json = R"({"user": {"name": "Alice", "age": 30}})";

auto name = glz::get_as_json<std::string, "/user/name">(json);
// name == "Alice"

auto age = glz::get_as_json<int, "/user/age">(json);
// age == 30
```

### get_sv_json

Get a `std::string_view` to the raw JSON at a path (no parsing):

```c++
std::string json = R"({"data": {"values": [1, 2, 3]}})";

auto view = glz::get_sv_json<"/data/values">(json);
// view == "[1, 2, 3]"
```

### write_at

Replace JSON at a specific path within a buffer:

```c++
std::string json = R"({"action": "pending", "count": 0})";

glz::write_at<"/action">(R"("complete")", json);
glz::write_at<"/count">("42", json);
// json == {"action": "complete", "count": 42}
```

## Comparison with JSON Patch

| Feature | Struct Operations | JSON Patch (glz::generic) |
|---------|------------------|---------------------------|
| Type safety | Compile-time | Runtime |
| Add new keys | No (struct is fixed) | Yes |
| Remove keys | No (struct is fixed) | Yes |
| Replace values | Yes (`glz::set`) | Yes |
| Move/Copy | No | Yes |
| Test operation | No | Yes |
| Batch operations | Manual loop | Atomic transaction |

### When to Use Each

**Use struct operations when:**
- You have a known schema at compile time
- You need maximum performance
- Type safety is important
- Structure is fixed (no dynamic keys)

**Use JSON Patch (`glz::generic`) when:**
- Schema is unknown or dynamic
- You need full RFC 6902 compliance
- You need to add/remove keys at runtime
- You're processing patches from external sources
- You need atomic batch operations with rollback

## Error Handling

Most operations return `expected` or `bool` to indicate success:

```c++
struct Data { int x; };
Data d{42};

// Path doesn't exist
auto result = glz::get<int>(d, "/nonexistent");
if (!result) {
   // result.error().ec == glz::error_code::nonexistent_json_ptr
}

// Wrong type
auto wrong = glz::get<std::string>(d, "/x");
if (!wrong) {
   // wrong.error().ec == glz::error_code::get_wrong_type
}

// set returns bool
bool ok = glz::set(d, "/x", 100);  // true
bool fail = glz::set(d, "/y", 0);  // false (path doesn't exist)
```

## See Also

- [JSON Pointer Syntax](./json-pointer-syntax.md) - Full JSON Pointer documentation
- [JSON Patch](./json-patch.md) - RFC 6902 JSON Patch for `glz::generic`
- [Generic JSON](./generic-json.md) - Working with dynamic JSON
