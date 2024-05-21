// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"

namespace glz::detail
{
   template <class F, class T>
      requires glaze_value_t<T>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

   template <class F, class T>
      requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || array_t<std::decay_t<T>> ||
               is_std_tuple<std::decay_t<T>>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

   template <class F, class T>
      requires nullable_t<std::decay_t<T>>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

   template <class F, class T>
      requires readable_map_t<std::decay_t<T>> || glaze_object_t<T> || reflectable<T>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

   template <class F, class T>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
   {
      if (json_ptr.empty()) {
         func(value);
         return true;
      }
      return false;
   }

   // TODO: compile time search for `~` and optimize if escape does not exist
   template <class F, class T>
      requires readable_map_t<std::decay_t<T>> || glaze_object_t<T> || reflectable<T>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
   {
      if (json_ptr.empty()) {
         func(value);
         return true;
      }
      if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

      static thread_local auto key = []() {
         if constexpr (writable_map_t<std::decay_t<T>>) {
            return typename std::decay_t<T>::key_type{};
         }
         else {
            return std::string{};
         }
      }();
      using Key = std::decay_t<decltype(key)>;
      static_assert(std::is_same_v<Key, std::string> || num_t<Key>);

      if constexpr (std::is_same_v<Key, std::string>) {
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
      else if constexpr (std::is_floating_point_v<Key>) {
         auto it = json_ptr.data();
         auto s = parse_float<Key>(key, it);
         if (!s) return false;
         json_ptr = json_ptr.substr(size_t(it - json_ptr.data()));
      }
      else {
         auto [p, ec] = std::from_chars(&json_ptr[1], json_ptr.data() + json_ptr.size(), key);
         if (ec != std::errc{}) return false;
         json_ptr = json_ptr.substr(p - json_ptr.data());
      }

      if constexpr (glaze_object_t<T> || reflectable<T>) {
         decltype(auto) frozen_map = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
#if ((defined _MSC_VER) && (!defined __clang__))
               static thread_local auto cmap = make_map<T>();
#else
               static thread_local constinit auto cmap = make_map<T>();
#endif
               populate_map(value, cmap); // Function required for MSVC to build
               return cmap;
            }
            else {
               static constexpr auto cmap = make_map<T>();
               return cmap;
            }
         }();

         const auto& member_it = frozen_map.find(key);
         if (member_it != frozen_map.end()) [[likely]] {
            return std::visit(
               [&](auto&& member_ptr) {
                  return seek_impl(std::forward<F>(func), get_member(value, member_ptr), json_ptr);
               },
               member_it->second);
         }
         else [[unlikely]] {
            return false;
         }
      }
      else {
         return seek_impl(std::forward<F>(func), value[key], json_ptr);
      }
   }

   template <class F, class T>
      requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || array_t<std::decay_t<T>> ||
               is_std_tuple<std::decay_t<T>>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
   {
      if (json_ptr.empty()) {
         func(value);
         return true;
      }
      if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

      size_t index{};
      auto [p, ec] = std::from_chars(&json_ptr[1], json_ptr.data() + json_ptr.size(), index);
      if (ec != std::errc{}) return false;
      json_ptr = json_ptr.substr(p - json_ptr.data());

      if constexpr (glaze_array_t<std::decay_t<T>>) {
         static constexpr auto member_array = glz::detail::make_array<decay_keep_volatile_t<T>>();
         if (index >= member_array.size()) return false;
         return std::visit(
            [&](auto&& member_ptr) {
               return seek_impl(std::forward<F>(func), get_member(value, member_ptr), json_ptr);
            },
            member_array[index]);
      }
      else if constexpr (tuple_t<std::decay_t<T>> || is_std_tuple<std::decay_t<T>>) {
         if (index >= glz::tuple_size_v<std::decay_t<T>>) return false;
         auto tuple_element_ptr = get_runtime(value, index);
         return std::visit(
            [&](auto&& element_ptr) { return seek_impl(std::forward<F>(func), *element_ptr, json_ptr); },
            tuple_element_ptr);
      }
      else {
         return seek_impl(std::forward<F>(func), *std::next(value.begin(), index), json_ptr);
      }
   }

   template <class F, class T>
      requires nullable_t<std::decay_t<T>>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
   {
      if (json_ptr.empty()) {
         func(value);
         return true;
      }
      if (!value) return false;
      return seek_impl(std::forward<F>(func), *value, json_ptr);
   }

   template <class F, class T>
      requires glaze_value_t<T>
   bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
   {
      decltype(auto) member = get_member(value, meta_wrapper_v<std::remove_cvref_t<T>>);
      if (json_ptr.empty()) {
         func(member);
         return true;
      }
      return seek_impl(std::forward<F>(func), member, json_ptr);
   }
}
