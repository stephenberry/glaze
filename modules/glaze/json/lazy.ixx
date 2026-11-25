// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/json/lazy.hpp"
// glz:header std=<array>
// glz:header std=<charconv>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<string>
// glz:header std=<string_view>
// glz:header std=<type_traits>
// glz:header std=<vector>
export module glaze.json.lazy;

import std;

import glaze.json.read;
import glaze.json.skip;
import glaze.json.write;

import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.core.read;

import glaze.util.expected;
import glaze.util.dump;
import glaze.util.parse;
import glaze.util.string_literal;
import glaze.util.atoi;
import glaze.util.glaze_fast_float;
import glaze.util.type_traits;

import glaze.concepts.container_concepts;

#include "glaze/util/inline.hpp"

#ifndef GLZ_NO_UNIQUE_ADDRESS
#if (__has_cpp_attribute(no_unique_address))
#define GLZ_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif (__has_cpp_attribute(msvc::no_unique_address)) || ((defined _MSC_VER) && (!defined __clang__))
#define GLZ_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define GLZ_NO_UNIQUE_ADDRESS
#endif
#endif

using std::uint8_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;
using std::ptrdiff_t;
using std::size_t;

export namespace glz
{
   // Forward declarations (needed for circular references)
   template <auto Opts>
   struct lazy_document;
   template <auto Opts>
   class lazy_iterator;
   template <auto Opts>
   struct indexed_lazy_view;
   template <auto Opts>
   class indexed_lazy_iterator;

   // ============================================================================
   // Truly Lazy JSON Parser - No upfront processing
   // ============================================================================

   namespace detail
   {
      // lazy_json_view::get<std::string> unescapes its raw span by re-reading it through the JSON
      // reader. That read is independent of the document's options except for UTF-8 validation,
      // which the caller can turn off and must not be silently reinstated here.
      struct lazy_string_opts : opts
      {
         bool validate_utf8 = true;
      };

      template <auto Opts>
      consteval lazy_string_opts lazy_string_opts_for()
      {
         lazy_string_opts o{};
         o.validate_utf8 = check_validate_utf8(Opts);
         return o;
      }

      // Skip string - returns position after closing quote
      template <auto Opts>
      GLZ_ALWAYS_INLINE const char* skip_string_fast(const char* p, const char* end) noexcept
      {
         const char* const start = p;
         ++p; // skip opening quote

         // Find closing quote using memchr (SIMD-optimized in libc)
         while (p < end) {
            const char* q = static_cast<const char*>(std::memchr(p, '"', static_cast<size_t>(end - p)));
            if (!q) [[unlikely]] {
               return end; // unclosed string
            }

            // Count preceding backslashes to check if escaped
            size_t backslashes = 0;
            const char* check = q - 1;
            while (check > start && *check == '\\') {
               ++backslashes;
               --check;
            }

            p = q + 1;
            if ((backslashes & 1) == 0) {
               return p; // even backslashes = real quote
            }
            // odd backslashes = escaped quote, continue searching
         }
         return p;
      }

      // The escalated scan below jumps to the next byte the depth loops actually act on, so its
      // character set must be exactly the bytes lazy_char_class does not classify as `other`.
      // The two live in different headers, and that equality IS the correctness argument for
      // escalating, so pin it here rather than leaving it to a comment.
      inline constexpr std::array<char, 5> lazy_structural_chars{'"', '[', ']', '{', '}'};

      template <size_t... I>
      GLZ_ALWAYS_INLINE const char* find_next_structural(const char* p, const char* end,
                                                         std::index_sequence<I...>) noexcept
      {
         return glz::find_first_of<lazy_structural_chars[I]...>(p, end);
      }

      // Scans to the next byte the depth loops act on. Expanded from lazy_structural_chars so the
      // scan and the assertion below cannot describe different sets.
      GLZ_ALWAYS_INLINE const char* find_next_structural(const char* p, const char* end) noexcept
      {
         return find_next_structural(p, end, std::make_index_sequence<lazy_structural_chars.size()>{});
      }

      static_assert(
         [] {
            for (size_t c = 0; c < 256; ++c) {
               const bool structural = lazy_char_class[c] != lazy_char_type::other;
               bool listed = false;
               for (const char l : lazy_structural_chars) {
                  if (uint8_t(l) == c) listed = true;
               }
               // `number` bytes are non-`other` but are consumed by the run walk, not the scan.
               if (lazy_char_class[c] == lazy_char_type::number) continue;
               if (structural != listed) return false;
            }
            return true;
         }(),
         "the wide-number escalation set must match lazy_char_class's structural bytes");

      // Skip a number inside a container, where the only bytes that matter are the structural
      // ones. Returns the position just past the numeric run.
      //
      // The default walk is a byte-at-a-time table lookup, which is what most JSON wants: typical
      // numbers are a few bytes long and a wide scan cannot amortize its setup over them. The
      // lazy_wide_number_skip option targets documents built from long numeric runs, where it
      // escalates to the SWAR scan once a run is still going after 8 bytes. Escalating is always
      // semantically safe - inside a container every non-structural byte is ignorable either way -
      // so the option changes only how fast the same position is reached.
      template <auto Opts>
      GLZ_ALWAYS_INLINE const char* skip_number_lazy(const char* p, const char* end) noexcept
      {
         ++p;
         if constexpr (Opts.null_terminated) {
            // A null-terminated buffer stops on its own sentinel, and end may be absent entirely
            // for a detached view, so the wide scan does not apply here.
            while (numeric_table[uint8_t(*p)]) ++p;
            return p;
         }
         else if constexpr (check_lazy_wide_number_skip(Opts)) {
            const char* const short_run_end = (end - p) > 8 ? p + 8 : end;
            while (p < short_run_end && numeric_table[uint8_t(*p)]) ++p;
            if (p == short_run_end && p < end) {
               return find_next_structural(p, end);
            }
            return p;
         }
         else {
            while (p < end && numeric_table[uint8_t(*p)]) ++p;
            return p;
         }
      }

      // Skip from current position to depth zero (end of enclosing container)
      // Used after partial scanning to find container end efficiently
      template <auto Opts>
      GLZ_ALWAYS_INLINE const char* skip_to_depth_zero(const char* p, const char* end, int depth) noexcept
      {
         using enum lazy_char_type;
         while (depth > 0) {
            if constexpr (Opts.null_terminated) {
               if (*p == '\0') break;
            }
            else {
               if (p >= end) break;
            }
            switch (lazy_char_class[uint8_t(*p)]) {
            case quote:
               p = skip_string_fast<Opts>(p, end);
               break;
            case open:
               ++depth;
               ++p;
               break;
            case close:
               --depth;
               ++p;
               break;
            case number:
               p = skip_number_lazy<Opts>(p, end);
               break;
            default:
               ++p;
               break;
            }
         }
         return p;
      }

