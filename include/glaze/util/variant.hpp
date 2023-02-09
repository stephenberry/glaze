// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>

#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <class T>
   concept is_variant = is_specialization_v<T, std::variant>;
}
