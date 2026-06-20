// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "glaze/core/context.hpp"
#include "glaze/core/traits.hpp"

namespace glz
{
   // Concept for std::chrono::duration types
   template <class T>
   concept is_duration = requires {
      typename T::rep;
      typename T::period;
      requires std::is_same_v<std::remove_cvref_t<T>, std::chrono::duration<typename T::rep, typename T::period>>;
   };

   // Concept for std::chrono::time_point types
   template <class T>
   concept is_time_point = requires {
      typename T::clock;
      typename T::duration;
      requires std::is_same_v<std::remove_cvref_t<T>, std::chrono::time_point<typename T::clock, typename T::duration>>;
   };

   // Concept for system_clock time_points (serialize as ISO 8601 string)
   template <class T>
   concept is_system_time_point =
      is_time_point<T> && std::is_same_v<typename std::remove_cvref_t<T>::clock, std::chrono::system_clock>;

   // Concept for steady_clock time_points (serialize as numeric count)
   template <class T>
   concept is_steady_time_point =
      is_time_point<T> && std::is_same_v<typename std::remove_cvref_t<T>::clock, std::chrono::steady_clock>;

   // Detect if high_resolution_clock is a true alias or a distinct type
   inline constexpr bool hrc_is_system = std::is_same_v<std::chrono::high_resolution_clock, std::chrono::system_clock>;
   inline constexpr bool hrc_is_steady = std::is_same_v<std::chrono::high_resolution_clock, std::chrono::steady_clock>;

   // Concept for high_resolution_clock when it's a distinct type (rare)
   template <class T>
   concept is_high_res_time_point =
      is_time_point<T> && std::is_same_v<typename std::remove_cvref_t<T>::clock, std::chrono::high_resolution_clock> &&
      !hrc_is_system && !hrc_is_steady;

   // ============================================
   // epoch_time wrapper for Unix timestamp format
   // ============================================

   // Wrapper that controls serialization format, not storage
   // Internally stores system_clock::time_point with native precision
   // Template parameter specifies the OUTPUT format precision
   template <class Duration>
   struct epoch_time
   {
      std::chrono::system_clock::time_point value{};

      // Implicit conversions for ergonomic use
      epoch_time() = default;
      epoch_time(std::chrono::system_clock::time_point tp) : value(tp) {}
      operator std::chrono::system_clock::time_point() const { return value; }

      // Comparison operators
      bool operator==(const epoch_time&) const = default;
      auto operator<=>(const epoch_time&) const = default;
   };

   // Convenience aliases - name indicates OUTPUT format
   using epoch_seconds = epoch_time<std::chrono::seconds>;
   using epoch_millis = epoch_time<std::chrono::milliseconds>;
   using epoch_micros = epoch_time<std::chrono::microseconds>;
   using epoch_nanos = epoch_time<std::chrono::nanoseconds>;

   // Concept to detect epoch_time wrapper
   template <class T>
   concept is_epoch_time = requires(T t) {
      { t.value } -> std::convertible_to<std::chrono::system_clock::time_point>;
   };

   // ============================================
   // TOML Local Date/Time support
   // ============================================

   // Concept for std::chrono::year_month_day (TOML Local Date)
   template <class T>
   concept is_year_month_day = std::is_same_v<std::remove_cvref_t<T>, std::chrono::year_month_day>;

   // Concept for std::chrono::hh_mm_ss (TOML Local Time)
   template <class T>
   concept is_hh_mm_ss = requires {
      typename std::remove_cvref_t<T>::precision;
      requires requires(T t) {
         { t.hours() } -> std::convertible_to<std::chrono::hours>;
         { t.minutes() } -> std::convertible_to<std::chrono::minutes>;
         { t.seconds() } -> std::convertible_to<std::chrono::seconds>;
         { t.subseconds() };
         { t.is_negative() } -> std::convertible_to<bool>;
      };
   };

