// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <any>
#include <algorithm>
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
            static constexpr auto frozen_map = detail::make_map<T>();
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
   
   inline constexpr size_t json_ptr_depth(const auto s)
   {
       size_t count = 0;
       for (size_t i = 0; (i = s.find('/', i)) != std::string::npos; ++i) {
           ++count;
       }
       return count;
   }
   
   // TODO: handle ~ and / characters for full JSON pointer support
   inline constexpr std::pair<sv, sv> tokenize_json_ptr(sv s)
   {
      if (s.empty()) {
         return { "", "" };
      }
       s.remove_prefix(1);
       if (s.find('/') == std::string::npos) {
           return { s, "" };
       }
       const auto i = s.find_first_of('/');
       return { s.substr(0, i), s.substr(i, s.size() - i) };
   }
   
   inline constexpr auto first_key(sv s)
   {
      return tokenize_json_ptr(s).first;
   }
   
   inline constexpr auto remove_first_key(sv s)
   {
      return tokenize_json_ptr(s).second;
   }
   
   // TODO: handle ~ and / characters for full JSON pointer support
   template <auto& Str>
   inline constexpr auto split_json_ptr()
   {
       constexpr auto N = std::count(Str.begin(), Str.end(), '/');
       std::array<sv, N> arr;
       sv s = Str;
       for (auto i = 0; i < N; ++i) {
           std::tie(arr[i], s) = tokenize_json_ptr(s);
       }
       return arr;
   }
   
   inline constexpr auto json_ptrs(auto&&... args)
   {
      return std::array{sv{args}...};
   }
   
   // must copy to allow mutation in constexpr context
   inline constexpr auto sort_json_ptrs(auto arr)
   {
       std::sort(arr.begin(), arr.end());
       return arr;
   }
   
   // input array must be sorted
   inline constexpr auto group_json_ptrs_impl(const auto& arr)
   {
      constexpr auto N = std::tuple_size_v<std::decay_t<decltype(arr)>>;
      
      std::array<sv, N> first_keys;
      std::transform(arr.begin(), arr.end(), first_keys.begin(), first_key);
      
      std::array<sv, N> unique_keys{};
      const auto it = std::unique_copy(first_keys.begin(), first_keys.end(), unique_keys.begin());
      
      const auto n_unique = static_cast<size_t>(std::distance(unique_keys.begin(), it));
      
      std::array<size_t, N> n_items_per_group{};
      
      for (size_t i = 0; i < n_unique; ++i) {
         n_items_per_group[i] = nano::ranges::count(first_keys, unique_keys[i]);
      }
      
      return std::tuple{ n_items_per_group, n_unique, unique_keys };
   }
   
   template <auto Arr, std::size_t...Is>
   inline constexpr auto make_arrays(std::index_sequence<Is...>)
   {
      return std::make_tuple(std::pair{ sv{}, std::array<sv, Arr[Is]>{} }...);
   };

   template <size_t N, auto& Arr>
   inline constexpr auto sub_group(const auto start)
   {
      constexpr auto arr = Arr; // Msvc currently generates an internal compiler error otherwise
      std::array<sv, N> ret;
      std::transform(arr.begin() + start, arr.begin() + start + N, ret.begin(), remove_first_key);
      return ret;
   }
   
   template <auto& Arr>
   inline constexpr auto group_json_ptrs()
   {
      constexpr auto arr =
         Arr;  // Msvc currently generates an internal compiler error otherwise
      constexpr auto group_info = group_json_ptrs_impl(arr);
      constexpr auto n_items_per_group = std::get<0>(group_info);
      constexpr auto n_unique = std::get<1>(group_info);
      constexpr auto unique_keys = std::get<2>(group_info);
      
      auto arrs = make_arrays<n_items_per_group>(std::make_index_sequence<n_unique>{});
      size_t start{};

      for_each<n_unique>([&](auto I) {
         // NOTE: VS 2019 wont let us grab n_items_per_group as constexpr unless
         // its static but this is a constexpr func This is fixed in VS 2022
         constexpr size_t n_items =
            std::tuple_size_v<std::decay_t<decltype(std::get<I>(arrs).second)>>;

         std::get<I>(arrs).first = unique_keys[I];
         std::get<I>(arrs).second = glaze::sub_group<n_items, Arr>(start);
         start += n_items_per_group[I];
      });
      
      return arrs;
   }
}  // namespace glaze
