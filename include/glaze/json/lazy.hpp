// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

#include "glaze/json/read.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/expected.hpp"

namespace glz
{
   // Forward declarations
   template <opts Opts>
   struct lazy_json_view;
   template <opts Opts>
   struct lazy_document;
   template <opts Opts>
   class lazy_iterator;

   // ============================================================================
   // Truly Lazy JSON Parser - No upfront processing
   // ============================================================================

   namespace detail
   {
      // SWAR helper to check for bracket chars
      GLZ_ALWAYS_INLINE uint64_t has_brackets(uint64_t chunk) noexcept
      {
         return has_char<'['>(chunk) | has_char<']'>(chunk) | has_char<'{'>(chunk) | has_char<'}'>(chunk);
      }

      // Skip string using memchr (SIMD-optimized in libc) - returns position after closing quote
      template <opts Opts>
      GLZ_ALWAYS_INLINE const char* skip_string_fast(const char* p, const char* end) noexcept
      {
         const char* const start = p;
         ++p; // skip opening quote

         // Use memchr to find closing quote - typically SIMD-optimized
         while (p < end) {
            const char* quote = static_cast<const char*>(std::memchr(p, '"', static_cast<size_t>(end - p)));
            if (!quote) [[unlikely]] {
               return end; // unclosed string
            }

            // Count preceding backslashes to check if escaped
            size_t backslashes = 0;
            const char* check = quote - 1;
            while (check > start && *check == '\\') {
               ++backslashes;
               --check;
            }

            p = quote + 1;
            if ((backslashes & 1) == 0) {
               return p; // even backslashes = real quote
            }
            // odd backslashes = escaped quote, continue searching
         }
         return p;
      }

      // Skip any JSON value - optimized container skip
      // Returns pointer after the value
      template <opts Opts>
      GLZ_ALWAYS_INLINE const char* skip_value_lazy(const char* p, const char* end) noexcept
      {
         switch (*p) {
         case '"':
            return skip_string_fast<Opts>(p, end);
         case 't':
            return p + 4;
         case 'f':
            return p + 5;
         case 'n':
            return p + 4;
         case '[':
         case '{': {
            int depth = 1;
            ++p;

            while (p < end && depth > 0) {
               // Direct character checks for common cases (avoid SWAR overhead for short spans)
               const char c = *p;

               if (c == '"') {
                  p = skip_string_fast<Opts>(p, end);
                  continue;
               }
               if (c == '{' || c == '[') {
                  ++depth;
                  ++p;
                  continue;
               }
               if (c == '}' || c == ']') {
                  --depth;
                  ++p;
                  continue;
               }

               // For whitespace, comma, colon - just advance
               if (whitespace_table[uint8_t(c)] || c == ',' || c == ':') {
                  ++p;
                  continue;
               }

               // Number - skip it
               if (c == '-' || is_digit(uint8_t(c))) {
                  ++p;
                  if constexpr (Opts.null_terminated) {
                     while (numeric_table[uint8_t(*p)]) ++p;
                  }
                  else {
                     while (p < end && numeric_table[uint8_t(*p)]) ++p;
                  }
                  continue;
               }

               // Unknown char - use SWAR to find next interesting position
               while (p + 8 <= end) {
                  uint64_t chunk;
                  std::memcpy(&chunk, p, 8);
                  if constexpr (std::endian::native == std::endian::big) {
                     chunk = std::byteswap(chunk);
                  }
                  const uint64_t interesting = has_brackets(chunk) | has_quote(chunk);
                  if (interesting == 0) {
                     p += 8;
                     continue;
                  }
                  p += (countr_zero(interesting) >> 3);
                  break;
               }
               if (p < end && p + 8 > end) ++p; // Handle remainder
            }
            return p;
         }
         default:
            // Number
            if constexpr (Opts.null_terminated) {
               while (numeric_table[uint8_t(*p)]) ++p;
            }
            else {
               while (p < end && numeric_table[uint8_t(*p)]) ++p;
            }
            return p;
         }
      }
   } // namespace detail

