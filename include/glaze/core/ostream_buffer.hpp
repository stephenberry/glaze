// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <ostream>
#include <vector>

#include "glaze/core/buffer_traits.hpp"

namespace glz
{
   // Concept for byte-oriented output streams
   // Requires write(const char*, std::streamsize) method
   // Excludes wide streams (wostream) which use wchar_t
   template <class S>
   concept byte_output_stream = requires(S& s, const char* data, std::streamsize n) { s.write(data, n); };

   // A streaming buffer adapter that writes to an output stream.
   // Supports incremental flushing during serialization for bounded memory usage.
   //
   // Usage:
   //   std::ofstream file("output.json");
   //   glz::basic_ostream_buffer<std::ofstream> buffer(file);  // Concrete type for devirtualization
   //   auto ec = glz::write_json(obj, buffer);
   //   if (ec || !file.good()) { /* handle error */ }
   //
   // Template parameters:
   //   Stream - Byte-oriented output stream (must satisfy byte_output_stream concept)
   //   DefaultCapacity - Initial buffer size in bytes (default 64KB).
   //                     Larger values reduce flush frequency but use more memory.

   // Minimum buffer capacity for streaming.
   // Must be large enough to hold any single JSON value (floats can be ~24 bytes,
   // plus overhead for keys, syntax, etc.). Set to 2 * write_padding_bytes since
   // the write code resizes buffers to this value on first write anyway.
   inline constexpr size_t min_ostream_buffer_size = 512;

   template <byte_output_stream Stream, size_t DefaultCapacity = 65536>
      requires(DefaultCapacity >= min_ostream_buffer_size)
   class basic_ostream_buffer
   {
      static_assert(DefaultCapacity >= min_ostream_buffer_size,
                    "Buffer capacity must be at least 256 bytes to handle all JSON value types");

      Stream* stream_;
      std::vector<char> buffer_;
      size_t flush_offset_ = 0; // Logical position that maps to buffer_[0]
      size_t logical_size_ = 0; // Reported size (set by resize)

     public:
      using value_type = char;
      using reference = char&;
      using const_reference = const char&;
      using size_type = size_t;
      using iterator = char*;
      using const_iterator = const char*;
      using stream_type = Stream;

      // Construct with output stream and optional initial capacity
      explicit basic_ostream_buffer(Stream& stream, size_t initial_capacity = DefaultCapacity) : stream_(&stream)
      {
         buffer_.resize(initial_capacity);
         logical_size_ = initial_capacity;
      }

      // Move-only to prevent accidental copies
      basic_ostream_buffer(const basic_ostream_buffer&) = delete;
      basic_ostream_buffer& operator=(const basic_ostream_buffer&) = delete;
      basic_ostream_buffer(basic_ostream_buffer&&) noexcept = default;
      basic_ostream_buffer& operator=(basic_ostream_buffer&&) noexcept = default;

      ~basic_ostream_buffer() = default;

      // Element access - maps logical position to physical buffer
      reference operator[](size_t ix) noexcept
      {
         assert(ix >= flush_offset_ && "Index before flush offset");
         return buffer_[ix - flush_offset_];
      }
      const_reference operator[](size_t ix) const noexcept
      {
         assert(ix >= flush_offset_ && "Index before flush offset");
         return buffer_[ix - flush_offset_];
      }

      // Current logical size
      size_t size() const noexcept { return logical_size_; }

      // Resize - called by serialization when more space is needed
      void resize(size_t new_size)
      {
         // Ensure buffer can hold data from flush_offset_ to new_size
         if (new_size > buffer_.size() + flush_offset_) {
            buffer_.resize(new_size - flush_offset_);
         }
         logical_size_ = new_size;
      }

      // Final flush - called by buffer_traits::finalize()
      void finalize(size_t total_written)
      {
         if (total_written > flush_offset_ && stream_) {
            const size_t to_flush = total_written - flush_offset_;
            stream_->write(buffer_.data(), static_cast<std::streamsize>(to_flush));
            flush_offset_ = total_written;
         }
      }

      // Incremental flush - called during serialization at safe points (between array
      // elements, object fields) to keep memory usage bounded.
      //
      // Flushes when buffer usage exceeds 50% of DefaultCapacity. This threshold balances
      // memory usage against syscall overhead. To control flush frequency, adjust
      // DefaultCapacity: smaller capacity = more frequent flushes, larger = fewer flushes.
      void flush(size_t written_so_far)
      {
         if (written_so_far > flush_offset_ && stream_) {
            const size_t unflushed = written_so_far - flush_offset_;
            if (unflushed >= DefaultCapacity / 2) {
               stream_->write(buffer_.data(), static_cast<std::streamsize>(unflushed));
               flush_offset_ = written_so_far;
            }
         }
      }

