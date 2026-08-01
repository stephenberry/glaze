// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <type_traits>

#include "glaze/core/context.hpp"

namespace glz
{
   // Type-erased interface for streaming buffer operations.
   // Allows parsers to trigger refill without knowing the concrete buffer type.
   //
   // Design Note:
   //   This uses function pointers for type erasure rather than templates. This prevents
   //   the core parsing logic from being templated on buffer type, avoiding code bloat
   //   and compile time increases. The trade-off is that function pointer calls cannot
   //   be inlined. In practice, this overhead is negligible because:
   //   - These functions are called infrequently relative to parsing work
   //   - I/O latency (disk/network) dominates CPU overhead
   //   - The function bodies are trivial (return pointer/size)
   //
   //   Streaming parsing is inherently slower than buffer-based parsing due to this
   //   overhead and the need to handle data spanning buffer boundaries. Users choosing
   //   streaming trade some performance for bounded memory usage.
   struct streaming_state
   {
      void* buffer_ptr = nullptr;

      // Function pointers for type-erased operations
      const char* (*get_data)(void*) = nullptr;
      size_t (*get_size)(void*) = nullptr;
      void (*consume)(void*, size_t) = nullptr;
      bool (*refill)(void*) = nullptr;
      bool (*eof)(void*) = nullptr;
      bool (*source_eof)(void*) = nullptr;

      // Check if streaming is enabled
      bool enabled() const noexcept { return buffer_ptr != nullptr; }

      // Get current data pointer
      const char* data() const noexcept { return get_data(buffer_ptr); }

      // Get current available size
      size_t size() const noexcept { return get_size(buffer_ptr); }

      // Consume n bytes from buffer
      void consume_bytes(size_t n) const noexcept { consume(buffer_ptr, n); }

      // Refill buffer, returns true if data available
      bool refill_buffer() const noexcept { return refill(buffer_ptr); }

      // Check if at end of stream
      bool at_eof() const noexcept { return eof(buffer_ptr); }

      // Whether the source has been read to its end, whether or not the window is drained.
      // at_eof() requires both; this asks only the first, which is how a reader tells input it has
      // seen in full from input the window cut short.
      bool source_at_eof() const noexcept { return source_eof(buffer_ptr); }

      // Consume up to current position and refill
      // Returns new iterators via out parameters
      // it_offset is how far into current buffer we've parsed
      bool consume_and_refill(size_t consumed_bytes, const char*& new_it, const char*& new_end) const noexcept
      {
         consume_bytes(consumed_bytes);
         bool has_data = refill_buffer();
         new_it = data();
         new_end = data() + size();
         return has_data || size() > 0;
      }
   };

   // Helper to create streaming_state from a concrete buffer type
   template <class Buffer>
   streaming_state make_streaming_state(Buffer& buffer) noexcept
   {
      streaming_state state;
      state.buffer_ptr = &buffer;
      state.get_data = [](void* p) -> const char* { return static_cast<Buffer*>(p)->data(); };
      state.get_size = [](void* p) -> size_t { return static_cast<Buffer*>(p)->size(); };
      state.consume = [](void* p, size_t n) { static_cast<Buffer*>(p)->consume(n); };
      state.refill = [](void* p) -> bool { return static_cast<Buffer*>(p)->refill(); };
      state.eof = [](void* p) -> bool { return static_cast<Buffer*>(p)->eof(); };
      if constexpr (requires(Buffer& b) {
                       { b.source_exhausted() } -> std::convertible_to<bool>;
                    }) {
         state.source_eof = [](void* p) -> bool { return static_cast<Buffer*>(p)->source_exhausted(); };
      }
      else {
         // A buffer that does not distinguish the two can only offer the stricter answer. That is
         // safe in the direction that matters: it never claims the source is exhausted when it is
         // not, so nothing concludes it has seen input it has not.
         state.source_eof = [](void* p) -> bool { return static_cast<Buffer*>(p)->eof(); };
      }
      return state;
   }

