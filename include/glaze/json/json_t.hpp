// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <map>
#include <variant>
#include <vector>

#include "glaze/core/meta.hpp"
#include "glaze/util/expected.hpp"

#ifdef _MSC_VER
// Turn off broken MSVC warning for "declaration of 'v' hides previous local declaration"
#pragma warning(push)
#pragma warning(disable : 4456)
#endif

namespace glz
{
   // Generic json type.
   struct json_t
   {
      using array_t = std::vector<json_t>;
      using object_t = std::map<std::string, json_t, std::less<>>;
      using null_t = std::nullptr_t;
      using val_t = std::variant<null_t, double, std::string, bool, array_t, object_t>;
      val_t data{};

      template <class T>
      [[nodiscard]] T& get()
      {
         return std::get<T>(data);
      }

      template <class T>
      [[nodiscard]] const T& get() const
      {
         return std::get<T>(data);
      }

      template <class T>
      [[nodiscard]] T* get_if() noexcept
      {
         return std::get_if<T>(&data);
      }

      template <class T>
      [[nodiscard]] const T* get_if() const noexcept
      {
         return std::get_if<T>(&data);
      }

      template <class T>
      [[nodiscard]] bool holds() const noexcept
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
            glaze_error("Key not found.");
         }
         return iter->second;
      }

      [[nodiscard]] json_t& at(std::convertible_to<std::string_view> auto&& key) { return operator[](key); }

      [[nodiscard]] const json_t& at(std::convertible_to<std::string_view> auto&& key) const { return operator[](key); }

      [[nodiscard]] bool contains(std::convertible_to<std::string_view> auto&& key) const
      {
         if (!holds<object_t>()) return false;
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         return iter != object.end();
      }

      explicit operator bool() const { return !std::holds_alternative<null_t>(data); }

      val_t* operator->() noexcept { return &data; }

      val_t& operator*() noexcept { return data; }

      const val_t& operator*() const noexcept { return data; }

      void reset() noexcept { data = null_t{}; }

      json_t() = default;
      json_t(const json_t&) = default;
      json_t& operator=(const json_t&) = default;
      json_t(json_t&&) = default;
      json_t& operator=(json_t&&) = default;

      template <class T>
         requires std::convertible_to<T, val_t> && (!std::same_as<json_t, std::decay_t<T>>)
      json_t(T&& val)
      {
         data = val;
      }

      template <class T>
         requires std::convertible_to<T, double> && (!std::same_as<json_t, std::decay_t<T>>) &&
                  (!std::convertible_to<T, val_t>)
      json_t(T&& val)
      {
         data = static_cast<double>(val);
      }

      json_t(std::initializer_list<std::pair<const char*, json_t>>&& obj)
      {
         // TODO try to see if there is a beter way to do this initialization withought copying the json_t
         // std::string in std::initializer_list<std::pair<const std::string, json_t>> would match with {"literal",
         // "other_literal"} So we cant use std::string or std::string view. Luckily const char * will not match with
         // {"literal", "other_literal"} but then we have to copy the data from the initializer list data =
         // object_t(obj); // This is what we would use if std::initializer_list<std::pair<const std::string, json_t>>
         // worked
         data.emplace<object_t>();
         auto& data_obj = std::get<object_t>(data);
         for (auto&& pair : obj) {
            data_obj.emplace(pair.first, std::move(pair.second));
         }
      }

      // Prevent conflict with object initializer list
      template <bool deprioritize = true>
      json_t(std::initializer_list<json_t>&& arr)
      {
         data.emplace<array_t>(std::move(arr));
      }

      template <class T>
      [[nodiscard]] T as() const
      {
         // Prefer get becuase it returns a reference
         return get<T>();
      }

      template <class T>
         requires std::convertible_to<double, T>
      [[nodiscard]] T as() const
      {
         // Can be used for int and the like
         return static_cast<T>(get<double>());
      }

      template <class T>
         requires std::convertible_to<std::string, T>
      [[nodiscard]] T as() const
      {
         // Can be used for string_view and the like
         return get<std::string>();
      }

      [[nodiscard]] bool is_array() const noexcept { return holds<json_t::array_t>(); }

      [[nodiscard]] bool is_object() const noexcept { return holds<json_t::object_t>(); }

      [[nodiscard]] bool is_number() const noexcept { return holds<double>(); }

      [[nodiscard]] bool is_string() const noexcept { return holds<std::string>(); }

      [[nodiscard]] bool is_boolean() const noexcept { return holds<bool>(); }

      [[nodiscard]] bool is_null() const noexcept { return holds<std::nullptr_t>(); }

      [[nodiscard]] array_t& get_array() noexcept { return get<array_t>(); }
      [[nodiscard]] const array_t& get_array() const noexcept { return get<array_t>(); }

      [[nodiscard]] object_t& get_object() noexcept { return get<object_t>(); }
      [[nodiscard]] const object_t& get_object() const noexcept { return get<object_t>(); }

      [[nodiscard]] double& get_number() noexcept { return get<double>(); }
      [[nodiscard]] const double& get_number() const noexcept { return get<double>(); }

      [[nodiscard]] std::string& get_string() noexcept { return get<std::string>(); }
      [[nodiscard]] const std::string& get_string() const noexcept { return get<std::string>(); }

      [[nodiscard]] bool& get_boolean() noexcept { return get<bool>(); }
      [[nodiscard]] const bool& get_boolean() const noexcept { return get<bool>(); }

      // empty() returns true if the value is an empty JSON object, array, or string, or a null value
      // otherwise returns false
      [[nodiscard]] bool empty() const noexcept
      {
         if (auto* v = get_if<object_t>()) {
            return v->empty();
         }
         else if (auto* v = get_if<array_t>()) {
            return v->empty();
         }
         else if (auto* v = get_if<std::string>()) {
            return v->empty();
         }
         else if (is_null()) {
            return true;
         }
         else {
            return false;
         }
      }

      // returns the count of items in an object or an array, or the size of a string, otherwise returns zero
      [[nodiscard]] size_t size() const noexcept
      {
         if (auto* v = get_if<object_t>()) {
            return v->size();
         }
         else if (auto* v = get_if<array_t>()) {
            return v->size();
         }
         else if (auto* v = get_if<std::string>()) {
            return v->size();
         }
         else {
            return 0;
         }
      }
   };

   [[nodiscard]] inline bool is_array(const json_t& value) { return value.is_array(); }

   [[nodiscard]] inline bool is_object(const json_t& value) { return value.is_object(); }

   [[nodiscard]] inline bool is_number(const json_t& value) { return value.is_number(); }

   [[nodiscard]] inline bool is_string(const json_t& value) { return value.is_string(); }

   [[nodiscard]] inline bool is_boolean(const json_t& value) { return value.is_boolean(); }

   [[nodiscard]] inline bool is_null(const json_t& value) { return value.is_null(); }
}

template <>
struct glz::meta<glz::json_t>
{
   static constexpr std::string_view name = "glz::json_t";
   using T = glz::json_t;
   static constexpr auto value = &T::data;
};

#ifdef _MSC_VER
// restore disabled warning
#pragma warning(pop)
#endif
