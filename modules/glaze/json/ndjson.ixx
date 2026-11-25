// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/json/ndjson.hpp"
// glz:header std=<cstddef>
// glz:header std=<cstring>
// glz:header std=<string>
// glz:header std=<tuple>
// glz:header std=<type_traits>
// glz:header std=<utility>
export module glaze.json.ndjson;

import std;

import glaze.json.read;
import glaze.json.write;

import glaze.core.common;
import glaze.core.context;
import glaze.core.meta;
import glaze.core.opts;
import glaze.core.read;
import glaze.core.streaming_state;
import glaze.core.reflect;
import glaze.core.write;

import glaze.concepts.container_concepts;

import glaze.file.file_ops;

import glaze.util.dump;
import glaze.util.expected;
import glaze.util.for_each;
import glaze.util.string_literal;
import glaze.util.tuple;
import glaze.tuplet;

using std::size_t;

#include "glaze/util/inline.hpp"

namespace glz
{
   template <>
   struct parse<NDJSON>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         from<NDJSON, std::remove_reference_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                     std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   template <>
   struct serialize<NDJSON>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<NDJSON, std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                        std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // Consume the line breaks that separate two records.
   //
   // Both line endings are handled by one loop rather than one after the other, so that a CRLF
   // following a bare LF is still a separator. Scanning for all the CRs and then all the LFs stops
   // at the first CR after an LF and calls it the start of a record.
   //
   // A '\r' whose '\n' has not arrived is left unconsumed rather than rejected. From in here a CRLF
   // split by the window edge and a stray carriage return are the same two bytes; only the caller
   // knows whether more input can still arrive. So this stops there without an error, and the
   // caller either refills and calls again or reports the syntax error itself.
   //
   // A null-terminated buffer stops on the trailing '\0' sentinel. A non-null-terminated buffer has
   // no sentinel, so bound the scan on it != end.
   template <auto Opts>
   GLZ_ALWAYS_INLINE void skip_record_separators(is_context auto& ctx, auto& it, auto& end) noexcept
   {
      if constexpr (Opts.null_terminated) {
         while (true) {
            if (*it == '\n') {
               ++it;
               continue;
            }
            if (*it == '\r') {
               if (*(it + 1) == '\n') {
                  it += 2;
                  continue;
               }
               ctx.error = error_code::syntax_error; // Expected '\n' after '\r'
            }
            return;
         }
      }
      else {
         while (it != end) {
            if (*it == '\n') {
               ++it;
               continue;
            }
            if (*it == '\r') {
               if ((it + 1) == end) {
                  return; // undecidable from this window; the caller refills or errors
               }
               if (*(it + 1) == '\n') {
                  it += 2;
                  continue;
               }
               ctx.error = error_code::syntax_error; // Expected '\n' after '\r'
            }
            return;
         }
      }
   }

   // Whether a record delimiter is anywhere in the window ahead of `it`.
   //
   // Either line ending closes a record, and '\r' has to count on its own: a CRLF can be split by
   // the window edge, and a window holding the whole record plus the '\r' does hold a complete
   // record. Requiring the '\n' would call that record unreadable while it is sitting right there.
   //
   // A raw '\r' or '\n' inside a JSON string is invalid -- both have to be escaped -- so the first
   // one after the start of a record always ends it.
   GLZ_ALWAYS_INLINE bool window_holds_record_end(const char* it, const char* end) noexcept
   {
      const size_t n = size_t(end - it);
      if (n == 0) {
         return false;
      }
      return std::memchr(it, '\n', n) != nullptr || std::memchr(it, '\r', n) != nullptr;
   }

   // Bring a whole record into the window, so the record reader is never handed a partial one.
   //
   // Records are newline delimited, which makes "does the window hold a complete record" a question
   // that can be answered before parsing: look for the delimiter. Nothing weaker works. A refill
   // policy driven by a byte count cannot tell "the window ran out in the middle of this record"
   // from "this record ended with the buffer", and the JSON reader reports both the same way, as a
   // value that parsed cleanly up to `end`. A bare number split by the window edge then reads as
   // two numbers, and neither the reader nor the caller has anything to notice it by.
   //
   // Refilling until the delimiter appears also drops the ceiling a byte count imposes. A record no
   // longer has to fit in whatever fraction of the window happened to be left over from the last
   // one; it only has to fit in the window.
   //
   // Returns false when no refill can produce a delimiter, which means the record really is wider
   // than the window -- the standing limit of streaming, since nothing refills inside a record.
   template <class Ctx>
   bool fill_window_to_record_end(Ctx& ctx, auto& it, auto& end) noexcept
   {
      if constexpr (has_streaming_state<Ctx>) {
         if (ctx.stream.enabled()) {
            while (!window_holds_record_end(it, end)) {
               // The last record in a document may end without a delimiter, so a source with
               // nothing left to give has already delivered the whole record.
               if (ctx.stream.source_at_eof()) {
                  return true;
               }
               const size_t available = size_t(end - it);
               refill_window(ctx, it, end);
               if (size_t(end - it) <= available) {
                  return false; // the refill brought nothing new: the record does not fit
               }
            }
         }
      }
      return true;
   }

