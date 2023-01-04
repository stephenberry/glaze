// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   template <class Buffer>
   concept raw_buffer = std::same_as<std::decay_t<Buffer>, char*>;
   
   template <class Buffer>
   concept output_buffer = range<Buffer> && (sizeof(range_value_t<Buffer>) == sizeof(char));
   
   template <class T>
   inline auto data_ptr(T& buffer) {
      if constexpr (detail::resizeable<T>) {
         return buffer.data();
      }
      else {
         return buffer;
      }
   }
   
   // For writing to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <opts Opts, class T, output_buffer Buffer>
   inline void write(T&& value, Buffer& buffer, is_context auto&& ctx) noexcept
   {
      if constexpr (detail::resizeable<Buffer>) {
         if (buffer.empty()) {
            buffer.resize(128);
         }
      }
      size_t ix = 0; // overwrite index
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (detail::resizeable<Buffer>) {
         buffer.resize(ix);
      }
   }
   
   template <opts Opts, class T, output_buffer Buffer>
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      context ctx{};
      write<Opts>(std::forward<T>(value), buffer, ctx);
   }
   
   template <opts Opts, class T, raw_buffer Buffer>
   inline size_t write(T&& value, Buffer&& buffer, is_context auto&& ctx) noexcept
   {
      size_t ix = 0;
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      return ix;
   }
   
   template <opts Opts, class T, raw_buffer Buffer>
   inline size_t write(T&& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <opts Opts, class T>
   inline void write(T&& value, std::ostream& os, is_context auto&& ctx) noexcept
   {
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), ctx,
                        std::ostreambuf_iterator<char>(os));
   }
   
   template <opts Opts, class T>
   inline void write(T&& value, std::ostream& os) noexcept
   {
      context ctx{};
      write<Opts>(std::forward<T>(value), os, ctx);
   }
}
