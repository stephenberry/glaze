// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ostream>
#include <string>

#include "glaze/util/itoa.hpp"

namespace glz
{
   struct progress_bar final
   {
      size_t width{};
      size_t completed{};
      size_t total{};
      double time_taken{};

      std::string string() const
      {
         const auto one_or_total = (std::max)(total, size_t{1});
         const auto one_or_completed = (std::min)(completed, one_or_total);
         const auto progress = static_cast<double>(one_or_completed) / one_or_total;
         const auto percentage = static_cast<size_t>(std::round(progress * 100));

         const auto eta_s = static_cast<size_t>(
            std::round(((one_or_total - one_or_completed) * time_taken) / (std::max)(one_or_completed, size_t{1})));
         const auto minutes = eta_s / 60;
         const auto seconds = eta_s - minutes * 60;

         // Worst-case suffix: " " + pct(<=20) + "% | ETA: " + min(<=20) + "m " + sec(<=20) + "s | "
         // + completed(<=20) + "/" + total(<=20)
         constexpr size_t suffix_max = 1 + 20 + 9 + 20 + 2 + 20 + 4 + 20 + 1 + 20;
         const size_t bar_len = width > 2 ? width : 0;

         std::string s;
         s.resize(bar_len + suffix_max);
         char* p = s.data();

         if (width > 2) {
            const auto len = width - 2;
            const auto filled = static_cast<size_t>(std::round(progress * len));
            *p++ = '[';
            std::memset(p, '=', filled);
            p += filled;
            std::memset(p, '-', len - filled);
            p += len - filled;
            *p++ = ']';
         }

         const auto write_lit = [&p]<size_t N>(const char (&lit)[N]) {
            std::memcpy(p, lit, N - 1);
            p += N - 1;
         };

         write_lit(" ");
         p = glz::to_chars(p, static_cast<uint64_t>(percentage));
         write_lit("% | ETA: ");
         p = glz::to_chars(p, static_cast<uint64_t>(minutes));
         write_lit("m ");
         p = glz::to_chars(p, static_cast<uint64_t>(seconds));
         write_lit("s | ");
         p = glz::to_chars(p, static_cast<uint64_t>(one_or_completed));
         *p++ = '/';
         p = glz::to_chars(p, static_cast<uint64_t>(one_or_total));

         s.resize(static_cast<size_t>(p - s.data()));
         return s;
      }
   };

   inline std::ostream& operator<<(std::ostream& o, const progress_bar& bar)
   {
      o << bar.string();
      return o;
   }
}
