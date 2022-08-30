// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

namespace glz
{
   template<class T> struct false_t : std::false_type {};
   namespace detail {
       struct aggressive_unicorn_type; // Do not unleash
   }
   template<> struct false_t<detail::aggressive_unicorn_type> : std::true_type {};
   
   template <class T>
   inline constexpr bool false_v = false_t<T>::value;
   
   // from
   // https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
   template <class, template<class...> class>
   inline constexpr bool is_specialization_v = false;
   
   template <template<class...> class T, class... Args>
   inline constexpr bool is_specialization_v<T<Args...>, T> = true;
}
