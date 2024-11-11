// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/opts.hpp"

namespace glz
{
   namespace detail
   {
      template <class T, auto OptsMemPtr>
      struct opts_wrapper_t
      {
         static constexpr bool glaze_wrapper = true;
         static constexpr auto glaze_reflect = false;
         static constexpr auto opts_member = OptsMemPtr;
         using value_type = T;
         T& val;
      };

      template <class T>
      concept is_opts_wrapper = requires {
         requires T::glaze_wrapper == true;
         requires T::glaze_reflect == false;
         T::opts_member;
         typename T::value_type;
         requires std::is_lvalue_reference_v<decltype(T::val)>;
      };

      template <auto MemPtr, auto OptsMemPtr>
      inline constexpr decltype(auto) opts_wrapper() noexcept
      {
         return [](auto&& val) {
            using V = std::remove_reference_t<decltype(val.*MemPtr)>;
            return opts_wrapper_t<V, OptsMemPtr>{val.*MemPtr};
         };
      }

      struct always_write_t
      {
         bool operator()() const { return true; }
      };

      // custom_t allows a user to register member functions (and std::function members) to implement custom reading and
      // writing
      template <class T, class From, class To, class Skippable, class SkipMask>
      struct custom_t final
      {
         static constexpr auto glaze_reflect = false;
         static constexpr auto glaze_wrapper = true;
         static constexpr auto glaze_skip_write_mask = SkipMask::value;
         using from_t = From;
         using to_t = To;
         T& val;
         From from;
         To to;
         constexpr bool write_skippable() const { return Skippable()(val); }
      };

      template <auto From, auto To, auto WriteSkippable, uint8_t SkipMask>
      inline constexpr auto custom_impl() noexcept
      {
         return [](auto&& v) {
            using skippable_t = decltype(WriteSkippable);
            using skip_mask_t =
               std::conditional_t<std::is_same_v<skippable_t, always_write_t>, std::integral_constant<uint8_t, 0>,
                                  std::integral_constant<uint8_t, SkipMask>>;
            return custom_t<std::remove_reference_t<decltype(v)>, decltype(From), decltype(To), skippable_t,
                            skip_mask_t>{v, From, To};
         };
      }

      struct deduct_default_t {};

      template <auto MemPtr, auto Default, class T>
      bool is_default(const T& val)
      {
         if constexpr (std::same_as<decltype(Default), deduct_default_t>) {
            return val.*MemPtr == T{}.*MemPtr;
         }
         else {
            return val.*MemPtr == Default;
         }
      }
   }

   // Read and write booleans as numbers
   template <auto MemPtr>
   constexpr auto bools_as_numbers = detail::opts_wrapper<MemPtr, &opts::bools_as_numbers>();

   // Read and write numbers as strings
   template <auto MemPtr>
   constexpr auto quoted_num = detail::opts_wrapper<MemPtr, &opts::quoted_num>();

   // Read numbers as strings and write these string as numbers
   template <auto MemPtr>
   constexpr auto number = detail::opts_wrapper<MemPtr, &opts::number>();

   // Write out string like types without quotes
   template <auto MemPtr>
   constexpr auto raw = detail::opts_wrapper<MemPtr, &opts::raw>();

   // Reads into only existing fields and elements and then exits without parsing the rest of the input
   template <auto MemPtr>
   constexpr auto partial_read = detail::opts_wrapper<MemPtr, &opts::partial_read>();

   template <auto From, auto To, auto WriteSkippable = detail::always_write_t{}, uint8_t SkipMask = UINT8_MAX>
   constexpr auto custom = detail::custom_impl<From, To, WriteSkippable, SkipMask>();

   // Skip writing out default value
   template <auto MemPtr, auto Default = detail::deduct_default_t{}>
   constexpr auto skip_write_default =
      detail::custom_impl<MemPtr, MemPtr, [](const auto& val) { return detail::is_default<MemPtr, Default>(val); },
                          skip_default_flag>();
}
