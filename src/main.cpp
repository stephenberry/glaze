// Glaze Library
// For the license information refer to glaze.hpp

// Dummy target since interface targets dont show up in some ides

#include "glaze/glaze.hpp"

struct sub
{
   double x = 400.0;
   double y = 200.0;
};

template <>
struct glaze::meta<sub>
{
   using T = sub;
   static constexpr auto value = glaze::object("x", &T::x, "y", &T::y);
};

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   sub sub{};
   std::map<std::string, int> map{};
};

template <>
struct glaze::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glaze::object("i", &T::i,          //
                                               "d", &T::d,          //
                                               "hello", &T::hello,  //
                                               "arr", &T::arr,      //
                                               "sub", &T::sub,      //
                                               "map", &T::map       //
   );
};

#include <iostream>

int main() {
   my_struct s{};
   my_struct s2{};
   std::string buffer = R"({"i":2,"map":{"fish":5,"cake":2,"bear":3}})";
   try {
      glaze::read_json(s, buffer);
      
      std::vector<std::byte> out;
      static constexpr auto partial = glaze::json_ptrs("/i",
                                                       "/d",
                                                       "/sub/x",
                                                       "/sub/y",
                                                       "/map/fish",
                                                       "/map/bear");
      
      static constexpr auto sorted = glaze::sort_json_ptrs(partial);

      static constexpr auto groups = glaze::group_json_ptrs<sorted>();
      
      static constexpr auto N = std::tuple_size_v<decltype(groups)>;
      glaze::for_each<N>([&](auto I){
         const auto group = std::get<I>(groups);
         std::cout << std::get<0>(group) << ": ";
         for (auto& rest : std::get<1>(group)) {
            std::cout << rest << ", ";
         }
         std::cout << '\n';
      });
      
      glaze::write_binary<partial>(s, out);
      
      s2.i = 5;
      s2.hello = "text";
      s2.d = 5.5;
      s2.sub.x = 0.0;
      s2.sub.y = 20;
      glaze::read_binary(s2, out);
   }
   catch (const std::exception& e) {
      std::cout << e.what() << '\n';
   }
   
   return 0; }
