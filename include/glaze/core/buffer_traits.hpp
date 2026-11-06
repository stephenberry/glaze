// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/core/context.hpp"
#include "glaze/util/inline.hpp"

namespace glz
{
   // Grows `b` to `n` bytes, leaving the new bytes indeterminate when the buffer type can do that.
   //
   // For callers that write every byte they go on to keep and then truncate to that length, so the
   // zero fill `resize` performs is never read: serialization into an output buffer, and the string
   // reader, which sizes its target to the raw span plus padding and then fills it. Neither is a
   // one time cost -- a finished write shrinks the buffer back to the document's length, so the
   // next write into the same buffer re-expands it past the write padding and fills it again, and
   // on short documents that fill is most of the call.
   //
   // Buffers without resize_and_overwrite (std::vector<char> and friends) keep the filling resize.
   //
   // NOTE: the callback below returns `n` without writing anything, which violates a precondition
   // of resize_and_overwrite. [string.capacity] requires that "after evaluating OP there are no
   // indeterminate values in the range [p, p + r)", and r is n here while only the first min(o, n)
   // bytes were carried over. Violating a precondition is undefined behavior, so this is not a
   // grey area in the standard -- it is outside it, deliberately, because skipping the fill is the
   // entire point and the API offers no conforming way to ask for capacity without content.
   //
   // What makes it work in practice is how the implementations lower it. libc++ is literally
   // `__resize_default_init(n); __erase_to_end(op(data(), n));`, so returning n erases nothing and
   // the new bytes are left as the allocator produced them. libstdc++ does not promise that: it
   // documents the requirement in the same terms the standard does, warning that `op` "must ensure
   // that all characters up to the returned length are valid after it returns". Nothing here is
   // guaranteed by contract on any implementation -- it is a bet that none of them read what they
   // were told to leave alone.
   //
   // The obligation this creates is on every caller, and it is not optional: a byte that is kept
   // must be written before it is read, and the buffer must be truncated to what was written
   // before anyone outside can observe it. That is why the write entry points resize to `ix` on
   // their error paths rather than leaving the buffer at its grown length -- returning it longer
   // would hand the caller indeterminate bytes inside size(). Reading one is undefined in its own
   // right, and a std::string's spare capacity will happily hide the mistake from a sanitizer.
   //
   // The conforming alternative is plain `resize`. Measured on the write and read benchmarks, that
   // costs about 11% across the board and roughly 2.7x on a small write -- one bool per call goes
   // from ~1030 to ~386 MB/s -- because a finished write shrinks the buffer back and the next one
   // re-expands and refills it.
   //
   // That trade has been made knowingly and this is not an oversight to be tidied away: the win is
   // large, the obligation above is one glaze already meets everywhere it matters, and no
   // implementation reads bytes it was asked to leave alone. What would force a revisit is an
   // implementation that starts touching the unwritten range -- a hardened or checked standard
   // library that fills or traps it, or a sanitizer mode that tracks indeterminate heap bytes --
   // rather than the wording itself, which is already known and accepted.
   template <class B>
   GLZ_ALWAYS_INLINE void resize_unfilled(B& b, const size_t n)
   {
      if constexpr (requires { b.resize_and_overwrite(n, [](auto*, size_t written) { return written; }); }) {
         b.resize_and_overwrite(n, [](auto*, size_t written) noexcept { return written; });
      }
      else {
         b.resize(n);
      }
   }

   // Primary template for buffer traits
   // Handles resizable buffers (std::string, std::vector<char>, etc.)
   template <class Buffer>
   struct buffer_traits
   {
      static constexpr bool is_resizable = resizable<std::remove_cvref_t<Buffer>>;
      static constexpr bool has_bounded_capacity = !is_resizable && has_size<std::remove_cvref_t<Buffer>>;
      static constexpr bool is_output_streaming = false; // True for output buffers that support incremental flushing
      static constexpr bool is_input_streaming = false; // True for input buffers that support incremental refilling

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const Buffer& b) noexcept
      {
         if constexpr (is_resizable) {
            return (std::numeric_limits<size_t>::max)(); // Effectively unlimited
         }
         else if constexpr (has_size<std::remove_cvref_t<Buffer>>) {
            return b.size();
         }
         else {
            return (std::numeric_limits<size_t>::max)();
         }
      }

      // Attempt to ensure buffer can hold `needed` bytes
      // Returns true if successful, false if buffer cannot accommodate
      GLZ_ALWAYS_INLINE static bool ensure_capacity(Buffer& b, size_t needed) noexcept(not is_resizable)
      {
         if constexpr (is_resizable) {
            if (needed > b.size()) {
               grow(b, needed);
            }
            return true;
         }
         else {
            return capacity(b) >= needed;
         }
      }

      // Grow a resizable buffer so that it can hold `required` bytes.
      // 2x growth amortizes repeated reallocations to O(n) total cost.
      // Constrained on `is_resizable` so that it is not merely ill-formed for a fixed buffer but
      // absent from the overload set: `grow_buffer` detects this member to decide whether a
      // specialization opts into a growth policy, and an unconstrained declaration would answer
      // yes for every buffer, including those that cannot grow at all.
      GLZ_ALWAYS_INLINE static void grow(Buffer& b, size_t required)
         requires(is_resizable)
      {
         resize_unfilled(b, 2 * required);
      }

      // Finalize buffer to actual written size
      GLZ_ALWAYS_INLINE static void finalize(Buffer& b, size_t written) noexcept(not is_resizable)
      {
         if constexpr (is_resizable) {
            b.resize(written);
         }
         // For fixed buffers: no-op, count is returned in result
      }

