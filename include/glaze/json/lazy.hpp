// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>
#include <vector>

#include "glaze/json/read.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/expected.hpp"

namespace glz
{
   // JSON value type for lazy parsing
   enum class lazy_type : uint8_t {
      null_t,
      boolean_t,
      number_t,
      string_t,
      array_t,
      object_t
   };

   // Forward declarations
   struct lazy_json;
   template <opts Opts>
   struct lazy_json_view;
   template <opts Opts>
   struct lazy_document;

   // Sentinel value indicating children not yet parsed
   inline constexpr uint32_t lazy_unparsed = UINT32_MAX;

   /**
    * @brief Internal lazy JSON node that stores value data and key (for object members).
    *
    * This is an internal type - use lazy_json_view for access.
    * Nodes are stored contiguously in a lazy_buffer vector.
    * Children are referenced by index into the buffer.
    *
    * Memory layout (32 bytes on 64-bit):
    * - data_: pointer to start of JSON value (8 bytes)
    * - key_: pointer to key string content, null if not an object member (8 bytes)
    * - children_start_: index of first child in buffer (4 bytes) [mutable]
    * - children_count_: number of children (4 bytes) [mutable]
    * - key_length_: length of key string (4 bytes)
    * - type: lazy_type (1 byte)
    * - padding: 3 bytes
    */
   struct lazy_json
   {
      const char* data_{};                             // 8 bytes - pointer to start of value
      const char* key_{};                              // 8 bytes - pointer to key content (inside quotes)
      mutable uint32_t children_start_{lazy_unparsed}; // 4 bytes - index into buffer
      mutable uint32_t children_count_{};              // 4 bytes
      uint32_t key_length_{};                          // 4 bytes - length of key
      lazy_type type{lazy_type::null_t};               // 1 byte
      // 3 bytes padding
      // Total: 32 bytes

      // Default constructor creates null
      lazy_json() = default;

      // Type checking methods
      [[nodiscard]] bool is_null() const noexcept { return type == lazy_type::null_t; }
      [[nodiscard]] bool is_boolean() const noexcept { return type == lazy_type::boolean_t; }
      [[nodiscard]] bool is_number() const noexcept { return type == lazy_type::number_t; }
      [[nodiscard]] bool is_string() const noexcept { return type == lazy_type::string_t; }
      [[nodiscard]] bool is_array() const noexcept { return type == lazy_type::array_t; }
      [[nodiscard]] bool is_object() const noexcept { return type == lazy_type::object_t; }

      // Get the key as string_view (empty if not an object member)
      [[nodiscard]] std::string_view key() const noexcept
      {
         return key_ ? std::string_view{key_, key_length_} : std::string_view{};
      }

      // Explicit bool conversion - true if not null
      explicit operator bool() const noexcept { return !is_null(); }
   };

   static_assert(sizeof(lazy_json) == 32, "lazy_json should be 32 bytes");

   /**
    * @brief User-managed buffer for lazy JSON nodes.
    *
    * All nodes from a lazy parse are stored contiguously in this buffer.
    * The buffer can be reused across multiple parse operations by calling clear().
    */
   using lazy_buffer = std::vector<lazy_json>;

   // ============================================================================
   // lazy_json_view - The public API for accessing lazy JSON values
   // ============================================================================

   /**
    * @brief A view into a lazy JSON value that provides access through indices.
    *
    * This is the primary interface for accessing lazy JSON data.
    * All accessor methods return new views, maintaining document context.
    * Errors are propagated through chained access - if any step fails, subsequent
    * operations will also return error views.
    *
    * IMPORTANT: The underlying buffer and JSON text must remain valid for the
    * lifetime of the view.
    *
    * NOTE: Lazy parsing is NOT thread-safe. Concurrent access to the same
    * lazy_json nodes from multiple threads may cause data races.
    *
    * @tparam Opts Parsing options (primarily null_terminated).
    *
    * Memory layout (16 bytes on 64-bit):
    * - doc_: pointer to document (8 bytes)
    * - index_: node index in buffer (4 bytes)
    * - error_: error code (4 bytes)
    */
   template <opts Opts = opts{}>
   struct lazy_json_view
   {
     private:
      const lazy_document<Opts>* doc_{};
      uint32_t index_{};
      error_code error_{error_code::none};

