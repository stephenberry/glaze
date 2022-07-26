// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/functional.hpp"
#include "glaze/api/std/string.hpp"
#include "glaze/api/std/unordered_map.hpp"
#include "glaze/api/std/vector.hpp"
#include "glaze/api/type_support.hpp"
#include "glaze/api/api.hpp"

#include "glaze/glaze.hpp"

namespace glaze
{
   template <class Interface>
   struct impl : api
   {
      Interface interface{};

      using sv = std::string_view;
      
      void* get(const sv path, const sv type_hash) noexcept override
      {
         return get_void(interface, path, type_hash);
      }
      
      virtual constexpr const version_type version() const noexcept override {
         return glaze::trait<Interface>::version;
      }
      
      constexpr const sv version_sv() const noexcept override {
         return glaze::trait<Interface>::version_sv;
      }
      
      constexpr const sv hash() const noexcept override {
         return glaze::hash<Interface>();
      }

      protected:
      // Get a pointer to a value at the location of a json_ptr. Will return
      // nullptr if value doesnt exist or is wrong type
      template <class T>
      void* get_void(T&& root_value, const sv json_ptr, const sv type_hash)
      {
         void* result{};
         detail::seek_impl(
            [&](auto&& val) {
               using T = std::decay_t<decltype(val)>;
               static constexpr auto h = glaze::hash<T>();
               if (h == type_hash) [[likely]] {
                  result = &val;
               }
               else [[unlikely]] {
                  error = "mismatching types";
                  error += ", expected: " + std::string(glaze::name<T>);
               }
            },
            std::forward<T>(root_value), json_ptr);
         if (result == nullptr) {
            error = "invalid path";
         }
         return result;
      }
   };
   
   template <class T>
   inline constexpr auto make_api() {
      return std::make_shared<impl<T>>();
   }
}
