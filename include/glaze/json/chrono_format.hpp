// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <string_view>
#include <type_traits>

#include "glaze/core/chrono.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

// Per-field chrono customization wrappers.
//
// These decouple the *format* from the chrono *type*, addressing the limitation
// that the clock type alone dictates the wire representation. They are applied
// inside glz::object(...) within a glz::meta<T> specialization:
//
//   template <> struct glz::meta<event> {
//      using T = event;
//      static constexpr auto value = glz::object(
//         "start",  glz::date_format(&T::start, "%Y-%m-%d %H:%M:%S"),
//         "logged", glz::epoch_count<std::chrono::milliseconds>(&T::logged));
//   };
//
// Both take the member pointer as a value argument. The format string (date_format)
// and member pointer live as data rather than as non-type template parameters, so a
// struct with many distinct formats does not multiply the wrapper's template
// instantiations: the view type is keyed on the member type alone, and the format is
// carried as a runtime std::string_view. The strftime pattern is still validated at
// compile time — a consteval guard in the date_format() factory rejects unsupported
// tokens, a missing calendar date, or time tokens on a year_month_day field. (glz::
// float_format keeps its NTTP form because it leans on std::format's compile-time
// format checking, which date_format's hand-rolled runtime walk does not need.)
//
// Scope (MVP): JSON only. date_format supports system_clock time_points and
// year_month_day; epoch_count supports system_clock time_points. utc_time and
// the binary backends are intentionally out of scope here.

namespace glz
{
   // ============================================
   // glz::date_format — per-field strftime-subset format string
   // ============================================