      // skip_value stops a scalar on the byte that follows it, so a scalar occupying the tail of
      // a document reports unexpected_end: it runs out of buffer (bounded) or reaches the '\0'
      // sentinel (null-terminated) with nothing left to look at. Nothing is truncated at the
      // document edge, so that report is spurious and the value is complete -- the same
      // situation finalize_read_context settles for readers that land there.
      //
      // Containers and strings are deliberately excluded. Each carries its own terminator, so
      // reaching the edge without one is genuine truncation and must stay an error.
      [[nodiscard]] inline bool lazy_scalar_reaches_end(const char lead, const error_code ec, const char* it,
                                                        const char* end) noexcept
      {
         return ec == error_code::unexpected_end && it == end && lead != '{' && lead != '[' && lead != '"';
      }

      // Skip any JSON value - optimized container skip
      // Returns pointer after the value
      template <auto Opts>
      GLZ_ALWAYS_INLINE const char* skip_value_lazy(const char* p, const char* end) noexcept
      {
         using enum lazy_char_type;
         if constexpr (!Opts.null_terminated) {
            // Non-null-terminated buffers have no '\0' sentinel, so a caller can reach here
            // with p == end (e.g. a key whose value is missing). Guard the tag dereference.
            if (p >= end) return p;
         }
         switch (*p) {
         case '"':
            return skip_string_fast<Opts>(p, end);
         case 't':
            return p + 4 <= end ? p + 4 : end;
         case 'f':
            return p + 5 <= end ? p + 5 : end;
         case 'n':
            return p + 4 <= end ? p + 4 : end;
         case '[':
         case '{': {
            int depth = 1;
            ++p;

            while (depth > 0) {
               if constexpr (Opts.null_terminated) {
                  if (*p == '\0') break;
               }
               else {
                  if (p >= end) break;
               }
               switch (lazy_char_class[uint8_t(*p)]) {
               case quote:
                  p = skip_string_fast<Opts>(p, end);
                  break;
               case open:
                  ++depth;
                  ++p;
                  break;
               case close:
                  --depth;
                  ++p;
                  break;
               case number:
                  p = skip_number_lazy<Opts>(p, end);
                  break;
               default:
                  ++p;
                  break;
               }
            }
            return p;
         }
         default: {
            // Number, or an unrecognized byte.
            const char* const start = p;
            if constexpr (Opts.null_terminated) {
               while (numeric_table[uint8_t(*p)]) ++p;
            }
            else {
               while (p < end && numeric_table[uint8_t(*p)]) ++p;
            }
            // Guarantee forward progress. A byte that begins no valid value (not a string,
            // literal, container, or number - impossible in well-formed JSON) leaves p unmoved,
            // which would spin every caller that skips values in a loop (size(), index(),
            // operator[], iteration) forever on malformed input. Treat it as a one-byte scalar so
            // the scan always advances whenever p < end. Bounded by end because json_end_ is the
            // authoritative message end: a byte within it is safe to step over, while at or past
            // end we must not move (the caller's own p >= end guard then stops the loop). On
            // valid input p is always a real value start, so this branch never fires - no
            // behavior change.
            if (p == start && p < end) ++p;
            return p;
         }
         }
      }

      // ============================================================================
      // Streaming cursor storage (lazy_streaming_cursor option)
      // ============================================================================
      //
      // A single slot on lazy_document remembering the byte extent of the most recently
      // consumed value. lazy_iterator::operator++ jumps to the recorded end instead of
      // re-scanning, but only when the recorded extent starts exactly at the element the
      // iterator is sitting on. That pointer-identity check is what makes a stale slot
      // harmless: a byte offset in the buffer identifies exactly one value, so an extent
      // that starts where the iterator stands always describes the value it is about to
      // skip. Anything else falls back to a normal scan.

      // Disabled: an empty type, so lazy_document pays nothing under GLZ_NO_UNIQUE_ADDRESS.
      struct lazy_extent_disabled
      {
         static constexpr void clear() noexcept {}
         static constexpr void set(const char*, const char*, const char*) noexcept {}
         [[nodiscard]] static constexpr bool try_jump(const char*, const char*, const char*&) noexcept { return false; }
      };

      // Offsets rather than pointers so a copied/moved document rebases onto its own buffer.
      // end_off == 0 means "no extent recorded" - a real value can never end at offset 0.
      struct lazy_extent
      {
         size_t start_off{};
         size_t end_off{};

         void clear() noexcept { end_off = 0; }

         void set(const char* base, const char* start, const char* end) noexcept
         {
            if (!base || start < base || end < start) [[unlikely]] {
               clear();
               return;
            }
            start_off = size_t(start - base);
            end_off = size_t(end - base);
         }

         [[nodiscard]] bool try_jump(const char* base, const char* pos, const char*& out) noexcept
         {
            if (end_off == 0 || !base) {
               return false;
            }
            if (pos != base + start_off) {
               // The extent describes some other value; leave it in place, it may still match a
               // later advance (an inner iterator can record before an outer one asks).
               return false;
            }
            out = base + end_off;
            clear(); // consumed exactly once
            return true;
         }
      };

      template <auto Opts>
      using lazy_extent_for = std::conditional_t<check_lazy_streaming_cursor(Opts), lazy_extent, lazy_extent_disabled>;
   } // namespace detail

   // ============================================================================
   // lazy_json_view - Truly lazy view with on-demand scanning
   // ============================================================================

   /**
    * @brief A truly lazy view into JSON data.
    *
    * No upfront processing - all navigation uses SWAR-accelerated scanning.
    *
    * For objects, parse_pos_ tracks the current scan position to enable
    * efficient sequential key access (O(n) total instead of O(n²)).
    *
    * Memory layout (48 bytes on 64-bit, 24 bytes on 32-bit):
    * - doc_: pointer to document (8/4 bytes)
    * - data_: pointer to value start in JSON (8/4 bytes)
    * - parse_pos_: current scan position for progressive parsing (8/4 bytes)
    * - key_: stored key for iteration (16/8 bytes - string_view)
    * - error_: error code (4 bytes)
    * - padding: 4/0 bytes
    */
   template <auto Opts = opts{}>
   struct lazy_json_view
   {
     private:
      const lazy_document<Opts>* doc_{};
      const char* data_{};
      mutable const char* parse_pos_{}; // Current scan position (advances on key access)
      std::string_view key_{};
      error_code error_{error_code::none};

      lazy_json_view(error_code ec) noexcept : error_(ec) {}

     public:
      lazy_json_view() = default;
      lazy_json_view(const lazy_document<Opts>* doc, const char* data) noexcept : doc_(doc), data_(data) {}

      [[nodiscard]] static lazy_json_view make_error(error_code ec) noexcept { return lazy_json_view{ec}; }

      [[nodiscard]] bool has_error() const noexcept { return error_ != error_code::none; }
      [[nodiscard]] error_code error() const noexcept { return error_; }

