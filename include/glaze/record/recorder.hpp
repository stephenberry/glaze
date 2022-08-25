#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <deque>

#include "glaze/util/type_traits.hpp"
#include "glaze/util/string_view.hpp"
#include "glaze/util/variant.hpp"

namespace glaze
{
   namespace detail
   {
      template <class... T>
      auto to_variant_deque(std::variant<T...>&&)
      {
         return std::variant<std::monostate, std::deque<T>...>{};
      }
      
      template <class Data>
      struct recorder_assigner
      {
         recorder_assigner(Data& data) : data(data) {}
         recorder_assigner(const recorder_assigner&) = default;
         recorder_assigner(recorder_assigner&&) = default;
         recorder_assigner& operator=(const recorder_assigner&) = default;
         recorder_assigner& operator=(recorder_assigner&&) = default;
         
         Data& data;
         
         template <class T>
         void operator=(T& ref) {
            data = std::make_pair(typename Data::first_type{std::deque<T>{}}, &ref);
         }
      };
   }

   /// <summary>
   /// recorder for saving state over the course of a run
   /// deques are used to avoid reallocation for large amounts of data as the recording length is typically unknown
   /// </summary>
   template <is_variant variant_t>
   struct recorder
   {
      using variant_p = decltype(to_variant_pointer(std::declval<variant_t>()));
      using container_type = decltype(detail::to_variant_deque(std::declval<variant_t>()));

      std::deque<std::pair<std::string, std::pair<container_type, void*>>>
         data;
      
      auto operator[](const sv name) {
         auto& d = data.emplace_back(name, std::make_pair(std::monostate{}, nullptr));
         return detail::recorder_assigner<std::pair<container_type, void*>>{ d.second };
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
