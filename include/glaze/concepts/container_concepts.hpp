// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <concepts>
#include <cstdint>
#include <ranges>
#include <utility>
#include <vector>
#include <version>

// Over time we want most concepts to use the nomenclature:
// is_
// has_
// _like
// Avoid the use of _t as that makes it seem like a type and not a concept

namespace glz
{
   template <class T, class... U>
   concept is_any_of = (std::same_as<T, U> || ...);

   template <class T>
   concept has_value_type = requires { typename T::value_type; };

   template <class T>
   concept has_element_type = requires { typename T::element_type; };

   template <class T>
   concept has_first_type = requires { typename T::first_type; };

   template <class T>
   concept has_second_type = requires { typename T::second_type; };

   template <class T>
   concept resizable = requires(T v) { v.resize(0); };

   template <class T>
   concept erasable = requires(T v) { v.erase(v.cbegin(), v.cend()); };

   template <class T>
   concept has_size = requires(T v) { v.size(); };

   template <class T>
   concept has_empty = requires(T v) {
      {
         v.empty()
      } -> std::convertible_to<bool>;
   };

   template <class T>
   concept has_data = requires(T v) { v.data(); };

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

   template <class Buffer>
   concept non_const_buffer = !std::is_const_v<Buffer>;
}

namespace glz::detail
{
   template <class T>
   concept char_t = std::same_as<std::remove_cvref_t<T>, char>;

   template <class T>
   concept wide_char_t =
      std::same_as<std::remove_cvref_t<T>, char16_t> || std::same_as<std::remove_cvref_t<T>, char32_t> ||
      std::same_as<std::remove_cvref_t<T>, wchar_t>;

   template <class T>
   concept bool_t =
      std::same_as<std::remove_cvref_t<T>, bool> || std::same_as<std::remove_cvref_t<T>, std::vector<bool>::reference>;

   template <class T>
   concept int_t = std::integral<std::remove_cvref_t<T>> && !char_t<T> && !wide_char_t<T> && !bool_t<T>;

   template <class T>
   concept num_t = std::floating_point<std::remove_cvref_t<T>> || int_t<T>;

   template <class T>
   concept is_float128 = requires(T x) {
      requires sizeof(x) == 16;
      requires std::floating_point<T>;
   };

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
   concept optional_like = requires(T t, typename T::value_type v) {
      {
         T()
      } -> std::same_as<T>;
      {
         T(v)
      } -> std::same_as<T>;
      {
         t.has_value()
      } -> std::convertible_to<bool>;
      {
         t.value()
      };
      {
         t = v
      };
      {
         t.reset()
      };
      {
         t.emplace()
      };
   };

   template <class T>
   concept pair_t = requires(T pair) {
      typename std::decay_t<T>::first_type;
      typename std::decay_t<T>::second_type;
      {
         pair.first
      };
      {
         pair.second
      };
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
   concept has_append = requires(T t, typename T::const_iterator it) { t.append(it, it); };

   template <class T>
   concept has_assign = requires(T t, const typename T::value_type* v, typename T::size_type sz) { t.assign(v, sz); };

   template <class T>
   concept accessible = requires(T container) {
      {
         container[size_t{}]
      } -> std::same_as<typename T::reference>;
   };

   template <class T>
   concept vector_like =
      resizable<std::remove_cvref_t<T>> && accessible<std::remove_cvref_t<T>> && has_data<std::remove_cvref_t<T>>;

   template <class T>
   concept map_subscriptable = requires(T container) {
      {
         container[std::declval<typename T::key_type>()]
      };
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

namespace glz
{
   template <class T>
   concept range = requires(T& t) {
      requires !std::same_as<void, decltype(t.begin())>;
      requires !std::same_as<void, decltype(t.end())>;
      requires std::input_iterator<decltype(t.begin())>;
   };

   // Concept for a matrix type (not a vector, which is a range)
   template <class T>
   concept matrix_t = requires(T matrix) {
      matrix.resize(2, 4);
      matrix.data();
      {
         matrix.rows()
      } -> std::convertible_to<int>;
      {
         matrix.cols()
      } -> std::convertible_to<int>;
      {
         matrix.size()
      } -> std::convertible_to<int>;
   } && !range<T>;

   // concept for the Eigen library: matrices and vectors
   template <class T>
   concept eigen_t = requires(T matrix) {
      matrix.data();
      {
         matrix.rows()
      } -> std::convertible_to<int>;
      {
         matrix.cols()
      } -> std::convertible_to<int>;
      {
         matrix.size()
      } -> std::convertible_to<int>;
   };

   // range like
   template <class T>
   using iterator_t = decltype(std::begin(std::declval<T&>()));

   template <range R>
   using range_value_t = std::iter_value_t<iterator_t<R>>;

   template <range R>
   [[nodiscard]] constexpr bool empty_range(R&& rng)
   {
#ifdef __cpp_lib_ranges
      return std::ranges::empty(rng);
#else
      // in lieu of std::ranges::empty
      if constexpr (requires() {
                       {
                          rng.empty()
                       } -> std::convertible_to<bool>;
                    }) {
         return rng.empty();
      }
      else if constexpr (requires() {
                            {
                               rng.size()
                            } -> std::same_as<std::size_t>;
                         }) {
         return rng.size() == std::size_t{0};
      }
      else {
         return std::cbegin(rng) == std::cend(rng);
      }
#endif
   }
}

namespace glz
{
   template <class Buffer>
   concept raw_buffer = std::same_as<std::decay_t<Buffer>, char*> && non_const_buffer<Buffer>;

   template <class Buffer>
   concept output_buffer = range<Buffer> && (sizeof(range_value_t<Buffer>) == sizeof(char)) && non_const_buffer<Buffer>;

   template <class T>
   constexpr bool const_value_v = std::is_const_v<std::remove_pointer_t<std::remove_reference_t<T>>>;
}
