// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>
#include <variant>
#include <vector>

#include "glaze/containers/flat_map.hpp"
#include "glaze/json/read.hpp"
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

   // Forward declaration
   struct lazy_json;

   /**
    * @brief A lazy JSON type that stores string_views to raw JSON data.
    *
    * Unlike glz::generic which eagerly parses all values, glz::lazy only
    * stores references to the original JSON buffer. Values are parsed
    * only when accessed via get<T>().
    *
    * IMPORTANT: The underlying buffer must remain valid for the lifetime
    * of the lazy_json object and any operations on it.
    */
   struct lazy_json
   {
      // The raw JSON representation (for primitives: the value text, for containers: the full JSON)
      std::string_view raw{};

      // Type of this JSON value
      lazy_type type = lazy_type::null_t;

      // Children for arrays and objects
      // For objects: stores key-value pairs with string_view keys (raw, may contain escapes)
      // Keys are stored WITHOUT quotes but escapes are NOT processed (truly lazy)
      // For keys with escape sequences, lookup requires matching the raw escaped form
      // Uses flat_map for O(log n) lookup; keys are sorted (original order not preserved)
      using array_t = std::vector<lazy_json>;
      using object_t = glz::flat_map<std::string_view, lazy_json>;

      std::variant<std::monostate, array_t, object_t> children{};

      // Default constructor creates null
      lazy_json() = default;

      // Type checking methods
      [[nodiscard]] bool is_null() const noexcept { return type == lazy_type::null_t; }
      [[nodiscard]] bool is_boolean() const noexcept { return type == lazy_type::boolean_t; }
      [[nodiscard]] bool is_number() const noexcept { return type == lazy_type::number_t; }
      [[nodiscard]] bool is_string() const noexcept { return type == lazy_type::string_t; }
      [[nodiscard]] bool is_array() const noexcept { return type == lazy_type::array_t; }
      [[nodiscard]] bool is_object() const noexcept { return type == lazy_type::object_t; }

      /**
       * @brief Get the value as the specified type.
       * @tparam T The type to convert to.
       * @return An expected containing the value or an error.
       *
       * Supported types:
       * - bool: for boolean values
       * - Numeric types (int, double, etc.): for number values
       * - std::string, std::string_view: for string values
       * - lazy_json::array_t: for array values (returns reference)
       * - lazy_json::object_t: for object values (returns reference)
       */
      template <class T>
      [[nodiscard]] expected<T, error_ctx> get() const;

      /**
       * @brief Get a reference to the underlying array.
       * @throws glaze_error if this is not an array.
       */
      [[nodiscard]] array_t& get_array()
      {
         if (!is_array()) {
            glaze_error("Expected array");
         }
         return std::get<array_t>(children);
      }

      [[nodiscard]] const array_t& get_array() const
      {
         if (!is_array()) {
            glaze_error("Expected array");
         }
         return std::get<array_t>(children);
      }

      /**
       * @brief Get a reference to the underlying object.
       * @throws glaze_error if this is not an object.
       */
      [[nodiscard]] object_t& get_object()
      {
         if (!is_object()) {
            glaze_error("Expected object");
         }
         return std::get<object_t>(children);
      }

      [[nodiscard]] const object_t& get_object() const
      {
         if (!is_object()) {
            glaze_error("Expected object");
         }
         return std::get<object_t>(children);
      }

      /**
       * @brief Array index access.
       */
      [[nodiscard]] lazy_json& operator[](size_t index) { return get_array()[index]; }

      [[nodiscard]] const lazy_json& operator[](size_t index) const { return get_array()[index]; }

      /**
       * @brief Object key access (O(log n) binary search).
       * @note Keys are stored raw (with escapes not processed). For keys with
       * escape sequences, use the raw form: e.g., for JSON key "hello\nworld",
       * search with "hello\\nworld" or R"(hello\nworld)".
       */
      [[nodiscard]] lazy_json& operator[](std::string_view key)
      {
         if (!is_object()) {
            glaze_error("Expected object");
         }
         auto& obj = std::get<object_t>(children);
         auto it = obj.find(key);
         if (it == obj.end()) {
            glaze_error("Key not found");
         }
         return it->second;
      }

      [[nodiscard]] const lazy_json& operator[](std::string_view key) const
      {
         if (!is_object()) {
            glaze_error("Expected object");
         }
         const auto& obj = std::get<object_t>(children);
         auto it = obj.find(key);
         if (it == obj.end()) {
            glaze_error("Key not found");
         }
         return it->second;
      }

      /**
       * @brief Check if object contains a key (O(log n) binary search).
       * @note Keys are stored raw. See operator[] for details on escaped keys.
       */
      [[nodiscard]] bool contains(std::string_view key) const
      {
         if (!is_object()) return false;
         const auto& obj = std::get<object_t>(children);
         return obj.contains(key);
      }

      /**
       * @brief Get the size (number of elements in array/object, or 0 for primitives).
       */
      [[nodiscard]] size_t size() const noexcept
      {
         if (is_array()) {
            return std::get<array_t>(children).size();
         }
         else if (is_object()) {
            return std::get<object_t>(children).size();
         }
         return 0;
      }

      /**
       * @brief Check if empty (null, empty array, or empty object).
       */
      [[nodiscard]] bool empty() const noexcept
      {
         if (is_null()) return true;
         if (is_array()) return std::get<array_t>(children).empty();
         if (is_object()) return std::get<object_t>(children).empty();
         return false;
      }

      /**
       * @brief Explicit bool conversion - true if not null.
       */
      explicit operator bool() const noexcept { return !is_null(); }

      /**
       * @brief Dump the raw JSON representation.
       */
      [[nodiscard]] std::string_view dump() const noexcept { return raw; }
   };

   // Template specializations for get<T>()

   template <>
   [[nodiscard]] inline expected<bool, error_ctx> lazy_json::get<bool>() const
   {
      if (!is_boolean()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      return raw == "true";
   }

   template <>
   [[nodiscard]] inline expected<std::nullptr_t, error_ctx> lazy_json::get<std::nullptr_t>() const
   {
      if (!is_null()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      return nullptr;
   }

   template <>
   [[nodiscard]] inline expected<std::string, error_ctx> lazy_json::get<std::string>() const
   {
      if (!is_string()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      // raw contains the full JSON string with quotes, use existing parser to unescape
      return glz::read_json<std::string>(raw);
   }

   template <>
   [[nodiscard]] inline expected<std::string_view, error_ctx> lazy_json::get<std::string_view>() const
   {
      if (!is_string()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      // raw contains the JSON string with quotes
      // Return the content without quotes (but escapes are not processed)
      // WARNING: This returns raw content which may contain escape sequences
      // For strings with escapes, use get<std::string>() instead
      if (raw.size() >= 2) {
         return raw.substr(1, raw.size() - 2);
      }
      return raw;
   }

   template <>
   [[nodiscard]] inline expected<double, error_ctx> lazy_json::get<double>() const
   {
      if (!is_number()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      double value{};
      auto [ptr, ec] = glz::from_chars<false>(raw.data(), raw.data() + raw.size(), value);
      if (ec != std::errc()) {
         return unexpected(error_ctx{0, error_code::parse_number_failure});
      }
      return value;
   }

   template <>
   [[nodiscard]] inline expected<float, error_ctx> lazy_json::get<float>() const
   {
      if (!is_number()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      float value{};
      auto [ptr, ec] = glz::from_chars<false>(raw.data(), raw.data() + raw.size(), value);
      if (ec != std::errc()) {
         return unexpected(error_ctx{0, error_code::parse_number_failure});
      }
      return value;
   }

   template <>
   [[nodiscard]] inline expected<int64_t, error_ctx> lazy_json::get<int64_t>() const
   {
      if (!is_number()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      int64_t value{};
      auto it = raw.data();
      auto end = raw.data() + raw.size();
      if (!glz::atoi(value, it, end)) {
         return unexpected(error_ctx{0, error_code::parse_number_failure});
      }
      return value;
   }

   template <>
   [[nodiscard]] inline expected<uint64_t, error_ctx> lazy_json::get<uint64_t>() const
   {
      if (!is_number()) {
         return unexpected(error_ctx{0, error_code::get_wrong_type});
      }
      uint64_t value{};
      auto it = raw.data();
      auto end = raw.data() + raw.size();
      if (!glz::atoi(value, it, end)) {
         return unexpected(error_ctx{0, error_code::parse_number_failure});
      }
      return value;
   }

   template <>
   [[nodiscard]] inline expected<int32_t, error_ctx> lazy_json::get<int32_t>() const
   {
      auto result = get<int64_t>();
      if (!result) return unexpected(result.error());
      return static_cast<int32_t>(*result);
   }

   template <>
   [[nodiscard]] inline expected<uint32_t, error_ctx> lazy_json::get<uint32_t>() const
   {
      auto result = get<uint64_t>();
      if (!result) return unexpected(result.error());
      return static_cast<uint32_t>(*result);
   }


   // Meta specialization for glz::lazy_json
   template <>
   struct meta<lazy_json>
   {
      static constexpr std::string_view name = "glz::lazy";
   };

   // JSON reader for lazy_json
   template <>
   struct from<JSON, lazy_json>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(lazy_json& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if constexpr (!check_ws_handled(Opts)) {
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }

         auto start = it;

         switch (*it) {
         case '{': {
            // Object
            value.type = lazy_type::object_t;
            value.children.template emplace<lazy_json::object_t>();
            auto& obj = std::get<lazy_json::object_t>(value.children);

            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }

            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }

            if (*it == '}') {
               ++it;
               value.raw = std::string_view{start, static_cast<size_t>(it - start)};
               return;
            }

            while (true) {
               if (*it != '"') {
                  ctx.error = error_code::expected_quote;
                  return;
               }

               // Parse key - store raw string_view (no allocation)
               ++it; // skip opening quote
               auto key_start = it;
               skip_string_view(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               // Store key content without quotes (raw, escapes not processed)
               std::string_view key{key_start, static_cast<size_t>(it - key_start)};
               ++it; // skip closing quote
               if constexpr (not Opts.null_terminated) {
                  if (it == end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
               }

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }

               if (match_invalid_end<':', Opts>(ctx, it, end)) {
                  return;
               }

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }

               // Parse value
               lazy_json child;
               from<JSON, lazy_json>::op<Opts>(child, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               obj.emplace(key, std::move(child));

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }

               if (*it == '}') {
                  ++it;
                  break;
               }

               if (match_invalid_end<',', Opts>(ctx, it, end)) {
                  return;
               }

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }
            }

            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         case '[': {
            // Array
            value.type = lazy_type::array_t;
            value.children.template emplace<lazy_json::array_t>();
            auto& arr = std::get<lazy_json::array_t>(value.children);

            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }

            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }

            if (*it == ']') {
               ++it;
               value.raw = std::string_view{start, static_cast<size_t>(it - start)};
               return;
            }

            while (true) {
               lazy_json child;
               from<JSON, lazy_json>::op<Opts>(child, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               arr.emplace_back(std::move(child));

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }

               if (*it == ']') {
                  ++it;
                  break;
               }

               if (match_invalid_end<',', Opts>(ctx, it, end)) {
                  return;
               }

               if (skip_ws<Opts>(ctx, it, end)) {
                  return;
               }
            }

            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         case '"': {
            // String - store WITH quotes so we can use glz::read_json to parse
            value.type = lazy_type::string_t;
            ++it; // skip opening quote
            skip_string_view(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            ++it; // skip closing quote
            // Store full JSON string representation with quotes
            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         case 't': {
            // true
            value.type = lazy_type::boolean_t;
            ++it;
            match<"rue", Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         case 'f': {
            // false
            value.type = lazy_type::boolean_t;
            ++it;
            match<"alse", Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         case 'n': {
            // null
            value.type = lazy_type::null_t;
            ++it;
            match<"ull", Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            break;
         }
         default: {
            // Number
            if (is_digit(uint8_t(*it)) || *it == '-') {
               value.type = lazy_type::number_t;
               skip_number<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.raw = std::string_view{start, static_cast<size_t>(it - start)};
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
            break;
         }
         }
      }
   };

   // JSON writer for lazy_json - just outputs the raw JSON
   template <>
   struct to<JSON, lazy_json>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(const lazy_json& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         // raw contains the complete JSON representation for all types (strings include quotes)
         const auto n = value.raw.size();
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
         std::memcpy(&b[ix], value.raw.data(), n);
         ix += n;
      }
   };

   // Type alias
   using lazy = lazy_json;

   // Helper function to read lazy JSON
   template <class Buffer>
   [[nodiscard]] inline expected<lazy_json, error_ctx> read_lazy(Buffer&& buffer)
   {
      lazy_json result;
      auto ec = read_json(result, std::forward<Buffer>(buffer));
      if (ec) {
         return unexpected(ec);
      }
      return result;
   }

} // namespace glz
