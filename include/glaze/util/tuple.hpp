// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/reflection/get_name.hpp"
#include "glaze/tuplet/tuple.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   namespace detail
   {
      // Unqualified `get<0>(t)` resolves via ADL: std types are found in namespace std,
      // user types via their own associated namespaces. Matches the structured-bindings
      // tuple-protocol lookup rule ([dcl.struct.bind]).
      template <class T>
      concept has_adl_tuple_get = requires(T&& t) { get<0>(static_cast<T&&>(t)); };
   }

   // True for any non-pair, non-range type that satisfies the standard tuple protocol:
   // std::tuple_size<T>::value is well-formed, std::tuple_element<0, T>::type is
   // well-formed, and ADL get<0>(t) is callable. This includes std::tuple as well
   // as user-defined types that opt in by specializing std::tuple_size /
   // std::tuple_element and providing an ADL `get<I>`.
   template <class T>
   concept std_tuple_protocol = requires {
      std::tuple_size<std::remove_cvref_t<T>>::value;
      typename std::tuple_element<0, std::remove_cvref_t<T>>::type;
   } && detail::has_adl_tuple_get<T> && !pair_t<T> && !range<T>;

   // TODO: This doesn't appear to be used. Should it be removed?
   template <class Type>
   concept is_schema_class = requires {
      requires std::is_class_v<Type>;
      requires Type::schema_attributes;
   };
}