   // Position `it` at the start of the next record, with the whole record in the window, and report
   // whether there is one.
   //
   // Records are independent, so the gap between two of them is the only place a streaming window
   // may release what has been parsed and pull in more. Doing that here is what lets a document
   // larger than the window be read at all; without it the read stops at the first window's edge,
   // carrying only the records that happened to fit and calling that a complete document.
   //
   // Returns false when the input is exhausted, when the separators were malformed, and when a
   // record is wider than the window; the caller tells them apart from ctx.error, which is what
   // distinguishes a document that ended from one that could not be read.
   template <auto Opts, class Ctx>
   bool ndjson_has_next_record(Ctx& ctx, auto& it, auto& end) noexcept
   {
      // A run of separators can be wider than the window, so this loops: each pass either reaches a
      // record byte or releases the separators it walked and pulls more input in behind them.
      while (true) {
         skip_record_separators<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return false;
         }
         if (it < end && *it != '\r') {
            break; // a record byte
         }

         // Either the window is spent, or it ends on a '\r' whose '\n' has not arrived. Both are
         // answered by more input, and both are the end of the document if there is none.
         bool progressed = false;
         if constexpr (has_streaming_state<Ctx>) {
            if (ctx.stream.enabled() && !ctx.stream.source_at_eof()) {
               const size_t available = size_t(end - it);
               refill_window(ctx, it, end);
               progressed = size_t(end - it) > available;
            }
         }
         if (!progressed) {
            if (it < end) [[unlikely]] {
               // a '\r' at the true end of the input, with no '\n' to pair it with
               ctx.error = error_code::syntax_error;
            }
            return false;
         }
      }

