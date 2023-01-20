// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <variant>
#include <vector>
#include <map>
#include <stddef.h>
#include <stdexcept>
#include <concepts>

namespace glz
{
   // Generic json type.
   struct json_t
   {
      using null_t = std::monostate;
      using array_t = std::vector<json_t>;
      using object_t = std::map<std::string, json_t, std::less<>>;
      using val_t = std::variant<null_t, double, std::string, bool, array_t, object_t>;
      val_t data{};

      template <class T>
      T& get()
      {
         return std::get<T>(data);
      }

      template <class T>
      T* get_if()
      {
         return std::get_if<T>(data);
      }

      template <class T>
      bool holds()
      {
         return std::holds_alternative<T>(data);
      }

      json_t& operator[](std::integral auto&& index) { return std::get<array_t>(data)[index]; }

      json_t& operator[](std::convertible_to<std::string_view> auto&& key)
      {
         //[] operator for maps does not support heterogeneous lookups yet
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            iter = object.insert(std::make_pair(std::string(key), json_t{})).first;
         }
         return iter->second;
      }

      bool contains(std::convertible_to<std::string_view> auto&& key)
      {
         if (!holds<object_t>()) return false;
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            return true;
         }
         return false;
      }

      operator bool() const { return !std::holds_alternative<null_t>(data); }

      val_t* operator->() { return &data; }

      val_t& operator*() { return data; }

      const val_t& operator*() const { return data; }
   };
}
