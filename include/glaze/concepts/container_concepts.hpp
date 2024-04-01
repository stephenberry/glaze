// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <concepts>
#include <cstdint>
#include <utility>

namespace glz::detail
{
   template <typename T>
   concept complex_t = requires(T a, T b) {
      {
         a.real()
      } -> std::convertible_to<typename T::value_type>;
      {
         a.imag()
      } -> std::convertible_to<typename T::value_type>;
      {
         T(a.real(), a.imag())
      } -> std::same_as<T>;
      {
         a + b
      } -> std::same_as<T>;
      {
         a - b
      } -> std::same_as<T>;
      {
         a* b
      } -> std::same_as<T>;
      {
         a / b
      } -> std::same_as<T>;
   };

   template <class T>
   concept pair_t = requires(T pair) {
      {
         pair.first
      } -> std::same_as<typename T::first_type&>;
      {
         pair.second
      } -> std::same_as<typename T::second_type&>;
   };

   template <class T>
   concept emplaceable = requires(T container) {
      {
         container.emplace(std::declval<typename T::value_type>())
      };
   };

   template <class T>
   concept push_backable = requires(T container) {
      {
         container.push_back(std::declval<typename T::value_type>())
      };
   };

   template <class T>
   concept emplace_backable = requires(T container) {
      {
         container.emplace_back()
      } -> std::same_as<typename T::reference>;
   };

   template <class T>
   concept resizeable = requires(T container) { container.resize(0); };

   template <class T>
   concept erasable = requires(T container) { container.erase(container.cbegin(), container.cend()); };

   template <class T>
   concept has_size = requires(T container) { container.size(); };

   template <class T>
   concept has_empty = requires(T container) {
      {
         container.empty()
      } -> std::convertible_to<bool>;
   };

   template <class T>
   concept has_data = requires(T container) { container.data(); };

   template <class T>
   concept has_push_back = requires(T t, typename T::value_type v) { t.push_back(v); };

   template <class T>
   concept has_reserve = requires(T t) { t.reserve(size_t(1)); };

   template <class T>
   concept has_capacity = requires(T t) {
      {
         t.capacity()
      } -> std::integral;
   };

   template <class T>
   concept contiguous = has_size<T> && has_data<T>;

   template <class T>
   concept accessible = requires(T container) {
      {
         container[size_t{}]
      } -> std::same_as<typename T::reference>;
   };

   template <class T>
   concept map_subscriptable = requires(T container) {
      {
         container[std::declval<typename T::key_type>()]
      } -> std::same_as<typename T::mapped_type&>;
   };

   template <typename T>
   concept string_like = requires(T s) {
      {
         s.size()
      } -> std::integral;
      {
         s.data()
      };
      {
         s.empty()
      } -> std::convertible_to<bool>;
      {
         s.substr(0, 1)
      };
   };

   template <typename T>
   concept is_bitset = requires(T bitset) {
      bitset.flip();
      bitset.set(0);
      requires string_like<decltype(bitset.to_string())>;
      {
         bitset.count()
      } -> std::same_as<size_t>;
   };

   template <class T>
   concept is_span = requires(T t) {
      T::extent;
      typename T::element_type;
   };

   template <class T>
   concept is_dynamic_span = T::extent == static_cast<size_t>(-1);

   template <class Map, class Key>
   concept findable = requires(Map& map, const Key& key) { map.find(key); };

   template <class T>
   concept filesystem_path = requires(T path) {
      path.native();
      requires string_like<decltype(path.string())>;
      path.filename();
      path.extension();
      path.parent_path();
      {
         path.has_filename()
      } -> std::convertible_to<bool>;
      {
         path.has_extension()
      } -> std::convertible_to<bool>;
   };
}
