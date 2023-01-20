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
   struct generic_json
   {
      using null_t = std::monostate;
      using array_t = std::vector<generic_json>;
      using object_t = std::map<std::string, generic_json, std::less<>>;
      using val_t = std::variant<null_t, double, std::string, bool, array_t, object_t>;
      val_t data{};

      template <class T>
      T& get()
      {
         return std::get<T>(data);
      }

      generic_json& operator[](std::integral auto&& index) { return std::get<array_t>(data)[index]; }

      generic_json& operator[](std::convertible_to<std::string_view> auto&& key)
      {
         //[] operator for maps does not support heterogeneous lookups yet
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            iter = object.insert(std::make_pair(std::string(key), generic_json{})).first;
         }
         return iter->second;
      }

      operator bool() const { return !std::holds_alternative<null_t>(data); }

      val_t* operator->() { return &data; }

      val_t& operator*() { return data; }

      const val_t& operator*() const { return data; }
   };
}