   // ============================================================================
   // lazy_json - Legacy node storage (kept for API compatibility)
   // ============================================================================

   struct lazy_json
   {
      const char* data_{};
      const char* key_{};
      uint32_t key_length_{};
      uint8_t type_{};  // 0=null, 1=bool, 2=number, 3=string, 4=array, 5=object

      lazy_json() = default;

      [[nodiscard]] bool is_null() const noexcept { return type_ == 0; }
      [[nodiscard]] bool is_boolean() const noexcept { return type_ == 1; }
      [[nodiscard]] bool is_number() const noexcept { return type_ == 2; }
      [[nodiscard]] bool is_string() const noexcept { return type_ == 3; }
      [[nodiscard]] bool is_array() const noexcept { return type_ == 4; }
      [[nodiscard]] bool is_object() const noexcept { return type_ == 5; }

      [[nodiscard]] std::string_view key() const noexcept
      {
         return key_ ? std::string_view{key_, key_length_} : std::string_view{};
      }

      explicit operator bool() const noexcept { return !is_null(); }
   };

   static_assert(sizeof(lazy_json) == 24, "lazy_json should be 24 bytes");

   // Legacy buffer type - not used by truly lazy parser but kept for API compatibility
   using lazy_buffer = std::vector<lazy_json>;

   // ============================================================================
   // lazy_json_view - Truly lazy view with on-demand scanning
   // ============================================================================

