// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/core/read.hpp"
// glz:header std=<concepts>
// glz:header std=<cstddef>
// glz:header std=<span>
// glz:header std=<string_view>
// glz:header std=<type_traits>
// glz:header std=<utility>
export module glaze.core.read;

import std;

import glaze.core.buffer_traits;
import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.core.streaming_state;

import glaze.api.std.span;

import glaze.util.parse;
import glaze.concepts.container_concepts;

#include "glaze/util/inline.hpp"

using std::size_t;

namespace glz
{
   // A container whose capacity is fixed at compile time -- glz::inplace_vector, std::inplace_vector --
   // reports being full only through try_emplace_back. Its resize(), reserve() and emplace_back()
   // throw std::bad_alloc past capacity, and abort outright when exceptions are disabled.
   export template <class T>
   concept fixed_capacity_container = has_try_emplace_back<std::remove_cvref_t<T>> && requires(T& t) {
      { t.max_size() } -> std::convertible_to<size_t>;
   };

   // Reject an element count a fixed-capacity target cannot hold, before it is grown to that count.
   // Untrusted input decides these counts, so without this the container's own bad_alloc escapes the
   // error-code API -- and terminates the process outright in a reader marked noexcept.
   export [[nodiscard]] GLZ_ALWAYS_INLINE bool exceeds_capacity(auto& value, const size_t n, is_context auto& ctx) noexcept
   {
      if constexpr (fixed_capacity_container<decltype(value)>) {
         if (n > value.max_size()) [[unlikely]] {
            ctx.error = error_code::exceeded_static_array_size;
            return true;
         }
      }
      return false;
   }

   export template <auto Opts, bool Padded = false>
   auto read_iterators(contiguous auto&& buffer) noexcept
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      auto it = reinterpret_cast<const char*>(buffer.data());
      auto end = reinterpret_cast<const char*>(buffer.data()); // to be incremented

      if constexpr (Padded) {
         end += buffer.size() - padding_bytes;
      }
      else {
         end += buffer.size();
      }

