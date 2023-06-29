// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>

#include "glaze/api/name.hpp"
#include "glaze/core/common.hpp"

namespace glz
{
   namespace detail
   {
      template <const std::string_view& Str, class Tuple, size_t I = 0>
      struct expander
      {
         static constexpr auto impl() noexcept
         {
            const auto N = std::tuple_size_v<Tuple>;
            if constexpr (I >= N) {
               return Str;
            }
            else if constexpr (I == N - 1) {
               return expander<detail::join_v<Str, name_v<std::tuple_element_t<I, Tuple>>>, Tuple, I + 1>::value;
            }
            else {
               return expander<detail::join_v<Str, name_v<std::tuple_element_t<I, Tuple>>, chars<",">>, Tuple,
                               I + 1>::value;
            }
         }

         static constexpr std::string_view value = impl();
      };

      template <const std::string_view& Str, class Tuple>
      inline constexpr std::string_view expander_v = expander<Str, Tuple>::value;
   }

   template <class T>
   concept function = is_specialization_v<T, std::function>;

   template <function T>
   struct meta<T>
   {
      static constexpr auto impl() noexcept
      {
         using fun = function_traits<T>;
         using R = typename fun::result_type;
         if constexpr (fun::N == 0 && named<R>) {
            return detail::join_v<chars<"std::function<">, name_v<R>, chars<"()>">>;
         }
         else if constexpr (fun::N == 0) {
            return chars<"std::function<void()>">;
         }
         else {
            return detail::join_v<chars<"std::function<">, name_v<R>, chars<"(">,
                                  detail::expander_v<chars<"">, typename fun::arguments>, chars<")>">>;
         }
      }

      static constexpr auto arr = impl(); // Give the joined string static storage
      static constexpr std::string_view name{arr.data(), arr.size()};
   };
}
