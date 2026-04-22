// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.core.chrono;

import std;

import glaze.core.context;
import glaze.core.traits;

using std::int64_t;
using std::size_t;

export namespace glz
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

         auto parse_digits = [&s](size_t start, size_t count) -> int {
            int val = 0;
            for (size_t i = 0; i < count; ++i) {
               const char c = s[start + i];
               if (c < '0' || c > '9') return -1;
               val = val * 10 + (c - '0');
            }
            return val;
         };

         const int yr = parse_digits(0, 4);
         const int mo = parse_digits(5, 2);
         const int dy = parse_digits(8, 2);
         const int hr = parse_digits(11, 2);
         const int mi = parse_digits(14, 2);
         const int sc = parse_digits(17, 2);

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
               const int tz_hour = parse_digits(pos, 2);
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
                  const int m = parse_digits(pos, 2);
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
   }
}
