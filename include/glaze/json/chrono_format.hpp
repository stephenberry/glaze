// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <string_view>
#include <type_traits>

#include "glaze/core/chrono.hpp"
#include "glaze/core/format_str.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

// Per-field chrono customization wrappers.
//
// These decouple the *format* from the chrono *type*, addressing the limitation
// that the clock type alone dictates the wire representation. They follow the
// established field-wrapper precedents (glz::float_format for the NTTP format
// string, glz::quoted for the fully-custom read+write pair) and are applied
// inside glz::object(...) within a glz::meta<T> specialization:
//
//   template <> struct glz::meta<event> {
//      using T = event;
//      static constexpr auto value = glz::object(
//         "start",  glz::date_format<&T::start, "%Y-%m-%d %H:%M:%S">,
//         "logged", glz::epoch_count<&T::logged, std::chrono::milliseconds>);
//   };
//
// Scope (MVP): JSON only. date_format supports system_clock time_points and
// year_month_day; epoch_count supports system_clock time_points. utc_time and
// the binary backends are intentionally out of scope here.

namespace glz
{
   // ============================================
   // glz::date_format — per-field strftime-subset format string
   // ============================================

   // Wrapper carrying the compile-time format string and a reference to the field.
   // Both directions are fully custom (no meta<>::value indirection) because the
   // underlying chrono parser only understands ISO 8601 and cannot honor Fmt.
   //
   // glaze_reflect = false keeps the binary backends from silently reflecting the
   // wrapper as an aggregate: date_format is a textual, JSON-only wrapper, so a
   // binary serialization should fail loudly (undefined to<BEVE, ...>) rather than
   // emit a meaningless encoding of the wrapper struct.
   template <format_str Fmt, class T>
   struct date_format_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr bool glaze_reflect = false;
      using value_type = T;
      T& val;
   };

   namespace chrono_detail
   {
      // Compile-time guards shared by the date_format read and write paths.
      template <format_str Fmt, class V>
      consteval void validate_date_format() noexcept
      {
         static_assert(is_system_time_point<V> || is_year_month_day<V>,
                       "glz::date_format requires a std::chrono::system_clock time_point or year_month_day member");
         static_assert(date_format_tokens_valid(std::string_view(Fmt)),
                       "glz::date_format: unsupported format token (supported: %Y %m %d %H %M %S %F %T %%)");
         static_assert(date_format_has_full_date(std::string_view(Fmt)),
                       "glz::date_format: format must include a full date (%Y %m %d, or %F)");
         if constexpr (is_year_month_day<V>) {
            static_assert(!date_format_has_time(std::string_view(Fmt)),
                          "glz::date_format: a year_month_day field must not include time tokens (%H %M %S %T)");
         }
      }
   }

   template <format_str Fmt, class T>
   struct to<JSON, date_format_t<Fmt, T>>
   {
      template <auto Opts, class B>
      static void op(auto&& wrapper, is_context auto&& ctx, B&& b, auto& ix) noexcept
      {
         using namespace std::chrono;
         using V = std::remove_cvref_t<T>;
         chrono_detail::validate_date_format<Fmt, V>();

         chrono_detail::date_time_fields f{};

         if constexpr (is_year_month_day<V>) {
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

         // Upper bound on rendered size: the widest token (%T) expands two source
         // characters to "HH:MM:SS" (8 bytes) and %F two chars to "YYYY-MM-DD"
         // (10 bytes), i.e. < 6 bytes per source char; plus the surrounding quotes.
         const size_t max_size = std::string_view(Fmt).size() * 6 + 4;
         if (!ensure_space(ctx, b, ix + max_size)) [[unlikely]] {
            return;
         }

         if constexpr (not check_unquoted(Opts)) {
            b[ix++] = '"';
         }
         chrono_detail::write_date_format(std::string_view(Fmt), f, b, ix);
         if constexpr (not check_unquoted(Opts)) {
            b[ix++] = '"';
         }
      }
   };

   template <format_str Fmt, class T>
   struct from<JSON, date_format_t<Fmt, T>>
   {
      template <auto Opts>
      static void op(auto&& wrapper, is_context auto&& ctx, auto&&... args) noexcept
      {
         using namespace std::chrono;
         using V = std::remove_cvref_t<T>;
         chrono_detail::validate_date_format<Fmt, V>();

         std::string_view str;
         from<JSON, std::string_view>::template op<Opts>(str, ctx, args...);
         if (bool(ctx.error)) [[unlikely]]
            return;

         chrono_detail::date_time_fields f{};
         chrono_detail::parse_date_format(std::string_view(Fmt), str, f, ctx.error);
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
            const auto tp = sys_days{ymd} + hours{f.hour} + minutes{f.minute} + seconds{f.second};
            wrapper.val = time_point_cast<Duration>(tp);
         }
      }
   };

   template <auto MemPtr, format_str Fmt>
   inline constexpr decltype(auto) date_format_impl() noexcept
   {
      return [](auto&& val) { return date_format_t<Fmt, std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }

   // Usage: glz::date_format<&T::member, "%Y-%m-%d %H:%M:%S">
   template <auto MemPtr, format_str Fmt>
   constexpr auto date_format = date_format_impl<MemPtr, Fmt>();

   // ============================================
   // glz::epoch_count — per-field Unix timestamp count
   // ============================================

   // Serializes a system_clock time_point field as a numeric Unix timestamp in
   // units of Duration, the per-field counterpart to the glz::epoch_time storage
   // wrapper (so one field can be an epoch count while others stay ISO 8601).
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
         using Rep = typename Duration::rep;
         Rep count{};
         from<JSON, Rep>::template op<Opts>(count, ctx, args...);
         if (bool(ctx.error)) [[unlikely]]
            return;
         using TPDuration = typename V::duration;
         wrapper.val = V{std::chrono::duration_cast<TPDuration>(Duration{count})};
      }
   };

   template <auto MemPtr, class Duration>
   inline constexpr decltype(auto) epoch_count_impl() noexcept
   {
      return [](auto&& val) {
         return epoch_count_t<Duration, std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr};
      };
   }

   // Usage: glz::epoch_count<&T::member, std::chrono::milliseconds>
   template <auto MemPtr, class Duration>
   constexpr auto epoch_count = epoch_count_impl<MemPtr, Duration>();
}