      // Type checking - direct from JSON byte
      [[nodiscard]] bool is_null() const noexcept { return has_error() || !data_ || *data_ == 'n'; }
      [[nodiscard]] bool is_boolean() const noexcept
      {
         return !has_error() && data_ && (*data_ == 't' || *data_ == 'f');
      }
      [[nodiscard]] bool is_number() const noexcept
      {
         return !has_error() && data_ && (is_digit(uint8_t(*data_)) || *data_ == '-');
      }
      [[nodiscard]] bool is_string() const noexcept { return !has_error() && data_ && *data_ == '"'; }
      [[nodiscard]] bool is_array() const noexcept { return !has_error() && data_ && *data_ == '['; }
      [[nodiscard]] bool is_object() const noexcept { return !has_error() && data_ && *data_ == '{'; }

      explicit operator bool() const noexcept { return !has_error() && data_ && *data_ != 'n'; }

      [[nodiscard]] const char* data() const noexcept { return data_; }
      [[nodiscard]] const char* json_end() const noexcept;

      /// @brief Get the raw JSON bytes for this value
      /// @return string_view of the raw JSON, or empty if error
      /// @note Useful for passing to glz::read_json to deserialize into a struct
      /// @note Consider using read_into<T>() instead for better performance
      [[nodiscard]] std::string_view raw_json() const noexcept
      {
         if (has_error() || !data_) return {};
         const char* end = detail::skip_value_lazy<Opts>(data_, json_end());
         return {data_, static_cast<size_t>(end - data_)};
      }

      /// @brief Parse this value directly into a C++ type (single-pass, no double scanning)
      /// @tparam T The type to parse into
      /// @param value The object to populate
      /// @return Error context (check error_ctx.error for success/failure)
      /// @note This is more efficient than raw_json() + read_json() as it avoids scanning twice
      template <class T>
      [[nodiscard]] error_ctx read_into(T& value) const
      {
         if (has_error()) {
            return error_ctx{0, error_};
         }
         if (!data_) {
            return error_ctx{0, error_code::unexpected_end};
         }
         // Use parse<JSON>::op which naturally stops at value end
         // This avoids the need to pre-scan to find the value's extent
         context ctx{};
         auto it = data_;
         auto end = json_end();
         parse<JSON>::op<Opts>(value, ctx, it, end);
         // Capture completeness before finalize_read_context, which downgrades
         // partial_read_complete and a depth-zero end_reached to success. Those leave `it`
         // short of the value's true end, so `it` is only the real extent when the parse
         // reported nothing at all.
         const bool consumed_whole_value = ctx.error == error_code::none;
         finalize_read_context<Opts>(ctx);
         if (bool(ctx.error)) {
            return error_ctx{static_cast<size_t>(it - data_), ctx.error};
         }
         // Streaming cursor: the value spans [data_, it). Record it so a subsequent iterator
         // advance over this element can jump rather than re-scan. Under partial_read the
         // parse deliberately stops early, so nothing is recorded and iteration falls back to
         // scanning - correct, just not accelerated.
         if constexpr (check_lazy_streaming_cursor(Opts)) {
            // doc_ is null only for detached subviews; lazy_json() views always carry it.
            if (doc_ && consumed_whole_value) [[likely]] {
               record_consumed_extent(it);
            }
         }
         return {};
      }

      // Record [data_, it) as consumed, but only when we can actually tell that `it` is this
      // value's end.
      //
      // read_into hands the iterator to parse<JSON>::op, and where that leaves it is up to the
      // from<JSON, T> specialization. Glaze's own readers stop at the value's end, but that is a
      // convention rather than something checkable here: glz::text deliberately swallows the
      // rest of the buffer, and a custom reader may consume only a prefix. Trusting `it` blindly
      // is how the cursor turns a well-formed document into a short or mis-valued element
      // stream, with no error anywhere.
      //
      // So only containers are recorded, and only when the parse finished exactly on the
      // element's own closing bracket. Scalars are skipped by the cheap paths anyway (a numeric
      // table walk or a memchr for strings), so declining to record them costs almost nothing
      // and removes the whole class of doubt.
      void record_consumed_extent(const char* it) const noexcept
      {
         const char open = *data_;
         const char close = (open == '{') ? '}' : ((open == '[') ? ']' : '\0');
         if (close == '\0') {
            return; // scalar: not worth recording
         }
         if (it <= data_ || it[-1] != close) {
            return; // the reader stopped somewhere other than this container's end
         }
         doc_->consumed_.set(doc_->json_data(), data_, it);
      }

      template <class T>
      [[nodiscard]] expected<T, error_ctx> get() const;

      [[nodiscard]] lazy_json_view operator[](size_t index) const;
      [[nodiscard]] lazy_json_view operator[](std::string_view key) const;
      [[nodiscard]] bool contains(std::string_view key) const;
      [[nodiscard]] size_t size() const;
      [[nodiscard]] bool empty() const noexcept;

      // Key access for object iteration
      [[nodiscard]] std::string_view key() const noexcept { return key_; }

      [[nodiscard]] lazy_iterator<Opts> begin() const;
      [[nodiscard]] lazy_iterator<Opts> end() const;

      /// @brief Build an index for O(1) iteration and random access
      /// @return indexed_lazy_view with pre-computed element positions
      [[nodiscard]] indexed_lazy_view<Opts> index() const;

     private:
      friend struct lazy_document<Opts>;
      friend class lazy_iterator<Opts>;
      friend struct indexed_lazy_view<Opts>;
      friend class indexed_lazy_iterator<Opts>;

      // Constructor with key for iteration
      lazy_json_view(const lazy_document<Opts>* doc, const char* data, std::string_view key) noexcept
         : doc_(doc), data_(data), key_(key)
      {}

