// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

namespace glaze
{
   template <class T>
   concept is_tuple = is_specialization_v<T, std::tuple>;
}
