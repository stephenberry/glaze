// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <type_traits>

#include "common.hpp"
#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/dtoa.hpp"
#include "glaze/util/itoa.hpp"

namespace glz::detail
{
   template <class T>
   GLZ_ALWAYS_INLINE constexpr auto sized_integer_conversion() noexcept
   {
      if constexpr (std::is_signed_v<T>) {
         if constexpr (sizeof(T) <= sizeof(int32_t)) {
            return int32_t{};
         }
         else if constexpr (sizeof(T) <= sizeof(int64_t)) {
            return int64_t{};
         }
         else {
            static_assert(false_v<T>, "type is not supported");
         }
      }
      else {
         if constexpr (sizeof(T) <= sizeof(uint32_t)) {
            return uint32_t{};
         }
         else if constexpr (sizeof(T) <= sizeof(uint64_t)) {
            return uint64_t{};
         }
         else {
            static_assert(false_v<T>, "type is not supported");
         }
      }
   }
   static_assert(std::is_same_v<decltype(sized_integer_conversion<long long>()), int64_t>);
   static_assert(std::is_same_v<decltype(sized_integer_conversion<unsigned long long>()), uint64_t>);

   struct write_chars
   {
      template <auto Opts, class B>
      inline static void op(num_t auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
      {
         /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
            // more efficient strings in C++23:
          https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
          }*/

         // https://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value
         // maximum length for a double should be 24 chars, we use 64 to be sufficient for float128_t
         if constexpr (detail::resizeable<B>) {
            if (ix + 64 > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + 64));
            }
         }

         using V = std::decay_t<decltype(value)>;

         if constexpr (is_any_of<V, float, double, int32_t, uint32_t, int64_t, uint64_t>) {
            auto start = data_ptr(b) + ix;
            auto end = glz::to_chars(start, value);
            ix += std::distance(start, end);
         }
         else if constexpr (is_float128<V>) {
            auto start = data_ptr(b) + ix;
            auto [ptr, ec] = std::to_chars(start, data_ptr(b) + b.size(), value, std::chars_format::general);
            if (ec != std::errc()) {
               // TODO: Do we need to handle this error state?
            }
            ix += std::distance(start, ptr);
         }
         else if constexpr (std::integral<V>) {
            using X = std::decay_t<decltype(sized_integer_conversion<V>())>;
            auto start = data_ptr(b) + ix;
            auto end = glz::to_chars(start, static_cast<X>(value));
            ix += std::distance(start, end);
         }
         else {
            static_assert(false_v<V>, "type is not supported");
         }
      }
   };
}
