// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <format>
#include <string_view>
#include <type_traits>

#include "glaze/core/opts.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   // Compile-time string for use as template parameter
   template <size_t N>
   struct format_str
   {
      char data[N]{};

      consteval format_str(const char (&str)[N]) noexcept { std::copy_n(str, N, data); }

      constexpr operator std::string_view() const noexcept { return {data, N - 1}; }
   };

   // Wrapper for formatting floats with a specific format string
   template <format_str Fmt, class T>
   struct float_format_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   template <format_str Fmt, class T>
   struct meta<float_format_t<Fmt, T>>
   {
      static constexpr bool custom_write = true;
      static constexpr auto value{[](auto&& s) -> auto& { return s.val; }}; // reading uses the value directly
   };

   template <format_str Fmt, class T>
   struct to<JSON, float_format_t<Fmt, T>>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(auto&& wrapper, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         using V = std::decay_t<decltype(wrapper.val)>;
         static_assert(std::floating_point<V>, "float_format wrapper requires a floating-point type");

         if constexpr (not check_write_unchecked(Opts) && vector_like<B>) {
            if (const auto n = ix + 64; n > b.size()) {
               b.resize(2 * n);
            }
         }

         if constexpr (Opts.quoted_num) {
            dump<'"'>(b, ix);
         }

         constexpr auto fmt = std::format_string<V>{std::string_view{Fmt}};

         if constexpr (check_write_unchecked(Opts)) {
            // Caller guarantees buffer has enough space
            const auto start = reinterpret_cast<char*>(&b[ix]);
            auto result = std::format_to(start, fmt, V(wrapper.val));
            ix += size_t(result - start);
         }
         else {
            // format_to_n writes up to 'available' chars and returns total size needed.
            // Common case: output fits in pre-allocated buffer (single pass).
            // Rare case: output exceeds buffer (e.g., "{:.100f}"), requires resize.
            const auto start = reinterpret_cast<char*>(&b[ix]);
            const auto available = b.size() - ix;
            auto [out, size] = std::format_to_n(start, available, fmt, V(wrapper.val));

            if (static_cast<size_t>(size) > available) {
               // Output was truncated - size tells us exactly how much space we need
               if constexpr (resizable<B>) {
                  b.resize(2 * (ix + size));
                  std::format_to(reinterpret_cast<char*>(&b[ix]), fmt, V(wrapper.val));
               }
               else {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            ix += size;
         }

         if constexpr (Opts.quoted_num) {
            dump<'"'>(b, ix);
         }
      }
   };

   // Helper to create wrapper from member pointer and format string
   template <auto MemPtr, format_str Fmt>
   inline constexpr decltype(auto) float_format_impl() noexcept
   {
      return
         [](auto&& val) { return float_format_t<Fmt, std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }

   // Usage: glz::float_format<&T::member, "{:.2f}">
   template <auto MemPtr, format_str Fmt>
   constexpr auto float_format = float_format_impl<MemPtr, Fmt>();
}
