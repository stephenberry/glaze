// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>
#include <variant>

namespace glaze
{
	template <class, template<class...> class>
	inline constexpr bool is_specialization_v = false;
	template <template<class...> class T, class... Args>
	inline constexpr bool is_specialization_v<T<Args...>, T> = true;

	template <class T>
	concept is_tuple = is_specialization_v<T, std::tuple>;

	template <class T>
	concept is_variant = is_specialization_v<T, std::variant>;
}
