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
      static void op(date& value, is_context auto&& ctx, auto&& it, auto&& end)
      {
         parse<JSON>::op<Opts>(value.human_readable, ctx, it, end);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <>
   struct to<JSON, date>
   {
      template <auto Opts>
      static void op(date& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
      {
         value.human_readable = std::to_string(value.data);
         serialize<JSON>::op<Opts>(value.human_readable, ctx, b, ix);
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

The templated `Opts` parameter contains the compile time options. 

For reading (`from` specializations), the parameters are:
- `value`: The object being parsed into
- `ctx`: The context containing runtime options (use `is_context auto&&`)
- `it`: Iterator to the current position in the input buffer
- `end`: Iterator to the end of the input buffer

For writing (`to` specializations), the parameters are:
- `value`: The object being serialized
- `ctx`: The context containing runtime options (use `is_context auto&&`)
- `b`: The output buffer to write to
- `ix`: The current index in the output buffer

## Bitfields

C++ bitfields cannot be referenced with a pointer-to-member, so they need `glz::custom` adapters in the `glz::meta` definition. The adapter lets you expose the bitfield as a regular integer while preserving the in-class bit packing.

```c++
struct bitfield_struct_t {
   uint8_t f1 : 4{};
   uint8_t f2 : 4{};
   uint8_t f3{};
};

template <>
struct glz::meta<bitfield_struct_t>
{
   using T = bitfield_struct_t;

   static constexpr auto read_f1  = [](T& self, uint8_t v) { self.f1 = v; };
   static constexpr auto write_f1 = [](const T& self) { return static_cast<uint8_t>(self.f1); };
   static constexpr auto read_f2  = [](T& self, uint8_t v) { self.f2 = v; };
   static constexpr auto write_f2 = [](const T& self) { return static_cast<uint8_t>(self.f2); };

   static constexpr auto value = object(
      "f1", glz::custom<read_f1, write_f1>,
      "f2", glz::custom<read_f2, write_f2>,
      "f3", &T::f3
   );
};
```

- Ordinary members such as `f3` can continue to use direct pointers-to-members alongside the custom fields.

## UUID Example

Say we have a UUID library for converting a `uuid_t` from a `std::string_view` and to a `std::string`.

```c++
namespace glz
{
   template <>
   struct from<JSON, uuid_t>
   {
      template <auto Opts>
      static void op(uuid_t& uuid, is_context auto&& ctx, auto&& it, auto&& end)
      {
         // Initialize a string_view with the appropriately lengthed buffer
         // Alternatively, use a std::string for any size (but this will allocate)
         std::string_view str = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
         parse<JSON>::op<Opts>(str, ctx, it, end);
         uuid = uuid_lib::uuid_from_string_view(str);
      }
   };

   template <>
   struct to<JSON, uuid_t>
   {
      template <auto Opts>
      static void op(const uuid_t& uuid, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
      {
         std::string str = uuid_lib::uuid_to_string(uuid);
         serialize<JSON>::op<Opts>(str, ctx, b, ix);
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
