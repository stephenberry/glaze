// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "glaze/core/context.hpp"
#include "glaze/core/feature_test.hpp"
#include "glaze/core/meta.hpp"
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

   // Concept for utc_clock time_points (serialize as ISO 8601 string, leap seconds as :60)
   //
   // utc_clock counts leap seconds, so utc_time and sys_time differ by a whole number of seconds
   // that grows with each leap second. Only periods that divide one second are supported: the
   // offset is then an exact number of ticks. A coarser utc_time (minutes, days) has tick
   // boundaries that drift away from the civil ones, so it has no exact calendar representation.
#if GLZ_HAS_UTC_CLOCK
   template <class T>
   concept is_utc_time_point =
      is_time_point<T> && std::is_same_v<typename std::remove_cvref_t<T>::clock, std::chrono::utc_clock> &&
      std::ratio_divide<std::ratio<1>, typename std::remove_cvref_t<T>::duration::period>::den == 1;
#else
   template <class T>
   concept is_utc_time_point = false;
#endif

   // Concept for time_points that denote a UTC calendar instant, which the text formats write
   // as an ISO 8601 / RFC 3339 timestamp.
   template <class T>
   concept is_calendar_time_point = is_system_time_point<T> || is_utc_time_point<T>;

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

   // Concept for time points that serialize as a bare numeric count: steady_clock and a
   // distinct high_resolution_clock. Their epochs are implementation-defined, so (unlike
   // system_clock) there is no portable calendar representation -- the count in the time
   // point's own duration period is the payload. The two clocks are mutually exclusive and
   // both disjoint from system_clock.
   template <class T>
   concept is_count_time_point = is_steady_time_point<T> || is_high_res_time_point<T>;

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

   // ============================================
   // std::chrono::duration generic serialization
   // ============================================
   //
   // A duration carries no calendar semantics, so every format serializes it as
   // the bare numeric `rep` count expressed in the duration's own period. The
   // conversion is identical across formats, so it is defined once here generically
   // (parameterized over Format) rather than repeated per format. Round-trips are
   // exact: the count is read and written at the duration's native precision with
   // no unit conversion.
   //
   // Every format's read/write header includes this file, so the generic applies
   // uniformly: a duration round-trips identically through every Glaze format. BSON
   // is the one exception: it overrides this generic with a more specialized
   // to<BSON, T>/from<BSON, T> (which wins under partial ordering) because its
   // element model requires a per-type `type_code`, so it delegates to the rep
   // type's writer itself.

   template <uint32_t Format, is_duration T>
      requires(not custom_write<T>)
   struct to<Format, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using Rep = typename std::remove_cvref_t<T>::rep;
         to<Format, Rep>::template op<Opts>(value.count(), ctx, std::forward<Args>(args)...);
      }

      // Used by binary formats (e.g. BEVE) that elide the per-value type tag in
      // certain contexts, such as numeric map keys. Only instantiated when such a
      // context applies, so formats without a no_header concept (e.g. JSON, CBOR,
      // MsgPack) never require to<Format, Rep>::no_header to exist.
      template <auto Opts, class... Args>
      static void no_header(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using Rep = typename std::remove_cvref_t<T>::rep;
         to<Format, Rep>::template no_header<Opts>(value.count(), ctx, std::forward<Args>(args)...);
      }
   };

   template <uint32_t Format, is_duration T>
      requires(not custom_read<T>)
   struct from<Format, T>
   {
      // Standard path: the type tag (if any) is still in the stream. Used by text
      // formats (JSON, CBOR) and by tagged binary formats reading with the header
      // present (e.g. BEVE outside a no_header context).
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using V = std::remove_cvref_t<T>;
         typename V::rep count{};
         from<Format, typename V::rep>::template op<Opts>(count, ctx, std::forward<Args>(args)...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = V(count);
      }

      // Pre-read-tag path: the caller has already consumed the type tag and passes
      // it through. MsgPack uses this for every value; BEVE uses it inside
      // no_header contexts. Selected unambiguously over the overload above because
      // its second parameter is the tag byte rather than the context.
      template <auto Opts, class... Args>
      static void op(auto&& value, const uint8_t tag, is_context auto&& ctx, Args&&... args) noexcept
      {
         using V = std::remove_cvref_t<T>;
         typename V::rep count{};
         from<Format, typename V::rep>::template op<Opts>(count, tag, ctx, std::forward<Args>(args)...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = V(count);
      }
   };

   // ============================================
   // steady_clock / high_resolution_clock time_point generic serialization
   // ============================================
   //
   // These clocks have implementation-defined epochs, so they serialize as the bare count
   // of their duration since epoch -- the same numeric form as a duration. The conversion
   // is format-agnostic and defined once here. As with durations, BSON overrides it with a
   // type_code-bearing specialization; every other format uses this generic.

   template <uint32_t Format, is_count_time_point T>
      requires(not custom_write<T>)
   struct to<Format, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using Rep = typename std::remove_cvref_t<T>::rep;
         to<Format, Rep>::template op<Opts>(value.time_since_epoch().count(), ctx, std::forward<Args>(args)...);
      }

      template <auto Opts, class... Args>
      static void no_header(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using Rep = typename std::remove_cvref_t<T>::rep;
         to<Format, Rep>::template no_header<Opts>(value.time_since_epoch().count(), ctx, std::forward<Args>(args)...);
      }
   };

   template <uint32_t Format, is_count_time_point T>
      requires(not custom_read<T>)
   struct from<Format, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         using V = std::remove_cvref_t<T>;
         using Duration = typename V::duration;
         typename V::rep count{};
         from<Format, typename V::rep>::template op<Opts>(count, ctx, std::forward<Args>(args)...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = V(Duration(count));
      }

      template <auto Opts, class... Args>
      static void op(auto&& value, const uint8_t tag, is_context auto&& ctx, Args&&... args) noexcept
      {
         using V = std::remove_cvref_t<T>;
         using Duration = typename V::duration;
         typename V::rep count{};
         from<Format, typename V::rep>::template op<Opts>(count, tag, ctx, std::forward<Args>(args)...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = V(Duration(count));
      }
   };

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

      // Write the character c to b at ix, advancing ix. Converts to the buffer's element type,
      // so binary formats writing into a std::byte buffer share the text writers below.
      template <class B>
      inline void write_char(B& b, auto& ix, char c) noexcept
      {
         b[ix++] = static_cast<std::remove_cvref_t<decltype(b[ix])>>(c);
      }

      // Write `val` as exactly N zero-padded decimal digits to b starting at ix, advancing ix.
      // Caller must ensure b has at least N bytes of capacity at ix.
      template <size_t N, class B>
      inline void write_digits(B& b, auto& ix, uint64_t val) noexcept
      {
         for (size_t i = N; i > 0; --i) {
            b[ix + i - 1] = static_cast<std::remove_cvref_t<decltype(b[ix])>>('0' + val % 10);
            val /= 10;
         }
         ix += N;
      }

      // ============================================
      // Shared ISO 8601 / RFC 3339 writers
      // ============================================
      //
      // The digit layout of an ISO 8601 date or timestamp is identical in every text
      // format; the only thing that varies is whether the scalar is wrapped in quotes.
      // JSON emits a quoted string (unless the caller opted out via `unquoted`), while
      // YAML emits a plain scalar, so the writers below take `Quote` as a parameter and
      // are shared rather than duplicated per format.
      //
      // Like write_digits, these do not grow the buffer: the caller reserves capacity
      // using the iso_*_max_size constants below and the writers then dump unchecked.

      // Fractional-second digits implied by a time point's period. Anything finer than
      // nanoseconds is still rendered at nanosecond precision, matching RFC 3339's
      // practical limit.
      template <class Period>
      inline constexpr size_t iso_frac_digits = [] {
         if constexpr (std::ratio_greater_equal_v<Period, std::ratio<1>>) {
            return 0; // seconds or coarser
         }
         else if constexpr (std::ratio_greater_equal_v<Period, std::milli>) {
            return 3;
         }
         else if constexpr (std::ratio_greater_equal_v<Period, std::micro>) {
            return 6;
         }
         else {
            return 9;
         }
      }();

      // "YYYY-MM-DD" plus both quotes.
      inline constexpr size_t iso_date_max_size = 12;

      // "YYYY-MM-DDTHH:MM:SSZ" (20) plus both quotes, plus '.' and the fraction.
      template <class Period>
      inline constexpr size_t iso_time_point_max_size =
         22 + (iso_frac_digits<Period> > 0 ? 1 + iso_frac_digits<Period> : 0);

      // Write "YYYY-MM-DD". RFC 3339 requires a 4-digit year, and the fixed-width parsers
      // on the read side cannot represent anything else, so a year outside [0000, 9999] is
      // rejected rather than silently emitted as wrapped digits.
      template <bool Quote, class B>
      inline void write_iso_date(int yr, unsigned mo, unsigned dy, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         if (yr < 0 || yr > 9999) [[unlikely]] {
            ctx.error = error_code::constraint_violated;
            return;
         }

         if constexpr (Quote) {
            write_char(b, ix, '"');
         }
         write_digits<4>(b, ix, static_cast<uint64_t>(yr));
         write_char(b, ix, '-');
         write_digits<2>(b, ix, mo);
         write_char(b, ix, '-');
         write_digits<2>(b, ix, dy);
         if constexpr (Quote) {
            write_char(b, ix, '"');
         }
      }

      // ============================================
      // UTC wall-clock readings
      // ============================================
      //
      // Every calendar time point is written as the UTC wall-clock reading of its instant. For
      // system_clock that reading is the time point itself. utc_clock also counts leap seconds,
      // and during a positive leap second the wall clock reads 23:59:60, which no sys_time can
      // represent. The reading therefore carries a flag: `time` holds the preceding :59 second
      // plus the elapsed fraction, and `leap_second` tells the writer to render the seconds
      // field as 60.

      template <class Duration>
      struct wall_clock_time
      {
         std::chrono::sys_time<Duration> time{};
         bool leap_second{};
      };

      template <is_system_time_point TP>
      constexpr bool to_wall_clock(const TP& value, wall_clock_time<typename TP::duration>& out, error_code&) noexcept
      {
         out = {value, false};
         return true;
      }

