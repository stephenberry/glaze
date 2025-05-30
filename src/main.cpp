#include <glaze/glaze.hpp>

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glz::meta<my_struct>
{
   static constexpr std::string_view name = "my_struct";
   using T = my_struct;
   static constexpr auto value = object(
      "i", [](auto&& v) -> auto& { return v.i; }, //
      &T::d, //
      "hello", &T::hello, //
      &T::arr //
   );
};

static constexpr auto info = glz::reflect<my_struct>{};

#include <iostream>

int main()
{
   std::cout << "Field types:\n";
   glz::for_each<info.size>([]<auto I>() {
      // std::cout << glz::name_v<decltype(glz::get<I>(info.values))> << '\n';
   });

   std::cout << "Field keys:\n";
   glz::for_each<info.size>([]<auto I>() { std::cout << info.keys[I] << '\n'; });

   my_struct obj{};
   std::cout << '\n' << glz::write_json(obj).value() << '\n';
   constexpr std::string_view input = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
   auto ec = glz::read_json(obj, input);
   if (ec) {
      std::cout << "error: " << glz::format_error(ec, input);
   }
}