   // View carrying the bound member and the (runtime) format string. Parameterized on the
   // member type alone, so two fields that differ only in their format string share one set
   // of to<>/from<> instantiations.
   //
   // glaze_reflect = false keeps the binary backends from silently reflecting the
   // wrapper as an aggregate: date_format is a textual, JSON-only wrapper, so a
   // binary serialization should fail loudly (undefined to<BEVE, ...>) rather than
   // emit a meaningless encoding of the wrapper struct.
   template <class T>
   struct date_format_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr bool glaze_reflect = false;
      using value_type = T;
      T& val;
      std::string_view fmt;
   };

   template <class T>
   struct to<JSON, date_format_t<T>>
   {
      template <auto Opts, class B>
      static void op(auto&& wrapper, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         using namespace std::chrono;
         using V = std::remove_cvref_t<T>;
         static_assert(is_system_time_point<V> || is_year_month_day<V>,
                       "glz::date_format requires a std::chrono::system_clock time_point or year_month_day member");

         const std::string_view fmt = wrapper.fmt;
         chrono_detail::date_time_fields f{};

         if constexpr (is_year_month_day<V>) {
            // Reject a default/invalid year_month_day (month 0, day 0, Feb 30, ...) on write so the
            // wrapper never emits a string its own reader would reject, and so an out-of-range
            // month/day cannot slip past write_digits<2> as silently truncated low digits.
            if (!wrapper.val.ok()) [[unlikely]] {
               ctx.error = error_code::constraint_violated;
               return;
            }
            f.year = static_cast<int>(wrapper.val.year());
            f.month = static_cast<int>(static_cast<unsigned>(wrapper.val.month()));
            f.day = static_cast<int>(static_cast<unsigned>(wrapper.val.day()));
         }
         else {
            const auto tp = wrapper.val;
            const auto dp = floor<days>(tp);
            const year_month_day ymd{dp};
            f.year = static_cast<int>(ymd.year());
            f.month = static_cast<int>(static_cast<unsigned>(ymd.month()));
            f.day = static_cast<int>(static_cast<unsigned>(ymd.day()));

            // hh_mm_ss at seconds precision: %S emits integer seconds only.
            const hh_mm_ss tod{floor<seconds>(tp - dp)};
            f.hour = static_cast<int>(tod.hours().count());
            f.minute = static_cast<int>(tod.minutes().count());
            f.second = static_cast<int>(tod.seconds().count());
         }

         // Same RFC 3339 constraint as the ISO 8601 writers: a 4-digit year only.
         if (f.year < 0 || f.year > 9999) [[unlikely]] {
            ctx.error = error_code::constraint_violated;
            return;
         }

         // Upper bound on rendered size: the widest token is %F, which expands two source
         // characters to "YYYY-MM-DD" (10 bytes, 5 per source char). *6 bounds it with room
         // to spare, plus a small constant for the surrounding quotes. Skipped on the
         // write_unchecked fast path, where the caller has already reserved the space.
         if constexpr (not check_write_unchecked(Opts)) {
            const size_t max_size = fmt.size() * 6 + 4;
            if (!ensure_space(ctx, b, ix + max_size)) [[unlikely]] {
               return;
            }
         }

         if constexpr (not check_unquoted(Opts)) {
            b[ix++] = '"';
         }
         chrono_detail::write_date_format(fmt, f, b, ix);
         if constexpr (not check_unquoted(Opts)) {
            b[ix++] = '"';
         }
      }
   };

   template <class T>
   struct from<JSON, date_format_t<T>>
   {
      template <auto Opts>
      static void op(auto&& wrapper, is_context auto&& ctx, auto&&... args) noexcept
      {
         using namespace std::chrono;
         using V = std::remove_cvref_t<T>;
         static_assert(is_system_time_point<V> || is_year_month_day<V>,
                       "glz::date_format requires a std::chrono::system_clock time_point or year_month_day member");

         std::string_view str;
         from<JSON, std::string_view>::template op<Opts>(str, ctx, args...);
         if (bool(ctx.error)) [[unlikely]]
            return;

         chrono_detail::date_time_fields f{};
         chrono_detail::parse_date_format(wrapper.fmt, str, f, ctx.error);
         if (bool(ctx.error)) [[unlikely]]
            return;

         const year_month_day ymd{year{f.year}, month{static_cast<unsigned>(f.month)},
                                  day{static_cast<unsigned>(f.day)}};
         if (!ymd.ok()) [[unlikely]] {
            ctx.error = error_code::parse_error;
            return;
         }

         if constexpr (is_year_month_day<V>) {
            wrapper.val = ymd;
         }
         else {
            using Duration = typename V::duration;
            // Anchor the day at seconds precision before adding the time-of-day. MSVC's
            // std::chrono::hours and minutes use a 32-bit rep, so `sys_days + hours + minutes`
            // overflows the intermediate minutes count (days * 24 * 60 exceeds INT_MAX for
            // far-future years, e.g. ~4.2e9 at year 9999) and silently wraps to a bogus
            // instant. sys_seconds widens the arithmetic to 64 bits before the time-of-day
            // is folded in, so the reconstruction is exact on every supported standard library.
            const auto tp = sys_seconds{sys_days{ymd}} + hours{f.hour} + minutes{f.minute} + seconds{f.second};
            wrapper.val = time_point_cast<Duration>(tp);
         }
      }
   };

   namespace chrono_detail
   {
      // Compile-time error reporting for the consteval date_format() factory.
      //
      // These are intentionally NOT constexpr: when validate_date_format() takes an error
      // branch during constant evaluation, calling one makes the surrounding immediate
      // invocation non-constant, which the compiler reports as a hard error naming the
      // function (the function name is the diagnostic). We cannot use `throw` here — it is
      // ill-formed under -fno-exceptions even inside an immediate function, and glaze's
      // test-suite (and many consumers) build with exceptions disabled. We cannot use
      // static_assert either, because a function parameter is never a constant expression
      // within the function body.
      //
      //   supported tokens: %Y %m %d %H %M %S %F %T %%
      //   a full date (%Y %m %d, or %F) is required
      //   a year_month_day field must not include time tokens (%H %M %S %T)
      inline void glaze_date_format_unsupported_token() {}
      inline void glaze_date_format_missing_full_date() {}
      inline void glaze_date_format_time_tokens_on_year_month_day() {}

      // Compile-time validation for date_format(). Rejects unsupported tokens, a missing
      // calendar date, and time tokens applied to a year_month_day field. Runs inside the
      // consteval date_format() factory, where the literal is a constant expression, even
      // though the format is carried onward as a runtime value.
      template <class Mem>
      consteval void validate_date_format(std::string_view fmt)
      {
         static_assert(is_system_time_point<Mem> || is_year_month_day<Mem>,
                       "glz::date_format requires a std::chrono::system_clock time_point or year_month_day member");
         if (!date_format_tokens_valid(fmt)) {
            glaze_date_format_unsupported_token();
         }
         if (!date_format_has_full_date(fmt)) {
            glaze_date_format_missing_full_date();
         }
         if constexpr (is_year_month_day<Mem>) {
            if (date_format_has_time(fmt)) {
               glaze_date_format_time_tokens_on_year_month_day();
            }
         }
      }
   }

   // The object element: an invocable value carrying the member pointer + format. When the
   // serializer iterates the meta object it calls this with the live instance (the same
   // std::invocable dispatch every glaze field wrapper uses) to bind the member by reference.
   template <class MemPtr>
   struct date_format_spec
   {
      MemPtr ptr;
      std::string_view fmt;

      constexpr auto operator()(auto&& parent) const
      {
         using Mem = std::remove_reference_t<decltype(parent.*ptr)>;
         return date_format_t<Mem>{parent.*ptr, fmt};
      }
   };

   // Usage: glz::date_format(&T::member, "%Y-%m-%d %H:%M:%S")
   template <class MemPtr>
   consteval auto date_format(MemPtr ptr, const char* fmt)
   {
      using Mem = std::remove_cvref_t<typename unwrap_pointer<MemPtr>::type>;
      const std::string_view sv{fmt};
      chrono_detail::validate_date_format<Mem>(sv);
      return date_format_spec<MemPtr>{ptr, sv};
   }

   // ============================================
   // glz::epoch_count — per-field Unix timestamp count
   // ============================================

   // View serializing a system_clock time_point field as a numeric Unix timestamp in units
   // of Duration, the per-field counterpart to the glz::epoch_time storage wrapper (so one
   // field can be an epoch count while others stay ISO 8601).
   template <class Duration, class T>
   struct epoch_count_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr bool glaze_reflect = false;
      using value_type = T;
      T& val;
   };

   template <class Duration, class T>
   struct to<JSON, epoch_count_t<Duration, T>>
   {
      template <auto Opts, class B>
      static void op(auto&& wrapper, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         using V = std::remove_cvref_t<T>;
         static_assert(is_system_time_point<V>,
                       "glz::epoch_count requires a std::chrono::system_clock time_point member");
         static_assert(is_duration<Duration>,
                       "glz::epoch_count requires a std::chrono::duration unit (e.g. std::chrono::milliseconds)");
         using Rep = typename Duration::rep;
         const auto count = std::chrono::duration_cast<Duration>(wrapper.val.time_since_epoch()).count();
         to<JSON, Rep>::template op<Opts>(count, ctx, b, ix);
      }
   };

   template <class Duration, class T>
   struct from<JSON, epoch_count_t<Duration, T>>
   {
      template <auto Opts>
      static void op(auto&& wrapper, is_context auto&& ctx, auto&&... args) noexcept
      {
         using V = std::remove_cvref_t<T>;
         static_assert(is_system_time_point<V>,
                       "glz::epoch_count requires a std::chrono::system_clock time_point member");
         static_assert(is_duration<Duration>,
                       "glz::epoch_count requires a std::chrono::duration unit (e.g. std::chrono::milliseconds)");
         using Rep = typename Duration::rep;
         Rep count{};
         from<JSON, Rep>::template op<Opts>(count, ctx, args...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         using TPDuration = typename V::duration;
         wrapper.val = V{std::chrono::duration_cast<TPDuration>(Duration{count})};
      }
   };

   template <class Duration, class MemPtr>
   struct epoch_count_spec
   {
      MemPtr ptr;

      constexpr auto operator()(auto&& parent) const
      {
         using Mem = std::remove_reference_t<decltype(parent.*ptr)>;
         return epoch_count_t<Duration, Mem>{parent.*ptr};
      }
   };

   // Usage: glz::epoch_count<std::chrono::milliseconds>(&T::member)
   template <class Duration, class MemPtr>
   constexpr auto epoch_count(MemPtr ptr)
   {
      using Mem = std::remove_cvref_t<typename unwrap_pointer<MemPtr>::type>;
      static_assert(is_system_time_point<Mem>,
                    "glz::epoch_count requires a std::chrono::system_clock time_point member");
      static_assert(is_duration<Duration>,
                    "glz::epoch_count requires a std::chrono::duration unit (e.g. std::chrono::milliseconds)");
      return epoch_count_spec<Duration, MemPtr>{ptr};
   }
}