      if (!fill_window_to_record_end(ctx, it, end)) {
         ctx.error = error_code::streaming_unsupported;
         ctx.custom_error_message =
            "this NDJSON record is wider than the buffer window; a record must fit in the window "
            "because nothing refills inside one";
         return false;
      }
      return true;
   }

   // Read one record into `record`. Returns false on error.
   //
   // A record that closed cleanly leaves ctx.depth at zero, so end_reached there means the input
   // ran out exactly where the record ended rather than in the middle of it -- the record is
   // complete either way, and whether anything follows it is the next iteration's question.
   //
   // Any other way of running out of input means the record itself was cut short. For a buffer that
   // is a truncated document. For a stream with input still pending it is instead the window that
   // ran out: the JSON reader refills between the members of an object and the elements of an
   // array, but nothing refills in the middle of a string or a number, so a single token has to fit
   // in the window. Naming that beats reporting a truncated document, since the document is fine
   // and the buffer is the thing to change.
   template <auto Opts, class Ctx>
   bool read_ndjson_record(auto&& record, Ctx& ctx, auto& it, auto& end)
   {
      parse<JSON>::op<Opts>(record, ctx, it, end);
      resync_window_end(ctx, end); // the JSON reader may have refilled inside the record

      if (!bool(ctx.error)) [[likely]] {
         return true;
      }

      // "The input ran out exactly where this record ended" is only the same statement as "this
      // record is complete" when the input that ran out was the document. If it was the window, the
      // record was cut mid-token and the bytes after the cut are the rest of that same token --
      // accepting it here splits one value into two and reports success, which is the one outcome a
      // reader must never produce. ndjson_has_next_record is what normally prevents this, by
      // refusing to start a record it cannot see the end of; this is the check that makes the
      // guarantee local rather than something the caller has to be trusted to have arranged.
      if (ctx.error == error_code::end_reached && ctx.depth == 0) {
         bool cut_by_window = false;
         if constexpr (has_streaming_state<Ctx>) {
            cut_by_window = ctx.stream.enabled() && !ctx.stream.source_at_eof() && it >= end;
         }
         if (!cut_by_window) {
            ctx.error = error_code::none;
            return true;
         }
      }

      if constexpr (has_streaming_state<Ctx>) {
         const bool ran_out_of_input = ctx.error == error_code::end_reached || ctx.error == error_code::unexpected_end;
         if (ran_out_of_input && ctx.stream.enabled() && !ctx.stream.source_at_eof()) {
            ctx.error = error_code::streaming_unsupported;
            ctx.custom_error_message =
               "this NDJSON record is wider than the buffer window; a record must fit in the window "
               "because nothing refills inside one";
         }
      }
      return false;
   }

   template <class T>
      requires readable_array_t<T> && (emplace_backable<T> || !resizable<T>)
   struct from<NDJSON, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if (it == end) {
            if constexpr (resizable<T>) {
               value.clear();

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
                  value.shrink_to_fit();
               }
            }
         }

         const auto n = value.size();

         auto value_it = value.begin();

         const auto truncate_to = [&](auto first_unwritten) {
            if constexpr (erasable<T>) {
               // erase rather than resize, for element types that are not default constructible
               value.erase(first_unwritten, value.end());

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
                  value.shrink_to_fit();
               }
            }
         };

         for (size_t i = 0; i < n; ++i) {
            if (!ndjson_has_next_record<Opts>(ctx, it, end)) {
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               truncate_to(value_it);
               return;
            }
            if (!read_ndjson_record<Opts>(*value_it, ctx, it, end)) [[unlikely]] {
               return;
            }
            ++value_it;
         }

         // growing
         if constexpr (emplace_backable<T>) {
            while (ndjson_has_next_record<Opts>(ctx, it, end)) {
               if (!read_ndjson_record<Opts>(value.emplace_back(), ctx, it, end)) [[unlikely]] {
                  return;
               }
            }
            if (bool(ctx.error)) [[unlikely]] {
               return; // malformed separators, not the end of the document
            }

            if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
               value.shrink_to_fit();
            }
         }
         else {
            ctx.error = error_code::exceeded_static_array_size;
         }
      }
   };

   template <class T>
      requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
   struct from<NDJSON, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<T>) {
               return reflect<T>::size;
            }
            else {
               return glz::tuple_size_v<T>;
            }
         }();

         for_each<N>([&]<auto I>() {
            if (bool(ctx.error) || !ndjson_has_next_record<Opts>(ctx, it, end)) {
               return; // for_each has no early exit; the guard above stands in for one
            }
            if constexpr (is_std_tuple<T>) {
               (void)read_ndjson_record<Opts>(std::get<I>(value), ctx, it, end);
            }
            else if constexpr (glaze_array_t<T>) {
               (void)read_ndjson_record<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, it, end);
            }
            else {
               (void)read_ndjson_record<Opts>(glz::get<I>(value), ctx, it, end);
            }
         });
      }
   };

   template <writable_array_t T>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const auto is_empty = [&]() -> bool {
            if constexpr (has_size<T>) {
               return value.size() ? false : true;
            }
            else {
               return value.empty();
            }
         }();

         if (!is_empty) {
            auto it = value.begin();
            using Value = core_t<decltype(*it)>;
            to<JSON, Value>::template op<Opts>(*it, ctx, b, ix);
            ++it;
            const auto end = value.end();
            for (; it != end; ++it) {
               dump('\n', b, ix);
               to<JSON, Value>::template op<Opts>(*it, ctx, b, ix);
            }
         }
      }
   };

   template <class T>
      requires glaze_array_t<T> || tuple_t<T>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
            }
            else {
               return glz::tuple_size_v<std::decay_t<T>>;
            }
         }();

         using V = std::decay_t<T>;
         for_each<N>([&]<auto I>() {
            if constexpr (glaze_array_t<V>) {
               serialize<JSON>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, args...);
            }
            else {
               serialize<JSON>::op<Opts>(glz::get<I>(value), ctx, args...);
            }
            constexpr bool needs_new_line = I < N - 1;
            if constexpr (needs_new_line) {
               dump('\n', args...);
            }
         });
      }
   };

   template <class T>
      requires is_std_tuple<std::decay_t<T>>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
            }
            else {
               return glz::tuple_size_v<std::decay_t<T>>;
            }
         }();

         using V = std::decay_t<T>;
         for_each<N>([&]<auto I>() {
            if constexpr (glaze_array_t<V>) {
               serialize<JSON>::op<Opts>(value.*std::get<I>(meta_v<V>), ctx, std::forward<Args>(args)...);
            }
            else {
               serialize<JSON>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...);
            }
            constexpr bool needs_new_line = I < N - 1;
            if constexpr (needs_new_line) {
               dump('\n', std::forward<Args>(args)...);
            }
         });
      }
   };

   export template <read_supported<NDJSON> T, class Buffer>
   [[nodiscard]] auto read_ndjson(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{.format = NDJSON}>(value, std::forward<Buffer>(buffer), ctx);
   }

   export template <read_supported<NDJSON> T, class Buffer>
   [[nodiscard]] expected<T, error_ctx> read_ndjson(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const auto ec = read<opts{.format = NDJSON}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec == error_code::none) {
         return value;
      }
      return unexpected(ec);
   }

   export template <auto Opts = opts{.format = NDJSON}, read_supported<NDJSON> T>
   [[nodiscard]] error_ctx read_file_ndjson(T& value, const sv file_name)
   {
      context ctx{};
      ctx.current_file = file_name;

      std::string buffer;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {0, ec};
      }

      return read<Opts>(value, buffer, ctx);
   }

   export template <write_supported<NDJSON> T, class Buffer>
   [[nodiscard]] error_ctx write_ndjson(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = NDJSON}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   export template <write_supported<NDJSON> T>
   [[nodiscard]] expected<std::string, error_ctx> write_ndjson(T&& value)
   {
      return write<opts{.format = NDJSON}>(std::forward<T>(value));
   }

   export template <write_supported<NDJSON> T>
   [[nodiscard]] error_ctx write_file_ndjson(T&& value, const std::string& file_name, auto&& buffer)
   {
      const auto ec = write<opts{.format = NDJSON}>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }
      const auto file_ec = buffer_to_file(buffer, file_name);
      if (bool(file_ec)) [[unlikely]] {
         return {0, file_ec};
      }
      return {buffer.size(), error_code::none};
   }
}
