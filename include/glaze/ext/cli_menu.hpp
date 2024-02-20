// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <glaze/glaze.hpp>

// The purpose of this command line interface menu is to use reflection to build the menu,
// but also allow this menu to be registered as an RPC interface.
// So, either the command line interface can be used or another program can call the same
// functions over RPC.

namespace glz
{
   template <class T>
   struct cli_menu
   {
      static constexpr auto size = [] {
         if constexpr (reflectable<T>) {
            return count_members<T>;
         }
         else {
            return std::tuple_size_v<meta_t<T>>;
         }
      }();
      
      std::string menu_heading = "CommandList";
      std::atomic<bool> show_menu = true;
      
      void run()
      {
         int32_t cmd = -1;

         while (show_menu) {
            std::cout << std::format("\n{:*>32}\n", "");
            std::cout << menu_heading << ":\n";
            for (auto& menuEntry : menu_items) {
               menuEntry.printMenuEntry();
            }
            std::cout << std::format("  {} -- Exit Menu\n", menu_items.size() + 1);
            std::cout << std::format("{:*>32}\n", "");
            
            std::cout << "cmd> ";
            std::cin >> cmd;
            execute_menu_item(cmd);
         }
      }
      
      void execute_menu_item(const int32_t item_number)
      {
         if (item_number == size + 1) {
            show_menu = false;
            return;
         }

         if (item_number > 0 && item_number <= size) {
            menu_items.at(item_number - 1).menu_function();
         }
         else {
            std::cerr << "Invalid menu item.\n";
            std::cin.clear(); // reset state
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // consume wrong input
         }
      }
   };
}
