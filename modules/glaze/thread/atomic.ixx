// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/thread/atomic.hpp"
// glz:header std=<atomic>
// glz:header std=<concepts>
// glz:header std=<cstdint>
// glz:header std=<type_traits>
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
         if (parse_failed(ctx.error)) [[unlikely]] {
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
