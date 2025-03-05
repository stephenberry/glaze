# Custom Serialization/Deserialization

See [Custom Read/Write](https://github.com/stephenberry/glaze/tree/main#custom-readwrite) in the main README.md for using `glz::custom`.

Advanced custom serialization/deserialization is achievable by implementing your own `from` and `to` specializations within the `glz` namespace.

> [!NOTE]
>
> Glaze provides `parse` and `serialize` helpers that decode the type of the value intended for `from` or `to` specializations. The developer only needs to specialize `from/to` structs, but you can use `parse/serialize` for cleaner syntax (see examples or Glaze's codebase).

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

namespace glz
{
   template <>
   struct from<JSON, date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args)
      {
         parse<JSON>::op<Opts>(value.human_readable, args...);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <>
   struct to<JSON, date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args) noexcept
      {
         value.human_readable = std::to_string(value.data);
         serialize<JSON>::op<Opts>(value.human_readable, args...);
      }
   };
}

void example() {
  date d{};
  d.data = 55;

  std::string s{};
  expect(not glz::write_json(d, s));

  expect(s == R"("55")");

  d.data = 0;
  expect(not glz::read_json(d, s));
  expect(d.data == 55);
}
```

Notes:

The templated `Opts` parameter contains the compile time options. The `args...` can be broken out into the runtime context (runtime options), iterators, and the buffer index when reading.

## UUID Example

Say we have a UUID library for converting a `uuid_t` from a `std::string_view` and to a `std::string`.

```c++
namespace glz
{
   template <>
   struct from<JSON, uuid_t>
   {
      template <auto Opts>
      static void op(uuid_t& uuid, auto&&... args)
      {
         // Initialize a string_view with the appropriately lengthed buffer
         // Alternatively, use a std::string for any size (but this will allocate)
         std::string_view str = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
         parse<JSON>::op<Opts>(str, args...);
         uuid = uuid_lib::uuid_from_string_view(str);
      }
   };

   template <>
   struct to<JSON, uuid_t>
   {
      template <auto Opts>
      static void op(const uuid_t& uuid, auto&&... args) noexcept
      {
         std::string str = uuid_lib::uuid_to_string(uuid);
         serialize<JSON>::op<Opts>(str, args...);
      }
   };
}
```

# Handling Ambiguous Partial Specialization

You may want to custom parse a class that matches an underlying glaze partial specialization for a template like below:

```c++
template <class T> requires readable_array_t<T>
struct from<JSON, T>
```

If your own parsing function desires partial template specialization, then ambiguity may occur:

```c++
template <class T> requires std::derived_from<T, vec_t>
struct from<JSON, T>
```

To solve this problem, glaze will check for `custom_read` or `custom_write` values within `glz::meta` and remove the ambiguity and use the custom parser.

```c++
template <class T> requires std::derived_from<T, vec_t>
struct glz::meta<T>
{
   static constexpr auto custom_read = true;
};
```