#if GLZ_HAS_UTC_CLOCK
      // Leap-second lookups (get_leap_second_info, utc_clock::from_sys) read the time zone
      // database, which libstdc++ and MSVC load on first use and report as unavailable by
      // throwing. The serializers are noexcept, so the failure is mapped to an error code.
      // Without exceptions the standard library terminates instead, which cannot be intercepted.
      template <class F>
      bool call_leap_second_lookup(F&& lookup, error_code& ec) noexcept
      {
#if __cpp_exceptions
         try {
            lookup();
         }
         catch (...) {
            ec = error_code::feature_not_supported;
            return false;
         }
#else
         lookup();
         (void)ec;
#endif
         return true;
      }

      template <is_utc_time_point TP>
      bool to_wall_clock(const TP& value, wall_clock_time<typename TP::duration>& out, error_code& ec) noexcept
      {
         using Duration = typename TP::duration;
         std::chrono::leap_second_info info{};
         if (!call_leap_second_lookup([&] { info = std::chrono::get_leap_second_info(value); }, ec)) [[unlikely]] {
            return false;
         }
         // `elapsed` includes the leap second `value` falls in, so subtracting it lands a leap
         // second on the preceding :59 second. The period divides one second, so the cast is exact.
         out.time = std::chrono::sys_time<Duration>{value.time_since_epoch() -
                                                    std::chrono::duration_cast<Duration>(info.elapsed)};
         out.leap_second = info.is_leap_second;
         return true;
      }
