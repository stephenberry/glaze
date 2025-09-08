# Custom Wrappers

Custom wrappers are used to change the serialization/deserialization on basic types (e.g. int, double, std::string).

The example below shows how numbers can be read in via strings by providing a wrapper to indicate the proper serialization/deserialization.

```c++
template <class T>
struct quoted_helper
{
   T& val;
};

namespace glz
{
   template <class T>
   struct from<JSON, quoted_helper<T>>
   {
      template <auto Opts>
      static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
      {
         skip_ws<Opts>(ctx, it, end);
         if (match<'"'>(ctx, it)) {
            return;
         }
         parse<JSON>::op<Opts>(value.val, ctx, it, end);
         if (match<'"'>(ctx, it)) {
            return;
         }
      }
   };
   
   template <class T>
   struct to<JSON, quoted_helper<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
      {
         dump<'"'>(b, ix);
         serialize<JSON>::op<Opts>(value.val, ctx, b, ix);
         dump<'"'>(b, ix);
      }
   };
}

template <auto MemPtr>
constexpr decltype(auto) qouted()
{
   return [](auto&& val) { return quoted_helper<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
}

struct A
{
   double x;
};

template <>
struct glz::meta<A>
{
  static constexpr auto value = object("x", qouted<&A::x>());
  // or...
  //static constexpr auto value = object("x", [](auto&& val) { return quoted_helper(val.x); });
};

void example() {
  A a{3.14};
  std::string buffer{};
  expect(not glz::write_json(a, buffer));
  expect(buffer == R"({"x":"3.14"})");

  buffer = R"({"x":"999.2"})";
  expect(not glz::read_json(a, buffer));
  expect(a.x == 999.2);
}
```

## Custom Type Parsing

Custom wrappers allow you to define specialized serialization/deserialization behavior for specific types. This is useful when working with formats that differ from standard JSON representations.

### Example: Python-style Booleans

Some systems (like Python) represent booleans as `True`/`False` instead of JSON's standard `true`/`false`. Here's how to create a custom wrapper to handle this format:

#### Step 1: Define the Wrapper Helper

```c++
template <class T>
struct python_bool_helper
{
   T& val;  // Reference to the actual boolean value
};
```

#### Step 2: Specialize the Parser (`from`)

```c++
namespace glz
{
   template <class T>
   struct from<JSON, python_bool_helper<T>>
   {
      template <auto Opts>
      static void op(auto&& value, auto&& ctx, auto&& it, auto&& end)
      {
         skip_ws<Opts>(ctx, it, end);
         
         if (it >= end) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         
         // Check for 'T' (True) or 'F' (False)
         if (*it == 'T') {
            if (it + 4 <= end && std::string_view(it, 4) == "True") {
               value.val = true;
               it += 4;
            } else {
               ctx.error = error_code::expected_true_or_false;
            }
         } else if (*it == 'F') {
            if (it + 5 <= end && std::string_view(it, 5) == "False") {
               value.val = false;
               it += 5;
            } else {
               ctx.error = error_code::expected_true_or_false;
            }
         } else {
            ctx.error = error_code::expected_true_or_false;
         }
      }
   };
```

#### Step 3: Specialize the Serializer (`to`)

```c++
   template <class T>
   struct to<JSON, python_bool_helper<T>>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
      {
         if (value.val) {
            std::memcpy(&b[ix], "True", 4);
            ix += 4;
         } else {
            std::memcpy(&b[ix], "False", 5);
            ix += 5;
         }
      }
   };
}
```

#### Step 4: Create a Convenience Function

```c++
template <auto MemPtr>
constexpr decltype(auto) python_bool()
{
   return [](auto&& val) { 
      return python_bool_helper<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; 
   };
}
```

#### Step 5: Use in Your Structs

```c++
struct ConfigData
{
   std::string name;
   int port;
   bool debug_mode;
   bool enable_logging;
};

template <>
struct glz::meta<ConfigData>
{
   using T = ConfigData;
   static constexpr auto value = object(
      &T::name,
      &T::port,
      "debug_mode", python_bool<&T::debug_mode>(),      // Custom wrapper for this field
      "enable_logging", python_bool<&T::enable_logging>() // Custom wrapper for this field
   );
};
```

#### Usage Example

```c++
ConfigData config{.name = "MyApp", .port = 8080, .debug_mode = true, .enable_logging = false};

// Writing produces: {"name":"MyApp","port":8080,"debug_mode":True,"enable_logging":False}
std::string json_output;
glz::write_json(config, json_output);

// Reading accepts Python-style booleans
std::string python_json = R"({"name":"TestApp","port":3000,"debug_mode":False,"enable_logging":True})";
ConfigData parsed_config;
glz::read_json(parsed_config, python_json);
```

### Alternative Approaches

1. **Custom Type Wrapper**: Create your own boolean type with `glz::to`/`glz::from` specializations, allowing pure reflection usage
2. **Global Override**: Override boolean parsing globally (affects all boolean fields)

The custom wrapper approach shown above provides the most flexibility and performance, allowing field-by-field control over serialization behavior.