      // Flush written data to underlying storage (for streaming buffers)
      // Default: no-op for regular buffers
      GLZ_ALWAYS_INLINE static void flush([[maybe_unused]] Buffer& b, [[maybe_unused]] size_t written) noexcept {}
   };

   // Grow a buffer so that it can hold `required` bytes, applying that buffer's growth policy.
   // Write code tracks a logical index, so `required` is a logical end position rather than an
   // amount of storage. The two differ for a streaming buffer, which keeps only its unflushed
   // window in memory: doubling a logical index there would grow storage with the document instead
   // of with the window, so such buffers translate the request themselves.
   template <class B>
   GLZ_ALWAYS_INLINE void grow_buffer(B& b, const size_t required)
   {
      using traits = buffer_traits<std::remove_cvref_t<B>>;
      if constexpr (requires { traits::grow(b, required); }) {
         traits::grow(b, required);
      }
      else {
         // A user specialization written before `grow` existed: keep the growth it used to get.
         resize_unfilled(b, 2 * required);
      }
   }

   // Concept to check if a buffer type supports output streaming (flushing)
   template <class B>
   concept is_output_streaming = buffer_traits<std::remove_cvref_t<B>>::is_output_streaming;

   // Concept to check if a buffer type is both bounded and supports streaming.
   // These buffers cannot grow but can flush to handle data larger than their capacity.
   template <class B>
   concept is_bounded_output_streaming =
      buffer_traits<std::remove_cvref_t<B>>::is_output_streaming &&
      buffer_traits<std::remove_cvref_t<B>>::has_bounded_capacity;

   // Flush helper for streaming output buffers
   template <class B>
   GLZ_ALWAYS_INLINE void flush_buffer(B&& b, size_t written) noexcept
   {
      buffer_traits<std::remove_cvref_t<B>>::flush(b, written);
   }

   // Concept to check if a buffer type supports input streaming (refilling)
   template <class B>
   concept is_input_streaming = buffer_traits<std::remove_cvref_t<B>>::is_input_streaming;

   // Concept to check if a buffer type has bounded capacity (like std::array, std::span)
   // Note: Bounded buffers must be at least 512 bytes for reliable serialization.
   // Smaller buffers will return error_code::buffer_overflow.
   template <class B>
   concept has_bounded_capacity = buffer_traits<std::remove_cvref_t<B>>::has_bounded_capacity;

   // Refill helper for streaming input buffers
   // Returns true if buffer has data available after refill, false if EOF
   template <class B>
   GLZ_ALWAYS_INLINE bool refill_buffer(B&& b) noexcept
   {
      if constexpr (is_input_streaming<B>) {
         return buffer_traits<std::remove_cvref_t<B>>::refill(b);
      }
      else {
         return false; // Non-streaming buffers cannot refill
      }
   }

   // Consume helper for streaming input buffers
   // Marks bytes as consumed after successful parsing
   template <class B>
   GLZ_ALWAYS_INLINE void consume_buffer(B&& b, size_t bytes) noexcept
   {
      if constexpr (is_input_streaming<B>) {
         buffer_traits<std::remove_cvref_t<B>>::consume(b, bytes);
      }
   }

   // Specialization for raw char pointers
   // Raw pointers have unknown capacity and must trust the caller
   template <>
   struct buffer_traits<char*>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = false;
      static constexpr bool is_output_streaming = false;
      static constexpr bool is_input_streaming = false;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(char*) noexcept
      {
         return (std::numeric_limits<size_t>::max)();
      }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(char*, size_t) noexcept
      {
         return true; // Trust caller
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(char*, size_t) noexcept {}
   };

   // Specialization for std::span
   template <class T, size_t Extent>
   struct buffer_traits<std::span<T, Extent>>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = true;
      static constexpr bool is_output_streaming = false;
      static constexpr bool is_input_streaming = false;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const std::span<T, Extent>& b) noexcept { return b.size(); }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(const std::span<T, Extent>& b, size_t needed) noexcept
      {
         return needed <= b.size();
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(std::span<T, Extent>&, size_t) noexcept {}
   };

   // Specialization for std::array
   template <class T, size_t N>
   struct buffer_traits<std::array<T, N>>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = true;
      static constexpr bool is_output_streaming = false;
      static constexpr bool is_input_streaming = false;
      static constexpr size_t static_capacity = N;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const std::array<T, N>&) noexcept { return N; }

      GLZ_ALWAYS_INLINE static constexpr bool ensure_capacity(std::array<T, N>&, size_t needed) noexcept
      {
         return needed <= N;
      }

      GLZ_ALWAYS_INLINE static constexpr void finalize(std::array<T, N>&, size_t) noexcept {}
   };

   // Unified buffer space checking for write operations
   // Handles resizable buffers (resize), bounded buffers (error on overflow), and raw pointers (trust caller)
   template <class B>
   GLZ_ALWAYS_INLINE bool ensure_space(is_context auto& ctx, B& b,
                                       size_t required) noexcept(not vector_like<std::remove_cvref_t<B>>)
   {
      using Buffer = std::remove_cvref_t<B>;

      if constexpr (vector_like<Buffer>) {
         if (required > b.size()) [[unlikely]] {
            grow_buffer(b, required);
         }
         return true;
      }
      else if constexpr (has_bounded_capacity<Buffer>) {
         if (required > buffer_traits<Buffer>::capacity(b)) [[unlikely]] {
            ctx.error = error_code::buffer_overflow;
            return false;
         }
         return true;
      }
      else {
         return true; // Raw pointer or other - trust caller
      }
   }

}
