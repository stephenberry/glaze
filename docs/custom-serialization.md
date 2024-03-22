# Custom Serialization/Deserialization

See [Custom Read/Write](https://github.com/stephenberry/glaze/tree/main#custom-readwrite) in the main README.md for using member functions for custom handling.

Advanced custom serialization/deserialization is achievable by implementing your own `from_json` and `to_json` specializations within the glz::detail namespace.

> Note that the API within the `detail` namespace is subject to change and is therefore not expected to be as stable.

Example:
```c++
struct date
{
   uint64_t data;
   std::string human_readable;
};

template <>
struct glz::meta<date>
{
   using T = date;
   static constexpr auto value = object("date", &T::human_readable);
};

namespace glz::detail
{
   template <>
   struct from_json<date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args)
      {
         read<json>::op<Opts>(value.human_readable, args...);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <>
   struct to_json<date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args) noexcept
      {
         value.human_readable = std::to_string(value.data);
         write<json>::op<Opts>(value.human_readable, args...);
      }
   };
}

void example() {
  date d{};
  d.data = 55;

  std::string s{};
  glz::write_json(d, s);

  expect(s == R"("55")");

  d.data = 0;
  glz::read_json(d, s);
  expect(d.data == 55);
}
```

Notes:

The templated `Opts` parameter contains the compile time options. The `args...` can be broken out into the runtime context (runtime options), iterators, and the buffer index when reading.

## UUID Example

Say we have a UUID library for converting a `uuid_t` from a `std::string_view` and to a `std::string`.

```c++
namespace glz::detail
{
   template <>
   struct from_json<uuid_t>
   {
      template <auto Opts>
      static void op(uuid_t& uuid, auto&&... args)
      {
         // Initialize a string_view with the appropriately lengthed buffer
         // Alternatively, use a std::string for any size (but this will allocate)
         std::string_view str = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
         read<json>::op<Opts>(str, args...);
         uuid = uuid_lib::uuid_from_string_view(str);
      }
   };

   template <>
   struct to_json<uuid_t>
   {
      template <auto Opts>
      static void op(const uuid_t& uuid, auto&&... args) noexcept
      {
         std::string str = uuid_lib::uuid_to_string(uuid);
         write<json>::op<Opts>(str, args...);
      }
   };
}
```

# Handling Ambiguous Partial Specialization

You may want to custom parse a class that matches an underlying glaze partial specialization for a template like below:

```c++
template <class T> requires readable_array_t<T>
struct from_json<T>
```

If your own parsing function desires partial template specialization, then ambiguity may occur:

```c++
template <class T> requires std::derived_from<T, vec_t>
struct from_json<T>
```

To solve this problem, glaze will check for `custom_read` or `custom_write` values within `glz::meta` and remove the ambiguity and use the custom parser.

```c++
template <class T> requires std::derived_from<T, vec_t>
struct glz::meta<T>
{
   static constexpr auto custom_read = true;
};
```

