#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <deque>

#include "glaze/util/type_traits.hpp"
#include "glaze/util/string_view.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      template <class Data>
      struct recorder_assigner
      {
         /*recorder_assigner(Data& data) : data(data) {}
         recorder_assigner(const recorder_assigner&) = default;
         recorder_assigner(recorder_assigner&&) = default;
         recorder_assigner& operator=(const recorder_assigner&) = default;
         recorder_assigner& operator=(recorder_assigner&&) = default;*/
         
         Data& data;
         sv name{};
         
         template <class T>
         void operator=(T& ref) {
            using container_type = std::decay_t<decltype(data[0].second.first)>;
            data.emplace_back(std::pair{ name, std::make_pair(container_type{std::deque<T>{}}, &ref) });
         }
      };
   }

   /// <summary>
   /// recorder for saving state over the course of a run
   /// deques are used to avoid reallocation for large amounts of data as the recording length is typically unknown
   /// </summary>
   template <class... Ts>
   struct recorder
   {
      using container_type = std::variant<std::deque<Ts>...>;

      std::deque<std::pair<std::string, std::pair<container_type, void*>>>
         data;
      
      auto operator[](const sv name) {
         return detail::recorder_assigner<decltype(data)>{ data, name };
      }

      void update()
      {
         for (auto& [name, value] : data) {
            auto* ptr = value.second;
            std::visit(
               [&](auto&& container) {
                  using ContainerType = std::decay_t<decltype(container)>;
                  using T = typename ContainerType::value_type;

                  container.emplace_back(*static_cast<T*>(ptr));
               },
               value.first);
         }
      }
   };
}