      return std::pair{it, end};
   }

   // Only a non-null-terminated read produces end_reached, and only ever to say "the buffer ran
   // out here"; whether that is an outcome or a failure is settled at the top level and nowhere
   // else. ctx.depth carries the answer: a value that closed cleanly leaves it at zero, so the
   // buffer ended exactly where the value did and the read succeeded. Any other depth means the
   // buffer ran out with containers still open, which is truncated input.
   //
   // Both halves have to be handled here. end_reached is documented as a non-error code and
   // callers are entitled to read it that way, so letting it escape hands them a truncated parse
   // labeled as a success that happened to stop early. It reached the wire that way: since #2732
   // every registry read runs non-null-terminated, so a truncated REPE body answered its client
   // with end_reached in the response header.
   export GLZ_ALWAYS_INLINE void settle_end_reached(is_context auto&& ctx) noexcept
   {
      if (ctx.error == error_code::end_reached) {
         ctx.error = (ctx.depth == 0) ? error_code::none : error_code::unexpected_end;
      }
   }

   export template <auto Opts, is_context Ctx>
   GLZ_ALWAYS_INLINE void finalize_read_context(Ctx&& ctx) noexcept
   {
      // A partial read stops as soon as it has the fields it was asked for, so it unwinds through
      // its enclosing containers on an error code rather than through their closing braces, and
      // none of them decrement depth on the way out. Depth is left standing at whatever nesting the
      // last field sat at.
      //
      // That has to be cleared here rather than left for the next read to trip over. This is the
      // one path that reports success with depth still raised, and depth is what settle_end_reached
      // reads to tell a completed parse from a truncated one -- so a context reused after a partial
      // read would see a later well-formed buffer settle to unexpected_end. Reads that end any
      // other way either return depth to zero themselves or carry an error, and a context holding
      // an error short-circuits the next read before it parses anything.
      if constexpr (check_partial_read(Opts)) {
         if (ctx.error == error_code::partial_read_complete) [[likely]] {
            ctx.error = error_code::none;
            ctx.depth = 0;
            return;
         }
      }
      settle_end_reached(ctx);
   }

   // Settle what a read that ran to the end of the buffer means, then finalize the context.
   //
   // Without a '\0' sentinel, end_reached means either "the value ended with the buffer" (a
   // completed read, which finalize_read_context clears) or "ran out of input looking for a value"
   // (not one). What was consumed tells them apart: a span of only whitespace and comments held no
   // value and is rejected the way an empty buffer is. Replaying skip_ws answers that exactly
   // instead of second-guessing which bytes count as whitespace, and because it == end the replay
   // covers the same bytes under the same options as the parse's own leading skip_ws.
   //
   // Two conditions are load-bearing rather than defensive:
   //   - it == end, because a variant alternative resolved by shape rewinds its iterator, so a
   //     completed read can end with end_reached, depth 0, and a span of only leading whitespace.
   //   - running after the parse, because readers that take the buffer verbatim (glz::text,
   //     glz::raw_json) must see every byte; they complete without error and never reach this.
   export template <auto Opts>
   GLZ_ALWAYS_INLINE void finalize_top_level_read(is_context auto&& ctx, const char* start, const char* it,
                                                  const char* end) noexcept
   {
      if constexpr (Opts.format == JSON && !check_null_terminated(Opts)) {
         // Deliberately not [[unlikely]]: a body that ends with the buffer lands here on every
         // successful read, which is the ordinary case for a registry parsing a wire span.
         // start < it, not !=, because a streaming origin can be empty or overshoot on an error
         // path; neither is an absent value.
         if (ctx.error == error_code::end_reached && ctx.depth == 0 && it == end && start < it) {
            context ws_ctx{}; // only written to when skip_ws parses a comment; the result is unused
            const char* consumed = start;
            skip_ws<Opts>(ws_ctx, consumed, it);
            if (consumed == it) {
               ctx.error = error_code::no_read_input;
            }
         }
      }
      finalize_read_context<Opts>(ctx);
   }

   export template <auto Opts, class T, contiguous Buf>
      requires read_supported<T, Opts.format> && (!is_input_streaming<Buf>)
   [[nodiscard]] error_ctx read(T& value, Buf&& buffer, is_context auto&& ctx)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      using Buffer = std::remove_reference_t<decltype(buffer)>;

      if constexpr (Opts.format != NDJSON) {
         if (buffer.empty()) [[unlikely]] {
            ctx.error = error_code::no_read_input;
            return {0, ctx.error, ctx.custom_error_message};
         }
      }

      constexpr bool use_padded = resizable<Buffer> && non_const_buffer<Buffer> && !check_disable_padding(Opts);

      [[maybe_unused]] size_t original_size{};
      if constexpr (use_padded) {
         // Pad the buffer for SWAR
         original_size = buffer.size();
         buffer.resize(original_size + padding_bytes);
      }

      auto [it, end] = read_iterators<Opts, use_padded>(buffer);
      auto start = it;
      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }

      // Bound the speculative re-parsing a variant resolution may do, in bytes, relative to this
      // input. Seeded per read so a reused context starts fresh. See charge_speculation.
      if constexpr (requires { ctx.speculation_budget; }) {
         const size_t proportional = max_speculative_parse_factor * size_t(end - it);
         ctx.speculation_budget =
            (proportional > min_speculative_parse_bytes ? proportional : min_speculative_parse_bytes) + 2;
      }

      // A YAML context marks its outermost parse with stream_begin and seeds its expansion budgets
      // there, so clearing it here is what makes those budgets per-read like the one above. A
      // reused context would otherwise carry a spent budget into the next document and reject a
      // valid one, and would leave stream_begin pointing into the buffer of the previous read.
      // Nested YAML parses do not route through glz::read, so they still cannot reseed themselves.
      if constexpr (requires { ctx.stream_begin; }) {
         ctx.stream_begin = nullptr;
      }

      if constexpr (use_padded) {
         parse<Opts.format>::template op<is_padded_on<Opts>()>(value, ctx, it, end);
      }
      else {
         parse<Opts.format>::template op<is_padded_off<Opts>()>(value, ctx, it, end);
      }

      if (bool(ctx.error)) [[unlikely]] {
         goto finish;
      }

      // The JSON RFC 8259 defines: JSON-text = ws value ws
      // So, trailing whitespace is permitted and sometimes we want to
      // validate this, even though this memory will not affect Glaze.
      if constexpr (check_validate_trailing_whitespace(Opts)) {
         if (it < end) {
            skip_ws<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               goto finish;
            }
            if (it != end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
            }
         }
      }

   finish:
      finalize_top_level_read<Opts>(ctx, start, it, end);

      if constexpr (use_padded) {
         // Restore the original buffer state
         buffer.resize(original_size);
      }

      return {size_t(it - start), ctx.error, ctx.custom_error_message};
   }

   export template <auto Opts, class T, contiguous Buf>
      requires read_supported<T, Opts.format> && (!is_input_streaming<Buf>)
   [[nodiscard]] error_ctx read(T& value, Buf&& buffer)
   {
      format_context_t<Opts.format> ctx{};
      return read<Opts>(value, buffer, ctx);
   }

   export template <class T>
   concept c_style_char_buffer = std::convertible_to<std::remove_cvref_t<T>, std::string_view> && !has_data<T>;

   export template <class T>
   concept is_buffer = c_style_char_buffer<T> || contiguous<T>;

   // for char array input
   export template <auto Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<T, Opts.format>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer, auto&& ctx)
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         return {0, error_code::no_read_input};
      }
      return read<Opts>(value, str, ctx);
   }

   export template <auto Opts, class T, c_style_char_buffer Buffer>
      requires read_supported<T, Opts.format>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer)
   {
      format_context_t<Opts.format> ctx{};
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }

   // Streaming read for input streaming buffers
   // Uses incremental parsing with internal refill points for arrays/objects
   // Returns error_ctx with total bytes consumed across all refills
   export template <auto Opts, class T, class Buffer, has_streaming_state Ctx>
      requires read_supported<T, Opts.format> && is_input_streaming<std::remove_reference_t<Buffer>>
   [[nodiscard]] error_ctx read_streaming(T& value, Buffer&& buffer, Ctx&& ctx)
   {
      // For streaming, we need null_terminated = false to track depth
      static constexpr auto StreamingOpts = [] {
         auto o = is_padded_off<Opts>();
         if constexpr (requires { o.null_terminated = false; }) {
            o.null_terminated = false;
         }
         return o;
      }();

      // Initial fill if buffer is empty
      if (buffer.empty()) {
         if (!refill_buffer(buffer)) {
            if constexpr (Opts.format != NDJSON) {
               ctx.error = error_code::no_read_input;
               return {0, ctx.error, ctx.custom_error_message};
            }
         }
      }

      // Set up streaming state so parsers can trigger internal refills
      ctx.stream = make_streaming_state(buffer);

      auto [it, end] = read_iterators<Opts>(buffer);

      // Parse with streaming-aware context
      // The parser will internally refill as needed at safe points (between array elements, etc.)
      parse<Opts.format>::template op<StreamingOpts>(value, ctx, it, end);

      // Calculate final consumed bytes
      // Note: During streaming, the buffer may have been refilled multiple times,
      // so we use bytes_consumed() which tracks total consumption
      const char* const window = ctx.stream.data();
      const size_t final_consumed = static_cast<size_t>(it - window);
      consume_buffer(buffer, final_consumed);

      // Settle end_reached the same way a buffered read does. StreamingOpts forces null_terminated
      // off, so a stream that held no value ends in the same end_reached a value ending with the
      // buffer does, and telling them apart means looking at what the parse walked.
      //
      // Only an exhausted stream can be said to have held no value, which is why this waits until
      // after consume_buffer: at_eof also requires the window to be drained. A stream that still
      // has data instead ran out of window -- nothing refills while the top level skips leading
      // whitespace, so whitespace wider than the window stops the parse short of a value that is
      // really there. That is a separate limitation of streaming, and it keeps the behavior it has
      // always had rather than being reported as an absent value.
      //
      // `window` still addresses the bytes the parse walked: consume only advances the buffer's
      // read position, it does not move or release them. It is taken from the streaming state
      // rather than from an iterator captured before the parse because a refill relocates the
      // buffer, and it can overshoot `it` on an error path, which is why the helper compares.
      if (ctx.stream.at_eof()) {
         finalize_top_level_read<StreamingOpts>(ctx, window, it, end);
      }
      else {
         finalize_read_context<StreamingOpts>(ctx);
      }

      // Only the JSON reader has refill points (see format_supports_streaming). Every other reader
      // parses whatever the first window happens to hold and stops at its edge, and what it reports
      // there does not name the window as the cause: NDJSON returns success carrying only the
      // elements that fit, which is silent data loss, and BEVE returns unexpected_end, which reads
      // as malformed input. Say what actually happened instead.
      //
      // The question is whether the reader was shown the whole document, so it asks source_at_eof
      // rather than at_eof: at_eof also demands a drained window, which would call a value that
      // legitimately left trailing bytes behind a truncated read. Once the source is exhausted the
      // window holds everything there will ever be, so whatever the reader concluded, it concluded
      // on the full input and stands.
      //
      // With input still pending, two outcomes are the window's doing rather than the document's:
      //   - an error, because a reader that cannot refill has no way to distinguish input that is
      //     malformed from input it simply has not been shown. Its own code would assert the first.
      //   - success with it == end, which is the reader having run out of window and called that
      //     the end. A parse that stopped short of the edge found a real end and is left alone.
      // `end` is still the window's edge to compare against: a reader with no refill points never
      // moved it.
      if constexpr (!format_supports_streaming<Opts.format>) {
         if (!ctx.stream.source_at_eof() && (bool(ctx.error) || it == end)) {
            ctx.error = error_code::streaming_unsupported;
            ctx.custom_error_message =
               "the document is larger than the buffer window and this format's reader cannot refill";
         }
      }

      if (bool(ctx.error)) {
         return {buffer.bytes_consumed(), ctx.error, ctx.custom_error_message};
      }

      return {buffer.bytes_consumed(), error_code::none, ctx.custom_error_message};
   }

   export template <auto Opts, class T, class Buffer>
      requires read_supported<T, Opts.format> && is_input_streaming<std::remove_reference_t<Buffer>>
   [[nodiscard]] error_ctx read_streaming(T& value, Buffer&& buffer)
   {
      streaming_context ctx{};
      return read_streaming<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }

   // A streaming buffer satisfies `contiguous`, but the span it exposes is one window of a larger
   // stream: it ends wherever the last fill stopped, it carries no terminator, and reading it as a
   // flat buffer parses that window alone. Route it to read_streaming rather than let it bind to
   // the overloads above, which would run with null_terminated on and read past the window's end.
   // Doing this here rather than per format is what keeps read_jsonc, read_beve and every other
   // helper that funnels through `read` from having to remember on its own.
   export template <auto Opts, class T, class Buffer>
      requires read_supported<T, Opts.format> && is_input_streaming<std::remove_reference_t<Buffer>>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer, is_context auto&& ctx)
   {
      if constexpr (has_streaming_state<decltype(ctx)>) {
         return read_streaming<Opts>(value, std::forward<Buffer>(buffer), ctx);
      }
      else {
         // The caller's context has nowhere to hold the streaming state the parsers refill
         // through, so the read runs on one that does and reports back through theirs.
         streaming_context stream_ctx{};
         const error_ctx ec = read_streaming<Opts>(value, std::forward<Buffer>(buffer), stream_ctx);
         ctx.error = stream_ctx.error;
         ctx.custom_error_message = stream_ctx.custom_error_message;
         return ec;
      }
   }

   export template <auto Opts, class T, class Buffer>
      requires read_supported<T, Opts.format> && is_input_streaming<std::remove_reference_t<Buffer>>
   [[nodiscard]] error_ctx read(T& value, Buffer&& buffer)
   {
      return read_streaming<Opts>(value, std::forward<Buffer>(buffer));
   }
}
