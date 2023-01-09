// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <utility>

namespace glz
{
   template <class To, class From>
   std::enable_if_t<
       sizeof(To) == sizeof(From) &&
       std::is_trivially_copyable_v<From> &&
       std::is_trivially_copyable_v<To>,
       To>
   // constexpr support needs compiler magic
   bit_cast(const From& src) noexcept
   {
       static_assert(std::is_trivially_constructible_v<To>,
           "This implementation additionally requires "
           "destination type to be trivially constructible");
    
       To dst;
       std::memcpy(&dst, &src, sizeof(To));
       return dst;
   }
}
