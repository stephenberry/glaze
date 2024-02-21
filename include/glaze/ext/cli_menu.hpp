// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdio>
#include <glaze/glaze.hpp>

// The purpose of this command line interface menu is to use reflection to build the menu,
// but also allow this menu to be registered as an RPC interface.
// So, either the command line interface can be used or another program can call the same
// functions over RPC.

namespace glz
{
   template <typename T>
   concept boolean_like = requires(T t) {
      {
         t
      } -> std::convertible_to<bool>;
   };

   template <class T, boolean_like Bool>
      requires(detail::glaze_object_t<T> || detail::reflectable<T>)
   inline void run_cli_menu(T& value, Bool& show_menu)
   {
      using namespace detail;

      static constexpr auto N = [] {
         if constexpr (reflectable<T>) {
            return count_members<T>;
         }
         else {
            return std::tuple_size_v<meta_t<T>>;
         }
      }();

      int32_t cmd = -1;

      auto execute_menu_item = [&](const int32_t item_number) {
         if (item_number == N + 1) {
            show_menu = false;
            return;
         }

         [[maybe_unused]] decltype(auto) t = [&] {
            if constexpr (reflectable<T>) {
               return to_tuple(value);
            }
            else {
               return nullptr;
            }
         }();

         if (item_number > 0 && item_number <= int32_t(N)) {
            for_each<N>([&](auto I) {
               using Element = glaze_tuple_element<I, N, T>;

               using E = typename Element::type;
               if constexpr (glaze_object_t<E> || reflectable<E>) {
                  if (I == item_number - 1) {
                     std::atomic<bool> menu_boolean = true;
                     if constexpr (reflectable<T>) {
                        run_cli_menu(std::get<I>(t), menu_boolean);
                     }
                     else {
                        decltype(auto) v = get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
                        run_cli_menu(v, menu_boolean);
                     }
                  }
               }
               else {
                  if (I == item_number - 1) {
                     if constexpr (reflectable<T>) {
                        using Func = decltype(std::get<I>(t));
                        using R = std::invoke_result_t<Func>;
                        if constexpr (std::is_convertible_v<R, sv>) {
                           const auto result = std::get<I>(t)();
                           std::printf("%.*s", int(result.size()), result.data());
                        }
                        else if constexpr (std::same_as<R, void>) {
                           std::get<I>(t)();
                        }
                        else {
                           static_assert(false_v<R>,
                                         "Function return must be convertible to a std::string_view or be void");
                        }
                     }
                     else {
                        decltype(auto) func = get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
                        using Func = decltype(func);
                        using R = std::invoke_result_t<Func>;
                        if constexpr (std::is_convertible_v<R, sv>) {
                           const auto result = func();
                           std::printf("%.*s", int(result.size()), result.data());
                        }
                        else if constexpr (std::same_as<R, void>) {
                           func();
                        }
                        else {
                           static_assert(false_v<R>,
                                         "Function return must be convertible to a std::string_view or be void");
                        }
                     }
                  }
               }
            });
         }
         else {
            std::fprintf(stderr, "Invalid menu item.\n");
            while (std::getchar() != '\n') {
            }; // clear the input buffer
         }
      };

      while (show_menu) {
         std::printf("================================\n");
         for_each<N>([&](auto I) {
            using Element = glaze_tuple_element<I, N, T>;
            constexpr sv key = key_name<I, T, Element::use_reflection>;

            std::printf("  %d   %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
         });
         std::printf("  %d   Exit Menu\n", uint32_t(N + 1));
         std::printf("--------------------------------\n");

         std::printf("cmd> ");
         if (std::scanf("%d", &cmd) == 1) {
            execute_menu_item(cmd);
         }
         else {
            std::fprintf(stderr, "Invalid input. Please enter an integer.\n");
            while (std::getchar() != '\n') {
            }; // clear the input buffer
         }
      }
   }
}
