// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/functional.hpp"
#include "glaze/api/std/string.hpp"
#include "glaze/api/std/unordered_map.hpp"
#include "glaze/api/std/map.hpp"
#include "glaze/api/std/vector.hpp"
#include "glaze/api/std/array.hpp"
#include "glaze/api/std/tuple.hpp"
#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/list.hpp"
#include "glaze/api/std/optional.hpp"
#include "glaze/api/std/shared_ptr.hpp"
#include "glaze/api/type_support.hpp"
#include "glaze/api/api.hpp"
#include "glaze/core/format.hpp"

#include "glaze/glaze.hpp"

#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   template <class UserType>
   struct impl : api
   {
      UserType user{};
      
      void* get(const sv path, const sv type_hash) noexcept override
      {
         return get_void(user, path, type_hash);
      }

      bool read(const uint32_t format, const sv path,
                 const sv data) noexcept override
      {
         if (format == json) {
            return detail::seek_impl(
               [&](auto&& val) { glz::read<opts{}>(val, data);
               },
               user, path);
         }
         else {
            return detail::seek_impl(
               [&](auto&& val) { glz::read<opts{.format = binary}>(val, data);
               },
               user, path);
         }
      }

      bool write(const uint32_t format, const sv path,
                std::string& data) noexcept override
      {
         if (format == json) {
            return detail::seek_impl(
               [&](auto&& val) {
                  glz::write_json(val, data); }, user,
               path);
         }
         else {
            return detail::seek_impl(
               [&](auto&& val) { glz::write_binary(val, data);
               },
               user, path);
         }
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
               using V = std::decay_t<decltype(val)>;
               static constexpr auto h = glz::hash<V>();
               if (h == type_hash) [[likely]] {
                  result = &val;
               }
               else [[unlikely]] {
                  error = "mismatching types";
                  error += ", expected: " + std::string(glz::name_v<T>);
               }
            },
            std::forward<T>(root_value), json_ptr);
         if (error.empty() && result == nullptr) {
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
