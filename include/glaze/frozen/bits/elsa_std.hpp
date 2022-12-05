// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "elsa.h"
#include "hash_string.h"

#include <string_view>
#include <string>

namespace glz::frozen
{
   template <typename CharT>
   struct elsa<std::basic_string_view<CharT>>
   {
       constexpr std::size_t operator()(const std::basic_string_view<CharT>& value) const {
           return hash_string(value);
       }
       constexpr std::size_t operator()(const std::basic_string_view<CharT>& value, std::size_t seed) const {
           return hash_string(value, seed);
       }
   };

   template <typename CharT>
   struct elsa<std::basic_string<CharT>>
   {
       constexpr std::size_t operator()(const std::basic_string<CharT>& value) const {
           return hash_string(value);
       }
       constexpr std::size_t operator()(const std::basic_string<CharT>& value, std::size_t seed) const {
           return hash_string(value, seed);
       }
   };
}
