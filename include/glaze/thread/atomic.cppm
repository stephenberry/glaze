// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.thread.atomic;
import glaze.core.common;
#else
#include "glaze/core/common.cppm"
#endif




// Supports serialization/deserialization of std::atomic

namespace glz::detail
{
   template <typename T>
   concept is_atomic = requires(T a, typename std::remove_reference_t<decltype(a.load())>& expected,
                                const typename std::remove_reference_t<decltype(a.load())>& desired) {
      {
         a.is_lock_free()
      } -> std::convertible_to<bool>;
      {
         a.store(desired)
      } noexcept;
      {
         a.load()
      } -> std::same_as<typename std::remove_reference_t<decltype(a.load())>>;
      {
         a.exchange(desired)
      } -> std::same_as<typename std::remove_reference_t<decltype(a.load())>>;
      {
         a.compare_exchange_weak(expected, desired)
      } -> std::convertible_to<bool>;
      {
         a.compare_exchange_strong(expected, desired)
      } -> std::convertible_to<bool>;
   };

   template <uint32_t Format, is_atomic T>
      requires(not custom_read<T>)
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         using V = typename T::value_type;
         V temp{};
         read<Format>::template op<Opts>(temp, ctx, it, end);
         value.store(temp);
      }
   };

   template <uint32_t Format, is_atomic T>
      requires(not custom_write<T>)
   struct to<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         const auto v = value.load();
         write<Format>::template op<Opts>(v, ctx, args...);
      }
   };
}
