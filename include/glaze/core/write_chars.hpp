// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <type_traits>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/dump.hpp"
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
      inline static void op(num_t auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
      {
         /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
            // more efficient strings in C++23:
          https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
          }*/

         // https://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value
         // maximum length for a double should be 24 chars, we use 64 to be sufficient for float128_t
         if constexpr (resizable<B> && not has_write_unchecked(Opts)) {
            if (ix + 64 > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + 64));
            }
         }

         using V = std::decay_t<decltype(value)>;

         if constexpr (std::floating_point<V>) {
            
            auto serialize_float = [&](auto&& value) {
               auto start = reinterpret_cast<char*>(data_ptr(b) + ix);
               const auto [ptr, ec] = std::to_chars(start, start + 64, value);
               if (ec != std::errc()) [[unlikely]] {
                  ctx.error = error_code::write_number_error;
               }
               else [[likely]] {
                  switch (*start) {
                        [[unlikely]] case 'n':  {
                           [[fallthrough]];
                     }
                        [[unlikely]] case 'i':  {
                        dump<"null", false>(b, ix);
                        break;
                     }
                        [[likely]] default:  {
                        ix += size_t(ptr - start);
                     }
                  }
               }
            };
            
            if constexpr (uint8_t(Opts.float_max_write_precision) > 0 &&
                          uint8_t(Opts.float_max_write_precision) < sizeof(V)) {
               // we cast to a lower precision floating point value before writing out
               if constexpr (uint8_t(Opts.float_max_write_precision) == 8) {
                  serialize_float(static_cast<double>(value));
               }
               else if constexpr (uint8_t(Opts.float_max_write_precision) == 4) {
                  serialize_float(static_cast<float>(value));
               }
               else {
                  static_assert(false_v<V>, "invalid float_max_write_precision");
               }
            }
            else if constexpr (is_any_of<V, float, double>) {
               serialize_float(value);
            }
            else if constexpr (is_float128<V>) {
               auto start = reinterpret_cast<char*>(data_ptr(b) + ix);
               const auto [ptr, ec] = std::to_chars(start, start + 64, value, std::chars_format::general);
               if (ec != std::errc()) [[unlikely]] {
                  ctx.error = error_code::write_number_error;
                  return;
               }
               else [[likely]] {
                  switch (*start) {
                        [[unlikely]] case 'n':  {
                           [[fallthrough]];
                     }
                        [[unlikely]] case 'i':  {
                        dump<"null", false>(b, ix);
                        break;
                     }
                        [[likely]] default:  {
                        ix += size_t(ptr - start);
                     }
                  }
               }
            }
            else {
               static_assert(false_v<V>, "type is not supported");
            }
         }
         else if constexpr (is_any_of<V, int32_t, uint32_t, int64_t, uint64_t>) {
            const auto start = data_ptr(b) + ix;
            const auto end = glz::to_chars(start, value);
            ix += size_t(end - start);
         }
         else if constexpr (std::integral<V>) {
            using X = std::decay_t<decltype(sized_integer_conversion<V>())>;
            const auto start = data_ptr(b) + ix;
            const auto end = glz::to_chars(start, static_cast<X>(value));
            ix += size_t(end - start);
         }
         else {
            static_assert(false_v<V>, "type is not supported");
         }
      }
   };
}
