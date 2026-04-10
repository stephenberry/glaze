// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.thread.atomic;

import std;

import glaze.core.common;
import glaze.core.context;
import glaze.core.meta;
import glaze.core.opts;

// Supports serialization/deserialization of std::atomic

using std::uint32_t;

export namespace glz
{
   template <typename T>
   concept is_atomic = requires(T a, typename std::remove_reference_t<decltype(a.load())>& expected,
                                const typename std::remove_reference_t<decltype(a.load())>& desired) {
      { a.is_lock_free() } -> std::convertible_to<bool>;
      { a.store(desired) } noexcept;
      { a.load() } -> std::same_as<typename std::remove_reference_t<decltype(a.load())>>;
      { a.exchange(desired) } -> std::same_as<typename std::remove_reference_t<decltype(a.load())>>;
      { a.compare_exchange_weak(expected, desired) } -> std::convertible_to<bool>;
      { a.compare_exchange_strong(expected, desired) } -> std::convertible_to<bool>;
   };

   template <uint32_t Format, is_atomic T>
      requires(not custom_read<T>)
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         using V = typename T::value_type;
         V temp{};
         parse<Format>::template op<Opts>(temp, ctx, it, end);
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
         serialize<Format>::template op<Opts>(v, ctx, args...);
      }
   };
}
