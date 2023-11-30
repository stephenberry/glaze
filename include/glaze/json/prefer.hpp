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
#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{

   namespace detail
   {

      template <typename Pair>
      struct base_prefer_array_adapter_tuple
      {
         // Cases:
         // - pair is const -> store const refs to all members
         // - pair contains const members -> store const refs to the const members
         // - pair is r-ref -> store r-refs to all members
         // - pair contains r-ref members -> store r-refs to the r-ref members
         using raw_pair_t = std::remove_reference_t<Pair>;
         using first_type = typename raw_pair_t::first_type;
         using second_type = typename raw_pair_t::second_type;
         // must apply remove_reference directly to first/second_type aliases in order to add const
         using first_unref_t = std::remove_reference_t<first_type>;
         using second_unref_t = std::remove_reference_t<second_type>;
         using first_const_t = std::conditional_t<std::is_const_v<raw_pair_t>, const first_unref_t, first_unref_t>;
         using second_const_t = std::conditional_t<std::is_const_v<raw_pair_t>, const second_unref_t, second_unref_t>;
         using type_0 = std::conditional_t<std::is_rvalue_reference_v<first_type>, first_const_t&&, first_const_t&>;
         using type_1 = std::conditional_t<std::is_rvalue_reference_v<second_type>, second_const_t&&, second_const_t&>;

         using base_tuple = std::tuple<type_0, type_1>;
      };
   }

   template <typename T>
   class prefer_array_adapter
   {
      static_assert(false_v<T>, "Only pairs or ranges of pairs can be adapted to prefer JSON arrays");
   };

   // template <typename T>
   //    requires(!detail::pair_t<T>)
   // prefer_array_adapter(T) -> prefer_array_adapter<T>;

   template <typename T>
   prefer_array_adapter(T&&) -> prefer_array_adapter<T&&>;

   // cannot give prefer_array_adapter interface of tuple, it must literally be a tuple
   template <detail::pair_t T>
   class prefer_array_adapter<T> : public detail::base_prefer_array_adapter_tuple<T>::base_tuple
   {
     public:
      using tuple_type = typename detail::base_prefer_array_adapter_tuple<T>::base_tuple;

      [[nodiscard]] const tuple_type& base_tuple() const& noexcept { return static_cast<const tuple_type&>(*this); }
      [[nodiscard]] tuple_type& base_tuple() & noexcept { return static_cast<tuple_type&>(*this); }
      [[nodiscard]] tuple_type&& base_tuple() && noexcept { return static_cast<tuple_type&&>(std::move(*this)); }

      constexpr explicit prefer_array_adapter(T pair)
         requires(std::is_reference_v<T>)
         : tuple_type(pair.first, pair.second)
      {}

      constexpr explicit prefer_array_adapter(T& pair)
         requires(!std::is_reference_v<T>)
         : tuple_type(pair.first, pair.second)
      {}

      // constexpr explicit prefer_array_adapter(T&& pair)
      //    requires(!std::is_reference_v<T> && std::is_const<>)
      //    : tuple_type(pair.first, pair.second)
      // {}

      prefer_array_adapter(const prefer_array_adapter&) = default;
      prefer_array_adapter(prefer_array_adapter&&) = delete;
      ~prefer_array_adapter() noexcept = default;

      // prefer_array_adapter& operator=(prefer_array_adapter&&) noexcept = delete;
      // constexpr prefer_array_adapter& operator=(T& pair)
      // {
      //    std::get<0>(*this) = pair.first;
      //    std::get<1>(*this) = pair.second;
      //    return *this;
      // }
      //
      // constexpr prefer_array_adapter& operator=(T&& pair) noexcept
      // {
      //    *this = prefer_array_adapter{std::move(pair)};
      //    return *this;
      // }

      [[nodiscard]] constexpr bool operator==(const prefer_array_adapter& other) const noexcept = default;
      [[nodiscard]] constexpr friend bool operator==(const prefer_array_adapter& adapter, const T& pair) noexcept
      {
         return std::get<0>(adapter) == pair.first && std::get<1>(adapter) == pair.second;
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

      // Common facilities required by prefer_array_adapters for ranges
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
   class prefer_array_adapter<T> : public detail::range_common_prefer_array_adapter<const T>
   {
      using common = detail::range_common_prefer_array_adapter<const T>;

     public:
      using value_type = typename common::const_value_type;
      using reference_type = typename common::const_reference_type;
      using const_reference_type = typename common::const_reference_type;
      using size_type = typename common::size_type;
      using difference_type = typename common::difference_type;
      using iterator = typename common::const_iterator;
      using const_iterator = typename common::const_iterator;

      using common::map;

      explicit prefer_array_adapter(const T& map) : common{map} {}

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

      explicit prefer_array_adapter(T& map) : common{map} {}
      explicit prefer_array_adapter(T&& map) : detail::range_common_prefer_array_adapter<const T&>{map} {}

      [[nodiscard]] constexpr iterator begin() noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr iterator end() noexcept { return {map.get().end()}; }
      [[nodiscard]] constexpr const_iterator begin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr const_iterator end() const noexcept { return {map.get().end()}; }
      [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {map.get().begin()}; }
      [[nodiscard]] constexpr const_iterator cend() const noexcept { return {map.get().end()}; }
   };

   template <typename T>
   struct prefer_arrays_t
   {
      T& val;
   };

   template <typename T>
   struct no_prefer_arrays_t
   {
      T& val;
   };
   namespace detail
   {
      template <class T>
         requires requires() { typename T::tuple_type; }
      struct to_json<prefer_array_adapter<T>>
      {
         template <opts Opts>
         static void op(prefer_array_adapter<T>&& value, auto&&... args) noexcept
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
         static void op(prefer_array_adapter<T>& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            // prefer_array_adaptor<pair_t> is not a range, it presents a tuple interface
            read<json>::op<Opts>(value.val.base_tuple(), args...);
         }
      };

      template <class T>
      struct to_json<prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&&... args) noexcept
         {
            write<json>::op<opt_true<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct from_json<prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&&... args) noexcept
         {
            read<json>::op<opt_true<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct to_json<no_prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&&... args) noexcept
         {
            write<json>::op<opt_false<Opts, &opts::prefer_arrays>>(value.val, args...);
         }
      };

      template <class T>
      struct from_json<no_prefer_arrays_t<T>>
      {
         template <opts Opts>
         static void op(auto&& value, auto&&... args) noexcept
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
