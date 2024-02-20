// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <glaze/glaze.hpp>

#include <iostream>

// The purpose of this command line interface menu is to use reflection to build the menu,
// but also allow this menu to be registered as an RPC interface.
// So, either the command line interface can be used or another program can call the same
// functions over RPC.

namespace glz
{
   template <class T>
   inline void run_cli_menu(T& value, std::atomic<bool>& show_menu)
   {
      static constexpr auto N = [] {
         if constexpr (detail::reflectable<T>) {
            return detail::count_members<T>;
         }
         else {
            return std::tuple_size_v<meta_t<T>>;
         }
      }();
      
      int32_t cmd = -1;
      
      auto execute_menu_item = [&](const int32_t item_number)
      {
         if (item_number == N + 1) {
            show_menu = false;
            return;
         }
         
         decltype(auto) t = detail::to_tuple(value);

         if (item_number > 0 && item_number <= int32_t(N)) {            
            for_each<N>([&](auto I) {
               if (I == item_number - 1) {
                  std::get<I>(t)();
               }
            });
         }
         else {
            std::fprintf(stderr, "Invalid menu item.\n");
            std::cin.clear(); // reset state
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // consume wrong input
         }
      };
      
      while (show_menu) {
         std::printf("================================\n");
         for_each<N>([&](auto I) {
            using Element = detail::glaze_tuple_element<I, N, T>;
            constexpr sv key = detail::key_name<I, T, Element::use_reflection>;
            
            std::printf("  %d -- %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
         });
         std::printf("  %d -- Exit Menu\n", uint32_t(N + 1));
         std::printf("================================\n");
         
         std::printf("cmd> ");
         std::cin >> cmd;
         execute_menu_item(cmd);
      }
   }
}