      // Reset for reuse
      void reset() noexcept
      {
         flush_offset_ = 0;
         logical_size_ = buffer_.size();
      }

      // Check if underlying stream is in good state
      bool good() const noexcept { return stream_ && stream_->good(); }

      // Check if underlying stream has failed
      bool fail() const noexcept { return !stream_ || stream_->fail(); }

      // Access underlying stream
      Stream* stream() const noexcept { return stream_; }

      // Bytes flushed so far
      size_t bytes_flushed() const noexcept { return flush_offset_; }

      // Current buffer capacity
      size_t buffer_capacity() const noexcept { return buffer_.size(); }

      // Iterator support (satisfies range concept)
      iterator begin() noexcept { return buffer_.data(); }
      iterator end() noexcept { return buffer_.data() + buffer_.size(); }
      const_iterator begin() const noexcept { return buffer_.data(); }
      const_iterator end() const noexcept { return buffer_.data() + buffer_.size(); }

      // Data access (satisfies vector_like concept)
      char* data() noexcept { return buffer_.data(); }
      const char* data() const noexcept { return buffer_.data(); }
   };

   // buffer_traits specialization for basic_ostream_buffer
   template <class Stream, size_t N>
   struct buffer_traits<basic_ostream_buffer<Stream, N>>
   {
      static constexpr bool is_resizable = true;
      static constexpr bool has_bounded_capacity = false;
      static constexpr bool is_output_streaming = true;

      GLZ_ALWAYS_INLINE static constexpr size_t capacity(const basic_ostream_buffer<Stream, N>&) noexcept
      {
         return std::numeric_limits<size_t>::max();
      }

      GLZ_ALWAYS_INLINE static bool ensure_capacity(basic_ostream_buffer<Stream, N>& b, size_t needed)
      {
         b.resize(needed);
         return true;
      }

      GLZ_ALWAYS_INLINE static void finalize(basic_ostream_buffer<Stream, N>& b, size_t written)
      {
         b.finalize(written);
      }

      GLZ_ALWAYS_INLINE static void flush(basic_ostream_buffer<Stream, N>& b, size_t written) { b.flush(written); }
   };

   // Convenience alias for std::ostream (polymorphic, works with any standard stream)
   template <size_t DefaultCapacity = 65536>
   using ostream_buffer = basic_ostream_buffer<std::ostream, DefaultCapacity>;

   // A bounded streaming buffer that has fixed capacity but can flush incrementally.
   // Unlike basic_ostream_buffer, this buffer will NOT grow beyond its initial capacity.
   // Instead, it flushes to the underlying stream when capacity is approached.
   //
   // This is useful for:
   // - Memory-constrained environments where buffer growth is not acceptable
   // - Serializing data larger than available memory by streaming to disk/network
   //
   // Usage:
   //   std::ofstream file("output.json");
   //   glz::bounded_ostream_buffer<std::ofstream, 4096> buffer(file);
   //   auto ec = glz::write_json(obj, buffer);
   //
   // Template parameters:
   //   Stream - Byte-oriented output stream (must satisfy byte_output_stream concept)
   //   Capacity - Fixed buffer size in bytes. Must be at least min_ostream_buffer_size.

   template <byte_output_stream Stream, size_t Capacity = 65536>
      requires(Capacity >= min_ostream_buffer_size)
   class bounded_ostream_buffer
   {
      static_assert(Capacity >= min_ostream_buffer_size,
                    "Buffer capacity must be at least 512 bytes to handle all JSON value types");

      Stream* stream_;
      std::array<char, Capacity> buffer_;
      size_t flush_offset_ = 0; // Logical position that maps to buffer_[0]
      size_t logical_size_ = Capacity; // Reported size

     public:
      using value_type = char;
      using reference = char&;
      using const_reference = const char&;
      using size_type = size_t;
      using iterator = char*;
      using const_iterator = const char*;
      using stream_type = Stream;

      static constexpr size_t buffer_capacity = Capacity;

      explicit bounded_ostream_buffer(Stream& stream) : stream_(&stream) {}

      // Move-only to prevent accidental copies
      bounded_ostream_buffer(const bounded_ostream_buffer&) = delete;
      bounded_ostream_buffer& operator=(const bounded_ostream_buffer&) = delete;
      bounded_ostream_buffer(bounded_ostream_buffer&&) noexcept = default;
      bounded_ostream_buffer& operator=(bounded_ostream_buffer&&) noexcept = default;

      ~bounded_ostream_buffer() = default;

      // Element access - maps logical position to physical buffer
      reference operator[](size_t ix) noexcept
      {
         assert(ix >= flush_offset_ && "Index before flush offset");
         assert(ix - flush_offset_ < Capacity && "Index exceeds buffer capacity");
         return buffer_[ix - flush_offset_];
      }
      const_reference operator[](size_t ix) const noexcept
      {
         assert(ix >= flush_offset_ && "Index before flush offset");
         assert(ix - flush_offset_ < Capacity && "Index exceeds buffer capacity");
         return buffer_[ix - flush_offset_];
      }