      // Private constructor for creating error views
      lazy_json_view(error_code ec) noexcept : error_(ec) {}

      // Get the node pointer (null if error or invalid)
      [[nodiscard]] const lazy_json* node() const noexcept;

      // Ensure children are parsed, returns error code
      [[nodiscard]] error_code ensure_children_parsed() const;

     public:
      lazy_json_view() = default;
      lazy_json_view(const lazy_document<Opts>* doc, uint32_t index) noexcept : doc_(doc), index_(index) {}

      // Create an error view
      [[nodiscard]] static lazy_json_view make_error(error_code ec) noexcept { return lazy_json_view{ec}; }

      // Error checking
      [[nodiscard]] bool has_error() const noexcept { return error_ != error_code::none; }
      [[nodiscard]] error_code error() const noexcept { return error_; }

      // Type checking methods
      [[nodiscard]] bool is_null() const noexcept
      {
         auto* n = node();
         return has_error() || !n || n->is_null();
      }
      [[nodiscard]] bool is_boolean() const noexcept
      {
         auto* n = node();
         return !has_error() && n && n->is_boolean();
      }
      [[nodiscard]] bool is_number() const noexcept
      {
         auto* n = node();
         return !has_error() && n && n->is_number();
      }
      [[nodiscard]] bool is_string() const noexcept
      {
         auto* n = node();
         return !has_error() && n && n->is_string();
      }
      [[nodiscard]] bool is_array() const noexcept
      {
         auto* n = node();
         return !has_error() && n && n->is_array();
      }
      [[nodiscard]] bool is_object() const noexcept
      {
         auto* n = node();
         return !has_error() && n && n->is_object();
      }

      // Explicit bool conversion - true if valid, not null, and no error
      explicit operator bool() const noexcept
      {
         auto* n = node();
         return !has_error() && n && !n->is_null();
      }

      // Get the start pointer
      [[nodiscard]] const char* data() const noexcept
      {
         auto* n = node();
         return n ? n->data_ : nullptr;
      }

      // Get the end pointer (shared buffer end)
      [[nodiscard]] const char* end() const noexcept;

      /**
       * @brief Get the value as the specified type.
       * Returns error if view has error, type mismatch, or parse failure.
       */
      template <class T>
      [[nodiscard]] expected<T, error_ctx> get() const;

      /**
       * @brief Array index access. Returns error view on type mismatch, out of bounds, or parse error.
       */
      [[nodiscard]] lazy_json_view operator[](size_t index) const;

      /**
       * @brief Object key access. Returns error view on type mismatch, key not found, or parse error.
       */
      [[nodiscard]] lazy_json_view operator[](std::string_view key) const;

      /**
       * @brief Check if object contains a key. Returns false on error.
       */
      [[nodiscard]] bool contains(std::string_view key) const;

      /**
       * @brief Get the size (number of elements in array/object). Returns 0 on error.
       */
      [[nodiscard]] size_t size() const;

      /**
       * @brief Check if empty. Returns true on error.
       */
      [[nodiscard]] bool empty() const noexcept;
   };

   static_assert(sizeof(lazy_json_view<opts{}>) == 16, "lazy_json_view should be 16 bytes");
   static_assert(sizeof(lazy_json_view<opts{.null_terminated = false}>) == 16, "lazy_json_view should be 16 bytes");

   // ============================================================================
   // lazy_document - Container returned by read_lazy()
   // ============================================================================

