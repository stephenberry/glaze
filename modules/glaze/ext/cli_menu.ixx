export module glaze.ext.cli_menu;

import std;

import glaze.core.common;
import glaze.core.meta;
import glaze.core.opts;
import glaze.core.reflect;
import glaze.core.read;
import glaze.core.write;
import glaze.concepts.container_concepts;
import glaze.reflection.to_tuple;
import glaze.util.for_each;
import glaze.util.help;
import glaze.util.string_literal;
import glaze.util.type_traits;
import glaze.util.tuple;

import glaze.tuplet;

import glaze;

// The purpose of this command line interface menu is to use reflection to build the menu,
// but also allow this menu to be registered as an RPC interface.
// So, either the command line interface can be used or another program can call the same
// functions over RPC.

using std::uint32_t;
using std::size_t;

namespace glz
{
   // To support bool and std::atomic<bool> and other custom boolean types
   template <class T>
   concept cli_menu_boolean = requires(T t) {
      { t } -> std::convertible_to<bool>;
   };

   namespace detail
   {
      inline constexpr bool is_ascii_space(const char ch) noexcept
      {
         switch (ch) {
         case ' ':
         case '\t':
         case '\n':
         case '\r':
         case '\f':
         case '\v':
            return true;
         default:
            return false;
         }
      }

      inline constexpr std::string_view trim_ascii_whitespace(std::string_view sv) noexcept
      {
         while (!sv.empty() && is_ascii_space(sv.front())) {
            sv.remove_prefix(1);
         }
         while (!sv.empty() && is_ascii_space(sv.back())) {
            sv.remove_suffix(1);
         }
         return sv;
      }

      inline bool parse_long(std::string_view sv, long& value) noexcept
      {
         sv = trim_ascii_whitespace(sv);
         if (sv.empty()) {
            return false;
         }

         const auto* first = sv.data();
         const auto* last = sv.data() + sv.size();

         auto [ptr, ec] = std::from_chars(first, last, value);
         return ec == std::errc{} && ptr == last;
      }

      template <class S>
      inline void write_raw(std::ostream& os, const S& s)
      {
         os.write(s.data(), static_cast<std::streamsize>(s.size()));
      }

      template <class S>
      inline void write_line(std::ostream& os, const S& s)
      {
         write_raw(os, s);
         os.put('\n');
      }

      template <class T>
      inline void print_input_type()
      {
         if constexpr (string_t<T>) {
            std::cout << "json string> ";
         }
         else if constexpr (num_t<T>) {
            std::cout << "json number> ";
         }
         else if constexpr (readable_array_t<T> || tuple_t<T> || is_std_tuple<T>) {
            if constexpr (tuple_t<T> || is_std_tuple<T>) {
               std::cout << "json array[" << int(glz::tuple_size_v<T>) << "]>";
            }
            else {
               std::cout << "json array> ";
            }
         }
         else if constexpr (boolean_like<T>) {
            std::cout << "json bool> ";
         }
         else if constexpr (glaze_object_t<T> || reflectable<T> || writable_map_t<T>) {
            std::cout << "json object> ";
         }
         else {
            std::cout << "json> ";
         }

         std::cout << std::flush;
      }
   }

   // When running with exceptions enabled we allow the user to provide an exceptions callback, which will be invoked
   // when an exception is thrown from running a menu item. The exception callback must take a `const std::exception&`

