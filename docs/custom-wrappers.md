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
