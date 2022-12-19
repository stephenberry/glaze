// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/util/itoa.hpp"
#include "glaze/util/dtoa.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/common.hpp"

namespace glz::detail
{
   struct write_chars
   {
      template <auto Opts, class B>
      static void op(num_t auto&& value, is_context auto&& ctx, B&& b) noexcept
      {
         if constexpr (std::same_as<std::decay_t<B>, char*>) {
            b = fmt::format_to(std::forward<B>(b), FMT_COMPILE("{}"), value);
         }
         else {
            fmt::format_to(std::back_inserter(b), FMT_COMPILE("{}"), value);
         }
      }
      
      template <auto Opts, class B>
      static void op(num_t auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
      {
         /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
            // more efficient strings in C++23:
          https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
          }*/
         
         // https://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value
         // maximum length for a double should be 24 chars, we use 64 to be sufficient
         if (ix + 64 > b.size()) [[unlikely]] {
            b.resize(std::max(b.size() * 2, ix + 64));
         }
         
         using V = std::decay_t<decltype(value)>;
         if constexpr (is_any_of<V, float, double, int32_t, uint32_t, int64_t, uint64_t>) {
            auto start = b.data() + ix;
            auto end = glz::to_chars(start, value);
            ix += std::distance(start, end);
         }
         else if constexpr (is_any_of<V, uint8_t, int8_t, uint16_t, int16_t>) {
            auto start = b.data() + ix;
            auto end = glz::to_chars(start, static_cast<int32_t>(value));
            ix += std::distance(start, end);
         }
         else if constexpr (std::same_as<V, long>) {
            auto start = b.data() + ix;
            auto end = glz::to_chars(start, static_cast<int64_t>(value));
            ix += std::distance(start, end);
         }
         else if constexpr (std::same_as<V, unsigned long>) {
            auto start = b.data() + ix;
            auto end = glz::to_chars(start, static_cast<uint64_t>(value));
            ix += std::distance(start, end);
         }
         else {
            static_assert(false_v<V>, "type is not supported");
         }
      }
   };
}
