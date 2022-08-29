// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glz
{
   // from
   // https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
   template <class, template<class...> class>
   inline constexpr bool is_specialization_v = false;
   
   template <template<class...> class T, class... Args>
   inline constexpr bool is_specialization_v<T<Args...>, T> = true;
}
