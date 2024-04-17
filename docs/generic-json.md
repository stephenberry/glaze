# Generic JSON

While Glaze is focused on strongly typed data, there is basic support for completely generic JSON.

If absolutely nothing is known about the JSON structure, then [glz::json_t](https://github.com/stephenberry/glaze/blob/main/include/glaze/json/json_t.hpp) may be helpful, but it comes at a hefty performance cost due to the heavy use of dynamic memory allocations.

```c++
glz::json_t json{};
std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
glz::read_json(json, buffer);
assert(json[0].get<double>() == 5.0);
assert(json[1].get<std::string>() == "Hello World");
assert(json[2]["pi"].get<double>() == 3.14);
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

## glz::raw_json

There are times when you want to parse JSON into a C++ string, to inspect or decode at a later point. `glz::raw_json` is a simple wrapper around a `std::string` that will decode and encode JSON without needing a concrete structure.

```c++
std::vector<glz::raw_json> v{"0", "1", "2"};
std::string s;
glz::write_json(v, s);
expect(s == R"([0,1,2])");
```

