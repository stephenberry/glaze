// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Glaze Feature Test Macros

// v5.0.0 moves to more generic read_supported and write_supported concepts
// removes concepts like `read_json_supported` and uses `read_supported<JSON, T>`
#define glaze_v5_0_0_generic_supported

// v4.3.0 removed global glz::trace
#define glaze_v4_3_0_trace

// v4.2.3 renamed glz::tuplet::tuple to glz::tuple
#define glaze_v4_2_3_tuple

// v3.6.0 renames glz::refl to glz::reflect and does not instantiate the struct as an
// inline constexpr variable
#define glaze_v3_6_0_reflect

// v3.5.0 change glz::detail::to_json and glz::detail::from_json specializations
// to glz::detail::to<JSON and glz::detail::from<JSON
// The template specialization takes a uint32_t Format as the first template parameter
#define glaze_v3_5_0_to_from
