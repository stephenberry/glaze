// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

namespace glz
{
   using sv = std::string_view;

   template <class T>
   concept sv_convertible = std::convertible_to<std::decay_t<T>, std::string_view>;
}
