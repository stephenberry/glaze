// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/cli_menu.hpp"

struct my_functions
{
   std::function<void()> hello;
   std::function<void()> world;
};

/*template <>
struct glz::meta<my_functions>
{
   using T = my_functions;
   static constexpr auto value = object(&T::hello, &T::world);
};*/

void run_menu()
{
   my_functions obj{[]{ std::cout << "Hello\n"; },
                    []{ std::cout << "World\n"; }
   };
   std::atomic<bool> show_menu = true;
   glz::run_cli_menu(obj, show_menu);
}

int main()
{
   run_menu();
   
   return 0;
}
