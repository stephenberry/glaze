// Glaze Library
// For the license information refer to glaze.hpp

// Dummy target since interface targets dont show up in some ides

#include "glaze/glaze.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/api/impl.hpp"

struct sub_t
{
   double x = 400.0;
   double y = 200.0;
};

template <>
struct glz::meta<sub_t>
{
   static constexpr std::string_view name = "sub";
   using T = sub_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   static constexpr std::string_view name = "Color";
   using enum Color;
   static constexpr auto value = enumerate("Red", Red,      //
                                           "Green", Green,  //
                                           "Blue", Blue     //
   );
};

struct my_struct
{
   int i = 287;
   double d = 3.14;
   Color c = Color::Red;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   sub_t sub{};
   std::map<std::string, int> map{};
};

template <>
struct glz::meta<my_struct>
{
   static constexpr std::string_view name = "my_struct";
   using T = my_struct;
   static constexpr auto value = object(
      "i", [](auto&& v) -> auto& { return v.i; },  //
                                        "d", &T::d,     //
                                        "c", &T::c,
                                               "hello", &T::hello,  //
                                               "arr", &T::arr,      //
                                               "sub", &T::sub,      //
                                               "map", &T::map       //
   );
};

#include <iostream>

int main() {
   std::cout << glz::name_v<glz::detail::member_tuple_t<my_struct>> << "\n";
   my_struct s{};
   my_struct s2{};
   std::string buffer = R"({"i":2,"map":{"fish":5,"cake":2,"bear":3}})";
   try {
      glz::read_json(s, buffer);
      
      std::vector<std::byte> out;
      static constexpr auto partial = glz::json_ptrs("/i",
                                                       "/d",
                                                       "/sub/x",
                                                       "/sub/y",
                                                       "/map/fish",
                                                       "/map/bear");
      
      static constexpr auto sorted = glz::sort_json_ptrs(partial);

      static constexpr auto groups = glz::group_json_ptrs<sorted>();
      
      static constexpr auto N = std::tuple_size_v<decltype(groups)>;
      glz::for_each<N>([&](auto I){
         const auto group = std::get<I>(groups);
         std::cout << std::get<0>(group) << ": ";
         for (auto& rest : std::get<1>(group)) {
            std::cout << rest << ", ";
         }
         std::cout << '\n';
      });
      
      glz::write_binary<partial>(s, out);
      
      s2.i = 5;
      s2.hello = "text";
      s2.d = 5.5;
      s2.sub.x = 0.0;
      s2.sub.y = 20;
      glz::read_binary(s2, out);
   }
   catch (const std::exception& e) {
      std::cout << e.what() << '\n';
   }
   
   return 0; }