   export template <auto Opts = opts{.prettify = true}, class T, cli_menu_boolean ShowMenu = std::atomic<bool>>
      requires(glaze_object_t<T> || reflectable<T>)
#if __cpp_exceptions
   inline void run_cli_menu(T& value, ShowMenu&& show_menu, auto&& exception_callback)
#else
   inline void run_cli_menu(T& value, ShowMenu&& show_menu = true)
#endif
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
            visit<N>(
               [&]<size_t I>() {
                  using E = refl_t<T, I>;

                  // MSVC bug requires Index alias here
                  decltype(auto) func = [&]<size_t J>() -> decltype(auto) {
                     if constexpr (reflectable<T>) {
                        return get_member(value, get<J>(t));
                     }
                     else {
                        return get_member(value, get<J>(reflect<T>::values));
                     }
                  }.template operator()<I>();

                  using Func = decltype(func);
                  if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>) {
                     using F = std::decay_t<Func>;
                     using Ret = typename return_type<F>::type;
                     using Tuple = typename inputs_as_tuple<F>::type;
                     constexpr auto n_args = glz::tuple_size_v<Tuple>;

                     if constexpr (n_args == 0) {
                        if constexpr (std::same_as<Ret, void>) {
                           (value.*func)();
                        }
                        else {
                           const auto result = glz::write<Opts>((value.*func)()).value_or("result serialization error");
                           write_line(std::cout, result);
                        }
                     }
                     else if constexpr (n_args == 1) {
                        using Params = glz::tuple_element_t<0, Tuple>;
                        using P = std::decay_t<Params>;
                        constexpr auto N = glz::tuple_size_v<Tuple>;
                        static_assert(N == 1, "Only one input is allowed for your function");
                        std::array<char, 256> input{};
                        if constexpr (is_help<P>) {
                           write_line(std::cout, P::help_message);
                           print_input_type<typename P::value_type>();
                        }
                        else {
                           print_input_type<P>();
                        }

                        if (std::getline(std::cin, input)) {
                           std::string_view input_sv{input};

                           P params{};
                           const auto ec = glz::read<Opts>(params, input_sv);
                           if (ec) {
                              const auto error = glz::format_error(ec, input_sv);
                              write_line(std::cout, error);
                           }
                           else {
                              if constexpr (std::same_as<Ret, void>) {
                                 (value.*func)(params);
                              }
                              else {
                                 const auto result =
                                    glz::write<Opts>((value.*func)(params)).value_or("result serialization error");
                                 write_line(std::cout, result);
                              }
                           }
                        }
                        else {
                           std::cerr << "Invalid input.\n";
                        }
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
                  else if constexpr (std::is_invocable_v<Func>) {
                     using R = std::invoke_result_t<Func>;
                     if constexpr (std::same_as<R, void>) {
                        func();
                     }
                     else {
                        const auto result = glz::write<Opts>(func()).value_or("result serialization error");
                        write_line(std::cout, result);
                     }
                  }
                  else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
                     using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
                     using Params = glz::tuple_element_t<0, Tuple>;
                     using P = std::decay_t<Params>;
                     constexpr auto N = glz::tuple_size_v<Tuple>;
                     static_assert(N == 1, "Only one input is allowed for your function");
                     std::array<char, 256> input{};
                     if constexpr (is_help<P>) {
                        write_line(std::cout, P::help_message);
                        print_input_type<typename P::value_type>();
                     }
                     else {
                        print_input_type<P>();
                     }

                     if (std::getline(std::cin, input)) {
                        std::string_view input_sv{input};
                        using R = std::invoke_result_t<Func, Params>;
                        P params{};
                        const auto ec = glz::read<Opts>(params, input_sv);
                        if (ec) {
                           const auto error = glz::format_error(ec, input_sv);
                           write_line(std::cout, error);
                        }
                        else {
                           if constexpr (std::same_as<R, void>) {
                              func(params);
                           }
                           else {
                              const auto result = glz::write<Opts>(func(params)).value_or("result serialization error");
                              write_line(std::cout, result);
                           }
                        }
                     }
                     else {
                        std::cerr << "Invalid input.\n";
                     }
                  }
                  else if constexpr (glaze_object_t<E> || reflectable<E>) {
                     std::atomic<bool> menu_boolean = true;
                     if constexpr (reflectable<T>) {
#if __cpp_exceptions
                        run_cli_menu<Opts>(get<I>(t), menu_boolean, exception_callback);
#else
                        run_cli_menu<Opts>(get<I>(t), menu_boolean);
#endif
                     }
                     else {
                        decltype(auto) v = get_member(value, get<I>(reflect<T>::values));
#if __cpp_exceptions
                        run_cli_menu<Opts>(v, menu_boolean, exception_callback);
#else
                        run_cli_menu<Opts>(v, menu_boolean);
#endif
                     }
                  }
                  else if constexpr (check_hide_non_invocable(Opts)) {
                  }
                  else {
                     static_assert(false_v<Func>, "Your function is not invocable or not concrete");
                  }
               },
               item_number - 1);
         }
         else {
            std::cerr << "Invalid menu item.\n";
         }
      };

      while (show_menu) {
         std::cout << "================================\n";
         for_each<N>([&]<auto I>() {
            using E = refl_t<T, I>;
            constexpr sv key = reflect<T>::keys[I];

            if constexpr (glaze_object_t<E> || reflectable<E>) {
               std::cout << "  " << uint32_t(I + 1) << "   ";
               write_line(std::cout, key);
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
               if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>) {
                  std::cout << "  " << uint32_t(I + 1) << "   ";
                  write_line(std::cout, key);
               }
               else if constexpr (std::is_invocable_v<Func>) {
                  std::cout << "  " << uint32_t(I + 1) << "   ";
                  write_line(std::cout, key);
               }
               else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
                  std::cout << "  " << uint32_t(I + 1) << "   ";
                  write_line(std::cout, key);
               }
               else if constexpr (check_hide_non_invocable(Opts)) {
                  // do not print non-invocable member
               }
               else {
                  static_assert(false_v<Func>, "Your function is not invocable or not concrete");
               }
            }
         });
         std::cout << "  " << uint32_t(N + 1) << "   Exit Menu\n";
         std::cout << "--------------------------------\n";

         std::cout << "cmd> " << std::flush;

      restart_input: // needed to support std::cin within user functions
         long cmd = -1;
         std::string buf{};

         if (std::getline(std::cin, buf)) {
            auto str = trim_ascii_whitespace(buf);

            if (str.empty()) {
               goto restart_input;
            }

            if (str == "cls" || str == "clear") {
               std::cout << '\n';
               continue;
            }

            if (parse_long(str, cmd)) {
#if __cpp_exceptions
               if constexpr (std::invocable<decltype(exception_callback), const std::exception&>) {
                  try {
                     execute_menu_item(cmd);
                  }
                  catch (const std::exception& e) {
                     exception_callback(e);
                  }
               }
               else {
                  execute_menu_item(cmd);
               }
#else
               execute_menu_item(cmd);
#endif
               continue;
            }
         }

         std::cerr << "Invalid input.\n";
      }
   }

#if __cpp_exceptions
   // Version without exception callback for platforms with exceptions enabled
   export template <auto Opts = opts{.prettify = true}, class T, cli_menu_boolean ShowMenu = std::atomic<bool>>
      requires(glaze_object_t<T> || reflectable<T>)
   inline void run_cli_menu(T& value, ShowMenu&& show_menu = true)
   {
      run_cli_menu<Opts>(value, show_menu, nullptr);
   }
#endif
}
