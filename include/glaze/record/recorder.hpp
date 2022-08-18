#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>

#include "glaze/util/type_traits.hpp"

namespace glaze
{
   template <class... T>
   auto to_deque(std::variant<T...>&&)
   {
      return std::variant<std::monostate, std::deque<T>...>{};
   }

   template <class... T>
   auto to_variant_pointer(std::variant<T...>&&)
   {
      return std::variant<T*...>{};
   }
   
   template <class Data>
   struct assigner
   {
      assigner(Data& data) : data(data) {}
      
      Data& data;
      
      template <class T>
      void operator=(T& ref) {
         data = std::make_pair(typename Data::first_type{std::deque<T>{}}, &ref);
      }
   };

   template <is_variant variant_t>
   struct recorder
   {
      using variant_p = decltype(to_variant_pointer(std::declval<variant_t>()));
      using container_type = decltype(to_deque(std::declval<variant_t>()));

      std::deque<std::pair<std::string, std::pair<container_type, void*>>>
         data;
      
      auto operator[](const std::string_view name) {
         auto& d = data.emplace_back(name, std::make_pair(std::monostate{}, nullptr));
         return assigner<std::pair<container_type, void*>>{ d.second };
      }

      void update()
      {
         for (auto& [name, value] : data) {
            auto* ptr = value.second;
            std::visit(
               [&](auto&& container) {
                  using ContainerType = std::decay_t<decltype(container)>;
                  if constexpr (std::same_as<ContainerType, std::monostate>) {
                     throw std::runtime_error("recorder::update container is monostate");
                  }
                  else {
                     using T = typename ContainerType::value_type;

                     container.emplace_back(*static_cast<T*>(ptr));
                  }
               },
               value.first);
         }
      }
   };
}