#endif

      // Write a UTC wall-clock reading as a full ISO 8601 timestamp, "YYYY-MM-DDTHH:MM:SS[.f]Z",
      // with fractional digits implied by the period.
      template <bool Quote, class Duration, class B>
      inline void write_iso_timestamp(const wall_clock_time<Duration>& wall, is_context auto&& ctx, B&& b,
                                      auto& ix) noexcept
      {
         using namespace std::chrono;
         using Period = typename Duration::period;

         const auto dp = floor<days>(wall.time);
         const year_month_day ymd{dp};
         const int yr = static_cast<int>(ymd.year());
         if (yr < 0 || yr > 9999) [[unlikely]] {
            ctx.error = error_code::constraint_violated;
            return;
         }

         const hh_mm_ss tod{floor<Duration>(wall.time - dp)};
         const auto hr = static_cast<unsigned>(tod.hours().count());
         const auto mi = static_cast<unsigned>(tod.minutes().count());
         const auto sc = static_cast<unsigned>(tod.seconds().count()) + (wall.leap_second ? 1u : 0u);

         if constexpr (Quote) {
            write_char(b, ix, '"');
         }
         write_digits<4>(b, ix, static_cast<uint64_t>(yr));
         write_char(b, ix, '-');
         write_digits<2>(b, ix, static_cast<unsigned>(ymd.month()));
         write_char(b, ix, '-');
         write_digits<2>(b, ix, static_cast<unsigned>(ymd.day()));
         write_char(b, ix, 'T');
         write_digits<2>(b, ix, hr);
         write_char(b, ix, ':');
         write_digits<2>(b, ix, mi);
         write_char(b, ix, ':');
         write_digits<2>(b, ix, sc);

         constexpr size_t frac_digits = iso_frac_digits<Period>;
         if constexpr (frac_digits > 0) {
            write_char(b, ix, '.');
            const auto subsec = tod.subseconds();
            if constexpr (frac_digits == 3) {
               write_digits<3>(b, ix, static_cast<uint64_t>(duration_cast<milliseconds>(subsec).count()));
            }
            else if constexpr (frac_digits == 6) {
               write_digits<6>(b, ix, static_cast<uint64_t>(duration_cast<microseconds>(subsec).count()));
            }
            else {
               write_digits<9>(b, ix, static_cast<uint64_t>(duration_cast<nanoseconds>(subsec).count()));
            }
         }

         write_char(b, ix, 'Z');
         if constexpr (Quote) {
            write_char(b, ix, '"');
         }
      }

      // Write a calendar time point as ISO 8601 in UTC.
      //
      // Time points whose period is exactly `days` (e.g. std::chrono::sys_days) are written
      // as a date-only "YYYY-MM-DD": the time of day is always zero at that precision and
      // the calendar date is the meaningful payload. Coarser periods (weeks, months, years)
      // fall through to the full timestamp path, which avoids silently truncating an
      // arbitrary date to a multi-day boundary on read.
      template <bool Quote, is_calendar_time_point TP, class B>
      inline void write_iso_time_point(const TP& value, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         using namespace std::chrono;
         using Duration = typename std::remove_cvref_t<TP>::duration;

         if constexpr (std::ratio_equal_v<typename Duration::period, std::ratio<86400>>) {
            // Only system_clock: is_utc_time_point excludes periods coarser than one second.
            const year_month_day ymd{floor<days>(value)};
            write_iso_date<Quote>(static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                                  static_cast<unsigned>(ymd.day()), ctx, b, ix);
         }
         else {
            wall_clock_time<Duration> wall{};
            if (!to_wall_clock(value, wall, ctx.error)) [[unlikely]] {
               return;
            }
            write_iso_timestamp<Quote>(wall, ctx, b, ix);
         }
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

      // Assemble a system_clock time_point from whole seconds since the epoch plus a
      // sub-second part in [0s, 1s). The sum is formed in the target's own precision: an
      // int64 nanosecond count only spans 1677-09-21 to 2262-04-11, so summing in
      // nanoseconds first wraps for dates a coarser target holds comfortably (year 9999
      // fits sys_seconds). Returns false and leaves value unchanged when the target cannot
      // represent the instant, rather than wrapping.
      template <is_system_time_point TP>
      [[nodiscard]] constexpr bool make_sys_time(TP& value, std::chrono::seconds secs,
                                                 std::chrono::nanoseconds subsec) noexcept
      {
         using namespace std::chrono;
         using Duration = typename TP::duration;

         // time_point_cast truncates toward zero. Measuring a pre-epoch instant back from
         // the next whole second keeps both parts on the same side of zero, so truncating
         // them separately matches truncating their sum.
         if (secs < seconds{0} && subsec > nanoseconds{0}) {
            secs += seconds{1};
            subsec -= seconds{1};
         }

         const Duration frac = duration_cast<Duration>(subsec);

         if constexpr (!treat_as_floating_point_v<typename Duration::rep>) {
            if constexpr (std::ratio_less_v<typename Duration::period, std::ratio<1>>) {
               // A target finer than seconds scales the count up. max()/min() are not whole
               // seconds (nanoseconds::max() is 2262-04-11T23:47:16.854775807), so the boundary
               // second accepts exactly the fraction the target still holds past it.
               constexpr seconds max_secs = duration_cast<seconds>((Duration::max)());
               constexpr seconds min_secs = duration_cast<seconds>((Duration::min)());
               constexpr Duration max_frac = (Duration::max)() - duration_cast<Duration>(max_secs);
               constexpr Duration min_frac = (Duration::min)() - duration_cast<Duration>(min_secs);
               if (secs > max_secs || secs < min_secs || (secs == max_secs && frac > max_frac) ||
                   (secs == min_secs && frac < min_frac)) {
                  return false;
               }
            }
            else {
               // A target of seconds or coarser divides the count down, which cannot overflow
               // in int64, but its rep may still be narrower than int64 (MSVC's minutes is int).
               const auto ticks = duration_cast<duration<int64_t, typename Duration::period>>(secs).count();
               if (std::cmp_greater(ticks, (Duration::max)().count()) ||
                   std::cmp_less(ticks, (Duration::min)().count())) {
                  return false;
               }
            }
         }

         value = TP{duration_cast<Duration>(secs) + frac};
         return true;
      }

      // Assign a calendar time point from a UTC wall-clock reading: whole POSIX seconds `secs`
      // (already corrected for any UTC offset) plus a sub-second part. `leap_second` marks a
      // reading whose seconds field was 60, in which case `secs` holds the preceding :59 second.
      // On failure, sets ec and leaves value unchanged.
      //
      // Only utc_clock can represent a leap second, so a system_clock target rejects one.
      template <is_system_time_point TP>
      inline void from_wall_clock(TP& value, std::chrono::seconds secs, std::chrono::nanoseconds subsec,
                                  bool leap_second, error_code& ec) noexcept
      {
         if (leap_second || !make_sys_time(value, secs, subsec)) [[unlikely]] {
            ec = error_code::parse_error;
         }
      }

#if GLZ_HAS_UTC_CLOCK
      // A reading of :60 is accepted only where a leap second was actually inserted.
      template <is_utc_time_point TP>
      inline void from_wall_clock(TP& value, std::chrono::seconds secs, std::chrono::nanoseconds subsec,
                                  bool leap_second, error_code& ec) noexcept
      {
         using namespace std::chrono;
         using Duration = typename TP::duration;

         sys_time<Duration> sys{};
         if (!make_sys_time(sys, secs, subsec)) [[unlikely]] {
            ec = error_code::parse_error;
            return;
         }

         // The leap seconds elapsed are constant across a whole second, so they are looked up
         // at seconds precision, where the conversion cannot overflow.
         utc_seconds anchor{};
         bool inserted = true;
         if (!call_leap_second_lookup(
                [&] {
                   anchor = utc_clock::from_sys(sys_seconds{secs});
                   if (leap_second) {
                      inserted = get_leap_second_info(anchor + seconds{1}).is_leap_second;
                   }
                },
                ec)) [[unlikely]] {
            return;
         }
         if (!inserted) [[unlikely]] {
            ec = error_code::parse_error;
            return;
         }

         const Duration offset =
            duration_cast<Duration>(anchor.time_since_epoch() - secs + seconds{leap_second ? 1 : 0});
         if constexpr (!treat_as_floating_point_v<typename Duration::rep>) {
            const Duration since_epoch = sys.time_since_epoch();
            if ((offset > Duration::zero() && since_epoch > (Duration::max)() - offset) ||
                (offset < Duration::zero() && since_epoch < (Duration::min)() - offset)) [[unlikely]] {
               ec = error_code::parse_error;
               return;
            }
         }
         value = TP{sys.time_since_epoch() + offset};
      }
#endif

      // Parse an RFC 3339 / ISO 8601 date-time string into a calendar time_point.
      // A seconds field of 60 is a leap second, which only a utc_clock target accepts.
      // On failure, sets ec (parse_error for malformed input) and leaves value unchanged.
      template <is_calendar_time_point TP>
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

         if (mo < 1 || mo > 12 || dy < 1 || dy > 31 || hr > 23 || mi > 59 || sc > 60) {
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

         // Anchored at seconds so the time-of-day is folded in with 64-bit arithmetic (MSVC's
         // hours and minutes use a 32-bit rep).
         const bool leap_second = sc == 60;
         const auto tp = sys_seconds{sys_days{ymd}} + hours{hr} + minutes{mi} + seconds{leap_second ? 59 : sc} +
                         seconds{tz_offset_seconds};
         from_wall_clock(value, tp.time_since_epoch(), nanoseconds{subsec_nanos}, leap_second, ec);
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
         // consume, and otherwise surfaces as unconsumed trailing input. 60 is a leap
         // second, which the caller accepts only for a utc_clock target.
         const auto parse_seconds = [&]() -> bool {
            const int sec = take(2);
            if (sec < 0 || sec > 60) return false;
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