      // Skip whitespace helper
      // Note: The minified option is not used here because branch prediction
      // makes the whitespace loop essentially free when there's no whitespace.
      // Benchmarks show no benefit from skipping this check.
      static void skip_ws(const char*& p, [[maybe_unused]] const char* end) noexcept
      {
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*p)]) ++p;
         }
         else {
            while (p < end && whitespace_table[uint8_t(*p)]) ++p;
         }
      }

      // Parse a key from current position, return key and advance p
      static std::string_view parse_key(const char*& p, const char* end) noexcept;
   };

   // ============================================================================
   // lazy_document - Minimal container, no upfront processing
   // ============================================================================

   template <auto Opts = opts{}>
   struct lazy_document
   {
     private:
      const char* json_{};
      size_t len_{};
      const char* root_data_{};
      mutable lazy_json_view<Opts> root_view_{}; // Cached root view with parse_pos_

      // Streaming cursor slot (lazy_streaming_cursor option). Empty, and therefore free, when
      // the option is off. Mutable because recording an extent is a caching side effect of
      // otherwise const traversal.
      GLZ_NO_UNIQUE_ADDRESS mutable detail::lazy_extent_for<Opts> consumed_{};

      friend struct lazy_json_view<Opts>;
      friend class lazy_iterator<Opts>;

      // Factory method for lazy_json
      template <auto O, class Buffer>
      friend expected<lazy_document<O>, error_ctx> lazy_json(Buffer&&);

      // Helper to initialize root_view_ with correct doc_ pointer
      void init_root_view() noexcept { root_view_ = lazy_json_view<Opts>{this, root_data_}; }

     public:
      lazy_document() = default;

      // Copy constructor - fix doc_ pointer and preserve parse_pos_
      lazy_document(const lazy_document& other)
         : json_(other.json_), len_(other.len_), root_data_(other.root_data_), consumed_(other.consumed_)
      {
         init_root_view();
         root_view_.parse_pos_ = other.root_view_.parse_pos_;
      }

      lazy_document& operator=(const lazy_document& other)
      {
         if (this != &other) {
            json_ = other.json_;
            len_ = other.len_;
            root_data_ = other.root_data_;
            consumed_ = other.consumed_;
            init_root_view();
            root_view_.parse_pos_ = other.root_view_.parse_pos_;
         }
         return *this;
      }

      // Move constructor - fix doc_ pointer and transfer parse_pos_
      lazy_document(lazy_document&& other) noexcept
         : json_(other.json_), len_(other.len_), root_data_(other.root_data_), consumed_(other.consumed_)
      {
         init_root_view();
         root_view_.parse_pos_ = other.root_view_.parse_pos_;
      }

      lazy_document& operator=(lazy_document&& other) noexcept
      {
         if (this != &other) {
            json_ = other.json_;
            len_ = other.len_;
            root_data_ = other.root_data_;
            consumed_ = other.consumed_;
            init_root_view();
            root_view_.parse_pos_ = other.root_view_.parse_pos_;
         }
         return *this;
      }

      /// @brief Get the root view (cached, enables progressive key scanning)
      [[nodiscard]] lazy_json_view<Opts>& root() noexcept { return root_view_; }

      [[nodiscard]] const lazy_json_view<Opts>& root() const noexcept { return root_view_; }

      [[nodiscard]] lazy_json_view<Opts> operator[](std::string_view key) const { return root_view_[key]; }
      [[nodiscard]] lazy_json_view<Opts> operator[](size_t index) const { return root_view_[index]; }

      [[nodiscard]] bool is_null() const noexcept { return !root_data_ || *root_data_ == 'n'; }
      [[nodiscard]] bool is_array() const noexcept { return root_data_ && *root_data_ == '['; }
      [[nodiscard]] bool is_object() const noexcept { return root_data_ && *root_data_ == '{'; }

      explicit operator bool() const noexcept { return !is_null(); }

      [[nodiscard]] const char* json_data() const noexcept { return json_; }
      [[nodiscard]] size_t json_size() const noexcept { return len_; }

      /// @brief Reset parse position to beginning (for re-scanning from start)
      void reset_parse_pos() noexcept { root_view_.parse_pos_ = nullptr; }
   };

   // ============================================================================
   // lazy_iterator - Forward iterator with lazy scanning
   // ============================================================================

   template <auto Opts>
   class lazy_iterator
   {
     private:
      const lazy_document<Opts>* doc_{};
      const char* json_end_{};
      const char* container_start_{}; // '[' or '{' of the container being iterated
      char close_char_{};
      bool is_object_{};
      bool at_end_{true};
      lazy_json_view<Opts> current_view_{}; // Stored view for parse_pos_ optimization

      // Streaming cursor: on reaching the close, publish this container's own extent so an
      // outer iterator advancing over it can jump too. That is what makes the optimization
      // compose through nesting - an inner loop running to completion pays for the outer
      // advance. No-op when the option is off.
      void record_container_extent(const char* close_pos) noexcept
      {
         if constexpr (check_lazy_streaming_cursor(Opts)) {
            if (doc_ && container_start_) [[likely]] {
               // close_pos is the close character, except on a truncated buffer where it is
               // json_end_ and there is nothing past it to include.
               const char* const extent_end = (close_pos < json_end_) ? close_pos + 1 : close_pos;
               doc_->consumed_.set(doc_->json_data(), container_start_, extent_end);
            }
         }
      }

      // Scan past the element beginning at elem_start, using the parse_pos_ shortcut when keyed
      // access already walked part of it.
      [[nodiscard]] const char* scan_past_element(const char* elem_start) const noexcept
      {
         if (current_view_.is_object() && current_view_.parse_pos_ && current_view_.parse_pos_ > elem_start) {
            // Skip the last accessed value, then scan to the end of this object (depth 1 -> 0).
            const char* p = detail::skip_value_lazy<Opts>(current_view_.parse_pos_, json_end_);
            return detail::skip_to_depth_zero<Opts>(p, json_end_, 1);
         }
         return detail::skip_value_lazy<Opts>(elem_start, json_end_);
      }

     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = lazy_json_view<Opts>;
      using difference_type = ptrdiff_t;
      using pointer = void;
      using reference = lazy_json_view<Opts>&;

      lazy_iterator() = default;

      lazy_iterator(const lazy_document<Opts>* doc, const char* container_start, const char* end, bool is_object);

      // Return reference to stored view - allows parse_pos_ optimization when user uses auto&
      reference operator*() { return current_view_; }
      const lazy_json_view<Opts>& operator*() const { return current_view_; }

      lazy_iterator& operator++();

      lazy_iterator operator++(int)
      {
         auto tmp = *this;
         ++*this;
         return tmp;
      }

      bool operator==(const lazy_iterator& other) const { return at_end_ == other.at_end_; }
      bool operator!=(const lazy_iterator& other) const { return !(*this == other); }

     private:
      void advance_to_next_element(const char*& pos);
      void skip_ws(const char*& p) noexcept
      {
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*p)]) ++p;
         }
         else {
            while (p < json_end_ && whitespace_table[uint8_t(*p)]) ++p;
         }
      }
   };

   // ============================================================================
   // indexed_lazy_view - Lazy view with pre-built element index for O(1) access
   // ============================================================================

   /**
    * @brief A lazy view with a pre-built index for O(1) iteration and random access.
    *
    * Created by calling `index()` on a lazy_json_view. Scans the container once
    * to build an index of element positions, then provides:
    * - O(1) iteration advancement (vs O(element_size) for lazy_json_view)
    * - O(1) random access by index
    * - O(1) size query
    *
    * Elements returned are still lazy_json_view objects, so nested access remains lazy.
    *
    * Memory: Base view + 8 bytes per element (+ 16 bytes per element for objects with keys)
    */
   template <auto Opts>
   struct indexed_lazy_view
   {
     private:
      const lazy_document<Opts>* doc_{};
      const char* json_end_{};
      std::vector<const char*> value_starts_;
      std::vector<std::string_view> keys_; // Only populated for objects
      bool is_object_{};

      friend class indexed_lazy_iterator<Opts>;

     public:
      indexed_lazy_view() = default;

      /// @brief Number of elements - O(1)
      [[nodiscard]] size_t size() const noexcept { return value_starts_.size(); }

      /// @brief Check if empty - O(1)
      [[nodiscard]] bool empty() const noexcept { return value_starts_.empty(); }

      /// @brief Check if this is an indexed object
      [[nodiscard]] bool is_object() const noexcept { return is_object_; }

      /// @brief Check if this is an indexed array
      [[nodiscard]] bool is_array() const noexcept { return !is_object_; }

      /// @brief O(1) random access by index
      [[nodiscard]] lazy_json_view<Opts> operator[](size_t index) const
      {
         if (index >= value_starts_.size()) {
            return lazy_json_view<Opts>::make_error(error_code::exceeded_static_array_size);
         }
         std::string_view key = is_object_ ? keys_[index] : std::string_view{};
         return lazy_json_view<Opts>{doc_, value_starts_[index], key};
      }

      /// @brief O(n) key lookup for objects (linear search in index)
      [[nodiscard]] lazy_json_view<Opts> operator[](std::string_view key) const
      {
         if (!is_object_) {
            return lazy_json_view<Opts>::make_error(error_code::get_wrong_type);
         }
         for (size_t i = 0; i < keys_.size(); ++i) {
            if (keys_[i] == key) {
               return lazy_json_view<Opts>{doc_, value_starts_[i], keys_[i]};
            }
         }
         return lazy_json_view<Opts>::make_error(error_code::key_not_found);
      }

      /// @brief Check if object contains key - O(n) linear search
      [[nodiscard]] bool contains(std::string_view key) const
      {
         if (!is_object_) return false;
         for (const auto& k : keys_) {
            if (k == key) return true;
         }
         return false;
      }

      [[nodiscard]] indexed_lazy_iterator<Opts> begin() const;
      [[nodiscard]] indexed_lazy_iterator<Opts> end() const;

     private:
      friend struct lazy_json_view<Opts>;

      // Private constructor used by lazy_json_view::index()
      indexed_lazy_view(const lazy_document<Opts>* doc, const char* json_end, bool is_object)
         : doc_(doc), json_end_(json_end), is_object_(is_object)
      {}

      void reserve(size_t n)
      {
         value_starts_.reserve(n);
         if (is_object_) {
            keys_.reserve(n);
         }
      }

      void add_element(const char* value_start, std::string_view key = {})
      {
         value_starts_.push_back(value_start);
         if (is_object_) {
            keys_.push_back(key);
         }
      }
   };

   // ============================================================================
   // indexed_lazy_iterator - O(1) advancement using pre-built index
   // ============================================================================

   template <auto Opts>
   class indexed_lazy_iterator
   {
     private:
      const indexed_lazy_view<Opts>* parent_{};
      size_t index_{};
      mutable lazy_json_view<Opts> current_view_{}; // Cached for reference return

     public:
      using iterator_category = std::random_access_iterator_tag;
      using value_type = lazy_json_view<Opts>;
      using difference_type = ptrdiff_t;
      using pointer = void;
      using reference = lazy_json_view<Opts>&;

      indexed_lazy_iterator() = default;
      indexed_lazy_iterator(const indexed_lazy_view<Opts>* parent, size_t index) : parent_(parent), index_(index) {}

      reference operator*() const
      {
         std::string_view key = parent_->is_object_ ? parent_->keys_[index_] : std::string_view{};
         current_view_ = lazy_json_view<Opts>{parent_->doc_, parent_->value_starts_[index_], key};
         return current_view_;
      }

      lazy_json_view<Opts>* operator->() const
      {
         operator*(); // Update current_view_
         return &current_view_;
      }

      indexed_lazy_iterator& operator++()
      {
         ++index_;
         return *this;
      }

      indexed_lazy_iterator operator++(int)
      {
         auto tmp = *this;
         ++index_;
         return tmp;
      }

      indexed_lazy_iterator& operator--()
      {
         --index_;
         return *this;
      }

      indexed_lazy_iterator operator--(int)
      {
         auto tmp = *this;
         --index_;
         return tmp;
      }

      indexed_lazy_iterator& operator+=(difference_type n)
      {
         index_ = static_cast<size_t>(static_cast<difference_type>(index_) + n);
         return *this;
      }

      indexed_lazy_iterator& operator-=(difference_type n)
      {
         index_ = static_cast<size_t>(static_cast<difference_type>(index_) - n);
         return *this;
      }

      indexed_lazy_iterator operator+(difference_type n) const
      {
         return {parent_, static_cast<size_t>(static_cast<difference_type>(index_) + n)};
      }

      indexed_lazy_iterator operator-(difference_type n) const
      {
         return {parent_, static_cast<size_t>(static_cast<difference_type>(index_) - n)};
      }

      difference_type operator-(const indexed_lazy_iterator& other) const
      {
         return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
      }

      reference operator[](difference_type n) const
      {
         std::string_view key =
            parent_->is_object_ ? parent_->keys_[index_ + static_cast<size_t>(n)] : std::string_view{};
         current_view_ =
            lazy_json_view<Opts>{parent_->doc_, parent_->value_starts_[index_ + static_cast<size_t>(n)], key};
         return current_view_;
      }

      bool operator==(const indexed_lazy_iterator& other) const { return index_ == other.index_; }
      bool operator!=(const indexed_lazy_iterator& other) const { return index_ != other.index_; }
      bool operator<(const indexed_lazy_iterator& other) const { return index_ < other.index_; }
      bool operator<=(const indexed_lazy_iterator& other) const { return index_ <= other.index_; }
      bool operator>(const indexed_lazy_iterator& other) const { return index_ > other.index_; }
      bool operator>=(const indexed_lazy_iterator& other) const { return index_ >= other.index_; }

      friend indexed_lazy_iterator operator+(difference_type n, const indexed_lazy_iterator& it) { return it + n; }
   };

   // ============================================================================
   // Implementation: lazy_json_view methods (truly lazy - no structural index)
   // ============================================================================

   template <auto Opts>
   inline const char* lazy_json_view<Opts>::json_end() const noexcept
   {
      return doc_ ? doc_->json_ + doc_->len_ : nullptr;
   }

   template <auto Opts>
   inline std::string_view lazy_json_view<Opts>::parse_key(const char*& p, const char* end) noexcept
   {
      // A non-null-terminated buffer has no '\0' sentinel, so a scan loop can reach the next
      // key position with p == end (e.g. after a trailing comma). Guard the opening dereference.
      if constexpr (!Opts.null_terminated) {
         if (p >= end) return {};
      }
      if (*p != '"') return {};
      const char* const start = p;
      ++p; // skip opening quote
      const char* key_start = p;

      while (p < end) {
         const char* quote = static_cast<const char*>(std::memchr(p, '"', static_cast<size_t>(end - p)));
         if (!quote) [[unlikely]] {
            p = end;
            return std::string_view{key_start, static_cast<size_t>(end - key_start)};
         }

         // Count preceding backslashes to check if escaped
         size_t backslashes = 0;
         const char* check = quote - 1;
         while (check > start && *check == '\\') {
            ++backslashes;
            --check;
         }

         if ((backslashes & 1) == 0) {
            // Even backslashes = real quote
            std::string_view key{key_start, static_cast<size_t>(quote - key_start)};
            p = quote + 1; // skip closing quote
            return key;
         }
         // Odd backslashes = escaped quote, continue searching
         p = quote + 1;
      }
      return std::string_view{key_start, static_cast<size_t>(p - key_start)};
   }

   template <auto Opts>
   inline lazy_json_view<Opts> lazy_json_view<Opts>::operator[](size_t index) const
   {
      if (has_error()) return *this;
      if (!is_array()) return make_error(error_code::get_wrong_type);

      const char* end = json_end();
      const char* p = data_ + 1; // skip '['
      skip_ws(p, end);

      // Check for empty array. json_end_ bounds the message in both modes: null_terminated does
      // not guarantee the '\0' sits at json_end_, so fall back to the bound here too.
      if (p >= end || *p == ']') return make_error(error_code::exceeded_static_array_size);

      // Skip 'index' elements using lazy scanning
      for (size_t i = 0; i < index; ++i) {
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);

         if (p >= end || *p == ']') return make_error(error_code::exceeded_static_array_size);

         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }

      // The requested index landed at or past end-of-input (a trailing comma with no following
      // element, or an unclosed array); a view positioned at end would read past the buffer, or,
      // when null_terminated, off the message into trailing bytes.
      if (p >= end) return make_error(error_code::exceeded_static_array_size);
      return {doc_, p};
   }

   template <auto Opts>
   inline lazy_json_view<Opts> lazy_json_view<Opts>::operator[](std::string_view key) const
   {
      if (has_error()) return *this;
      if (!is_object()) return make_error(error_code::get_wrong_type);

      const char* end = json_end();
      const char* obj_start = data_ + 1; // skip '{'

      // Determine search start position
      // parse_pos_ points to the VALUE of the last found key (lazy skip)
      // We need to skip past it before continuing the search
      const char* search_start = obj_start;
      if (parse_pos_ && parse_pos_ > data_) {
         // Lazily skip past the last found value now
         const char* p = parse_pos_;
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);
         if (p < end && *p == ',') {
            ++p;
            skip_ws(p, end);
         }
         search_start = p;
      }

      const char* p = search_start;
      skip_ws(p, end);

      // Forward pass: search from current position to end of object
      while (true) {
         // Check for end of object. json_end_ bounds the message in both modes (null_terminated
         // does not guarantee the '\0' sits at json_end_); this is also the loop's backstop
         // against an unclosed object spinning forever on a key parse that never advances.
         if (p >= end || *p == '}') break;

         // Parse key
         auto k = parse_key(p, end);
         skip_ws(p, end);

         // Skip ':'
         if (p < end && *p == ':') {
            ++p;
            skip_ws(p, end);
         }

         // Check if key matches
         if (k == key) {
            // A matched key whose value begins at or past json_end_ is truncated; a view
            // positioned at end would read past the buffer (or, when null_terminated, off the
            // message into trailing bytes). Return the error before storing parse_pos_ so a
            // truncated match leaves no state.
            if (p >= end) return make_error(error_code::unexpected_end);
            parse_pos_ = p; // Store value position (lazy - don't skip yet)
            return {doc_, p};
         }

         // Skip value using lazy scanning
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);

         // Skip comma
         if (p < end && *p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }

      // Wrap-around pass: search from beginning to where we started
      if (search_start != obj_start) {
         p = obj_start;
         skip_ws(p, end);

         while (p < search_start) {
            // Check for end of object (see forward pass).
            if (p >= end || *p == '}') break;

            // Parse key
            auto k = parse_key(p, end);
            skip_ws(p, end);

            // Skip ':'
            if (p < end && *p == ':') {
               ++p;
               skip_ws(p, end);
            }

            // Check if key matches
            if (k == key) {
               // A matched key whose value begins at or past json_end_ is truncated; a view
               // positioned at end would read past the buffer (or, when null_terminated, off the
               // message into trailing bytes). Return the error before storing parse_pos_ so a
               // truncated match leaves no state.
               if (p >= end) return make_error(error_code::unexpected_end);
               parse_pos_ = p; // Store value position (lazy - don't skip yet)
               return {doc_, p};
            }

            // Skip value using lazy scanning
            p = detail::skip_value_lazy<Opts>(p, end);
            skip_ws(p, end);

            // Skip comma
            if (p < end && *p == ',') {
               ++p;
               skip_ws(p, end);
            }
         }
      }

      return make_error(error_code::key_not_found);
   }

   template <auto Opts>
   inline bool lazy_json_view<Opts>::contains(std::string_view key) const
   {
      auto result = (*this)[key];
      return !result.has_error();
   }

   template <auto Opts>
   inline size_t lazy_json_view<Opts>::size() const
   {
      if (has_error() || !data_) return 0;
      if (!is_array() && !is_object()) return 0;

      const char* end = json_end();
      const char* p = data_ + 1;
      skip_ws(p, end);

      const char close_char = is_array() ? ']' : '}';

      // json_end_ bounds the message in both modes: null_terminated does not guarantee the '\0'
      // sits at json_end_, so fall back to the bound here and throughout the loop below.
      if (p >= end || *p == close_char) return 0;

      size_t count = 0;
      const bool is_obj = is_object();

      while (true) {
         // Stop before scanning a key/value at or past end. This keeps skip_string_fast and
         // skip_value_lazy from dereferencing off the buffer, and is the loop's backstop against
         // an unclosed container running off the end.
         if (p >= end) return count;

         if (is_obj) {
            // Skip key
            p = detail::skip_string_fast<Opts>(p, end);
            skip_ws(p, end);
            if (p < end && *p == ':') {
               ++p;
               skip_ws(p, end);
            }
            // The value begins at or past end (truncated pair); don't count it, so size() agrees
            // with index() and iteration.
            if (p >= end) return count;
         }

         // Skip value
         p = detail::skip_value_lazy<Opts>(p, end);
         ++count;
         skip_ws(p, end);

         if (p >= end || *p == close_char) return count;

         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }
   }

   template <auto Opts>
   inline bool lazy_json_view<Opts>::empty() const noexcept
   {
      if (has_error() || !data_) return true;
      if (is_null()) return true;
      if (!is_array() && !is_object()) return false;

      const char* end = json_end();
      const char* p = data_ + 1;
      skip_ws(p, end);

      const char close_char = is_array() ? ']' : '}';
      // json_end_ bounds the message in both modes (null_terminated does not guarantee the '\0'
      // sits at json_end_); a container with nothing before end is empty. This keeps empty()
      // consistent with size()/index()/iteration reporting 0 on a bare, unclosed container.
      return p >= end || *p == close_char;
   }

   // ============================================================================
   // lazy_iterator implementation (truly lazy)
   // ============================================================================

   template <auto Opts>
   inline lazy_iterator<Opts>::lazy_iterator(const lazy_document<Opts>* doc, const char* container_start,
                                             const char* end, bool is_object)
      : doc_(doc), json_end_(end), container_start_(container_start), is_object_(is_object), at_end_(false)
   {
      close_char_ = is_object ? '}' : ']';
      const char* pos = container_start + 1; // skip '[' or '{'
      skip_ws(pos);

      // Check for empty container
      if constexpr (Opts.null_terminated) {
         if (*pos == close_char_) {
            at_end_ = true;
            record_container_extent(pos);
            return;
         }
      }
      else {
         if (pos >= json_end_ || *pos == close_char_) {
            at_end_ = true;
            record_container_extent(pos);
            return;
         }
      }

      // Position at first element and initialize current_view_
      advance_to_next_element(pos);
   }

   template <auto Opts>
   inline void lazy_iterator<Opts>::advance_to_next_element(const char*& pos)
   {
      std::string_view key{};
      if (is_object_) {
         // Parse key
         key = lazy_json_view<Opts>::parse_key(pos, json_end_);
         skip_ws(pos);

         // Skip ':'
         if (pos < json_end_ && *pos == ':') {
            ++pos;
            skip_ws(pos);
         }
      }
      // A value that begins at or past json_end_ is a truncated element; stop iterating instead
      // of handing back a view whose data pointer sits at end (every view op reads *data_, an
      // out-of-bounds read for a non-null-terminated buffer). This bound is unconditional on
      // purpose: json_end_ is the authoritative end of the message, whereas null_terminated only
      // promises a '\0' sentinel exists, not that it sits at json_end_ (it may be trailing buffer
      // content further on), so '\0' cannot substitute here. It is also the iterator's only
      // backstop against spinning forever on unclosed null-terminated input.
      if (pos >= json_end_) {
         at_end_ = true;
         return;
      }
      // Store key in current_view_ (will be set properly after this call)
      current_view_ = lazy_json_view<Opts>{doc_, pos, key};
   }

   template <auto Opts>
   inline lazy_iterator<Opts>& lazy_iterator<Opts>::operator++()
   {
      if (at_end_) return *this;

      const char* const elem_start = current_view_.data();
      const char* pos = elem_start;

      // Streaming cursor: if this element was already consumed end-to-end (read_into, or a
      // nested iterator that ran to its close) the recorded extent says where it finishes, so
      // skip the scan entirely.
      bool jumped = false;
      if constexpr (check_lazy_streaming_cursor(Opts)) {
         if (doc_) [[likely]] {
            jumped = doc_->consumed_.try_jump(doc_->json_data(), pos, pos);
         }
      }
      if (not jumped) {
         pos = scan_past_element(elem_start);
      }

      skip_ws(pos);

      // Check for end
      if constexpr (Opts.null_terminated) {
         if (*pos == close_char_) {
            at_end_ = true;
            record_container_extent(pos);
            return *this;
         }
      }
      else {
         if (pos >= json_end_ || *pos == close_char_) {
            at_end_ = true;
            record_container_extent(pos);
            return *this;
         }
      }

      // Skip comma
      if (*pos == ',') {
         ++pos;
         skip_ws(pos);
      }

      // Advance to next element (updates current_view_)
      advance_to_next_element(pos);

      return *this;
   }

   template <auto Opts>
   inline lazy_iterator<Opts> lazy_json_view<Opts>::begin() const
   {
      if (has_error() || !data_) return end();
      if (!is_array() && !is_object()) return end();
      return lazy_iterator<Opts>{doc_, data_, json_end(), is_object()};
   }

   template <auto Opts>
   inline lazy_iterator<Opts> lazy_json_view<Opts>::end() const
   {
      return lazy_iterator<Opts>{};
   }

   // ============================================================================
   // indexed_lazy_view implementation
   // ============================================================================

   template <auto Opts>
   inline indexed_lazy_iterator<Opts> indexed_lazy_view<Opts>::begin() const
   {
      return indexed_lazy_iterator<Opts>{this, 0};
   }

   template <auto Opts>
   inline indexed_lazy_iterator<Opts> indexed_lazy_view<Opts>::end() const
   {
      return indexed_lazy_iterator<Opts>{this, value_starts_.size()};
   }

   // ============================================================================
   // lazy_json_view::index() implementation
   // ============================================================================

   template <auto Opts>
   inline indexed_lazy_view<Opts> lazy_json_view<Opts>::index() const
   {
      // Return empty indexed view for non-containers or errors
      if (has_error() || !data_ || (!is_array() && !is_object())) {
         return indexed_lazy_view<Opts>{};
      }

      const char* end = json_end();
      indexed_lazy_view<Opts> result{doc_, end, is_object()};

      const char* p = data_ + 1; // skip '[' or '{'
      skip_ws(p, end);

      const char close_char = is_array() ? ']' : '}';
      const bool is_obj = is_object();

      // Check for empty container. json_end_ bounds the message in both modes: null_terminated
      // does not guarantee the '\0' sits at json_end_, so fall back to the bound throughout.
      if (p >= end || *p == close_char) return result;

      // Scan and record all element positions
      while (true) {
         // Stop before parsing a key/value at or past end: this keeps the scans from reading off
         // the buffer and is the loop's backstop against an unclosed container looping forever
         // (repeatedly recording an element positioned at end).
         if (p >= end) break;

         std::string_view key{};
         if (is_obj) {
            // Parse and record key
            key = parse_key(p, end);
            skip_ws(p, end);

            // Skip ':'
            if (p < end && *p == ':') {
               ++p;
               skip_ws(p, end);
            }
         }

         // A truncated element whose value begins at or past json_end_ must not be recorded:
         // an indexed view would later hand back a view positioned at end.
         if (p >= end) break;

         // Record element start position
         result.add_element(p, key);

         // Skip value
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);

         // Check for end of container
         if (p >= end || *p == close_char) break;

         // Skip comma
         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }

      return result;
   }

   // ============================================================================
   // lazy_json_view::get<T>() implementation
   // ============================================================================

   template <auto Opts>
   template <class T>
   [[nodiscard]] inline expected<T, error_ctx> lazy_json_view<Opts>::get() const
   {
      if (has_error()) {
         return unexpected(error_ctx{0, error_});
      }

      const char* end = json_end();

      if constexpr (std::is_same_v<T, bool>) {
         if (!is_boolean()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         return *data_ == 't';
      }
      else if constexpr (std::is_same_v<T, std::nullptr_t>) {
         if (!is_null()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         return nullptr;
      }
      else if constexpr (std::is_same_v<T, std::string>) {
         if (!is_string()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto it = data_;
         context ctx{};
         skip_value<JSON>::op<Opts>(ctx, it, end);
         if (bool(ctx.error)) {
            return unexpected(error_ctx{0, ctx.error});
         }
         std::string_view raw{data_, static_cast<size_t>(it - data_)};
         // The read validates UTF-8 unless the document's options turned it off, so the returned
         // string is well formed whenever validation is enabled.
         static constexpr auto string_opts = detail::lazy_string_opts_for<Opts>();
         std::string unescaped{};
         context read_ctx{};
         if (const error_ctx ec = glz::read<string_opts>(unescaped, raw, read_ctx)) {
            return unexpected(ec);
         }
         return unescaped;
      }
      else if constexpr (std::is_same_v<T, std::string_view>) {
         if (!is_string()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto it = data_ + 1;
         context ctx{};
         skip_string_view(ctx, it, end);
         if (bool(ctx.error)) {
            return unexpected(error_ctx{0, ctx.error});
         }
         return std::string_view{data_ + 1, static_cast<size_t>(it - data_ - 1)};
      }
      else if constexpr (std::is_same_v<T, double>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         double value{};
         auto [ptr, ec] = glz::from_chars<false>(data_, end, value);
         if (ec != std::errc()) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, float>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         float value{};
         auto [ptr, ec] = glz::from_chars<false>(data_, end, value);
         if (ec != std::errc()) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, int64_t>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         int64_t value{};
         auto it = data_;
         if (!glz::atoi(value, it, end)) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, uint64_t>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         uint64_t value{};
         auto it = data_;
         if (!glz::atoi(value, it, end)) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, int32_t>) {
         auto result = get<int64_t>();
         if (!result) return unexpected(result.error());
         return static_cast<int32_t>(*result);
      }
      else if constexpr (std::is_same_v<T, uint32_t>) {
         auto result = get<uint64_t>();
         if (!result) return unexpected(result.error());
         return static_cast<uint32_t>(*result);
      }
      else {
         static_assert(false_v<T>, "Unsupported type for lazy_json_view::get<T>()");
      }
   }

   // ============================================================================
   // JSON writer for lazy_json_view
   // ============================================================================

   template <auto Opts>
   struct to<JSON, lazy_json_view<Opts>>
   {
      template <auto WriteOpts, class B>
      GLZ_ALWAYS_INLINE static void op(const lazy_json_view<Opts>& view, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (view.has_error()) {
            ctx.error = view.error();
            return;
         }
         if (!view.data()) {
            dump<false>("null", b, ix);
            return;
         }

         const char* const view_end = view.json_end();
         auto it = view.data();
         context parse_ctx{};
         // Skip under the document's own options, not a default-constructed set. `opts{}` has
         // null_terminated = true, so a view over a buffer without a '\0' sentinel would scan
         // past json_end() looking for one and copy out-of-window bytes into the output.
         skip_value<JSON>::op<Opts>(parse_ctx, it, view_end);
         if (bool(parse_ctx.error)) [[unlikely]] {
            if (!detail::lazy_scalar_reaches_end(*view.data(), parse_ctx.error, it, view_end)) {
               ctx.error = parse_ctx.error;
               return;
            }
            // Complete after all: trim the run of whitespace skip_value walked into so the
            // written bytes match raw_json() exactly.
            while (it > view.data() && whitespace_table[uint8_t(it[-1])]) {
               --it;
            }
         }

         const size_t n = static_cast<size_t>(it - view.data());
         if (n == 0) [[unlikely]] {
            // No JSON value is zero bytes. skip_value consumes nothing when the view is not on a
            // value at all, which navigation can produce on malformed input: looking up "a" in
            // {"a":} leaves the view on the closing brace. Writing nothing while reporting
            // success would splice invalid JSON into the caller's buffer, so refuse.
            ctx.error = error_code::syntax_error;
            return;
         }

         if constexpr (resizable<B>) {
            if (ix + n > b.size()) [[unlikely]] {
               b.resize((std::max)(b.size() * 2, ix + n));
            }
         }
         else {
            if (ix + n > b.size()) [[unlikely]] {
               ctx.error = error_code::buffer_overflow;
               return;
            }
         }
         std::memcpy(&b[ix], view.data(), n);
         ix += n;
      }
   };

   // ============================================================================
   // lazy_json - Main entry point (truly lazy - minimal upfront work)
   // ============================================================================

   /**
    * @brief Create a lazy JSON document - no upfront processing.
    *
    * - Just validates first byte and stores buffer reference - O(1)
    * - All work happens on-demand when accessing fields
    *
    * @tparam Opts Options for parsing
    * @param buffer The JSON text buffer (must remain valid for document lifetime)
    * @return lazy_document on success, error_ctx on failure
    */
   template <auto Opts = opts{}, class Buffer>
   [[nodiscard]] inline expected<lazy_document<Opts>, error_ctx> lazy_json(Buffer&& buffer)
   {
      lazy_document<Opts> doc;
      doc.json_ = buffer.data();
      doc.len_ = buffer.size();

      // Find root value (skip leading whitespace)
      const char* p = buffer.data();
      const char* end = buffer.data() + buffer.size();

      if constexpr (Opts.null_terminated) {
         while (whitespace_table[uint8_t(*p)]) ++p;
      }
      else {
         while (p < end && whitespace_table[uint8_t(*p)]) ++p;
      }

      if (p >= end) {
         return unexpected(error_ctx{0, error_code::unexpected_end});
      }

      // Validate first character is valid JSON start
      const char c = *p;
      if (c != '{' && c != '[' && c != '"' && c != 't' && c != 'f' && c != 'n' && !is_digit(uint8_t(c)) && c != '-') {
         return unexpected(error_ctx{0, error_code::syntax_error});
      }

      doc.root_data_ = p;
      doc.init_root_view(); // Initialize cached root view
      return doc;
   }

   // ============================================================================
   // read_json overload for lazy_json_view (single-pass deserialization)
   // ============================================================================

   /// @brief Read JSON from a lazy_json_view into a C++ type
   /// @tparam T The type to parse into
   /// @tparam Opts The lazy view options
   /// @param value The object to populate
   /// @param view The lazy view to read from
   /// @return Error context if parsing failed
   /// @note This provides the familiar glz::read_json API while using the efficient single-pass read_into internally
   template <class T, auto Opts>
   [[nodiscard]] inline error_ctx read_json(T& value, const lazy_json_view<Opts>& view)
   {
      return view.template read_into<T>(value);
   }

   /// @brief Read JSON from a lazy_json_view rvalue into a C++ type
   /// @note Handles temporaries like glz::read_json(value, doc["field"])
   template <class T, auto Opts>
   [[nodiscard]] inline error_ctx read_json(T& value, lazy_json_view<Opts>&& view)
   {
      return view.template read_into<T>(value);
   }

} // namespace glz
