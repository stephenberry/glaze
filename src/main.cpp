// Glaze Library
// For the license information refer to glaze.hpp

// Dummy target since interface targets dont show up in some ides

#include "glaze/glaze.hpp"

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glaze::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glaze::object("i", &T::i,          //
                                               "d", &T::d,          //
                                               "hello", &T::hello,  //
                                               "arr", &T::arr       //
   );

   //static constexpr auto value = glaze::array(&T::i);
};

#include <iostream>

int main() {
   my_struct s{};
   std::string buffer = R"({"i":2})";
   auto b = std::ranges::begin(buffer);
   auto e = std::ranges::end(buffer);
   //std::array s = {1};
   //static_assert(glaze::detail::array_t<std::decay_t<decltype(s)>>);
    try {
      //glaze::detail::from_json<std::decay_t<decltype(s)>>::op(s, b, e);
       glaze::read_json(s, buffer);
   }
   catch (const std::exception& e) {
      std::cout << e.what() << '\n';
   }
   
   return 0; }
