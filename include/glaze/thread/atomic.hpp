// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>

#include "glaze/core/common.hpp"

// Supports serialization/deserialization of std::atomic

namespace glz::detail
{
   template <uint32_t Format, class T>
      requires (not custom_read<T>)
   struct from<Format, std::atomic<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         using V = typename std::atomic<T>::value_type;
         V temp{};
         read<Format>::template op<Opts>(temp, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         value.store(temp);
      }
   };
   
   template <uint32_t Format, class T>
      requires (not custom_write<T>)
   struct to<Format, std::atomic<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         const auto v = value.load();
         write<Format>::template op<Opts>(v, ctx, args...);
      }
   };
}
