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
};

int main() {
   my_struct s{};
   glaze::read_json(s, R"({"i":2})");
   return 0; }
