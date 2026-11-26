// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/reflection/requires_key.hpp"
// glz:header std=<concepts>
// glz:header std=<string_view>
// glz:header std=<type_traits>
export module glaze.reflection.requires_key;

import std;

export import glaze.core.meta_fwd;

namespace glz
{
   /// Concept for when requires_key is available in glz::meta
   export template<class T>
   concept meta_has_requires_key = requires(T t, const std::string_view s, const bool is_nullable) {
      { glz::meta<std::remove_cvref_t<T>>::requires_key(s, is_nullable) } -> std::same_as<bool>;
   };
}
