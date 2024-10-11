// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Glaze Feature Test Macros

// v3.6.0 renames glz::refl to glz::reflect and does not instantiate the struct as an
// inline constexpr variable
#define glaze_v3_6_0_reflect

// v3.5.0 change glz::detail::to_json and glz::detail::from_json specializations
// to glz::detail::to<JSON and glz::detail::from<JSON
// The template specialization take in a uint32_t Format as the first template parameter
#define glaze_v3_5_0_to_from
