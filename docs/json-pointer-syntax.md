# JSON Pointer Syntax

[Link to simple JSON pointer syntax explanation](https://github.com/stephenberry/JSON-Pointer)

Glaze supports JSON pointer syntax access in a C++ context. This is extremely helpful for building generic APIs, which allows values of complex objects to be accessed without needed know the encapsulating class.

```c++
my_struct s{};
auto d = glz::get<double>(s, "/d");
// d.value() is a std::reference_wrapper to d in the structure s
```

```c++
my_struct s{};
glz::set(s, "/d", 42.0);
// d is now 42.0
```

> JSON pointer syntax works with deeply nested objects and anything serializable.

```c++
// Tuple Example
auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
glz::set(tuple, "/0", 5);
expect(std::get<0>(tuple) == 5.0);
```

### read_as

`read_as` allows you to read into an object from a JSON pointer and an input buffer.

```c++
Thing thing{};
glz::read_as_json(thing, "/vec3", "[7.6, 1292.1, 0.333]");
expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 &&
thing.vec3.z == 0.333);

glz::read_as_json(thing, "/vec3/2", "999.9");
expect(thing.vec3.z == 999.9);
```

### get_as_json

`get_as_json` allows you to get a targeted value from within an input buffer. This is especially useful if you need to change how an object is parsed based on a value within the object.

```c++
std::string s = R"({"obj":{"x":5.5}})";
auto z = glz::get_as_json<double, "/obj/x">(s);
expect(z == 5.5);
```

> [!IMPORTANT]
>
> `get_as_json` does not validate JSON beyond the targeted value. This is to avoid parsing the entire document once the targeted value is reached.

### get_sv_json

`get_sv_json` allows you to get a `std::string_view` to a targeted value within an input buffer. This can be more efficient to check values and handle custom parsing than constructing a new value with `get_as_json`.

```c++
std::string s = R"({"obj":{"x":5.5}})";
auto view = glz::get_sv_json<"/obj/x">(s);
expect(view == "5.5");
```

> [!IMPORTANT]
>
> `get_sv_json` does not validate JSON beyond the targeted value. This is to avoid parsing the entire document once the targeted value is reached.

## write_at

`write_at` allows raw text to be written to a specific JSON value pointed at via JSON Pointer syntax.

```c++
std::string buffer = R"( { "action": "DELETE", "data": { "x": 10, "y": 200 }})";
auto ec = glz::write_at<"/action">(R"("GO!")", buffer);
expect(buffer == R"( { "action": "GO!", "data": { "x": 10, "y": 200 }})");
```

Another example:

```c++
std::string buffer = R"({"str":"hello","number":3.14,"sub":{"target":"X"}})";
auto ec = glz::write_at<"/sub/target">("42", buffer);
expect(not ec);
expect(buffer == R"({"str":"hello","number":3.14,"sub":{"target":42}})");
```

## Using JSON Pointers with `glz::generic`

When working with `glz::generic`, JSON pointers can extract both primitives and containers:

```c++
glz::generic json{};
std::string buffer = R"({"names": ["Alice", "Bob"], "count": 2})";
glz::read_json(json, buffer);

// Extract container types - returns expected<T, error_ctx>
auto names = glz::get<std::vector<std::string>>(json, "/names");
if (names) {
  // names->size() == 2, efficient direct conversion
}

// Extract primitives - returns expected<reference_wrapper<T>, error_ctx>
auto count = glz::get<double>(json, "/count");
if (count) {
  // count->get() == 2.0
}
```

See [Generic JSON](./generic-json.md) for more details on the optimized conversion from `generic` to containers and primitives.

## Seek

`glz::seek` allows you to call a lambda on a nested value.

```c++
my_struct s{};
std::any a{};
glz::seek([&](auto& value) { a = value; }, s, "/hello");
expect(a.has_value() && std::any_cast<std::string>(a) == "Hello World");
```
