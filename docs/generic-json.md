# Generic JSON

While Glaze is focused on strongly typed data, there is basic support for completely generic JSON.

If absolutely nothing is known about the JSON structure, then [glz::json_t](https://github.com/stephenberry/glaze/blob/main/include/glaze/json/json_t.hpp) may be helpful, but it comes at a performance cost due to the use of dynamic memory allocations.

```c++
glz::json_t json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[0].get<double>() == 5.0);
assert(json[1].get<std::string>() == "Hello World");
assert(json[2]["pi"].get<double>() == 3.14);
assert(json[2]["pi"].as<int>() == 3);
```

```c++
glz::json_t json = {
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

`glz::json_t` is a variant underneath that stores all numbers in `double`. The `get()` method mimics a `std::get` call for a variant, which rejects conversions. If you want to access a number as an `int`, then call `json.as<int>()`, which will cast the `double` to an `int`.

## Type Checking json_t

`glz::json_t` has member functions to check the JSON type:

- `.is_object()`
- `.is_array()`
- `.is_string()`
- `.is_number()`
- `.is_null()`

There are also free functions of these, such as `glz::is_object(...)`

## .empty()

Calling `.empty()` on a `json_t` value will return true if it contains an empty object, array, or string, or a null value. Otherwise, returns false.

## .size()

Calling `.size()` on a `json_t` value will return the number of items in an object or array, or the size of a string. Otherwise, returns zero.

## .dump()

Calling `.dump()` on a `json_t` value is equivalent to calling `glz::write_json(value)`, which returns an `expected<std::string, glz::error_ctx>`.

## glz::raw_json

There are times when you want to parse JSON into a C++ string, to inspect or decode at a later point. `glz::raw_json` is a simple wrapper around a `std::string` that will decode and encode JSON without needing a concrete structure.

```c++
std::vector<glz::raw_json> v{"0", "1", "2"};
std::string s;
glz::write_json(v, s);
expect(s == R"([0,1,2])");
```

## Using `json_t` As The Source

After parsing into a `json_t` it is sometimes desirable to parse into a concrete struct or a portion of the `json_t` into a struct. Glaze allows a `json_t` value to be used as the source where a buffer would normally be passed.

```c++
auto json = glz::read_json<glz::json_t>(R"({"foo":"bar"})");
expect(json->contains("foo"));
auto obj = glz::read_json<std::map<std::string, std::string>>(json.value());
// This reads the json_t into a std::map
```

Another example:

```c++
glz::json_t json{};
expect(not glz::read_json(json, R"("Beautiful beginning")"));
std::string v{};
expect(not glz::read<glz::opts{}>(v, json));
expect(v == "Beautiful beginning");
```

