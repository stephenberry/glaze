// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "glaze/core/common.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <typename T>
   class prefer_array_adapter
   {
      static_assert(false_v<T>, "Only pairs or ranges of pairs can be adapted to JSON arrays");
   };

   template <class T>
   prefer_array_adapter(T) -> prefer_array_adapter<T>;

   template <detail::pair_t T>
   class prefer_array_adapter<T> : public std::tuple<const typename T::first_type&, const typename T::second_type&>
   {
     public:
      using tuple_type = std::tuple<const typename T::first_type&, const typename T::second_type&>;

      prefer_array_adapter() = delete;

      constexpr explicit prefer_array_adapter(const T& pair)
         : tuple_type{std::forward_as_tuple(pair.first, pair.second)}
      {}

      constexpr explicit prefer_array_adapter(T&& pair) : tuple_type{std::forward_as_tuple(pair.first, pair.second)} {}

      [[nodiscard]] bool operator==(const prefer_array_adapter& other) const noexcept = default;

      [[nodiscard]] friend bool operator==(const prefer_array_adapter& adapter, const T& pair) noexcept
      {
         return adapter == prefer_array_adapter{pair};
      }
   };

   namespace detail
   {

#define _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T) \
   using value_type = prefer_array_adapter<range_value_t<T>>;   \
   using const_value_type = const value_type;                   \
   using reference_type = value_type;                           \
   using const_reference_type = const reference_type;           \
   using size_type = std::size_t;                               \
   using difference_type = std::ptrdiff_t;

      template <typename T>
      struct range_common_prefer_array_adapter
      {
         _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)
         std::reference_wrapper<T> map;

         [[nodiscard]] constexpr bool empty() const noexcept
            requires(requires(T t) { empty_range(map.get()); })
         {
            return empty_range(map.get());
         }

         [[nodiscard]] constexpr size_type size() const noexcept
#ifdef __cpp_lib_ranges
            requires std::ranges::sized_range<T>
         {
            return std::ranges::size(map.get());
         }
#else
            requires requires(const T t) {
                        {
                           t.size()
                           } -> std::unsigned_integral;
                     }
         {
            return map.get().size();
         }
#endif

         // An adaptor over the map's iterator, presenting a mutable view of its pairs adapted with an array interface
         class iterator
         {
           public:
            _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)

            iterator_t<T> map_iterator;

            [[nodiscard]] std::strong_ordering operator<=>(const iterator& other) const noexcept = default;
            [[nodiscard]] constexpr reference_type operator*() noexcept { return reference_type{*map_iterator}; }
            constexpr iterator& operator++() noexcept
            {
               ++map_iterator;
               return *this;
            }

            [[nodiscard]] constexpr iterator operator++(int) & noexcept
            {
               const iterator current{*this};
               map_iterator++;
               return current;
            }
         };

         // An adaptor over the map's iterator, presenting a const view of its pairs adapted with an array interface
         class const_iterator
         {
           public:
            _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)

            iterator_t<T> map_iterator;

            [[nodiscard]] std::strong_ordering operator<=>(const const_iterator& other) const noexcept = default;
            [[nodiscard]] constexpr reference_type operator*() const noexcept { return reference_type{*map_iterator}; }
            constexpr const_iterator& operator++() noexcept
            {
               ++map_iterator;
               return *this;
            }

            [[nodiscard]] constexpr const_iterator operator++(int) & noexcept
            {
               const_iterator current{*this};
               map_iterator++;
               return current;
            }
         };

        protected:
         explicit constexpr range_common_prefer_array_adapter(T& map) : map{map} {}
      };
   }

   template <detail::writable_map_t T>
      requires(!detail::readable_map_t<T>)
   class prefer_array_adapter<T> : public detail::range_common_prefer_array_adapter<T>
   {
      using common = detail::range_common_prefer_array_adapter<T>;

     public:
      using value_type = typename common::const_value_type;
      using reference_type = typename common::const_reference_type;
      using const_reference_type = typename common::const_reference_type;
      using size_type = typename common::size_type;
      using difference_type = typename common::difference_type;
      using iterator = typename common::const_iterator;
      using const_iterator = typename common::const_iterator;

      using common::map;

      explicit prefer_array_adapter(T& map) : detail::range_common_prefer_array_adapter<T>{map} {}

      [[nodiscard]] constexpr iterator begin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr iterator end() const noexcept { return {map.get().end()}; }
      [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr const_iterator cend() const noexcept { return {map.get().end()}; }
   };

   template <detail::readable_map_t T>
   class prefer_array_adapter<T> : public detail::range_common_prefer_array_adapter<T>
   {
      using common = detail::range_common_prefer_array_adapter<T>;

     public:
      using value_type = typename common::value_type;
      using reference_type = typename common::reference_type;
      using const_reference_type = typename common::const_reference_type;
      using size_type = typename common::size_type;
      using difference_type = typename common::difference_type;
      using iterator = typename common::iterator;
      using const_iterator = typename common::const_iterator;

      using common::map;

      explicit prefer_array_adapter(T& map) : detail::range_common_prefer_array_adapter<T>{map} {}

      [[nodiscard]] constexpr iterator begin() noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr iterator end() noexcept { return {map.get().end()}; }
      [[nodiscard]] constexpr const_iterator begin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr const_iterator end() const noexcept { return {map.get().end()}; }
      [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr const_iterator cend() const noexcept { return {map.get().end()}; }
   };

   //   static_assert(readable_arrray_t<>)
}
