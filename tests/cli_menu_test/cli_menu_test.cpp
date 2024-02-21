// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/cli_menu.hpp"

struct my_functions
{
   std::function<void()> hello = [] { std::printf("Hello\n"); };
   std::function<void()> world = [] { std::printf("World\n"); };
};

template <>
struct glz::meta<my_functions>
{
   using T = my_functions;
   static constexpr auto value = object(&T::hello, &T::world);
};

void run_menu()
{
   my_functions obj{};
   glz::run_cli_menu(obj);
}

struct more_functions
{
   std::function<std::string()> one = [] { return "one\n"; };
   std::function<void()> two = [] { std::printf("two\n"); };
   std::function<std::string_view()> three = [] { return "three\n"; };
};

struct my_nested_menu
{
   my_functions first_menu{};
   more_functions second_menu{};
};

void nested_menu()
{
   my_nested_menu menu{};
   glz::run_cli_menu(menu);
}

int main()
{
   // run_menu();
   nested_menu();

   return 0;
}
