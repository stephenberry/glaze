// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

#include <tuple>
#include <variant>

namespace glaze
{
	template <class T>
	concept is_tuple = detail::is_specialization_v<T, std::tuple>;

	template <class T>
	concept is_variant = detail::is_specialization_v<T, std::variant>;
}
