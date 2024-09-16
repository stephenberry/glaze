// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"

// An argument parsing interface for a reflected C++ struct of options

namespace glz
{
   template <opts Opts = opts{}, class T, class char_ptr_t> requires (std::is_pointer_v<char_ptr_t>)
   inline error_ctx args(T& value, const std::integral auto argc, char_ptr_t argv)
   {
      if (argc == 1) {
         if constexpr (requires { T::print_help_when_no_options == true; }) {
            //help(opts);
         }
         return {};
      }
      
      auto get_id = [&](char alias) -> std::string_view {
         static constexpr auto N = refl<T>.N;
         for (auto& key : refl<T>.keys) {
            if (key.starts_with(alias)) {
               return key;
            }
         }
         return {};
      };

      using int_t = std::decay_t<decltype(argc)>;
      for (int_t i = 1; i < argc; ++i) {
         const char* flag = argv[i];
         if (*flag != '-') {
            return {error_code::syntax_error};
         }
         ++flag;
         
         if (*flag == '-') {
            ++flag;
         }
         std::string_view str{ flag };
         if (str.size() == 1) {
            str = get_id(*flag);
            if (str.empty()) {
               return {error_code::syntax_error};
               // "Invalid alias flag '-' for: " + std::string(str)
            }
         }
         if (str.empty()) { break; }
         
         static constexpr auto N = refl<T>.N;
         
         decltype(auto) tuple = [&]() -> decltype(auto) {
            if constexpr (detail::reflectable<T>) {
               return to_tuple(value);
            }
            else {
               return nullptr;
            }
         }();
         
         static constexpr auto HashInfo = detail::hash_info<T>;
         
         context ctx{};
         
         for_each<N>([&](auto I) {
            if (str == refl<T>.keys[I]) {
               using val_t = std::remove_cvref_t<refl_t<T, I>>;
               /*if constexpr (std::same_as<val_t, bool>) {
                  glz::get<I>(refl<T>.values) = true;
               }
               else {
                  glz::read_json(glz::get<I>(refl<Options>.values), argv[++i]);
               }*/

               constexpr auto type = HashInfo.type;

               if constexpr (not bool(type)) {
                  static_assert(false_v<T>, "invalid hash algorithm");
               }

               if constexpr (N == 1) {
                  decode_index<Opts, T, 0>(func, tuple, value, ctx, it, end);
               }
               else {
                  const auto index = decode_hash<T, HashInfo>(it, end);

                  if (index >= N) [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else {
                        // we need to search until we find the ending quote of the key
                        auto start = it;
                        skip_string_view<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        const sv key = {start, size_t(it - start)};
                        ++it; // skip the quote
                        GLZ_INVALID_END();

                        GLZ_PARSE_WS_COLON;

                        read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                        return;
                     }
                  }

                  jump_table<N>([&]<size_t I>() { decode_index<Opts, T, I>(func, tuple, value, ctx, it, end); }, index);
               }
            }
         });
      }
   }
}
