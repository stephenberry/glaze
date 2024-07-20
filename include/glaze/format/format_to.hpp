// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/write.hpp"
#include "glaze/util/itoa.hpp"

namespace glz
{
   template <detail::num_t T>
   void format_to(std::string& buffer, T&& value) noexcept
   {
      auto ix = buffer.size();
      buffer.resize((std::max)(buffer.size() * 2, ix + 64));

      auto start = reinterpret_cast<char*>(data_ptr(b) + ix);
      const auto [ptr, ec] = std::to_chars(start, start + 64, std::forward<T>(value));
      if (ec != std::errc()) [[unlikely]] {
         dump<"null", false>(b, ix);
      }
      else [[likely]] {
         if constexpr (std::floating_point<T>) {
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
         else {
            ix += size_t(ptr - start);
         }
      }
      
      buffer.resize(ix);
   }
}
