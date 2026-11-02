// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/common.hpp"
#include "glaze/forward.hpp"

namespace glz
{
   // BSON's wire format requires the top-level value to be a document. Allow
   // reflected structs, maps with string keys, sequence containers (encoded
   // as integer-keyed documents), and glaze_array_t. Primitives have no valid
   // BSON document encoding on their own.
   template <class T>
   concept bson_top_level_t = glaze_object_t<std::remove_cvref_t<T>> || reflectable<std::remove_cvref_t<T>> ||
                              writable_map_t<std::remove_cvref_t<T>> || writable_array_t<std::remove_cvref_t<T>> ||
                              glaze_array_t<std::remove_cvref_t<T>>;

   // Reads and writes accept the same top-level types.
   template <class T>
   concept bson_top_level_read_t = bson_top_level_t<T>;

   template <class T>
   inline constexpr bool top_level_supported<BSON, T> = bson_top_level_t<T>;
}
