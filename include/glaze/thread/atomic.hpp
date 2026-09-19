// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

// Supports serialization/deserialization of std::atomic

namespace glz
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
         // Seed with the current value so that a partial read (e.g. an object with only some keys
         // present) behaves the same as it does for a non-atomic member. Only a trivially copyable
         // value may be seeded: std::atomic<std::shared_ptr<T>> hands back a pointer that aliases
         // the live pointee, and parsing into it would mutate what every other holder sees, even
         // when the parse fails.
         V temp = [&] {
            if constexpr (std::is_trivially_copyable_v<V>) {
               return value.load();
            }
            else {
               return V{};
            }
         }();
         parse<Format>::template op<Opts>(temp, ctx, it, end);
         // end_reached and partial_read_complete are non-error codes: the value parsed, the read
         // simply stopped at the buffer end or at a partial_read boundary, so the store must happen.
         if (bool(ctx.error) && ctx.error != error_code::end_reached &&
             ctx.error != error_code::partial_read_complete) [[unlikely]] {
            return; // leave the atomic untouched on a failed parse
         }
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
