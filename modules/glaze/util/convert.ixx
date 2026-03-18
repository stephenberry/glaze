// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.util.convert;

import std;

namespace glz
{
   // Creates a std::uint16_t from two chars that matches memcpy behavior on any endianness
   export consteval std::uint16_t to_uint16_t(const char chars[2])
   {
      if constexpr (std::endian::native == std::endian::little) {
         return std::uint16_t(chars[0]) | (std::uint16_t(chars[1]) << 8);
      }
      else {
         return (std::uint16_t(chars[0]) << 8) | std::uint16_t(chars[1]);
      }
   }
}
