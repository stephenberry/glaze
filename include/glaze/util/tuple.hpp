// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/reflection/get_name.hpp"
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;
}
