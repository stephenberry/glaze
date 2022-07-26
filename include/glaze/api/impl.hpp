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

      // Get a pointer to a value at the location of a json_ptr. Will return
      // nullptr if value doesnt exist or is wrong type
      template <class T>
      void* get_void(T&& root_value, std::string_view json_ptr,
                     const std::string_view type_hash)
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
      
      void* get(const std::string_view path, const std::string_view type_hash) noexcept override
      {
         return get_void(interface, path, type_hash);
      }
      
      virtual constexpr const version_type version() const noexcept override {
         return glaze::trait<Interface>::version;
      }
      
      constexpr const std::string_view version_sv() const noexcept override {
         return glaze::trait<Interface>::version_sv;
      }
      
      constexpr const std::string_view hash() const noexcept override {
         return glaze::hash<Interface>();
      }
   };
   
   template <class T>
   inline constexpr std::shared_ptr<impl<T>> make_impl() {
      return std::make_shared<impl<T>>();
   }
}
