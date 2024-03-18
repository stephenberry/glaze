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

   struct cli_menu_opts
   {
      bool hide_non_invocable = true; // hides non-invocable members from the menu
      glz::opts opts{.prettify = true};
   };

   template <cli_menu_opts Opts = cli_menu_opts{}, class T>
      requires(detail::glaze_object_t<T> || detail::reflectable<T>)
   inline void run_cli_menu(T& value, cli_menu_boolean auto& show_menu)
   {
      using namespace detail;

      static constexpr auto N = reflection_count<T>;

      auto execute_menu_item = [&](const auto item_number) {
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

         if (item_number > 0 && item_number <= long(N)) {
            for_each_ex<N>([&](auto I) {
               using Element = glaze_tuple_element<I, N, T>;

               using E = typename Element::type;

               if (I == item_number - 1) {
                  decltype(auto) func = [&]() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return std::get<I>(t);
                     }
                     else {
                        return get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
                     }
                  }();

                  using Func = decltype(func);
                  if constexpr (std::is_invocable_v<Func>) {
                     using R = std::invoke_result_t<Func>;
                     if constexpr (std::same_as<R, void>) {
                        func();
                     }
                     else {
                        const auto result = glz::write<Opts.opts>(func());
                        std::printf("%.*s\n", int(result.size()), result.data());
                     }
                  }
                  else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
                     using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
                     using Params = std::tuple_element_t<0, Tuple>;
                     using P = std::decay_t<Params>;
                     constexpr auto N = std::tuple_size_v<Tuple>;
                     static_assert(N == 1, "Only one input is allowed for your function");
                     static thread_local std::array<char, 256> input{};
                     if constexpr (string_t<P>) {
                        std::printf("json string> ");
                     }
                     else if constexpr (num_t<P>) {
                        std::printf("json number> ");
                     }
                     else if constexpr (readable_array_t<P> || tuple_t<P> || is_std_tuple<P>) {
                        std::printf("json array> ");
                     }
                     else if constexpr (boolean_like<P>) {
                        std::printf("json bool> ");
                     }
                     else if constexpr (glaze_object_t<P> || reflectable<P> || writable_map_t<P>) {
                        std::printf("json object> ");
                     }
                     else {
                        std::printf("json> ");
                     }

                     if (fgets(input.data(), int(input.size()), stdin)) {
                        std::string_view input_sv{input.data()};
                        if (input_sv.back() == '\n') {
                           input_sv = input_sv.substr(0, input_sv.size() - 1);
                        }
                        using R = std::invoke_result_t<Func, Params>;
                        std::decay_t<Params> params{};
                        const auto ec = glz::read<Opts.opts>(params, input_sv);
                        if (ec) {
                           const auto error = glz::format_error(ec, input_sv);
                           std::printf("%.*s\n", int(error.size()), error.data());
                        }
                        else {
                           if constexpr (std::same_as<R, void>) {
                              func(params);
                           }
                           else {
                              const auto result = glz::write<Opts.opts>(func(params));
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
                        run_cli_menu<Opts>(std::get<I>(t), menu_boolean);
                     }
                     else {
                        decltype(auto) v = get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
                        run_cli_menu<Opts>(v, menu_boolean);
                     }
                  }
                  else if constexpr (Opts.hide_non_invocable) {
                  }
                  else {
                     static_assert(false_v<Func>, "Your function is not invocable or not concrete");
                  }
               }
            });
         }
         else {
            std::fprintf(stderr, "Invalid menu item.\n");
         }
      };

      while (show_menu) {
         std::printf("================================\n");
         for_each<N>([&](auto I) {
            using Element = glaze_tuple_element<I, N, T>;
            constexpr sv key = key_name<I, T, Element::use_reflection>;

            using E = typename Element::type;
            if constexpr (glaze_object_t<E> || reflectable<E>) {
               std::printf("  %d   %.*s\n", uint32_t(I + 1), int(key.size()), key.data());
            }
            else {
               [[maybe_unused]] decltype(auto) t = [&] {
                  if constexpr (reflectable<T>) {
                     return to_tuple(value);
                  }
                  else {
                     return nullptr;
                  }
               }();

               decltype(auto) func = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return std::get<I>(t);
                  }
                  else {
                     return get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
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

   template <cli_menu_opts Opts = cli_menu_opts{}, class T>
      requires(detail::glaze_object_t<T> || detail::reflectable<T>)
   inline void run_cli_menu(T& value)
   {
      std::atomic<bool> menu_boolean = true;
      run_cli_menu<Opts>(value, menu_boolean);
   }
}
