# Custom Wrappers

Custom wrappers are used to change the serialization/deserialization on basic types (e.g. int, double, std::string).

> Note that the API within the `detail` namespace is subject to change and is therefore not expected to be as stable.

The example below shows how numbers can be read in via strings by providing a wrapper to indicate the proper serialization/deserialization.

```c++
template <class T>
struct quoted_t
{
   T& val;
};

namespace glz::detail
{
   template <class T>
   struct from_json<quoted_t<T>>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&... args)
      {
         skip_ws<Opts>(args...);
         match<'"'>(args...);
         read<json>::op<Opts>(value.val, args...);
         match<'"'>(args...);
      }
   };

   template <class T>
   struct to_json<quoted_t<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         dump<'"'>(args...);
         write<json>::op<Opts>(value.val, ctx, args...);
         dump<'"'>(args...);
      }
   };
}

template <auto MemPtr>
constexpr decltype(auto) qouted()
{
   return [](auto&& val) { return quoted_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
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
  //static constexpr auto value = object("x", [](auto&& val) { return qouted_t(val.x); });
};

void example() {
  A a{3.14};
  std::string buffer{};
  glz::write_json(a, buffer);
  expect(buffer == R"({"x":"3.14"})");

  buffer = R"({"x":"999.2"})";
  expect(glz::read_json(a, buffer) == glz::error_code::none);
  expect(a.x == 999.2);
}
```