   // Register chrono types as having specified Glaze serialization
   // This prevents P2996 automatic reflection from creating ambiguous specializations
   template <class Rep, class Period>
   struct specified<std::chrono::duration<Rep, Period>> : std::true_type
   {};

   template <class Clock, class Duration>
   struct specified<std::chrono::time_point<Clock, Duration>> : std::true_type
   {};

   template <>
   struct specified<std::chrono::year_month_day> : std::true_type
   {};

   template <class Duration>
   struct specified<std::chrono::hh_mm_ss<Duration>> : std::true_type
   {};

   template <class Duration>
   struct specified<epoch_time<Duration>> : std::true_type
   {};

   namespace chrono_detail
   {
      // Parse `count` decimal digits starting at s[start]. Returns -1 if any character
      // is not a digit. Precondition: `count > 0` and `count` digits are readable at
      // s[start..start+count). `count == 0` returns 0 rather than an error.
      inline int parse_digits(const char* s, size_t start, size_t count) noexcept
      {
         int val = 0;
         for (size_t i = 0; i < count; ++i) {
            const char c = s[start + i];
            if (c < '0' || c > '9') return -1;
            val = val * 10 + (c - '0');
         }
         return val;
      }

      // Write `val` as exactly N zero-padded decimal digits to b starting at ix, advancing ix.
      // Caller must ensure b has at least N bytes of capacity at ix.
      template <size_t N, class B>
      inline void write_digits(B& b, auto& ix, uint64_t val) noexcept
      {
         for (size_t i = N; i > 0; --i) {
            b[ix + i - 1] = static_cast<char>('0' + val % 10);
            val /= 10;
         }
         ix += N;
      }

      // Parse exactly "YYYY-MM-DD" (10 chars) into a year_month_day.
      // On failure, sets ec to parse_error and leaves ymd unchanged.
      // The 10-char length check implicitly caps the year at 9999, which keeps the
      // writer's [0000, 9999] range symmetric on read; loosening the size check would
      // break that symmetry, so it's worth holding fixed.
      inline void parse_ymd(std::string_view str, std::chrono::year_month_day& ymd, error_code& ec) noexcept
      {
         if (str.size() != 10) {
            ec = error_code::parse_error;
            return;
         }

         const char* s = str.data();

         const int yr = parse_digits(s, 0, 4);
         const int mo = parse_digits(s, 5, 2);
         const int dy = parse_digits(s, 8, 2);

         if (yr < 0 || mo < 0 || dy < 0 || s[4] != '-' || s[7] != '-') {
            ec = error_code::parse_error;
            return;
         }

         // Fast-fail on obviously out-of-range components before constructing year_month_day.
         // year_month_day::ok() below catches the remaining cases (e.g. Feb 30, leap years).
         if (mo < 1 || mo > 12 || dy < 1 || dy > 31) {
            ec = error_code::parse_error;
            return;
         }

         using namespace std::chrono;
         // yr is in [0, 9999] (4-digit parse, validated non-negative above), which fits
         // inside std::chrono::year's [-32767, 32767] range, so the int -> year conversion
         // is in range.
         const auto candidate =
            year_month_day{year{yr}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(dy)}};
         if (!candidate.ok()) {
            ec = error_code::parse_error;
            return;
         }

         ymd = candidate;
      }

