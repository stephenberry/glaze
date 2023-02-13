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
      using array_t = std::vector<json_t>;
      using object_t = std::map<std::string, json_t, std::less<>>;
      using null_t = double*;
      using val_t = std::variant<null_t, double, std::string, bool, array_t, object_t>;
      val_t data{};

      template <class T>
      T& get()
      {
         return std::get<T>(data);
      }

      template <class T>
      const T& get() const
      {
         return std::get<T>(data);
      }

      template <class T>
      T* get_if() noexcept
      {
         return std::get_if<T>(data);
      }

      template <class T>
      const T* get_if() const noexcept
      {
         return std::get_if<T>(&data);
      }

      template <class T>
      bool holds() const noexcept
      {
         return std::holds_alternative<T>(data);
      }

      json_t& operator[](std::integral auto&& index) { return std::get<array_t>(data)[index]; }

      const json_t& operator[](std::integral auto&& index) const { return std::get<array_t>(data)[index]; }

      json_t& operator[](std::convertible_to<std::string_view> auto&& key)
      {
         //[] operator for maps does not support heterogeneous lookups yet
         if (holds<null_t>()) data = object_t{};
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            iter = object.insert(std::make_pair(std::string(key), json_t{})).first;
         }
         return iter->second;
      }

      const json_t& operator[](std::convertible_to<std::string_view> auto&& key) const
      {
         //[] operator for maps does not support heterogeneous lookups yet
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            throw std::runtime_error("Key not found.");
         }
         return iter->second;
      }

      bool contains(std::convertible_to<std::string_view> auto&& key) const
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

      val_t* operator->() noexcept { return &data; }

      val_t& operator*() noexcept { return data; }

      const val_t& operator*() const noexcept { return data; }

      void reset() noexcept { data = null_t{}; }

      json_t() = default;

      template <class T>
      requires std::convertible_to<T, val_t> && (!std::same_as<json_t, std::decay_t<T>>)
      json_t(T&& val)
      {
         data = val;
      }

      template <class T>
      requires std::convertible_to<T, double> &&(!std::convertible_to<T, val_t>) json_t(T&& val) {
         data = static_cast<double>(val);
      }

      json_t(std::initializer_list<std::pair<const std::string, json_t>>&& obj) { data = object_t(obj); }

      // Prevent conflict with object initializer list
      template <bool deprioritize = true>
      json_t(std::initializer_list<json_t>&& arr)
      {
         data = array_t(arr);
      }
   };
}