   // Streaming context extends base context with streaming state
   // This avoids adding streaming overhead to non-streaming use cases
   struct streaming_context : context
   {
      streaming_state stream{};
   };

   // Concept to detect streaming-capable context at compile time
   // Automatically removes cv-ref qualifiers for convenience
   template <class T>
   concept has_streaming_state = is_context<std::remove_cvref_t<T>> && requires(std::remove_cvref_t<T>& ctx) {
      { ctx.stream } -> std::same_as<streaming_state&>;
   };

   // Re-derive the window's edge after handing the input to another reader.
   //
   // parse<Format>::op takes `end` by value, so a reader that refills partway through leaves the
   // caller holding an edge from the window the parse started in. `it` moved with the refill, and
   // the two stop describing the same span -- offsets taken from them then run past the buffer.
   template <class Ctx, class End>
   GLZ_ALWAYS_INLINE void resync_window_end(Ctx& ctx, End& end) noexcept
   {
      if constexpr (has_streaming_state<Ctx>) {
         if (ctx.stream.enabled()) {
            end = ctx.stream.data() + ctx.stream.size();
         }
      }
   }

   // Release everything up to `it` and pull more input into the space behind it. Both iterators
   // move, since a refill relocates the window.
   //
   // This is mechanism only: when to refill is the calling reader's decision, because only it knows
   // where one value ends and the next begins. Calling this anywhere else -- in the middle of a
   // value -- invalidates every pointer the reader is holding into the window.
   //
   // `it` is clamped to the window rather than trusted. A reader can leave it past `end` on an
   // error path, and a variant alternative resolved by shape rewinds it, so an unclamped
   // subtraction underflows the byte count handed to consume(); the buffer's size then wraps and
   // the next refill memmoves a garbage length.
   //
   // A context with no streaming state -- every buffered read -- compiles this away to nothing.
   template <class Ctx, class It, class End>
   GLZ_ALWAYS_INLINE void refill_window(Ctx& ctx, It& it, End& end) noexcept
   {
      if constexpr (has_streaming_state<Ctx>) {
         if (ctx.stream.enabled()) {
            const char* const window = ctx.stream.data();
            const size_t size = ctx.stream.size();
            const size_t offset = (it > window) ? size_t(it - window) : 0;
            const size_t consumed = (offset < size) ? offset : size;

            const char* new_it;
            const char* new_end;
            ctx.stream.consume_and_refill(consumed, new_it, new_end);
            it = new_it;
            end = new_end;
         }
      }
   }
}

// A streaming read advances by refilling its window in place, so a view pointed into that window
// addresses bytes that a later refill overwrites. Nothing at runtime can catch it: the pointer
// stays inside the buffer's own allocation, so it stays valid, compares equal to whatever now
// occupies those bytes, and the parse reports success. Nor does a larger window fix it -- it only
// changes which values happen to survive.
//
// The check belongs on the readers that alias the buffer, not on the destination type. A reader
// knows it is about to hand out a pointer into the window and knows from its context whether that
// window moves, so it catches the case wherever the view sits -- behind a tuple, a map key, a
// custom setter, or ten structs down -- and never fires on a type that merely looks like it holds
// one. Every reader that aliases the input places this next to the assignment that does it.
#define GLZ_ASSERT_OWNS_ITS_BYTES(Ctx)                                                                          \
   static_assert(!::glz::has_streaming_state<Ctx>,                                                              \
                 "This read fills a non-owning view (std::string_view, glz::raw_json_view, glz::text_view, or " \
                 "a std::span) that would point into the streaming window. A streaming read refills that "      \
                 "window as it goes, so the view would address overwritten bytes by the time the read "         \
                 "returns, and the read would report success while handing back silently wrong data. Read "     \
                 "into the owning equivalent instead (std::string, glz::raw_json, glz::text, or an owning "     \
                 "container), or read from a buffer that holds the whole document.")