      // Parse an RFC 3339 / ISO 8601 date-time string into a system_clock time_point.
      // On failure, sets ec to parse_error and leaves value unchanged.
      template <is_system_time_point TP>
      inline void parse_iso8601(std::string_view str, TP& value, error_code& ec) noexcept
      {
         // Minimum: YYYY-MM-DDTHH:MM:SS = 19 chars (timezone optional, defaults to UTC)
         if (str.size() < 19) {
            ec = error_code::parse_error;
            return;
         }

         const char* s = str.data();
         const auto n = str.size();

         const int yr = parse_digits(s, 0, 4);
         const int mo = parse_digits(s, 5, 2);
         const int dy = parse_digits(s, 8, 2);
         const int hr = parse_digits(s, 11, 2);
         const int mi = parse_digits(s, 14, 2);
         const int sc = parse_digits(s, 17, 2);

         if (yr < 0 || mo < 0 || dy < 0 || hr < 0 || mi < 0 || sc < 0 || s[4] != '-' || s[7] != '-' || s[10] != 'T' ||
             s[13] != ':' || s[16] != ':') {
            ec = error_code::parse_error;
            return;
         }

         if (mo < 1 || mo > 12 || dy < 1 || dy > 31 || hr > 23 || mi > 59 || sc > 59) {
            ec = error_code::parse_error;
            return;
         }

         size_t pos = 19;
         int64_t subsec_nanos = 0;
         if (pos < n && s[pos] == '.') {
            ++pos;
            int64_t frac = 0;
            int digits = 0;
            while (pos < n && s[pos] >= '0' && s[pos] <= '9') {
               if (digits < 9) {
                  frac = frac * 10 + (s[pos] - '0');
                  ++digits;
               }
               ++pos;
            }
            if (digits == 0) {
               ec = error_code::parse_error;
               return;
            }
            static constexpr int64_t scale[] = {1000000000, 100000000, 10000000, 1000000, 100000,
                                                10000,      1000,      100,      10,      1};
            subsec_nanos = frac * scale[digits];
         }

         int tz_offset_seconds = 0;
         if (pos < n) {
            if (s[pos] == 'Z') {
               ++pos;
            }
            else if (s[pos] == '+' || s[pos] == '-') {
               // UTC = local - offset. "+05:30" means local is 5h30 ahead of UTC, so we
               // subtract 5h30 (multiplier -1); "-08:00" means local is behind UTC, so we add
               // 8h (multiplier +1).
               const int utc_adjustment = (s[pos] == '+') ? -1 : 1;
               ++pos;
               if (pos + 2 > n) {
                  ec = error_code::parse_error;
                  return;
               }
               const int tz_hour = parse_digits(s, pos, 2);
               if (tz_hour < 0 || tz_hour > 23) {
                  ec = error_code::parse_error;
                  return;
               }
               pos += 2;
               int tz_min = 0;
               bool has_tz_colon = false;
               if (pos < n && s[pos] == ':') {
                  ++pos;
                  has_tz_colon = true;
               }
               if (pos + 2 <= n) {
                  const int m = parse_digits(s, pos, 2);
                  if (m < 0 || m > 59) {
                     ec = error_code::parse_error;
                     return;
                  }
                  tz_min = m;
                  pos += 2;
               }
               else if (has_tz_colon) {
                  ec = error_code::parse_error;
                  return;
               }
               tz_offset_seconds = utc_adjustment * (tz_hour * 3600 + tz_min * 60);
            }
         }

         if (pos != n) {
            ec = error_code::parse_error;
            return;
         }

         using namespace std::chrono;
         const auto ymd = year_month_day{year{yr}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(dy)}};
         if (!ymd.ok()) {
            ec = error_code::parse_error;
            return;
         }

         const auto tp = sys_days{ymd} + hours{hr} + minutes{mi} + seconds{sc} + seconds{tz_offset_seconds} +
                         nanoseconds{subsec_nanos};

