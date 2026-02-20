// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

namespace glz
{
   // Trait to mark types with specified Glaze read/write implementations.
   // When P2996 reflection is enabled, specialize this to std::true_type
   // for types that have explicit to/from implementations to prevent
   // automatic reflection from creating ambiguous specializations.
   template <class T>
   struct specified : std::false_type {};

   template <class T>
   concept is_specified = specified<std::remove_cvref_t<T>>::value;
}
