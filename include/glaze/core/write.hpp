// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"

namespace glaze
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <uint32_t Format, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> &&
      std::same_as<std::byte, nano::ranges::range_value_t<Buffer>>
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::vector<std::byte>>) {
         detail::write<Format>::op(std::forward<T>(value), buffer);
      }
      else {
         detail::write<Format>::op(std::forward<T>(value), std::back_inserter(buffer));
      }
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <uint32_t Format, class T>
   inline void write(T&& value, std::ostream& os) noexcept
   {
      detail::write<Format>::op(std::forward<T>(value),
                        std::ostreambuf_iterator<char>(os));
   }
   
   // For writing to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <class Opts, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> &&
      std::same_as<char, nano::ranges::range_value_t<Buffer>>
   inline void write_c(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string>) {
         detail::write<Opts::format>::template op<Opts>(std::forward<T>(value), buffer);
      }
      else {
         detail::write<Opts::format>::template op<Opts>(std::forward<T>(value), std::back_inserter(buffer));
      }
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <class Opts, class T>
   inline void write_c(T&& value, std::ostream& os) noexcept
   {
      detail::write<Opts::format>::template op<Opts>(std::forward<T>(value),
                        std::ostreambuf_iterator<char>(os));
   }
}
