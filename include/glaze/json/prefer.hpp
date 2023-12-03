// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <compare>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   template <typename T>
   class prefer_array_adapter
   {
      static_assert(false_v<T>, "Only pairs or ranges of pairs can be adapted to prefer JSON arrays");
   };

   template <typename T>
   prefer_array_adapter(T&&) -> prefer_array_adapter<T>;

   template <detail::pair_t T>
   class prefer_array_adapter<T>
   {
     public:
      // no-ref to allow explicitly making non-ref
      T adaptee;

      [[nodiscard]] constexpr bool operator==(const prefer_array_adapter other) const noexcept
      {
         return adaptee == other.adaptee;
      }

      template <detail::pair_t Pair>
      [[nodiscard]] constexpr friend bool operator==(const prefer_array_adapter adapter, const Pair& pair) noexcept
         requires(std::equality_comparable_with<T, Pair>)
      {
         return adapter.adaptee == pair;
      }
   };

   template <std::size_t idx, detail::pair_t T>
   [[nodiscard]] constexpr decltype(auto) get(const prefer_array_adapter<T>& adapter) noexcept
   {
      return std::get<idx>(adapter.adaptee);
   }

   template <std::size_t idx, detail::pair_t T>
   [[nodiscard]] constexpr decltype(auto) get(prefer_array_adapter<T>& adapter) noexcept
   {
      return std::get<idx>(adapter.adaptee);
   }

   template <std::size_t idx, detail::pair_t T>
   [[nodiscard]] constexpr decltype(auto) get(prefer_array_adapter<T>&& adapter) noexcept
   {
      return std::get<idx>(std::move(adapter.adaptee));
   }

   namespace detail
   {

#define _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)                                 \
   using value_type = prefer_array_adapter<std::ranges::range_reference_t<T>>;                  \
   using const_value_type = const value_type;                                                   \
   using reference = value_type;                                                                \
   using const_reference = const prefer_array_adapter<const std::ranges::range_reference_t<T>>; \
   using size_type = std::size_t;                                                               \
   using difference_type = std::ptrdiff_t;

      // Common facilities required by prefer_array_adapters for ranges
      template <typename T>
      struct range_common_prefer_array_adapter
      {
         _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)
         std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_cvref_t<T>, T&> map;

         [[nodiscard]] constexpr bool empty() const noexcept
            requires(empty_range(map))
         {
            return empty_range(map);
         }

         [[nodiscard]] constexpr size_type size() const noexcept
#ifdef __cpp_lib_ranges
            requires std::ranges::sized_range<T>
         {
            return std::ranges::size(map);
         }
#else
            requires requires(const T t) {
               {
                  t.size()
               } -> std::unsigned_integral;
            }
         {
            return map.size();
         }
#endif

         // An adaptor over the map's iterator, presenting a mutable view of its pairs adapted with an array
         // interface
         class iterator
         {
           public:
            _GLAZE_DETAIL_RANGE_COMMON_PREFER_ARRAY_TYPE_ALIASES(T)

            iterator_t<T> map_iterator;

            [[nodiscard]] std::strong_ordering operator<=>(const iterator& other) const noexcept = default;
            [[nodiscard]] constexpr reference operator*() noexcept { return reference{*map_iterator}; }
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

            std::ranges::iterator_t<T> map_iterator;

            [[nodiscard]] std::strong_ordering operator<=>(const const_iterator& other) const noexcept = default;
            [[nodiscard]] constexpr const_reference operator*() const noexcept
            {
               return const_reference{*map_iterator};
            }
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
      };
   }

   template <detail::writable_map_t T>
      requires(!detail::readable_map_t<T>)
   class prefer_array_adapter<T> : public detail::range_common_prefer_array_adapter<T>
   {
      using common = detail::range_common_prefer_array_adapter<T>;

     public:
      using value_type = typename common::const_value_type;
      using reference = typename common::const_reference;
      using const_reference = typename common::const_reference;
      using size_type = typename common::size_type;
      using difference_type = typename common::difference_type;
      using iterator = typename common::const_iterator;
      using const_iterator = typename common::const_iterator;

      using common::map;

      explicit prefer_array_adapter(T map) : common{map} {}

      [[nodiscard]] constexpr iterator begin() const noexcept { return {map.begin()}; }
      [[nodiscard]] constexpr iterator end() const noexcept { return {map.end()}; }
      [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {map.begin()}; }
      [[nodiscard]] constexpr const_iterator cend() const noexcept { return {map.end()}; }
   };

   template <detail::readable_map_t T>
   class prefer_array_adapter<T> : public detail::range_common_prefer_array_adapter<T>
   {
      using common = detail::range_common_prefer_array_adapter<T>;

     public:
      using value_type = typename common::value_type;
      using reference = typename common::reference;
      using const_reference = typename common::const_reference;
      using size_type = typename common::size_type;
      using difference_type = typename common::difference_type;
      using iterator = typename common::iterator;
      using const_iterator = typename common::const_iterator;

      using common::map;

      explicit prefer_array_adapter(T map) : common{map} {}

      [[nodiscard]] constexpr iterator begin() noexcept { return {map.begin()}; }
      [[nodiscard]] constexpr iterator end() noexcept { return {map.end()}; }
      [[nodiscard]] constexpr const_iterator begin() const noexcept { return {map.begin()}; }
      [[nodiscard]] constexpr const_iterator end() const noexcept { return {map.end()}; }
      [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {map.begin()}; }
      [[nodiscard]] constexpr const_iterator cend() const noexcept { return {map.end()}; }
   };

   template <typename T>
   struct prefer_arrays_t
   {
      T val;
   };

   template <typename T>
   struct no_prefer_arrays_t
   {
      T val;
   };

   template <typename T>
   prefer_arrays_t(T&&) -> prefer_arrays_t<T>;

   template <typename T>
   no_prefer_arrays_t(T&&) -> no_prefer_arrays_t<T>;

   namespace detail
   {
      template <class T>
         requires requires() { typename T::tuple_type; }
      struct to_json<prefer_array_adapter<T>>
      {
         template <opts Opts>
         static void op(prefer_array_adapter<T>&& value, auto&... args) noexcept
         {
            // prefer_array_adaptor<pair_t> is not a range, it presents a tuple interface.
            write<json>::op<Opts>(value.val.base_tuple(), args...);
         }
      };

      template <class T>
         requires requires() { typename T::tuple_type; }
      struct from_json<prefer_array_adapter<T>>
      {
         template <opts Opts>
         static void op(prefer_array_adapter<T>& value, auto&... args) noexcept
         {
            // prefer_array_adaptor<pair_t> is not a range, it presents a tuple interface
            read<json>::op<Opts>(value.val.base_tuple(), args...);
         }
      };

      template <class T>
      struct to_json<prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct from_json<prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<no_prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&... args) noexcept
         {
            write<json>::op<opt_false<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct from_json<no_prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&... args) noexcept
         {
            read<json>::op<opt_false<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <auto MemPtr>
      constexpr decltype(auto) prefer_arrays_impl() noexcept
      {
         return [](auto&& val) { return prefer_arrays_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }

      template <auto MemPtr>
      constexpr decltype(auto) no_prefer_arrays_impl() noexcept
      {
         return
            [](auto&& val) { return no_prefer_arrays_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   template <auto MemPtr>
   constexpr auto prefer_arrays = detail::prefer_arrays_impl<MemPtr>();

   template <auto MemPtr>
   constexpr auto no_prefer_arrays = detail::no_prefer_arrays_impl<MemPtr>();
}

namespace std
{

   template <glz::detail::pair_t Pair>
   struct tuple_size<glz::prefer_array_adapter<Pair>>
   {
      static constexpr std::size_t value{2};
   };

   template <std::size_t idx, glz::detail::pair_t Pair>
   struct tuple_element<idx, glz::prefer_array_adapter<Pair>>
   {
      static_assert(glz::false_v<>, "Index to adapted pair outside of [0,1]");
   };

   template <glz::detail::pair_t Pair>
   struct tuple_element<0, glz::prefer_array_adapter<Pair>>
   {
      using type = typename std::remove_reference_t<Pair>::first_type;
   };

   template <glz::detail::pair_t Pair>
   struct tuple_element<1, glz::prefer_array_adapter<Pair>>
   {
      using type = typename std::remove_reference_t<Pair>::second_type;
   };

}