   /**
    * @brief A lazy JSON document that references nodes in a user-managed buffer.
    *
    * Returned by read_lazy(). Provides view access to the parsed JSON.
    *
    * IMPORTANT: Both the underlying JSON text buffer AND the lazy_buffer
    * must remain valid for the lifetime of the document and any views
    * derived from it.
    *
    * @tparam Opts Parsing options (primarily null_terminated).
    */
   template <opts Opts = opts{}>
   struct lazy_document
   {
     private:
      lazy_buffer* buffer_{};
      uint32_t root_index_{};
      const char* end_{};

      friend struct lazy_json_view<Opts>;

     public:
      lazy_document() = default;
      lazy_document(lazy_buffer* buffer, uint32_t root_index, const char* end) noexcept
         : buffer_(buffer), root_index_(root_index), end_(end)
      {}

      // Move-only
      lazy_document(lazy_document&&) = default;
      lazy_document& operator=(lazy_document&&) = default;
      lazy_document(const lazy_document&) = delete;
      lazy_document& operator=(const lazy_document&) = delete;

      // Get a view to the root
      [[nodiscard]] lazy_json_view<Opts> root() const noexcept { return {this, root_index_}; }

      // Convenience: forward to root view
      [[nodiscard]] lazy_json_view<Opts> operator[](std::string_view key) const { return root()[key]; }
      [[nodiscard]] lazy_json_view<Opts> operator[](size_t index) const { return root()[index]; }

      // Type checking on root
      [[nodiscard]] bool is_null() const noexcept
      {
         return !buffer_ || root_index_ >= (*buffer_).size() || (*buffer_)[root_index_].is_null();
      }
      [[nodiscard]] bool is_array() const noexcept
      {
         return buffer_ && root_index_ < (*buffer_).size() && (*buffer_)[root_index_].is_array();
      }
      [[nodiscard]] bool is_object() const noexcept
      {
         return buffer_ && root_index_ < (*buffer_).size() && (*buffer_)[root_index_].is_object();
      }

      explicit operator bool() const noexcept { return !is_null(); }
   };

   // ============================================================================
   // lazy_json_view method implementations
   // ============================================================================

   template <opts Opts>
   inline const lazy_json* lazy_json_view<Opts>::node() const noexcept
   {
      if (!doc_ || !doc_->buffer_ || index_ >= (*doc_->buffer_).size()) {
         return nullptr;
      }
      return &(*doc_->buffer_)[index_];
   }

   template <opts Opts>
   inline const char* lazy_json_view<Opts>::end() const noexcept
   {
      return doc_ ? doc_->end_ : nullptr;
   }

