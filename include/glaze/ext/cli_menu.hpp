// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <cstdio>

#include "glaze/glaze.hpp"

// The purpose of this command line interface menu is to use reflection to build the menu,
// but also allow this menu to be registered as an RPC interface.
// So, either the command line interface can be used or another program can call the same
// functions over RPC.

namespace glz
{
   // To support bool and std::atomic<bool> and other custom boolean types
   template <class T>
   concept cli_menu_boolean = requires(T t) {
      {
         t
      } -> std::convertible_to<bool>;
   };

   namespace detail
   {
      template <class T>
      inline void print_input_type()
      {
         if constexpr (string_t<T>) {
            std::printf("json string> ");
         }
         else if constexpr (num_t<T>) {
            std::printf("json number> ");
         }
         else if constexpr (readable_array_t<T> || tuple_t<T> || is_std_tuple<T>) {
            if constexpr (tuple_t<T> || is_std_tuple<T>) {
               std::printf("json array[%d]>", int(glz::tuple_size_v<T>));
            }
            else {
               std::printf("json array> ");
            }
         }
         else if constexpr (boolean_like<T>) {
            std::printf("json bool> ");
         }
         else if constexpr (glaze_object_t<T> || reflectable<T> || writable_map_t<T>) {
            std::printf("json object> ");
         }
         else {
            std::printf("json> ");
         }
      }
   }

   template <auto Opts = opts{.prettify = true}, class T>
      requires(glaze_object_t<T> || reflectable<T>)
   inline void run_cli_menu(T& value, cli_menu_boolean auto& show_menu)
   {
      using namespace detail;

      static constexpr auto N = reflect<T>::size;

      auto execute_menu_item = [&](const auto item_number) {
         if (item_number == N + 1) {
            show_menu = false;
            return;
         }

         [[maybe_unused]] decltype(auto) t = [&] {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         if (item_number > 0 && item_number <= long(N)) {
            for_each_short_circuit<N>([&](auto I) {
               if (I == item_number - 1) {
                  using E = refl_t<T, I>;

                  // MSVC bug requires Index alias here
                  decltype(auto) func = [&]<size_t I>() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get_member(value, get<I>(t));
                     }
                     else {
                        return get_member(value, get<I>(reflect<T>::values));
                     }
                  }.template operator()<I>();

                  using Func = decltype(func);
                  if constexpr (std::is_invocable_v<Func>) {
                     using R = std::invoke_result_t<Func>;
                     if constexpr (std::same_as<R, void>) {
                        func();
                     }
                     else {
                        const auto result = glz::write<Opts>(func()).value_or("result serialization error");
                        std::printf("%.*s\n", int(result.size()), result.data());
                     }
                  }
                  else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
                     using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
                     using Params = glz::tuple_element_t<0, Tuple>;
                     using P = std::decay_t<Params>;
                     constexpr auto N = glz::tuple_size_v<Tuple>;
                     static_assert(N == 1, "Only one input is allowed for your function");
                     static thread_local std::array<char, 256> input{};
                     if constexpr (is_help<P>) {
                        std::printf("%.*s\n", int(P::help_message.size()), P::help_message.data());
                        print_input_type<typename P::value_type>();
                     }
                     else {
                        print_input_type<P>();
                     }

                     if (fgets(input.data(), int(input.size()), stdin)) {
                        std::string_view input_sv{input.data()};
                        if (input_sv.back() == '\n') {
                           input_sv = input_sv.substr(0, input_sv.size() - 1);
                        }
                        using R = std::invoke_result_t<Func, Params>;
                        P params{};
                        const auto ec = glz::read<Opts>(params, input_sv);
                        if (ec) {
                           const auto error = glz::format_error(ec, input_sv);
                           std::printf("%.*s\n", int(error.size()), error.data());
                        }
                        else {
                           if constexpr (std::same_as<R, void>) {
                              func(params);
                           }
                           else {
                              const auto result = glz::write<Opts>(func(params)).value_or("result serialization error");
                              std::printf("%.*s\n", int(result.size()), result.data());
                           }
                        }
                     }
                     else {
                        std::fprintf(stderr, "Invalid input.\n");
                     }
                  }
                  else if constexpr (glaze_object_t<E> || reflectable<E>) {
                     std::atomic<bool> menu_boolean = true;
                     if constexpr (reflectable<T>) {
                        run_cli_menu<Opts>(get<I>(t), menu_boolean);
                     }
                     else {
                        decltype(auto) v = get_member(value, get<I>(reflect<T>::values));
                        run_cli_menu<Opts>(v, menu_boolean);
                     }
                  }
                  else if constexpr (Opts.hide_non_invocable) {
                  }
                  else {
                     static_assert(false_v<Func>, "Your function is not invocable or not concrete");
                  }
                  return true; // exit
               }
               return false; // continue
            });
         }
         else {
            std::fprintf(stderr, "Invalid menu item.\n");
         }
      };

      while (show_menu) {
         std::printf("================================\n");
         for_each<N>([&](auto I) {
            using E = refl_t<T, I>;
            constexpr sv key = reflect<T>::keys[I];

            if constexpr (glaze_object_t<E> || reflectable<E>) {
               std::printf("  %d   %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
            }
            else {
               [[maybe_unused]] decltype(auto) t = [&] {
                  if constexpr (reflectable<T>) {
                     return to_tie(value);
                  }
                  else {
                     return nullptr;
                  }
               }();

               decltype(auto) func = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return get<I>(t);
                  }
                  else {
                     return get_member(value, get<I>(reflect<T>::values));
                  }
               }();

               using Func = decltype(func);
               if constexpr (std::is_invocable_v<Func>) {
                  std::printf("  %d   %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
               }
               else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
                  std::printf("  %d   %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
               }
               else if constexpr (Opts.hide_non_invocable) {
                  // do not print non-invocable member
               }
               else {
                  static_assert(false_v<Func>, "Your function is not invocable or not concrete");
               }
            }
         });
         std::printf("  %d   Exit Menu\n", uint32_t(N + 1));
         std::printf("--------------------------------\n");

         std::printf("cmd> ");
         std::fflush(stdout);
      restart_input: // needed to support std::cin within user functions
         // https://web.archive.org/web/20201112034702/http://sekrit.de/webdocs/c/beginners-guide-away-from-scanf.html
         long cmd = -1;
         constexpr auto buffer_length = 64; // only needed to parse numbers
         char buf[buffer_length]{};
         if (std::fgets(buf, buffer_length, stdin)) {
            if (buf[0] == '\n') {
               goto restart_input;
            }

            auto* it = buf;
            for (const auto* end = buf + 5; it < end; ++it) {
               if (*it == '\n' || *it == '\0') {
                  break;
               }
            }
            std::string_view str{buf, size_t(it - buf)};
            if (str == "cls" || str == "clear") {
               continue;
            }

            char* endptr{};
            errno = 0; // reset error number
            cmd = std::strtol(buf, &endptr, 10);
            if ((errno != ERANGE) && (endptr != buf) && (*endptr == '\0' || *endptr == '\n')) {
               execute_menu_item(cmd);
               continue;
            }
         }

         std::fprintf(stderr, "Invalid input.\n");
      }
   }

   template <auto Opts = opts{.prettify = true}, class T>
      requires(glaze_object_t<T> || reflectable<T>)
   inline void run_cli_menu(T& value)
   {
      std::atomic<bool> menu_boolean = true;
      run_cli_menu<Opts>(value, menu_boolean);
   }
}
