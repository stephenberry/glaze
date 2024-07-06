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
   static constexpr auto value = glz::detail::Object{ glz::tuplet::tuple{
      "i", [](auto&& v) -> auto& { return v.i; }, //
      &T::d, //
      "hello", &T::hello, //
      &T::arr //
   } };
};

static constexpr auto info = glz::make_reflection_info<my_struct>::op();

#include <iostream>

int main() {
   std::cout << "Field types:\n";
   glz::for_each<info.N>([](auto I) {
      std::cout << glz::name_v<decltype(glz::get<I>(info.values))> << '\n';
   });
   
   std::cout << "Field keys:\n";
   glz::for_each<info.N>([](auto I) {
      std::cout << glz::get<I>(info.keys) << '\n';
   });
}
