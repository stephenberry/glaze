// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <memory>
#include <optional>

#include "glaze/core/common.hpp"

namespace glz
{
   template <typename T>
   struct unwrapped_nullable
   {
      using type = T;
   };

   template <typename T>
   struct unwrapped_nullable<T*>
   {
      using type = T;
   };

   template <typename T>
   struct unwrapped_nullable<std::optional<T>>
   {
      using type = T;
   };

   template <typename T>
   struct unwrapped_nullable<std::unique_ptr<T>>
   {
      using type = T;
   };

   template <typename T>
   struct unwrapped_nullable<std::shared_ptr<T>>
   {
      using type = T;
   };

   template <typename T>
   using unwrapped_nullable_t = typename unwrapped_nullable<T>::type;

   template <nullable_like T>
   bool nullable_emplace(T& nullable)
   {
      if constexpr (optional_like<T>) {
         nullable.emplace();
      }
      else if constexpr (is_specialization_v<T, std::unique_ptr>) {
         nullable = std::make_unique<typename T::element_type>();
      }
      else if constexpr (is_specialization_v<T, std::shared_ptr>) {
         nullable = std::make_shared<typename T::element_type>();
      }
      else if constexpr (requires { nullable.emplace(); }) {
         nullable.emplace();
      }
      else if constexpr (constructible<T>) {
         nullable = meta_construct_v<T>();
      }
      else {
         return false;
      }
      return true;
   }
}