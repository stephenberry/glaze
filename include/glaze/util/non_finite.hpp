// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <string_view>

#include "glaze/util/dump.hpp"
#include "glaze/util/inline.hpp"

// Helpers for formats that spell non-finite floats as tokens. The JSON number writer emits null for
// these values, so formats with their own spellings handle them before falling through to it.

namespace glz::detail
{
   // Reads inf or nan, optionally signed with '+' or '-', when the token ends at `end` or at a character
   // for which `is_terminator` returns true. These are the TOML spellings, which the CSV format reuses.
   // Returns false and leaves `it` unchanged otherwise.
   template <std::floating_point T>
   constexpr bool parse_non_finite_float(T& value, auto& it, auto end, auto&& is_terminator) noexcept
   {
      auto p = it;
      bool negative = false;
      if (p != end && (*p == '+' || *p == '-')) {
         negative = *p == '-';
         ++p;
      }
      if (end - p < 3) {
         return false;
      }

      const bool inf = p[0] == 'i' && p[1] == 'n' && p[2] == 'f';
      if (not inf && not(p[0] == 'n' && p[1] == 'a' && p[2] == 'n')) {
         return false;
      }
      p += 3;
      if (p != end && not is_terminator(*p)) {
         return false;
      }

      value = inf ? std::numeric_limits<T>::infinity() : std::numeric_limits<T>::quiet_NaN();
      if (negative) {
         value = -value;
      }
      it = p;
      return true;
   }

   // Writes a non-finite float using a format's spellings and returns true. Returns false, writing
   // nothing, for a finite value. A NaN is written without its sign.
   template <class B>
   GLZ_ALWAYS_INLINE bool write_non_finite_float(const std::floating_point auto value, const std::string_view nan,
                                                 const std::string_view inf, const std::string_view negative_inf,
                                                 B& b, size_t& ix)
   {
      if (std::isfinite(value)) [[likely]] {
         return false;
      }
      if (std::isnan(value)) {
         dump(nan, b, ix);
      }
      else {
         dump(value < 0 ? negative_inf : inf, b, ix);
      }
      return true;
   }
}