      // Current logical size (capacity available from position 0)
      size_t size() const noexcept { return logical_size_; }

      // Resize - for bounded buffers, we update logical size but cannot grow physical buffer
      void resize(size_t new_size) noexcept { logical_size_ = new_size; }

      // Final flush - called by buffer_traits::finalize()
      // Returns true on success, false if stream write failed
      bool finalize(size_t total_written)
      {
         if (total_written > flush_offset_ && stream_) {
            const size_t to_flush = total_written - flush_offset_;
            stream_->write(buffer_.data(), static_cast<std::streamsize>(to_flush));
            if (stream_->fail()) [[unlikely]] {
               return false;
            }
            flush_offset_ = total_written;
         }
         return true;
      }

      // Flush all pending data and reset buffer position
      // After flush, capacity increases by the amount flushed
      // Returns true on success, false if stream write failed
      bool flush(size_t written_so_far)
      {
         if (written_so_far > flush_offset_ && stream_) {
            const size_t to_flush = written_so_far - flush_offset_;
            stream_->write(buffer_.data(), static_cast<std::streamsize>(to_flush));
            if (stream_->fail()) [[unlikely]] {
               return false;
            }
            flush_offset_ = written_so_far;
            // Update logical size to reflect new capacity from current position
            logical_size_ = flush_offset_ + Capacity;
         }
         return true;
      }

      // Reset for reuse
      void reset() noexcept
      {
         flush_offset_ = 0;
         logical_size_ = Capacity;
      }

      // Effective capacity from position 0 (includes already-flushed space)
      size_t effective_capacity() const noexcept { return flush_offset_ + Capacity; }

      // Physical buffer capacity
      static constexpr size_t physical_capacity() noexcept { return Capacity; }

      // Check if underlying stream is in good state
      bool good() const noexcept { return stream_ && stream_->good(); }

      // Check if underlying stream has failed
      bool fail() const noexcept { return !stream_ || stream_->fail(); }

      // Access underlying stream
      Stream* stream() const noexcept { return stream_; }

      // Bytes flushed so far
      size_t bytes_flushed() const noexcept { return flush_offset_; }

      // Iterator support (for physical buffer)
      iterator begin() noexcept { return buffer_.data(); }
      iterator end() noexcept { return buffer_.data() + Capacity; }
      const_iterator begin() const noexcept { return buffer_.data(); }
      const_iterator end() const noexcept { return buffer_.data() + Capacity; }

      // Data access
      char* data() noexcept { return buffer_.data(); }
      const char* data() const noexcept { return buffer_.data(); }
   };

   // buffer_traits specialization for bounded_ostream_buffer
   template <class Stream, size_t N>
   struct buffer_traits<bounded_ostream_buffer<Stream, N>>
   {
      static constexpr bool is_resizable = false;
      static constexpr bool has_bounded_capacity = true;
      static constexpr bool is_output_streaming = true;

      // Capacity grows as data is flushed
      GLZ_ALWAYS_INLINE static size_t capacity(const bounded_ostream_buffer<Stream, N>& b) noexcept
      {
         return b.effective_capacity();
      }

      GLZ_ALWAYS_INLINE static bool ensure_capacity(bounded_ostream_buffer<Stream, N>& b, size_t needed)
      {
         if (needed > capacity(b)) {
            // Cannot grow beyond capacity. Callers must flush at safe points
            // (between array elements, object fields) to make room.
            return false;
         }
         return true;
      }

      // Basic finalize without error reporting (for backward compatibility)
      GLZ_ALWAYS_INLINE static void finalize(bounded_ostream_buffer<Stream, N>& b, size_t written)
      {
         b.finalize(written);
      }

      // Context-aware finalize that reports stream errors through ctx.error
      GLZ_ALWAYS_INLINE static void finalize(bounded_ostream_buffer<Stream, N>& b, size_t written, is_context auto& ctx)
      {
         if (!b.finalize(written)) [[unlikely]] {
            ctx.error = error_code::send_error;
         }
      }

      // Basic flush without error reporting (for backward compatibility)
      GLZ_ALWAYS_INLINE static void flush(bounded_ostream_buffer<Stream, N>& b, size_t written) { b.flush(written); }

      // Context-aware flush that reports stream errors through ctx.error
      GLZ_ALWAYS_INLINE static void flush(bounded_ostream_buffer<Stream, N>& b, size_t written, is_context auto& ctx)
      {
         if (!b.flush(written)) [[unlikely]] {
            ctx.error = error_code::send_error;
         }
      }
   };

} // namespace glz