   template <opts Opts>
   inline error_code lazy_json_view<Opts>::ensure_children_parsed() const
   {
      auto* n = node();
      if (!n) return error_code::syntax_error;
      if (n->children_start_ != lazy_unparsed) return error_code::none; // Already parsed

      const char* json_end = end();
      if (!json_end) return error_code::syntax_error;

      auto& buffer = *doc_->buffer_;
      const uint32_t parent_index = index_; // Save index since node pointer may be invalidated
      context ctx{};

      if (n->is_object()) {
         auto it = n->data_ + 1; // skip '{'

         // Track whitespace patterns for skip_matching_ws optimization
         auto ws_start = it;
         uint64_t ws_size{};

         auto skip_expected_ws = [&] {
            auto new_ws_start = it;
            if constexpr (Opts.null_terminated) {
               if (ws_size) [[likely]] {
                  skip_matching_ws(ws_start, it, ws_size);
               }
               while (whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            else {
               if (ws_size && ws_size < size_t(json_end - it)) [[likely]] {
                  skip_matching_ws(ws_start, it, ws_size);
               }
               while (it < json_end && whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            ws_start = new_ws_start;
            ws_size = size_t(it - new_ws_start);
         };

         // Initial whitespace skip
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*it)]) ++it;
         }
         else {
            while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
         }

         // Mark children_start now, before we add any children
         // Note: Access via index since pointer may be invalidated by push_back
         buffer[parent_index].children_start_ = static_cast<uint32_t>(buffer.size());
         buffer[parent_index].children_count_ = 0;

         if constexpr (Opts.null_terminated) {
            if (*it == '}') {
               return error_code::none;
            }
         }
         else {
            if (it >= json_end || *it == '}') {
               return error_code::none;
            }
         }

         while (true) {
            if (*it != '"') [[unlikely]] {
               return error_code::expected_quote;
            }

            ++it;
            auto key_start = it;
            skip_string_view(ctx, it, json_end);
            if (bool(ctx.error)) [[unlikely]] {
               return ctx.error;
            }
            size_t key_len = static_cast<size_t>(it - key_start);
            if (key_len > (std::numeric_limits<uint32_t>::max)()) [[unlikely]] {
               return error_code::parse_error;
            }
            ++it;

            // Skip whitespace after key
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) ++it;
               if (*it != ':') [[unlikely]] {
                  return error_code::expected_colon;
               }
            }
            else {
               while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
               if (it >= json_end || *it != ':') [[unlikely]] {
                  return error_code::expected_colon;
               }
            }
            ++it;

            // Skip whitespace after colon
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) ++it;
            }
            else {
               while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
            }

            // Create child node
            lazy_json child;
            child.key_ = key_start;
            child.key_length_ = static_cast<uint32_t>(key_len);
            child.data_ = it;

            // Determine child type and skip value
            switch (*it) {
            case '{':
               child.type = lazy_type::object_t;
               skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, json_end);
               break;
            case '[':
               child.type = lazy_type::array_t;
               skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, json_end);
               break;
            case '"':
               child.type = lazy_type::string_t;
               ++it;
               skip_string_view(ctx, it, json_end);
               if (bool(ctx.error)) [[unlikely]]
                  return ctx.error;
               ++it;
               break;
            case 't':
               child.type = lazy_type::boolean_t;
               ++it;
               match<"rue", Opts>(ctx, it, json_end);
               break;
            case 'f':
               child.type = lazy_type::boolean_t;
               ++it;
               match<"alse", Opts>(ctx, it, json_end);
               break;
            case 'n':
               child.type = lazy_type::null_t;
               ++it;
               match<"ull", Opts>(ctx, it, json_end);
               break;
            default:
               if (is_digit(uint8_t(*it)) || *it == '-') {
                  child.type = lazy_type::number_t;
                  skip_number<Opts>(ctx, it, json_end);
               }
               else {
                  return error_code::syntax_error;
               }
               break;
            }

            if (bool(ctx.error)) [[unlikely]] {
               return ctx.error;
            }

            buffer.push_back(child);
            ++buffer[parent_index].children_count_;

            // Skip whitespace after value
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) ++it;
               if (*it == '}') break;
            }
            else {
               while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
               if (it >= json_end || *it == '}') break;
            }
            if (*it != ',') [[unlikely]] {
               return error_code::expected_comma;
            }
            ++it;
            skip_expected_ws();
         }

         return error_code::none;
      }
      else if (n->is_array()) {
         auto it = n->data_ + 1; // skip '['

         auto ws_start = it;
         uint64_t ws_size{};

         auto skip_expected_ws = [&] {
            auto new_ws_start = it;
            if constexpr (Opts.null_terminated) {
               if (ws_size) [[likely]] {
                  skip_matching_ws(ws_start, it, ws_size);
               }
               while (whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            else {
               if (ws_size && ws_size < size_t(json_end - it)) [[likely]] {
                  skip_matching_ws(ws_start, it, ws_size);
               }
               while (it < json_end && whitespace_table[uint8_t(*it)]) {
                  ++it;
               }
            }
            ws_start = new_ws_start;
            ws_size = size_t(it - new_ws_start);
         };

         // Initial whitespace skip
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*it)]) ++it;
         }
         else {
            while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
         }

         // Mark children_start now
         buffer[parent_index].children_start_ = static_cast<uint32_t>(buffer.size());
         buffer[parent_index].children_count_ = 0;

         if constexpr (Opts.null_terminated) {
            if (*it == ']') {
               return error_code::none;
            }
         }
         else {
            if (it >= json_end || *it == ']') {
               return error_code::none;
            }
         }

         while (true) {
            // Create child node (no key for array elements)
            lazy_json child;
            child.data_ = it;

            // Determine child type and skip value
            switch (*it) {
            case '{':
               child.type = lazy_type::object_t;
               skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, json_end);
               break;
            case '[':
               child.type = lazy_type::array_t;
               skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, json_end);
               break;
            case '"':
               child.type = lazy_type::string_t;
               ++it;
               skip_string_view(ctx, it, json_end);
               if (bool(ctx.error)) [[unlikely]]
                  return ctx.error;
               ++it;
               break;
            case 't':
               child.type = lazy_type::boolean_t;
               ++it;
               match<"rue", Opts>(ctx, it, json_end);
               break;
            case 'f':
               child.type = lazy_type::boolean_t;
               ++it;
               match<"alse", Opts>(ctx, it, json_end);
               break;
            case 'n':
               child.type = lazy_type::null_t;
               ++it;
               match<"ull", Opts>(ctx, it, json_end);
               break;
            default:
               if (is_digit(uint8_t(*it)) || *it == '-') {
                  child.type = lazy_type::number_t;
                  skip_number<Opts>(ctx, it, json_end);
               }
               else {
                  return error_code::syntax_error;
               }
               break;
            }

            if (bool(ctx.error)) [[unlikely]] {
               return ctx.error;
            }

            buffer.push_back(child);
            ++buffer[parent_index].children_count_;

            // Skip whitespace after value
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[uint8_t(*it)]) ++it;
               if (*it == ']') break;
            }
            else {
               while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
               if (it >= json_end || *it == ']') break;
            }
            if (*it != ',') [[unlikely]] {
               return error_code::expected_comma;
            }
            ++it;
            skip_expected_ws();
         }

         return error_code::none;
      }

      return error_code::syntax_error;
   }

   template <opts Opts>
   inline lazy_json_view<Opts> lazy_json_view<Opts>::operator[](size_t index) const
   {
      if (has_error()) {
         return *this; // Propagate existing error
      }
      if (!is_array()) {
         return make_error(error_code::get_wrong_type);
      }
      auto ec = ensure_children_parsed();
      if (ec != error_code::none) {
         return make_error(ec);
      }
      auto* n = node();
      if (index >= n->children_count_) {
         return make_error(error_code::exceeded_static_array_size);
      }
      return {doc_, n->children_start_ + static_cast<uint32_t>(index)};
   }

   template <opts Opts>
   inline lazy_json_view<Opts> lazy_json_view<Opts>::operator[](std::string_view key) const
   {
      if (has_error()) {
         return *this; // Propagate existing error
      }
      if (!is_object()) {
         return make_error(error_code::get_wrong_type);
      }
      auto ec = ensure_children_parsed();
      if (ec != error_code::none) {
         return make_error(ec);
      }
      auto* n = node();
      const auto& nodes = (*doc_->buffer_);
      for (uint32_t i = 0; i < n->children_count_; ++i) {
         const auto& child = nodes[n->children_start_ + i];
         if (child.key() == key) {
            return {doc_, n->children_start_ + i};
         }
      }
      return make_error(error_code::key_not_found);
   }

   template <opts Opts>
   inline bool lazy_json_view<Opts>::contains(std::string_view key) const
   {
      if (has_error() || !is_object()) return false;
      auto ec = ensure_children_parsed();
      if (ec != error_code::none) return false;
      auto* n = node();
      const auto& nodes = (*doc_->buffer_);
      for (uint32_t i = 0; i < n->children_count_; ++i) {
         if (nodes[n->children_start_ + i].key() == key) {
            return true;
         }
      }
      return false;
   }

   template <opts Opts>
   inline size_t lazy_json_view<Opts>::size() const
   {
      auto* n = node();
      if (has_error() || !n || n->is_null()) return 0;
      if (n->is_object() || n->is_array()) {
         auto ec = ensure_children_parsed();
         if (ec != error_code::none) return 0;
         // Re-fetch node since ensure_children_parsed may have invalidated pointer
         return (*doc_->buffer_)[index_].children_count_;
      }
      return 0;
   }

   template <opts Opts>
   inline bool lazy_json_view<Opts>::empty() const noexcept
   {
      auto* n = node();
      if (has_error() || !n || n->is_null()) return true;
      if (n->is_array()) {
         if (n->children_start_ != lazy_unparsed) return n->children_count_ == 0;
         // Not yet parsed - check for empty array "[]"
         auto it = n->data_ + 1;
         const char* json_end = end();
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*it)]) ++it;
            return *it == ']';
         }
         else {
            while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
            return it < json_end && *it == ']';
         }
      }
      if (n->is_object()) {
         if (n->children_start_ != lazy_unparsed) return n->children_count_ == 0;
         // Not yet parsed - check for empty object "{}"
         auto it = n->data_ + 1;
         const char* json_end = end();
         if constexpr (Opts.null_terminated) {
            while (whitespace_table[uint8_t(*it)]) ++it;
            return *it == '}';
         }
         else {
            while (it < json_end && whitespace_table[uint8_t(*it)]) ++it;
            return it < json_end && *it == '}';
         }
      }
      return false;
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

      if constexpr (std::is_same_v<T, bool>) {
         if (!is_boolean()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         return *node()->data_ == 't';
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
         // Parse the string to find its end and unescape
         auto* n = node();
         auto it = n->data_;
         const char* json_end = end();
         context ctx{};
         skip_value<JSON>::op<opts{}>(ctx, it, json_end);
         if (bool(ctx.error)) {
            return unexpected(error_ctx{0, ctx.error});
         }
         std::string_view raw{n->data_, static_cast<size_t>(it - n->data_)};
         return glz::read_json<std::string>(raw);
      }
      else if constexpr (std::is_same_v<T, std::string_view>) {
         if (!is_string()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         // Find the end of the string (skip opening quote, find closing quote)
         auto* n = node();
         auto it = n->data_ + 1; // skip opening quote
         const char* json_end = end();
         context ctx{};
         skip_string_view(ctx, it, json_end);
         if (bool(ctx.error)) {
            return unexpected(error_ctx{0, ctx.error});
         }
         // Return content without quotes (escapes not processed)
         return std::string_view{n->data_ + 1, static_cast<size_t>(it - n->data_ - 1)};
      }
      else if constexpr (std::is_same_v<T, double>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto* n = node();
         double value{};
         auto [ptr, ec] = glz::from_chars<false>(n->data_, end(), value);
         if (ec != std::errc()) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, float>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto* n = node();
         float value{};
         auto [ptr, ec] = glz::from_chars<false>(n->data_, end(), value);
         if (ec != std::errc()) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, int64_t>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto* n = node();
         int64_t value{};
         auto it = n->data_;
         if (!glz::atoi(value, it, end())) {
            return unexpected(error_ctx{0, error_code::parse_number_failure});
         }
         return value;
      }
      else if constexpr (std::is_same_v<T, uint64_t>) {
         if (!is_number()) {
            return unexpected(error_ctx{0, error_code::get_wrong_type});
         }
         auto* n = node();
         uint64_t value{};
         auto it = n->data_;
         if (!glz::atoi(value, it, end())) {
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
   // Meta and JSON reader/writer specializations
   // ============================================================================

   template <>
   struct meta<lazy_json>
   {
      static constexpr std::string_view name = "glz::lazy_json";
   };

   template <opts Opts>
   struct meta<lazy_json_view<Opts>>
   {
      static constexpr std::string_view name = "glz::lazy_json_view";
   };

   // JSON writer for lazy_json_view
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

         // Find the end of this value by parsing
         auto it = view.data();
         context parse_ctx{};
         skip_value<JSON>::op<opts{}>(parse_ctx, it, view.end());
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
   // Helper function to read lazy JSON
   // ============================================================================

   namespace detail
   {
      // Parse the root node and add it to the buffer
      template <opts Opts>
      GLZ_ALWAYS_INLINE error_code parse_lazy_root(lazy_json& root, is_context auto&& ctx, auto&& it, auto end)
      {
         if (skip_ws<Opts>(ctx, it, end)) {
            return ctx.error;
         }

         root.data_ = it;

         switch (*it) {
         case '{': {
            root.type = lazy_type::object_t;
            skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, end);
            break;
         }
         case '[': {
            root.type = lazy_type::array_t;
            skip_value<JSON>::op<ws_handled_off<Opts>()>(ctx, it, end);
            break;
         }
         case '"': {
            root.type = lazy_type::string_t;
            ++it;
            skip_string_view(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return ctx.error;
            ++it;
            break;
         }
         case 't': {
            root.type = lazy_type::boolean_t;
            ++it;
            match<"rue", Opts>(ctx, it, end);
            break;
         }
         case 'f': {
            root.type = lazy_type::boolean_t;
            ++it;
            match<"alse", Opts>(ctx, it, end);
            break;
         }
         case 'n': {
            root.type = lazy_type::null_t;
            ++it;
            match<"ull", Opts>(ctx, it, end);
            break;
         }
         default: {
            if (is_digit(uint8_t(*it)) || *it == '-') {
               root.type = lazy_type::number_t;
               skip_number<Opts>(ctx, it, end);
            }
            else {
               ctx.error = error_code::syntax_error;
            }
            break;
         }
         }

         return ctx.error;
      }
   } // namespace detail

   /**
    * @brief Parse JSON lazily into a user-managed buffer.
    *
    * @tparam Opts Options for parsing (default: opts{}). Set null_terminated = false
    *              for non-null-terminated buffers like string_view slices.
    * @param buffer The JSON text buffer (must remain valid for document lifetime)
    * @param node_buffer User-managed buffer for storing parsed nodes
    * @return lazy_document on success, error_ctx on parse failure
    *
    * Usage:
    * @code
    * glz::lazy_buffer nodes;
    * std::string json = R"({"name": "Alice", "age": 30})";
    * auto result = glz::read_lazy(json, nodes);
    * if (result) {
    *    auto name = (*result)["name"].get<std::string>();
    * }
    * @endcode
    */
   template <opts Opts = opts{}, class Buffer>
   [[nodiscard]] inline expected<lazy_document<Opts>, error_ctx> read_lazy(Buffer&& buffer, lazy_buffer& node_buffer)
   {
      // Ensure buffer has padding for SIMD operations
      if constexpr (resizable<std::decay_t<Buffer>>) {
         if (buffer.size() < 8) {
            buffer.resize(8);
         }
      }

      // Record where root will be placed
      uint32_t root_index = static_cast<uint32_t>(node_buffer.size());

      // Add root node placeholder
      lazy_json& root = node_buffer.emplace_back();

      context ctx{};
      const char* it = buffer.data();
      const char* buffer_end = buffer.data() + buffer.size();

      auto ec = detail::parse_lazy_root<Opts>(root, ctx, it, buffer_end);
      if (ec != error_code::none) {
         // Remove the placeholder on error
         node_buffer.pop_back();
         return unexpected(error_ctx{0, ec});
      }

      return lazy_document<Opts>{&node_buffer, root_index, buffer_end};
   }

} // namespace glz
