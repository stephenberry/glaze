// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

namespace glz::detail
{
   struct header8 final {
      uint8_t config : 2;
      uint8_t size : 6;
   };
   static_assert(sizeof(header8) == 1);
   
   struct header16 final {
      uint16_t config : 2;
      uint16_t size : 14;
   };
   static_assert(sizeof(header16) == 2);
   
   struct header32 final {
      uint32_t config : 2;
      uint32_t size : 30;
   };
   static_assert(sizeof(header32) == 4);
   
   struct header64 final {
      uint64_t config : 2;
      uint64_t size : 62;
   };
   static_assert(sizeof(header64) == 8);
}
