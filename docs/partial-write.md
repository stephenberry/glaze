# Partial Write

Glaze supports partial object writing, allowing you to serialize only specific fields. There are two approaches:

1. **Compile-time partial write** - Uses JSON pointers as template parameters (zero runtime overhead)
2. **Runtime partial write** - Specify keys dynamically at runtime

## Compile-Time Partial Write

Use `glz::json_ptrs` to specify JSON pointer paths at compile time. This approach supports nested paths and has zero runtime overhead.

```c++
struct animals_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";

   struct glaze
   {
      using T = animals_t;
      static constexpr auto value = glz::object(&T::lion, &T::tiger, &T::panda);
   };
};

struct zoo_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};

   struct glaze
   {
      using T = zoo_t;
      static constexpr auto value = glz::object(&T::animals, &T::name);
   };
};

"partial write"_test = [] {
    static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

    zoo_t obj{};
    std::string s{};
    const auto ec = glz::write_json<partial>(obj, s);
    expect(!ec);
    expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
 };
```

## Runtime Partial Write

Use `glz::write_json_partial` to specify keys dynamically at runtime. This is useful when the set of keys to serialize is determined at runtime (e.g., based on user input, configuration, or message type).

### Basic Usage

```c++
struct my_struct
{
   int x = 10;
   std::string name = "example";
   double value = 3.14;
   bool active = true;
};

my_struct obj{};
std::string buffer;

// Specify keys at runtime
std::vector<std::string> keys = {"name", "x"};
auto ec = glz::write_json_partial(obj, keys, buffer);
// Result: {"name":"example","x":10}
```

### Key Order

The output key order matches the order of keys in your input container:

```c++
std::vector<std::string_view> keys = {"value", "name", "x"};
glz::write_json_partial(obj, keys, buffer);
// Result: {"value":3.14,"name":"example","x":10}
```

### Supported Key Containers

Any range of string-like types works:

```c++
// std::vector<std::string>
std::vector<std::string> keys1 = {"x", "name"};

// std::vector<std::string_view>
std::vector<std::string_view> keys2 = {"x", "name"};

// std::array
std::array<std::string_view, 2> keys3 = {"x", "name"};
```

### Error Handling

If a key doesn't exist in the struct, `error_code::unknown_key` is returned:

```c++
std::vector<std::string> keys = {"x", "nonexistent"};
auto ec = glz::write_json_partial(obj, keys, buffer);
if (ec.ec == glz::error_code::unknown_key) {
    // Handle unknown key error
}
```

### Empty Keys

An empty key container produces an empty JSON object:

```c++
std::vector<std::string> keys = {};
glz::write_json_partial(obj, keys, buffer);
// Result: {}
```

### Duplicate Keys

Duplicate keys are allowed and will write the field multiple times:

```c++
std::vector<std::string> keys = {"x", "x"};
glz::write_json_partial(obj, keys, buffer);
// Result: {"x":10,"x":10}
```

### Options Support

Runtime partial write supports standard Glaze options like `prettify`:

```c++
std::vector<std::string> keys = {"x", "name"};
glz::write_json_partial<glz::opts{.prettify = true}>(obj, keys, buffer);
// Result:
// {
//    "x": 10,
//    "name": "example"
// }
```

### Return Types

Three overloads are available:

```c++
// 1. Write to resizable buffer (std::string, std::vector<char>)
error_ctx write_json_partial(T&& value, const Keys& keys, Buffer&& buffer);

// 2. Write to raw buffer (char*)
expected<size_t, error_ctx> write_json_partial(T&& value, const Keys& keys, Buffer&& buffer);

// 3. Return a new string
expected<std::string, error_ctx> write_json_partial(T&& value, const Keys& keys);
```

### Works with Reflectable Types

Runtime partial write works with both explicit Glaze metadata and pure reflection (C++ aggregates):

```c++
// No glz::meta needed - pure reflection
struct reflectable_struct
{
   int field1 = 100;
   std::string field2 = "test";
};

reflectable_struct obj{};
std::vector<std::string> keys = {"field2"};
glz::write_json_partial(obj, keys, buffer);
// Result: {"field2":"test"}
```

## Choosing Between Compile-Time and Runtime

| Feature | Compile-Time (`write_json<partial>`) | Runtime (`write_json_partial`) |
|---------|--------------------------------------|--------------------------------|
| Key specification | `constexpr` JSON pointers | Runtime container |
| Nested paths | Supported (`/animals/tiger`) | Top-level keys only |
| Performance | Zero overhead | Hash lookup per key |
| Use case | Fixed field sets | Dynamic field selection |

Use **compile-time partial write** when:
- The fields to serialize are known at compile time
- You need nested JSON pointer paths
- Maximum performance is critical

Use **runtime partial write** when:
- The fields to serialize depend on runtime conditions
- Different message types need different field subsets
- User configuration determines which fields to include

