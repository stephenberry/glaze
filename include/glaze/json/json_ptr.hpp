// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <any>
#include <charconv>

#include "fast_float/fast_float.h"
#include "glaze/core/common.hpp"
#include "glaze/util/string_view.hpp"

namespace glaze
{
   namespace detail
   {
      template <class F, class T>
      requires array_t<std::decay_t<T>> || glaze_array_t<std::decay_t<T>> ||
         tuple_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr);

      template <class F, class T>
      requires nullable_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr);

      template <class F, class T>
      requires map_t<std::decay_t<T>> || glaze_object_t<T>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr);

      template <class F, class T>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr)
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         return false;
      }
      
      // TODO: compile time search for `~` and optimize if escape does not exist
      template <class F, class T>
      requires map_t<std::decay_t<T>> || glaze_object_t<T>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr)
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

         static thread_local auto key = []() {
            if constexpr (map_t<std::decay_t<T>>) {
               return typename std::decay_t<T>::key_type{};
            }
            else {
               return std::string{};
            }
         }();
         using key_t = decltype(key);
         static_assert(std::is_same_v<key_t, std::string> || num_t<key_t>);

         if constexpr (std::is_same_v<key_t, std::string>) {
            key.clear();
            size_t i = 1;
            for (; i < json_ptr.size(); ++i) {
               auto c = json_ptr[i];
               if (c == '/')
                  break;
               else if (c == '~') {
                  if (++i == json_ptr.size()) return false;
                  c = json_ptr[i];
                  if (c == '0')
                     c = '~';
                  else if (c == '1')
                     c = '/';
                  else
                     return false;
               }
               key.push_back(c);
            }
            json_ptr = json_ptr.substr(i);
         }
         else if constexpr (std::is_floating_point_v<key_t>) {
            auto [p, ec] = fast_float::from_chars(
               &json_ptr[1], json_ptr.data() + json_ptr.size(), key);
            if (ec != std::errc{}) return false;
            json_ptr = json_ptr.substr(p - json_ptr.data());
         }
         else {
            auto [p, ec] = std::from_chars(
               &json_ptr[1], json_ptr.data() + json_ptr.size(), key);
            if (ec != std::errc{}) return false;
            json_ptr = json_ptr.substr(p - json_ptr.data());
         }

         if constexpr (glaze_object_t<T>) {
            static constexpr auto frozen_map =
               glaze::detail::make_map<std::decay_t<T>>();
            const auto& member_it = frozen_map.find(frozen::string(key));
            if (member_it != frozen_map.end()) {
               return std::visit(
                  [&](auto&& member_ptr) {
                     return seek_impl(std::forward<F>(func), value.*member_ptr,
                                      json_ptr);
                  },
                  member_it->second);
            }
            else
               return false;
         }
         else {
            return seek_impl(std::forward<F>(func), value[key], json_ptr);
         }
      }

      template <class F, class T>
      requires array_t<std::decay_t<T>> || glaze_array_t<std::decay_t<T>> ||
         tuple_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr)
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

         size_t index{};
         auto [p, ec] = std::from_chars(
            &json_ptr[1], json_ptr.data() + json_ptr.size(), index);
         if (ec != std::errc{}) return false;
         json_ptr = json_ptr.substr(p - json_ptr.data());

         if constexpr (glaze_array_t<std::decay_t<T>>) {
            static constexpr auto member_array =
               glaze::detail::make_array<std::decay_t<T>>();
            if (index >= member_array.size()) return false;
            return std::visit(
               [&](auto&& member_ptr) {
                  return seek_impl(std::forward<F>(func), value.*member_ptr,
                                   json_ptr);
               },
               member_array[index]);
         }
         else if constexpr (tuple_t<std::decay_t<T>>) {
            if (index >= std::tuple_size_v<std::decay_t<T>>) return false;
            auto tuple_element_ptr = get_runtime(value, index);
            return std::visit(
               [&](auto&& element_ptr) {
                  return seek_impl(std::forward<F>(func), *element_ptr,
                                   json_ptr);
               },
               tuple_element_ptr);
         }
         else {
            return seek_impl(std::forward<F>(func),
                             *std::next(value.begin(), index), json_ptr);
         }
      }

      template <class F, class T>
      requires nullable_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, std::string_view json_ptr)
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (!value) return false;
         return seek_impl(std::forward<F>(func), *value, json_ptr);
      }
   }  // namespace detail

   // Call a function on an value at the location of a json_ptr
   template <class F, class T>
   bool seek(F&& func, T&& value, std::string_view json_ptr)
   {
      return detail::seek_impl(std::forward<F>(func), std::forward<T>(value),
                               json_ptr);
   }

   // Get a refrence to a value at the location of a json_ptr. Will throw if
   // value doesnt exist or is wrong type
   template <class V, class T>
   V& get(T&& root_value, std::string_view json_ptr)
   {
      V* result{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (!std::is_same_v<V, std::decay_t<decltype(val)>>)
               throw std::runtime_error("Called get on \"" +
                                        std::string(json_ptr) +
                                        "\" with wrong type");
            else if constexpr (!std::is_lvalue_reference_v<decltype(val)>)
               throw std::runtime_error(
                  " Called get on \"" + std::string(json_ptr) +
                  "\" that points to data that cannot be refrenced directly");
            else
               result = &val;
         },
         std::forward<T>(root_value), json_ptr);
      if (!result)
         throw std::runtime_error("Called get on \"" + std::string(json_ptr) +
                                  "\" which doesnt exist");
      return *result;
   }

   // Get a pointer to a value at the location of a json_ptr. Will return
   // nullptr if value doesnt exist or is wrong type
   template <class V, class T>
   V* get_if(T&& root_value, std::string_view json_ptr)
   {
      V* result{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_same_v<V, std::decay_t<decltype(val)>>) {
               result = &val;
            }
         },
         std::forward<T>(root_value), json_ptr);
      return result;
   }

   // Get a value at the location of a json_ptr. Will throw if
   // value doesnt exist or is not asignable or is a narrowing conversion.
   template <class V, class T>
   V get_value(T&& root_value, std::string_view json_ptr)
   {
      V result{};
      bool found{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_assignable_v<V, decltype(val)> &&
                          detail::non_narrowing_convertable<std::decay_t<decltype(val)>, V>) {
               found = true;
               result = val;
            }
            else {
               throw std::runtime_error("Called get_value on \"" +
                                        std::string(json_ptr) +
                                        "\" with wrong type");
            }
         },
         std::forward<T>(root_value), json_ptr);
      if (!found)
         throw std::runtime_error("Called get_value on \"" + std::string(json_ptr) +
                                  "\" which doesnt exist");
      return result;
   }

   // Assign to a value at the location of a json_ptr with respect to the root_value
   // if assignable and not a narrowing conversion
   template <class T, class V>
   bool set(T&& root_value, const std::string_view json_ptr, V&& value)
   {
      bool result{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_assignable_v<decltype(val),
                                               decltype(value)> &&
                          detail::non_narrowing_convertable<
                             std::decay_t<decltype(value)>,
                             std::decay_t<decltype(val)>>) {
               result = true;
               val = value;
            }
         },
         std::forward<T>(root_value), json_ptr);
      return result;
   }
   
   inline constexpr std::pair<sv, sv> tokenize_json_ptr(sv s)
   {
       s.remove_prefix(1);
       if (s.find('/') == std::string::npos) {
           return { s, "" };
       }
       const auto i = s.find_first_of('/');
       return { s.substr(0, i), s.substr(i, s.size() - i) };
   }

   template <auto& Str>
   inline constexpr auto split_json_ptr()
   {
       constexpr auto N = std::count(Str.begin(), Str.end(), '/');
       std::array<sv, N> arr{};
       sv s = Str;
       for (auto i = 0; i < N; ++i) {
           std::tie(arr[i], s) = tokenize_json_ptr(s);
       }
       return arr;
   }
}  // namespace glaze