   /**
    * @brief A truly lazy view into JSON data.
    *
    * No upfront processing - all navigation uses SWAR-accelerated scanning.
    * This matches simdjson's on-demand approach.
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
   template <opts Opts = opts{}>
   struct lazy_json_view
   {
     private:
      const lazy_document<Opts>* doc_{};
      const char* data_{};
      mutable const char* parse_pos_{};  // Current scan position (advances on key access)
      std::string_view key_{};
      error_code error_{error_code::none};

      lazy_json_view(error_code ec) noexcept : error_(ec) {}

     public:
      lazy_json_view() = default;
      lazy_json_view(const lazy_document<Opts>* doc, const char* data) noexcept
         : doc_(doc), data_(data)
      {}

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

     private:
      friend struct lazy_document<Opts>;
      friend class lazy_iterator<Opts>;

      // Constructor with key for iteration
      lazy_json_view(const lazy_document<Opts>* doc, const char* data, std::string_view key) noexcept
         : doc_(doc), data_(data), key_(key)
      {}

      // Skip whitespace helper
      static void skip_ws(const char*& p, const char* end) noexcept
      {
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*p)]) ++p;
         }
         else {
            while (p < end && whitespace_table[uint8_t(*p)]) ++p;
         }
      }

      // Parse a key from current position using memchr/strchr, return key and advance p
      static std::string_view parse_key(const char*& p, [[maybe_unused]] const char* end) noexcept;
   };

   // ============================================================================
   // lazy_document - Minimal container, no upfront processing
   // ============================================================================

   template <opts Opts = opts{}>
   struct lazy_document
   {
     private:
      const char* json_{};
      size_t len_{};
      const char* root_data_{};
      mutable lazy_json_view<Opts> root_view_{};  // Cached root view with parse_pos_

      friend struct lazy_json_view<Opts>;
      friend class lazy_iterator<Opts>;

      // Factory method for read_lazy
      template <opts O, class Buffer>
      friend expected<lazy_document<O>, error_ctx> read_lazy(Buffer&&, lazy_buffer&);

      // Helper to initialize root_view_ with correct doc_ pointer
      void init_root_view() noexcept
      {
         root_view_ = lazy_json_view<Opts>{this, root_data_};
      }

     public:
      lazy_document() = default;

      // Copy constructor - fix doc_ pointer and preserve parse_pos_
      lazy_document(const lazy_document& other)
         : json_(other.json_), len_(other.len_), root_data_(other.root_data_)
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
            init_root_view();
            root_view_.parse_pos_ = other.root_view_.parse_pos_;
         }
         return *this;
      }

      // Move constructor - fix doc_ pointer and transfer parse_pos_
      lazy_document(lazy_document&& other) noexcept
         : json_(other.json_), len_(other.len_), root_data_(other.root_data_)
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
            init_root_view();
            root_view_.parse_pos_ = other.root_view_.parse_pos_;
         }
         return *this;
      }

      /// @brief Get the root view (cached, enables progressive key scanning)
      [[nodiscard]] lazy_json_view<Opts>& root() noexcept
      {
         return root_view_;
      }

      [[nodiscard]] const lazy_json_view<Opts>& root() const noexcept
      {
         return root_view_;
      }

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

   template <opts Opts>
   class lazy_iterator
   {
     private:
      const lazy_document<Opts>* doc_{};
      const char* pos_{};
      const char* json_end_{};
      char close_char_{};
      bool is_object_{};
      bool at_end_{true};
      std::string_view current_key_{};

     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = lazy_json_view<Opts>;
      using difference_type = std::ptrdiff_t;
      using pointer = void;
      using reference = lazy_json_view<Opts>;

      lazy_iterator() = default;

      lazy_iterator(const lazy_document<Opts>* doc, const char* container_start, const char* end, bool is_object);

      reference operator*() const { return {doc_, pos_, current_key_}; }

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
      void advance_to_next_element();
      void skip_ws() noexcept
      {
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*pos_)]) ++pos_;
         }
         else {
            while (pos_ < json_end_ && whitespace_table[uint8_t(*pos_)]) ++pos_;
         }
      }
   };

   // ============================================================================
   // Implementation: lazy_json_view methods (truly lazy - no structural index)
   // ============================================================================

   template <opts Opts>
   inline const char* lazy_json_view<Opts>::json_end() const noexcept
   {
      return doc_ ? doc_->json_ + doc_->len_ : nullptr;
   }

   template <opts Opts>
   inline std::string_view lazy_json_view<Opts>::parse_key(const char*& p, [[maybe_unused]] const char* end) noexcept
   {
      if (*p != '"') return {};
      const char* const start = p;
      ++p; // skip opening quote
      const char* key_start = p;

      // Use memchr/strchr to find closing quote (SIMD-optimized in libc)
      if constexpr (Opts.null_terminated) {
         while (true) {
            const char* quote = std::strchr(p, '"');
            if (!quote) [[unlikely]] {
               // Unclosed string - return what we have
               std::string_view key{key_start, static_cast<size_t>(p - key_start)};
               return key;
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
      }
      else {
         while (p < end) {
            const char* quote = static_cast<const char*>(std::memchr(p, '"', static_cast<size_t>(end - p)));
            if (!quote) [[unlikely]] {
               // Unclosed string - return what we have
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
   }

   template <opts Opts>
   inline lazy_json_view<Opts> lazy_json_view<Opts>::operator[](size_t index) const
   {
      if (has_error()) return *this;
      if (!is_array()) return make_error(error_code::get_wrong_type);

      const char* end = json_end();
      const char* p = data_ + 1; // skip '['
      skip_ws(p, end);

      // Check for empty array
      if constexpr (Opts.null_terminated) {
         if (*p == ']') return make_error(error_code::exceeded_static_array_size);
      }
      else {
         if (p >= end || *p == ']') return make_error(error_code::exceeded_static_array_size);
      }

      // Skip 'index' elements using lazy scanning
      for (size_t i = 0; i < index; ++i) {
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);

         if constexpr (Opts.null_terminated) {
            if (*p == ']') return make_error(error_code::exceeded_static_array_size);
         }
         else {
            if (p >= end || *p == ']') return make_error(error_code::exceeded_static_array_size);
         }

         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }

      return {doc_, p};
   }

   template <opts Opts>
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
         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
         search_start = p;
      }

      const char* p = search_start;
      skip_ws(p, end);

      // Forward pass: search from current position to end of object
      while (true) {
         // Check for end of object
         if constexpr (Opts.null_terminated) {
            if (*p == '}') break;
         }
         else {
            if (p >= end || *p == '}') break;
         }

         // Parse key
         auto k = parse_key(p, end);
         skip_ws(p, end);

         // Skip ':'
         if (*p == ':') {
            ++p;
            skip_ws(p, end);
         }

         // Check if key matches
         if (k == key) {
            parse_pos_ = p;  // Store value position (lazy - don't skip yet)
            return {doc_, p};
         }

         // Skip value using lazy scanning
         p = detail::skip_value_lazy<Opts>(p, end);
         skip_ws(p, end);

         // Skip comma
         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }

      // Wrap-around pass: search from beginning to where we started
      if (search_start != obj_start) {
         p = obj_start;
         skip_ws(p, end);

         while (p < search_start) {
            // Check for end of object
            if constexpr (Opts.null_terminated) {
               if (*p == '}') break;
            }
            else {
               if (p >= end || *p == '}') break;
            }

            // Parse key
            auto k = parse_key(p, end);
            skip_ws(p, end);

            // Skip ':'
            if (*p == ':') {
               ++p;
               skip_ws(p, end);
            }

            // Check if key matches
            if (k == key) {
               parse_pos_ = p;  // Store value position (lazy - don't skip yet)
               return {doc_, p};
            }

            // Skip value using lazy scanning
            p = detail::skip_value_lazy<Opts>(p, end);
            skip_ws(p, end);

            // Skip comma
            if (*p == ',') {
               ++p;
               skip_ws(p, end);
            }
         }
      }

      return make_error(error_code::key_not_found);
   }

   template <opts Opts>
   inline bool lazy_json_view<Opts>::contains(std::string_view key) const
   {
      auto result = (*this)[key];
      return !result.has_error();
   }

   template <opts Opts>
   inline size_t lazy_json_view<Opts>::size() const
   {
      if (has_error() || !data_) return 0;
      if (!is_array() && !is_object()) return 0;

      const char* end = json_end();
      const char* p = data_ + 1;
      skip_ws(p, end);

      const char close_char = is_array() ? ']' : '}';

      if constexpr (Opts.null_terminated) {
         if (*p == close_char) return 0;
      }
      else {
         if (p >= end || *p == close_char) return 0;
      }

      size_t count = 0;
      const bool is_obj = is_object();

      while (true) {
         if (is_obj) {
            // Skip key
            p = detail::skip_string_fast<Opts>(p, end);
            skip_ws(p, end);
            if (*p == ':') {
               ++p;
               skip_ws(p, end);
            }
         }

         // Skip value
         p = detail::skip_value_lazy<Opts>(p, end);
         ++count;
         skip_ws(p, end);

         if constexpr (Opts.null_terminated) {
            if (*p == close_char) return count;
         }
         else {
            if (p >= end || *p == close_char) return count;
         }

         if (*p == ',') {
            ++p;
            skip_ws(p, end);
         }
      }
   }

   template <opts Opts>
   inline bool lazy_json_view<Opts>::empty() const noexcept
   {
      if (has_error() || !data_) return true;
      if (is_null()) return true;
      if (!is_array() && !is_object()) return false;

      const char* end = json_end();
      const char* p = data_ + 1;
      skip_ws(p, end);

      const char close_char = is_array() ? ']' : '}';
      if constexpr (Opts.null_terminated) {
         return *p == close_char;
      }
      else {
         return p >= end || *p == close_char;
      }
   }

   // ============================================================================
   // lazy_iterator implementation (truly lazy)
   // ============================================================================

   template <opts Opts>
   inline lazy_iterator<Opts>::lazy_iterator(const lazy_document<Opts>* doc, const char* container_start,
                                              const char* end, bool is_object)
      : doc_(doc), json_end_(end), is_object_(is_object), at_end_(false)
   {
      close_char_ = is_object ? '}' : ']';
      pos_ = container_start + 1; // skip '[' or '{'
      skip_ws();

      // Check for empty container
      if constexpr (Opts.null_terminated) {
         if (*pos_ == close_char_) {
            at_end_ = true;
            return;
         }
      }
      else {
         if (pos_ >= json_end_ || *pos_ == close_char_) {
            at_end_ = true;
            return;
         }
      }

      // Position at first element
      advance_to_next_element();
   }

   template <opts Opts>
   inline void lazy_iterator<Opts>::advance_to_next_element()
   {
      if (is_object_) {
         // Parse key
         current_key_ = lazy_json_view<Opts>::parse_key(pos_, json_end_);
         skip_ws();

         // Skip ':'
         if (*pos_ == ':') {
            ++pos_;
            skip_ws();
         }
      }
   }

   template <opts Opts>
   inline lazy_iterator<Opts>& lazy_iterator<Opts>::operator++()
   {
      if (at_end_) return *this;

      // Skip current value using lazy scanning
      pos_ = detail::skip_value_lazy<Opts>(pos_, json_end_);
      skip_ws();

      // Check for end
      if constexpr (Opts.null_terminated) {
         if (*pos_ == close_char_) {
            at_end_ = true;
            return *this;
         }
      }
      else {
         if (pos_ >= json_end_ || *pos_ == close_char_) {
            at_end_ = true;
            return *this;
         }
      }

      // Skip comma
      if (*pos_ == ',') {
         ++pos_;
         skip_ws();
      }

      // Advance to next element
      advance_to_next_element();

      return *this;
   }

   template <opts Opts>
   inline lazy_iterator<Opts> lazy_json_view<Opts>::begin() const
   {
      if (has_error() || !data_) return end();
      if (!is_array() && !is_object()) return end();
      return lazy_iterator<Opts>{doc_, data_, json_end(), is_object()};
   }

   template <opts Opts>
   inline lazy_iterator<Opts> lazy_json_view<Opts>::end() const
   {
      return lazy_iterator<Opts>{};
   }

   // ============================================================================
   // lazy_json_view::get<T>() implementation
   // ============================================================================

   template <opts Opts>
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
         skip_value<JSON>::op<opts{}>(ctx, it, end);
         if (bool(ctx.error)) {
            return unexpected(error_ctx{0, ctx.error});
         }
         std::string_view raw{data_, static_cast<size_t>(it - data_)};
         return glz::read_json<std::string>(raw);
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

   template <opts Opts>
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

         auto it = view.data();
         context parse_ctx{};
         skip_value<JSON>::op<opts{}>(parse_ctx, it, view.json_end());
         if (bool(parse_ctx.error)) [[unlikely]] {
            ctx.error = parse_ctx.error;
            return;
         }

         const size_t n = static_cast<size_t>(it - view.data());
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
   // read_lazy - Main entry point (truly lazy - minimal upfront work)
   // ============================================================================

   /**
    * @brief Parse JSON lazily - no upfront processing.
    *
    * This is a truly lazy parser like simdjson on-demand:
    * - No structural index built upfront
    * - Just validates first byte and stores buffer reference
    * - All work happens on-demand when accessing fields
    *
    * @tparam Opts Options for parsing
    * @param buffer The JSON text buffer (must remain valid for document lifetime)
    * @return lazy_document on success, error_ctx on parse failure
    *
    * NOTE: The lazy_buffer parameter is kept for API compatibility but not used.
    */
   template <opts Opts = opts{}, class Buffer>
   [[nodiscard]] inline expected<lazy_document<Opts>, error_ctx> read_lazy(Buffer&& buffer, lazy_buffer&)
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
      if (c != '{' && c != '[' && c != '"' && c != 't' && c != 'f' && c != 'n' &&
          !is_digit(uint8_t(c)) && c != '-') {
         return unexpected(error_ctx{0, error_code::syntax_error});
      }

      doc.root_data_ = p;
      doc.init_root_view();  // Initialize cached root view
      return doc;
   }

} // namespace glz
