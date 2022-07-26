#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>

#include "glaze/utility.hpp"

namespace glaze
{
   template <class... T>
   auto to_vector(std::variant<T...>&&)
   {
      return std::variant<std::vector<T>...>{};
   }

   template <class... T>
   auto to_variant_pointer(std::variant<T...>&&)
   {
      return std::variant<T*...>{};
   }

   template <is_variant variant_t>
   struct recorder
   {
      using variant_p = decltype(to_variant_pointer(std::declval<variant_t>()));
      using container_type = decltype(to_vector(std::declval<variant_t>()));

      std::vector<std::pair<std::string, std::pair<container_type, void*>>>
         data;

      void register_variable(const std::string& name, variant_p var)
      {
         std::visit(
            [&](auto&& pointer) {
               using T = std::decay_t<decltype(*pointer)>;
               data.emplace_back(
                  name,
                  std::make_pair(container_type{std::vector<T>{}}, pointer));
            },
            var);
      }

      void update()
      {
         for (auto& [name, value] : data) {
            std::visit(
               [&](auto&& container) {
                  using T = std::decay_t<decltype(container)>::value_type;

                  container.emplace_back(*static_cast<T*>(value.second));
               },
               value.first);
         }
      }
   };
}
