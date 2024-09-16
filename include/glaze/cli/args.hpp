// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"

// An argument parsing interface for a reflected C++ struct of options

namespace glz
{
   template <class Options, class char_ptr_t> requires (std::is_pointer_v<char_ptr_t>)
   inline error_ctx args(Options& opts, const std::integral auto argc, char_ptr_t argv)
   {
      if (argc == 1) {
         if constexpr (requires { Options::print_help_when_no_options == true; }) {
            //help(opts);
         }
         return {};
      }
      
      auto get_id = [&](char alias) -> std::string_view {
         static constexpr auto N = refl<Options>.N;
         for (auto& key : refl<Options>.keys) {
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
         
         static constexpr auto N = refl<Options>.N;
         
         for_each<N>([&](auto I) {
            if (str == refl<Options>.keys[I]) {
               using val_t = std::remove_cvref_t<refl_t<Options, I>>;
               if constexpr (std::same_as<val_t, bool>) {
                  glz::get<I>(refl<Options>.values) = true;
               }
               else {
                  glz::read_json(glz::get<I>(refl<Options>.values), argv[++i]);
               }
            }
         });
      }
   }
}
