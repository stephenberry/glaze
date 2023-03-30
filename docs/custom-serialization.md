# Custom Serialization/Deserialization

Custom serialization/deserialization is achievable by implementing your own `from_json` and `to_json` specializations within the glz::detail namespace.

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

