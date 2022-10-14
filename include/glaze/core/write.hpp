// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <opts Opts, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> && (sizeof(nano::ranges::range_value_t<Buffer>) == sizeof(char))
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string> || std::same_as<Buffer, std::vector<std::byte>>) {
         if constexpr (Opts.format == json) {
            if (buffer.empty()) {
               buffer.resize(32);
            }
            size_t ix = 0; // overwrite index
            detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), buffer, ix);
            buffer.resize(ix);
         }
         else {
            // TODO: add ix optimization to binary
            buffer.clear();
            detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), buffer);
         }
      }
      else {
         buffer.clear();
         detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), std::back_inserter(buffer));
      }
   }
   
   template <opts Opts, class T, class Buffer>
   requires std::same_as<std::decay_t<Buffer>, char*>
   inline size_t write(T&& value, Buffer&& buffer) noexcept
   {
      auto start = buffer;
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value), buffer);
      return static_cast<size_t>(std::distance(start, buffer));
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <opts Opts, class T>
   inline void write(T&& value, std::ostream& os) noexcept
   {
      detail::write<Opts.format>::template op<Opts>(std::forward<T>(value),
                        std::ostreambuf_iterator<char>(os));
   }
}
