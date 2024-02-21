// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/cli_menu.hpp"

struct my_functions
{
   std::function<void()> hello = [] { std::cout << "Hello\n"; };
   std::function<void()> world = [] { std::cout << "World\n"; };
};

/*template <>
struct glz::meta<my_functions>
{
   using T = my_functions;
   static constexpr auto value = object(&T::hello, &T::world);
};*/

void run_menu()
{
   my_functions obj{};
   std::atomic<bool> show_menu = true;
   glz::run_cli_menu(obj, show_menu);
}

struct more_functions
{
   std::function<void()> one = [] { std::cout << "one\n"; };
   std::function<void()> two = [] { std::cout << "two\n"; };
};

struct my_nested_menu
{
   my_functions first_menu{};
   more_functions second_menu{};
};

void nested_menu()
{
   my_nested_menu obj{};
   std::atomic<bool> show_menu = true;
   glz::run_cli_menu(obj, show_menu);
}

int main()
{
   // run_menu();
   nested_menu();

   return 0;
}