         using Duration = typename std::remove_cvref_t<TP>::duration;
         value = time_point_cast<Duration>(tp);
      }

      // ============================================
      // strftime-subset format support (glz::date_format)
      // ============================================
      //
      // Glaze deliberately hand-rolls a small, locale-independent strftime subset
      // rather than relying on std::chrono::format / std::chrono::parse, whose
      // calendar-type support is uneven across the supported compiler matrix
      // (libstdc++ / libc++ / MSVC). The subset reuses the same write_digits /
      // parse_digits primitives as the ISO 8601 path so the [0000,9999] year and
      // component-range guarantees are preserved.
      //
      // Supported conversion specifiers:
      //   %Y  year   (4 digits)        %H  hour   (2 digits, 24-hour)
      //   %m  month  (2 digits)        %M  minute (2 digits)
      //   %d  day    (2 digits)        %S  second (2 digits)
      //   %F  = %Y-%m-%d               %T  = %H:%M:%S
      //   %%  literal '%'
      //
      // Times are treated as UTC wall-clock (no timezone token in this subset).
      // %S writes integer seconds only, matching POSIX strftime: sub-second
      // precision is truncated on write. Round-tripping a value finer than seconds
      // through a %S format is therefore lossy by design (the format the user typed
      // has no place to put the fraction). Explicit-width fraction tokens
      // (%3S/%6S/%9S) are a planned extension. Locale-dependent tokens
      // (%A, %B, %p, ...) are intentionally unsupported.

      // Decomposed UTC wall-clock fields shared by the date_format writer and parser.
      struct date_time_fields
      {
         int year{};
         int month{};
         int day{};
         int hour{};
         int minute{};
         int second{};
      };

      // Compile-time validation: every '%' must introduce a supported token.
      inline consteval bool date_format_tokens_valid(std::string_view fmt) noexcept
      {
         for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%') continue;
            if (i + 1 >= fmt.size()) return false; // dangling '%'
            switch (fmt[i + 1]) {
            case 'Y':
            case 'm':
            case 'd':
            case 'H':
            case 'M':
            case 'S':
            case 'F':
            case 'T':
            case '%':
               ++i;
               break;
            default:
               return false;
            }
         }
         return true;
      }

      // Compile-time check: the format supplies a full calendar date (year, month,
      // day) either explicitly or via %F. A time_point cannot be reconstructed on
      // read without one, so this is enforced where date_format is applied.
      inline consteval bool date_format_has_full_date(std::string_view fmt) noexcept
      {
         bool y = false, mo = false, d = false, f = false;
         for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%' || i + 1 >= fmt.size()) continue;
            switch (fmt[i + 1]) {
            case 'Y':
               y = true;
               break;
            case 'm':
               mo = true;
               break;
            case 'd':
               d = true;
               break;
            case 'F':
               f = true;
               break;
            }
            ++i;
         }
         return f || (y && mo && d);
      }

      // Compile-time check: the format references any time-of-day field.
      inline consteval bool date_format_has_time(std::string_view fmt) noexcept
      {
         for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%' || i + 1 >= fmt.size()) continue;
            switch (fmt[i + 1]) {
            case 'H':
            case 'M':
            case 'S':
            case 'T':
               return true;
            }
            ++i;
         }
         return false;
      }

      // Render decomposed fields through the format string. Caller must ensure the buffer has
      // space for at least fmt.size() * 5 + a small constant bytes: the widest token, %F,
      // expands its two source characters to the 10-byte "YYYY-MM-DD" (5 bytes per source char).
      // The JSON caller reserves fmt.size() * 6 + 4 (extra slack plus the surrounding quotes).
      template <class B>
      inline void write_date_format(std::string_view fmt, const date_time_fields& f, B& b, auto& ix) noexcept
      {
         for (size_t i = 0; i < fmt.size(); ++i) {
            const char c = fmt[i];
            if (c != '%') {
               b[ix++] = c;
               continue;
            }
            switch (fmt[++i]) {
            case 'Y':
               write_digits<4>(b, ix, static_cast<uint64_t>(f.year));
               break;
            case 'm':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.month));
               break;
            case 'd':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.day));
               break;
            case 'H':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.hour));
               break;
            case 'M':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.minute));
               break;
            case 'S':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.second));
               break;
            case 'F':
               write_digits<4>(b, ix, static_cast<uint64_t>(f.year));
               b[ix++] = '-';
               write_digits<2>(b, ix, static_cast<uint64_t>(f.month));
               b[ix++] = '-';
               write_digits<2>(b, ix, static_cast<uint64_t>(f.day));
               break;
            case 'T':
               write_digits<2>(b, ix, static_cast<uint64_t>(f.hour));
               b[ix++] = ':';
               write_digits<2>(b, ix, static_cast<uint64_t>(f.minute));
               b[ix++] = ':';
               write_digits<2>(b, ix, static_cast<uint64_t>(f.second));
               break;
            case '%':
               b[ix++] = '%';
               break;
            }
         }
      }

      // Parse a string against the format string into decomposed fields. Component
      // ranges are validated the same way parse_iso8601 validates them; on failure
      // ec is set to parse_error and fields are left partially written.
      inline void parse_date_format(std::string_view fmt, std::string_view str, date_time_fields& f,
                                    error_code& ec) noexcept
      {
         const char* s = str.data();
         const size_t n = str.size();
         size_t si = 0;

         // Consume exactly `count` digits at the current position, advancing si on
         // success. Returns -1 on non-digit or if fewer than `count` chars remain.
         const auto take = [&](size_t count) -> int {
            if (si + count > n) return -1;
            const int v = parse_digits(s, si, count);
            if (v >= 0) si += count;
            return v;
         };

         // Parse the seconds field (2 digits). Sub-second fractions are not part of
         // the %S contract; a trailing fraction is left for the format/literal to
         // consume, and otherwise surfaces as unconsumed trailing input.
         const auto parse_seconds = [&]() -> bool {
            const int sec = take(2);
            if (sec < 0 || sec > 59) return false;
            f.second = sec;
            return true;
         };

         const auto expect_literal = [&](char c) -> bool {
            if (si >= n || s[si] != c) return false;
            ++si;
            return true;
         };

         for (size_t i = 0; i < fmt.size(); ++i) {
            const char c = fmt[i];
            if (c != '%') {
               if (!expect_literal(c)) {
                  ec = error_code::parse_error;
                  return;
               }
               continue;
            }
            switch (fmt[++i]) {
            case 'Y': {
               const int v = take(4);
               if (v < 0) {
                  ec = error_code::parse_error;
                  return;
               }
               f.year = v;
               break;
            }
            case 'm': {
               const int v = take(2);
               if (v < 1 || v > 12) {
                  ec = error_code::parse_error;
                  return;
               }
               f.month = v;
               break;
            }
            case 'd': {
               const int v = take(2);
               if (v < 1 || v > 31) {
                  ec = error_code::parse_error;
                  return;
               }
               f.day = v;
               break;
            }
            case 'H': {
               const int v = take(2);
               if (v < 0 || v > 23) {
                  ec = error_code::parse_error;
                  return;
               }
               f.hour = v;
               break;
            }
            case 'M': {
               const int v = take(2);
               if (v < 0 || v > 59) {
                  ec = error_code::parse_error;
                  return;
               }
               f.minute = v;
               break;
            }
            case 'S':
               if (!parse_seconds()) {
                  ec = error_code::parse_error;
                  return;
               }
               break;
            case 'F': {
               const int y = take(4);
               if (y < 0 || !expect_literal('-')) {
                  ec = error_code::parse_error;
                  return;
               }
               const int mo = take(2);
               if (mo < 1 || mo > 12 || !expect_literal('-')) {
                  ec = error_code::parse_error;
                  return;
               }
               const int d = take(2);
               if (d < 1 || d > 31) {
                  ec = error_code::parse_error;
                  return;
               }
               f.year = y;
               f.month = mo;
               f.day = d;
               break;
            }
            case 'T': {
               const int h = take(2);
               if (h < 0 || h > 23 || !expect_literal(':')) {
                  ec = error_code::parse_error;
                  return;
               }
               const int mi = take(2);
               if (mi < 0 || mi > 59 || !expect_literal(':')) {
                  ec = error_code::parse_error;
                  return;
               }
               f.hour = h;
               f.minute = mi;
               if (!parse_seconds()) {
                  ec = error_code::parse_error;
                  return;
               }
               break;
            }
            case '%':
               if (!expect_literal('%')) {
                  ec = error_code::parse_error;
                  return;
               }
               break;
            }
         }

         // Reject trailing input the format did not consume.
         if (si != n) {
            ec = error_code::parse_error;
         }
      }
   }
}
