// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/custom_meta.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/to.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/core/write_wrappers.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/variant.hpp"
#include "glaze/yaml/common.hpp"
#include "glaze/yaml/opts.hpp"

namespace glz
{
   template <>
   struct serialize<YAML>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<YAML, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                             std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // glaze_value_t - unwrap custom value types
   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         to<YAML, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                        std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // nullable_like (std::optional, pointers)
   template <nullable_like T>
   struct to<YAML, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if (value) {
            serialize<YAML>::op<Opts>(*value, ctx, b, ix);
         }
         else {
            if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
               return;
            }
            dump("null", b, ix);
         }
      }
   };

   // Always-null literal types (std::nullptr_t, std::monostate, std::nullopt_t)
   template <always_null_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&&, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }
         dump("null", b, ix);
      }
   };

   // boolean_like
   template <boolean_like T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(const bool value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }

         if constexpr (check_bools_as_numbers(Opts)) {
            if (value) {
               b[ix++] = '1';
            }
            else {
               b[ix++] = '0';
            }
         }
         else {
            if (value) {
               std::memcpy(&b[ix], "true", 4);
               ix += 4;
            }
            else {
               std::memcpy(&b[ix], "false", 5);
               ix += 5;
            }
         }
      }
   };

   // num_t (integers and floats)
   template <num_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 32 + write_padding_bytes)) [[unlikely]] {
            return;
         }
         // YAML supports .inf, -.inf, and .nan for special float values
         if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            if (std::isnan(value)) {
               dump(".nan", b, ix);
               return;
            }
            else if (std::isinf(value)) {
               if (value < 0) {
                  dump("-.inf", b, ix);
               }
               else {
                  dump(".inf", b, ix);
               }
               return;
            }
         }
         write_chars::op<Opts>(value, ctx, b, ix);
      }
   };

   // char_t
   template <char_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }
         dump('"', b, ix);
         b[ix++] = value;
         dump('"', b, ix);
      }
   };

   namespace yaml
   {
      // Write a YAML double-quoted string with proper escaping
      template <class B>
      inline void write_double_quoted_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix)
      {
         // Estimate max size: original + quotes + escapes
         if (!ensure_space(ctx, b, ix + str.size() * 2 + 3 + write_padding_bytes)) [[unlikely]] {
            return;
         }

         dump('"', b, ix);
         for (char c : str) {
            switch (c) {
            case '"':
               dump("\\\"", b, ix);
               break;
            case '\\':
               dump("\\\\", b, ix);
               break;
            case '\n':
               dump("\\n", b, ix);
               break;
            case '\r':
               dump("\\r", b, ix);
               break;
            case '\t':
               dump("\\t", b, ix);
               break;
            case '\0':
               dump("\\0", b, ix);
               break;
            default:
               if (static_cast<unsigned char>(c) < 0x20) {
                  // Control characters - use hex escape
                  dump("\\x", b, ix);
                  constexpr char hex[] = "0123456789abcdef";
                  b[ix++] = hex[(c >> 4) & 0xF];
                  b[ix++] = hex[c & 0xF];
               }
               else {
                  b[ix++] = c;
               }
               break;
            }
         }
         dump('"', b, ix);
      }

      // Write a YAML single-quoted string (only ' needs escaping as '')
      template <class B>
      inline void write_single_quoted_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + str.size() * 2 + 3 + write_padding_bytes)) [[unlikely]] {
            return;
         }

         dump('\'', b, ix);
         for (char c : str) {
            if (c == '\'') {
               dump("''", b, ix);
            }
            else {
               b[ix++] = c;
            }
         }
         dump('\'', b, ix);
      }

      // Write a literal block scalar (|)
      template <class B>
      inline void write_literal_block(std::string_view str, is_context auto&& ctx, B&& b, auto& ix,
                                      int32_t indent_level, uint8_t indent_width, char chomping)
      {
         if (!ensure_space(ctx, b, ix + str.size() + 64 + write_padding_bytes)) [[unlikely]] {
            return;
         }

         if (chomping == '-' || chomping == '+') {
            dump('|', b, ix);
            dump(chomping, b, ix);
            dump('\n', b, ix);
         }
         else {
            dump("|\n", b, ix);
         }

         // Write each line with proper indentation
         size_t pos = 0;
         while (pos < str.size()) {
            // Write indentation
            const int32_t spaces = (indent_level + 1) * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + 256)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }

            // Find end of line
            size_t eol = str.find('\n', pos);
            if (eol == std::string_view::npos) {
               eol = str.size();
            }

            // Write line content
            const size_t line_len = eol - pos;
            if (!ensure_space(ctx, b, ix + line_len + 8)) [[unlikely]] {
               return;
            }
            std::memcpy(&b[ix], &str[pos], line_len);
            ix += line_len;
            dump('\n', b, ix);

            pos = eol + 1;
         }
      }

      // Write string with appropriate style
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code from if constexpr
#endif
      template <auto Opts, class B>
      inline void write_yaml_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix,
                                    int32_t indent_level = 0)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

         // Use literal block style for multiline strings
         if (str.find('\n') != std::string_view::npos) {
            // Block scalars are not valid inside flow collections ({...}, [...]).
            // Emit double-quoted escaped form in flow context.
            if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
               write_double_quoted_string(str, ctx, b, ix);
               return;
            }

            // Block scalars have no escape mechanism, so characters like \r, \0,
            // and other control chars cannot be represented. Fall back to double-quoted.
            {
               bool has_unrepresentable = false;
               for (char c : str) {
                  if (c == '\r' || c == '\0' || (static_cast<unsigned char>(c) < 0x20 && c != '\n' && c != '\t')) {
                     has_unrepresentable = true;
                     break;
                  }
               }
               if (has_unrepresentable) {
                  write_double_quoted_string(str, ctx, b, ix);
                  return;
               }
            }

            size_t trailing_newlines = 0;
            for (size_t i = str.size(); i > 0; --i) {
               if (str[i - 1] == '\n') {
                  ++trailing_newlines;
               }
               else {
                  break;
               }
            }
            char chomping = '\0';
            if (trailing_newlines == 0) {
               chomping = '-';
            }
            else if (trailing_newlines > 1) {
               chomping = '+';
            }
            write_literal_block(str, ctx, b, ix, indent_level, indent_width, chomping);
            return;
         }

         // Check if string needs quoting
         if (yaml::needs_quoting(str)) {
            // Double-quoted style is required for strings with characters that need
            // escape sequences (\r, \0, control chars) since single-quoted strings
            // have no escape mechanism for these.
            bool needs_escapes = false;
            for (char c : str) {
               if (c == '\r' || c == '\0' || (static_cast<unsigned char>(c) < 0x20 && c != '\t')) {
                  needs_escapes = true;
                  break;
               }
            }
            if (needs_escapes || str.find('\'') != std::string_view::npos) {
               write_double_quoted_string(str, ctx, b, ix);
            }
            else {
               write_single_quoted_string(str, ctx, b, ix);
            }
         }
         else {
            // Plain scalar
            if (!ensure_space(ctx, b, ix + str.size() + write_padding_bytes)) [[unlikely]] {
               return;
            }
            dump(str, b, ix);
         }
      }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
   } // namespace yaml

   // str_t (strings)
   template <str_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         const sv str{value};
         yaml::write_yaml_string<Opts>(str, ctx, b, ix);
      }
   };

   // Enum with glz::meta - writes string representation
   template <class T>
      requires((glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<std::decay_t<T>>)) && not custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         const sv str = get_enum_name(value);
         if (!str.empty()) {
            yaml::write_yaml_string<Opts>(str, ctx, b, ix);
         }
         else [[unlikely]] {
            // Value doesn't have a mapped string, serialize as underlying number
            serialize<YAML>::op<Opts>(static_cast<std::underlying_type_t<T>>(value), ctx, b, ix);
         }
      }
   };

   // Raw enum (without glz::meta) - writes as underlying numeric type
   template <class T>
      requires(!meta_keys<T> && std::is_enum_v<std::decay_t<T>> && !glaze_enum_t<T> && !custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         serialize<YAML>::op<Opts>(static_cast<std::underlying_type_t<std::decay_t<T>>>(value), ctx,
                                   std::forward<Args>(args)...);
      }
   };

   namespace yaml
   {
      // Forward declarations for helpers used in block sequences
      template <auto Opts, class T, class B>
      inline void write_block_mapping_nested(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level);

      template <auto Opts, class T, class B>
      inline void write_block_mapping(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level,
                                      bool skip_first_indent = false);

      // Forward declaration for tagged-variant block output (definition needs write_block_mapping).
      template <auto Opts, class Variant, class T, class B>
      inline void write_tagged_block_object(T&& inner, size_t index, is_context auto&& ctx, B&& b, auto& ix,
                                            int32_t indent_level, bool skip_first_indent);

      // Forward declaration for custom-alternative tagged-variant block output (needs serialize<YAML>).
      template <auto Opts, class Variant, class T, class B>
      inline void write_tagged_block_custom(T&& inner, size_t index, is_context auto&& ctx, B&& b, auto& ix,
                                            int32_t indent_level, bool skip_first_indent);

      // Helper to check if a type is "simple" (writes on same line)
      template <class T>
      consteval bool is_simple_type()
      {
         using V = std::remove_cvref_t<T>;
         return bool_t<V> || num_t<V> || str_t<V> || char_t<V> || std::is_enum_v<V> || std::same_as<V, std::nullptr_t>;
      }

      // Runtime check if a variant currently holds a simple type
      template <class T>
      inline bool variant_holds_simple_type([[maybe_unused]] const T& value)
      {
         if constexpr (is_variant<T>) {
            return std::visit(
               [](const auto& v) {
                  using V = std::remove_cvref_t<decltype(v)>;
                  return is_simple_type<V>();
               },
               value);
         }
         else if constexpr (glaze_value_t<T>) {
            // Unwrap glaze_value types (like glz::generic) to check the inner variant
            decltype(auto) inner = get_member(value, meta_wrapper_v<T>);
            return variant_holds_simple_type(inner);
         }
         else {
            return is_simple_type<T>();
         }
      }

      // Compile-time check if a type is or wraps a variant
      template <class T>
      consteval bool is_or_wraps_variant()
      {
         if constexpr (is_variant<T>) {
            return true;
         }
         else if constexpr (glaze_value_t<T>) {
            using inner_t = std::decay_t<decltype(get_member(std::declval<T>(), meta_wrapper_v<T>))>;
            return is_or_wraps_variant<inner_t>();
         }
         else {
            return false;
         }
      }

      // Write a variant's held value in block context, ensuring strings get correct indent_level.
      template <auto Opts, class T, class B>
      inline void write_variant_value(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level)
      {
         using V = std::remove_cvref_t<T>;
         if constexpr (is_variant<V>) {
            std::visit(
               [&](auto&& inner) {
                  using inner_t = std::remove_cvref_t<decltype(inner)>;
                  if constexpr (str_t<inner_t>) {
                     write_yaml_string<Opts>(sv{inner}, ctx, b, ix, indent_level);
                  }
                  else {
                     serialize<YAML>::op<Opts>(inner, ctx, b, ix);
                  }
               },
               value);
         }
         else if constexpr (glaze_value_t<V>) {
            write_variant_value<Opts>(get_member(value, meta_wrapper_v<V>), ctx, b, ix, indent_level);
         }
         else if constexpr (str_t<V>) {
            write_yaml_string<Opts>(sv{value}, ctx, b, ix, indent_level);
         }
         else {
            serialize<YAML>::op<Opts>(value, ctx, b, ix);
         }
      }

      // Write block-style sequence
      template <auto Opts, class T, class B>
      inline void write_block_sequence(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

         bool is_empty = false;
         if constexpr (requires { value.empty(); }) {
            is_empty = value.empty();
         }
         else if constexpr (requires {
                               value.begin();
                               value.end();
                            }) {
            is_empty = (value.begin() == value.end());
         }

         if (is_empty) {
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + 8)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }
            dump("[]", b, ix);
            if (indent_level > 0) {
               dump('\n', b, ix);
            }
            return;
         }

         bool first = true;
         for (auto&& element : value) {
            if (bool(ctx.error)) [[unlikely]]
               return;

            using element_t = std::remove_cvref_t<decltype(element)>;

            if (!first) {
               // Nothing needed between elements - each starts with indented dash
            }
            first = false;

            // Write indentation and dash
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + 8)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }
            dump('-', b, ix);

            if constexpr (str_t<element_t>) {
               dump(' ', b, ix);
               write_yaml_string<Opts>(sv{element}, ctx, b, ix, indent_level);
               dump('\n', b, ix);
            }
            else if constexpr (is_simple_type<element_t>()) {
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(element, ctx, b, ix);
               dump('\n', b, ix);
            }
            else if constexpr (nullable_like<element_t>) {
               using inner_t = std::remove_cvref_t<decltype(*element)>;
               if (!element) {
                  dump(' ', b, ix);
                  dump("null", b, ix);
                  dump('\n', b, ix);
               }
               else if constexpr (str_t<inner_t>) {
                  dump(' ', b, ix);
                  write_yaml_string<Opts>(sv{*element}, ctx, b, ix, indent_level);
                  dump('\n', b, ix);
               }
               else if constexpr (is_simple_type<inner_t>()) {
                  dump(' ', b, ix);
                  serialize<YAML>::op<Opts>(*element, ctx, b, ix);
                  dump('\n', b, ix);
               }
               else {
                  // Complex inner type - check for empty containers first
                  bool wrote_empty = false;
                  if constexpr (writable_map_t<inner_t>) {
                     if (element->empty()) {
                        dump(" {}\n", b, ix);
                        wrote_empty = true;
                     }
                  }
                  else if constexpr (writable_array_t<inner_t>) {
                     if constexpr (requires { element->empty(); }) {
                        if (element->empty()) {
                           dump(" []\n", b, ix);
                           wrote_empty = true;
                        }
                     }
                  }
                  if (!wrote_empty) {
                     if constexpr (glaze_object_t<inner_t> || reflectable<inner_t>) {
                        dump(' ', b, ix);
                        write_block_mapping<Opts>(*element, ctx, b, ix, indent_level + 1, true);
                     }
                     else {
                        dump('\n', b, ix);
                        write_block_mapping_nested<Opts>(*element, ctx, b, ix, indent_level + 1);
                     }
                  }
               }
            }
            else if constexpr (is_or_wraps_variant<element_t>()) {
               // For variants, check at runtime if they hold a simple type
               if (variant_holds_simple_type(element)) {
                  dump(' ', b, ix);
                  write_variant_value<Opts>(element, ctx, b, ix, indent_level);
                  dump('\n', b, ix);
               }
               else {
                  // Complex variant content (maps/arrays/objects) - write in block style
                  if constexpr (is_variant<element_t>) {
                     const size_t index = element.index();
                     std::visit(
                        [&](auto&& inner) {
                           using inner_t = std::remove_cvref_t<decltype(inner)>;
                           // not custom_write: a custom alternative that is also reflectable (e.g. a
                           // non-aggregate class under P2996) must take the custom branch below.
                           if constexpr ((glaze_object_t<inner_t> || reflectable<inner_t>) &&
                                         not custom_write<inner_t>) {
                              // Compact form: first key (or the discriminator tag) inline after dash
                              dump(' ', b, ix);
                              if constexpr (check_write_type_info(Opts) && not tag_v<element_t>.empty()) {
                                 write_tagged_block_object<Opts, element_t>(inner, index, ctx, b, ix, indent_level + 1,
                                                                            true);
                              }
                              else {
                                 write_block_mapping<Opts>(inner, ctx, b, ix, indent_level + 1, true);
                              }
                           }
                           else if constexpr (custom_write<inner_t> && check_write_type_info(Opts) &&
                                              not tag_v<element_t>.empty()) {
                              // Custom alternative: discriminator tag inline after the dash, body merged.
                              dump(' ', b, ix);
                              write_tagged_block_custom<Opts, element_t>(inner, index, ctx, b, ix, indent_level + 1,
                                                                         true);
                           }
                           else {
                              if constexpr (writable_map_t<inner_t>) {
                                 if (inner.empty()) {
                                    dump(" {}\n", b, ix);
                                    return;
                                 }
                              }
                              else if constexpr (writable_array_t<inner_t>) {
                                 if constexpr (requires { inner.empty(); }) {
                                    if (inner.empty()) {
                                       dump(" []\n", b, ix);
                                       return;
                                    }
                                 }
                              }
                              dump('\n', b, ix);
                              write_block_mapping_nested<Opts>(inner, ctx, b, ix, indent_level + 1);
                           }
                        },
                        element);
                  }
                  else {
                     // glaze_value_t wrapping a variant — simple types were already
                     // handled by variant_holds_simple_type above, so the held value
                     // is guaranteed to be a complex type (map/array/object).
                     dump('\n', b, ix);
                     write_block_mapping_nested<Opts>(element, ctx, b, ix, indent_level + 1);
                  }
               }
            }
            else if constexpr (glaze_object_t<element_t> || reflectable<element_t>) {
               // Compact form: first key inline after dash
               dump(' ', b, ix);
               write_block_mapping<Opts>(element, ctx, b, ix, indent_level + 1, true);
            }
            else if constexpr (has_custom_meta_v<element_t>) {
               // Types with top-level custom serialization produce scalar output -
               // write inline after dash
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(element, ctx, b, ix);
               dump('\n', b, ix);
            }
            else if constexpr (writable_map_t<element_t> || writable_array_t<element_t> || glaze_value_t<element_t>) {
               // Containers and glaze_value_t (which may wrap containers) -
               // write on next line with increased indent
               dump('\n', b, ix);
               write_block_mapping_nested<Opts>(element, ctx, b, ix, indent_level + 1);
            }
            else {
               // Other types (pairs, tuples, etc.) - write inline after dash
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(element, ctx, b, ix);
               dump('\n', b, ix);
            }
         }
      }

      // Write flow-style sequence
      template <auto Opts, class T, class B>
      inline void write_flow_sequence(T&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }
         dump('[', b, ix);

         bool first = true;
         for (auto&& element : value) {
            if (bool(ctx.error)) [[unlikely]]
               return;

            if (!first) {
               dump(", ", b, ix);
            }
            first = false;

            serialize<YAML>::op<yaml::flow_context_on<Opts>()>(element, ctx, b, ix);
         }

         dump(']', b, ix);
      }
   } // namespace yaml

   // writable_array_t (vectors, arrays, etc.)
   template <writable_array_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
            yaml::write_flow_sequence<Opts>(value, ctx, b, ix);
         }
         else {
            // Get indent level from context if available, otherwise 0
            int32_t indent_level = 0;
            if constexpr (requires { ctx.indent_level; }) {
               indent_level = ctx.indent_level;
            }
            yaml::write_block_sequence<Opts>(value, ctx, b, ix, indent_level);
         }
      }
   };

   // Tuples (std::tuple, glaze_array_t, tuple_t)
   template <class T>
      requires(glaze_array_t<T> || tuple_t<std::decay_t<T>> || is_std_tuple<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
            }
            else {
               return glz::tuple_size_v<std::decay_t<T>>;
            }
         }();

         if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
            // Flow style: [a, b, c]
            if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
               return;
            }
            dump('[', b, ix);

            using V = std::decay_t<T>;
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if constexpr (I != 0) {
                  dump(", ", b, ix);
               }

               if constexpr (glaze_array_t<V>) {
                  serialize<YAML>::op<yaml::flow_context_on<Opts>()>(get_member(value, glz::get<I>(meta_v<T>)), ctx, b,
                                                                     ix);
               }
               else if constexpr (is_std_tuple<T>) {
                  using element_t = core_t<decltype(std::get<I>(value))>;
                  to<YAML, element_t>::template op<yaml::flow_context_on<Opts>()>(std::get<I>(value), ctx, b, ix);
               }
               else {
                  using element_t = core_t<decltype(glz::get<I>(value))>;
                  to<YAML, element_t>::template op<yaml::flow_context_on<Opts>()>(glz::get<I>(value), ctx, b, ix);
               }
            });

            dump(']', b, ix);
         }
         else {
            // Block style: - a\n- b\n- c
            constexpr uint8_t indent_width = yaml::check_indent_width(yaml::yaml_opts{});
            int32_t indent_level = 0;
            if constexpr (requires { ctx.indent_level; }) {
               indent_level = ctx.indent_level;
            }

            using V = std::decay_t<T>;
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               // Write indentation and dash
               const int32_t spaces = indent_level * indent_width;
               if (!ensure_space(ctx, b, ix + spaces + 8)) [[unlikely]] {
                  return;
               }
               for (int32_t i = 0; i < spaces; ++i) {
                  b[ix++] = ' ';
               }
               dump("- ", b, ix);

               if constexpr (glaze_array_t<V>) {
                  using element_t = std::decay_t<decltype(get_member(value, glz::get<I>(meta_v<T>)))>;
                  if constexpr (yaml::is_simple_type<element_t>()) {
                     serialize<YAML>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, b, ix);
                     dump('\n', b, ix);
                  }
                  else {
                     dump('\n', b, ix);
                     serialize<YAML>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, b, ix);
                  }
               }
               else if constexpr (is_std_tuple<T>) {
                  using element_t = std::decay_t<std::tuple_element_t<I, std::remove_cvref_t<T>>>;
                  if constexpr (yaml::is_simple_type<element_t>()) {
                     to<YAML, element_t>::template op<Opts>(std::get<I>(value), ctx, b, ix);
                     dump('\n', b, ix);
                  }
                  else {
                     dump('\n', b, ix);
                     to<YAML, element_t>::template op<Opts>(std::get<I>(value), ctx, b, ix);
                  }
               }
               else {
                  using element_t = std::decay_t<decltype(glz::get<I>(value))>;
                  if constexpr (yaml::is_simple_type<element_t>()) {
                     to<YAML, element_t>::template op<Opts>(glz::get<I>(value), ctx, b, ix);
                     dump('\n', b, ix);
                  }
                  else {
                     dump('\n', b, ix);
                     to<YAML, element_t>::template op<Opts>(glz::get<I>(value), ctx, b, ix);
                  }
               }
            });
         }
      }
   };

   // Pairs (std::pair)
   template <pair_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         const auto& [key, val] = value;

         using first_type = typename std::remove_cvref_t<T>::first_type;
         using second_type = typename std::remove_cvref_t<T>::second_type;

         if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
            // Flow style: {key: value}
            if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
               return;
            }
            dump('{', b, ix);

            // Write key
            if constexpr (str_t<first_type>) {
               yaml::write_yaml_string<Opts>(sv{key}, ctx, b, ix);
            }
            else {
               serialize<YAML>::op<yaml::flow_context_on<Opts>()>(key, ctx, b, ix);
            }

            dump(": ", b, ix);

            // Write value
            serialize<YAML>::op<yaml::flow_context_on<Opts>()>(val, ctx, b, ix);

            dump('}', b, ix);
         }
         else {
            // Block style: key: value
            constexpr uint8_t indent_width = yaml::check_indent_width(yaml::yaml_opts{});
            int32_t indent_level = 0;
            if constexpr (requires { ctx.indent_level; }) {
               indent_level = ctx.indent_level;
            }

            // Write indentation
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + 64)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }

            // Write key
            if constexpr (str_t<first_type>) {
               yaml::write_yaml_string<Opts>(sv{key}, ctx, b, ix);
            }
            else {
               serialize<YAML>::op<Opts>(key, ctx, b, ix);
            }

            dump(':', b, ix);

            if constexpr (yaml::is_simple_type<second_type>()) {
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(val, ctx, b, ix);
               dump('\n', b, ix);
            }
            else {
               dump('\n', b, ix);
               serialize<YAML>::op<Opts>(val, ctx, b, ix);
            }
         }
      }
   };

   namespace yaml
   {
      // Forward declaration for nested object helper
      template <auto Opts, class T, class B>
      inline void write_block_mapping_nested(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level);

      // Write the value portion of a block-mapping entry (everything after the already-written
      // "key:"). Dispatches on `val_t` to choose the layout: simple scalars stay on the same
      // line, complex objects/arrays/maps move to the next line with increased indent. Factored
      // out so transparent write wrappers (mimic glaze_value_t and glz::custom getters) can be
      // unwrapped and routed through the same logic by their resolved type (issue #2595).
      template <auto Opts, class val_t, class Member, class B>
      inline void write_block_mapping_value(Member&& member, is_context auto&& ctx, B&& b, auto& ix,
                                            int32_t indent_level)
      {
         // Handle empty containers inline (before type dispatch)
         if constexpr (range<val_t> && !str_t<val_t> && !is_simple_type<val_t>() && has_empty<val_t>) {
            if (member.empty()) {
               if constexpr (writable_map_t<val_t>) {
                  dump(" {}\n", b, ix);
               }
               else {
                  dump(" []\n", b, ix);
               }
               return;
            }
         }

         if constexpr (is_simple_type<val_t>()) {
            // Simple types go on same line
            dump(' ', b, ix);
            if constexpr (str_t<val_t>) {
               yaml::write_yaml_string<Opts>(sv{member}, ctx, b, ix, indent_level);
            }
            else {
               serialize<YAML>::op<Opts>(member, ctx, b, ix);
            }
            dump('\n', b, ix);
         }
         else if constexpr (nullable_like<val_t>) {
            using inner_t = std::remove_cvref_t<decltype(*std::declval<val_t&>())>;
            if (!member) {
               // Null - always same line
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(member, ctx, b, ix);
               dump('\n', b, ix);
            }
            else if constexpr (is_simple_type<inner_t>() || str_t<inner_t>) {
               // Simple inner type - same line
               dump(' ', b, ix);
               if constexpr (str_t<inner_t>) {
                  yaml::write_yaml_string<Opts>(sv{*member}, ctx, b, ix, indent_level);
               }
               else {
                  serialize<YAML>::op<Opts>(*member, ctx, b, ix);
               }
               dump('\n', b, ix);
            }
            else {
               // Complex inner type - check for empty containers first
               bool wrote_empty = false;
               if constexpr (writable_map_t<inner_t>) {
                  if (member->empty()) {
                     dump(" {}\n", b, ix);
                     wrote_empty = true;
                  }
               }
               else if constexpr (writable_array_t<inner_t>) {
                  if constexpr (requires { member->empty(); }) {
                     if (member->empty()) {
                        dump(" []\n", b, ix);
                        wrote_empty = true;
                     }
                  }
               }
               if (!wrote_empty) {
                  // Non-empty complex inner type - next line with increased indent
                  dump('\n', b, ix);
                  if constexpr (requires { ctx.indent_level; }) {
                     auto nested_ctx = ctx;
                     nested_ctx.indent_level = indent_level + 1;
                     serialize<YAML>::op<Opts>(*member, nested_ctx, b, ix);
                  }
                  else {
                     write_block_mapping_nested<Opts>(*member, ctx, b, ix, indent_level + 1);
                  }
               }
            }
         }
         else if constexpr (is_or_wraps_variant<val_t>()) {
            // Variants and variant-wrappers (e.g., glz::generic) may hold simple values
            if (variant_holds_simple_type(member)) {
               dump(' ', b, ix);
               write_variant_value<Opts>(member, ctx, b, ix, indent_level);
               dump('\n', b, ix);
            }
            else {
               dump('\n', b, ix);
               if constexpr (requires { ctx.indent_level; }) {
                  auto nested_ctx = ctx;
                  nested_ctx.indent_level = indent_level + 1;
                  serialize<YAML>::op<Opts>(member, nested_ctx, b, ix);
               }
               else {
                  write_block_mapping_nested<Opts>(member, ctx, b, ix, indent_level + 1);
               }
            }
         }
         else if constexpr (writable_map_t<val_t> || writable_array_t<val_t> || glaze_object_t<val_t> ||
                            reflectable<val_t>) {
            // Complex types go on next line with increased indent
            dump('\n', b, ix);

            // Create a modified context with incremented indent level
            if constexpr (requires { ctx.indent_level; }) {
               auto nested_ctx = ctx;
               nested_ctx.indent_level = indent_level + 1;
               serialize<YAML>::op<Opts>(member, nested_ctx, b, ix);
            }
            else {
               write_block_mapping_nested<Opts>(member, ctx, b, ix, indent_level + 1);
            }
         }
         else {
            // All other types (custom_t, types with custom to/from<YAML>, etc.)
            // are assumed to produce scalar values - write on same line
            dump(' ', b, ix);
            serialize<YAML>::op<Opts>(member, ctx, b, ix);
            dump('\n', b, ix);
         }
      }

      // Write block-style mapping
      template <auto Opts, class T, class B>
      inline void write_block_mapping(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level,
                                      bool skip_first_indent)
      {
         using V = std::remove_cvref_t<T>;
         constexpr auto N = reflect<V>::size;
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;

            using val_t = field_t<V, I>;

            if constexpr (!always_skipped<val_t>) {
               static constexpr sv key = get<I>(reflect<V>::keys);

               // Get member value (supports both glaze_object_t and reflectable)
               auto&& member = [&]() -> decltype(auto) {
                  if constexpr (glaze_object_t<V>) {
                     return get_member(value, get<I>(reflect<V>::values));
                  }
                  else {
                     return get<I>(to_tie(value));
                  }
               }();

               // Skip fields based on meta::skip (compile-time) and meta::skip_if (runtime)
               if constexpr (meta_has_skip<V>) {
                  static constexpr meta_context mctx{.op = operation::serialize};
                  if constexpr (meta<V>::skip(reflect<V>::keys[I], mctx)) return;
               }
               if constexpr (meta_has_skip_if<V>) {
                  static constexpr auto k = glz::get<I>(reflect<V>::keys);
                  static constexpr meta_context mctx{.op = operation::serialize};
                  if (meta<V>::skip_if(member, k, mctx)) return;
               }

               // Skip null members if configured
               if constexpr (nullable_like<val_t>) {
                  if constexpr (Opts.skip_null_members) {
                     if (!member) {
                        return;
                     }
                  }
               }

               if constexpr (check_skip_default_members(Opts) && has_skippable_default<val_t>) {
                  if (is_default_value(member)) return;
               }

               // Write indentation (skip for first field when in compact sequence context)
               if (skip_first_indent) {
                  skip_first_indent = false;
                  if (!ensure_space(ctx, b, ix + key.size() + 8)) [[unlikely]] {
                     return;
                  }
               }
               else {
                  const int32_t spaces = indent_level * indent_width;
                  if (!ensure_space(ctx, b, ix + spaces + key.size() + 8)) [[unlikely]] {
                     return;
                  }
                  for (int32_t i = 0; i < spaces; ++i) {
                     b[ix++] = ' ';
                  }
               }

               // Write key
               dump(key, b, ix);
               dump(':', b, ix);

               // A field exposed through a transparent write wrapper (mimic glaze_value_t or a
               // glz::custom getter) that does not itself wrap a variant is laid out by the type
               // it resolves to, not the wrapper type. Without this it would fall through to the
               // scalar branch and emit a nested object/array on the same line (issue #2595).
               // Variant-wrappers (e.g. glz::generic) keep their existing path unchanged.
               if constexpr (transparent_write_wrapper<val_t> && !is_or_wraps_variant<val_t>()) {
                  unwrap_write_value(member, ctx, [&]<class Inner>(Inner&& inner) {
                     write_block_mapping_value<Opts, std::remove_cvref_t<Inner>>(std::forward<Inner>(inner), ctx, b, ix,
                                                                                 indent_level);
                  });
               }
               else {
                  write_block_mapping_value<Opts, val_t>(member, ctx, b, ix, indent_level);
               }
            }
         });
      }

      // Helper for nested objects
      template <auto Opts, class T, class B>
      inline void write_block_mapping_nested(T&& value, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level)
      {
         using V = std::remove_cvref_t<T>;

         if constexpr (glaze_object_t<V> || reflectable<V>) {
            write_block_mapping<Opts>(value, ctx, b, ix, indent_level);
         }
         else if constexpr (glaze_value_t<V>) {
            // Unwrap glaze_value_t types (like glz::generic) and recurse
            write_block_mapping_nested<Opts>(get_member(value, meta_wrapper_v<V>), ctx, b, ix, indent_level);
         }
         else if constexpr (is_variant<V>) {
            // Handle variants by visiting and recursing with the same indent level. A tagged
            // variant holding an object also emits its discriminator (meta::tag) entry.
            const size_t index = value.index();
            std::visit(
               [&](auto&& inner) {
                  using inner_t = std::remove_cvref_t<decltype(inner)>;
                  // not custom_write: a custom alternative that is also reflectable (e.g. a
                  // non-aggregate class under P2996) must take the custom branch below.
                  if constexpr ((glaze_object_t<inner_t> || reflectable<inner_t>) && not custom_write<inner_t> &&
                                check_write_type_info(Opts) && not tag_v<V>.empty()) {
                     write_tagged_block_object<Opts, V>(inner, index, ctx, b, ix, indent_level, false);
                  }
                  else if constexpr (custom_write<inner_t> && check_write_type_info(Opts) && not tag_v<V>.empty()) {
                     write_tagged_block_custom<Opts, V>(inner, index, ctx, b, ix, indent_level, false);
                  }
                  else {
                     write_block_mapping_nested<Opts>(inner, ctx, b, ix, indent_level);
                  }
               },
               value);
         }
         else if constexpr (writable_array_t<V>) {
            write_block_sequence<Opts>(value, ctx, b, ix, indent_level);
         }
         else if constexpr (writable_map_t<V>) {
            // Map handling
            constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

            for (auto&& [k, v] : value) {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               // Write indentation and key
               const int32_t spaces = indent_level * indent_width;
               if (!ensure_space(ctx, b, ix + spaces + 64)) [[unlikely]] {
                  return;
               }
               for (int32_t i = 0; i < spaces; ++i) {
                  b[ix++] = ' ';
               }

               // Write key
               using key_t = std::remove_cvref_t<decltype(k)>;
               if constexpr (str_t<key_t>) {
                  write_yaml_string<Opts>(sv{k}, ctx, b, ix);
               }
               else {
                  serialize<YAML>::op<Opts>(k, ctx, b, ix);
               }
               dump(':', b, ix);

               using val_t = std::remove_cvref_t<decltype(v)>;
               if constexpr (str_t<val_t>) {
                  dump(' ', b, ix);
                  write_yaml_string<Opts>(sv{v}, ctx, b, ix, indent_level);
                  dump('\n', b, ix);
               }
               else if constexpr (is_simple_type<val_t>()) {
                  dump(' ', b, ix);
                  serialize<YAML>::op<Opts>(v, ctx, b, ix);
                  dump('\n', b, ix);
               }
               else if constexpr (nullable_like<val_t>) {
                  using inner_t = std::remove_cvref_t<decltype(*v)>;
                  if (!v) {
                     dump(' ', b, ix);
                     dump("null", b, ix);
                     dump('\n', b, ix);
                  }
                  else if constexpr (str_t<inner_t>) {
                     dump(' ', b, ix);
                     write_yaml_string<Opts>(sv{*v}, ctx, b, ix, indent_level);
                     dump('\n', b, ix);
                  }
                  else if constexpr (is_simple_type<inner_t>()) {
                     dump(' ', b, ix);
                     serialize<YAML>::op<Opts>(*v, ctx, b, ix);
                     dump('\n', b, ix);
                  }
                  else {
                     // Complex inner type - check for empty containers first
                     bool wrote_empty = false;
                     if constexpr (writable_map_t<inner_t>) {
                        if (v->empty()) {
                           dump(" {}\n", b, ix);
                           wrote_empty = true;
                        }
                     }
                     else if constexpr (writable_array_t<inner_t>) {
                        if constexpr (requires { v->empty(); }) {
                           if (v->empty()) {
                              dump(" []\n", b, ix);
                              wrote_empty = true;
                           }
                        }
                     }
                     if (!wrote_empty) {
                        dump('\n', b, ix);
                        write_block_mapping_nested<Opts>(*v, ctx, b, ix, indent_level + 1);
                     }
                  }
               }
               else if constexpr (is_or_wraps_variant<val_t>()) {
                  // For variants and types wrapping variants (like glz::generic),
                  // check at runtime if they hold a simple type
                  if (variant_holds_simple_type(v)) {
                     dump(' ', b, ix);
                     write_variant_value<Opts>(v, ctx, b, ix, indent_level);
                     dump('\n', b, ix);
                  }
                  else {
                     dump('\n', b, ix);
                     write_block_mapping_nested<Opts>(v, ctx, b, ix, indent_level + 1);
                  }
               }
               else {
                  dump('\n', b, ix);
                  write_block_mapping_nested<Opts>(v, ctx, b, ix, indent_level + 1);
               }
            }
         }
         else {
            // Fallback for types not explicitly handled above.
            // Note: Variants with complex content are handled in the map value loop above
            // (lines 785-796), so they don't reach here. This fallback handles edge cases
            // like custom types. The newline is safe because serialize<YAML>::op for simple
            // types doesn't add trailing newlines.
            serialize<YAML>::op<Opts>(value, ctx, b, ix);
            dump('\n', b, ix);
         }
      }

      // Write the comma-separated "key: value" entries of a flow mapping (no surrounding braces).
      // `first` tracks whether a leading ", " separator is needed, letting callers prepend
      // entries (e.g. a tagged variant's discriminator) before the object's own members.
      template <auto Opts, class T, class B>
      inline void write_flow_mapping_members(T&& value, is_context auto&& ctx, B&& b, auto& ix, bool& first)
      {
         using V = std::remove_cvref_t<T>;
         constexpr auto N = reflect<V>::size;

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;

            using val_t = field_t<V, I>;

            if constexpr (!always_skipped<val_t>) {
               static constexpr sv key = get<I>(reflect<V>::keys);

               // Get member value (supports both glaze_object_t and reflectable)
               auto&& member = [&]() -> decltype(auto) {
                  if constexpr (glaze_object_t<V>) {
                     return get_member(value, get<I>(reflect<V>::values));
                  }
                  else {
                     return get<I>(to_tie(value));
                  }
               }();

               // Skip null members if configured
               if constexpr (nullable_like<val_t>) {
                  if constexpr (Opts.skip_null_members) {
                     if (!member) {
                        return;
                     }
                  }
               }

               if constexpr (check_skip_default_members(Opts) && has_skippable_default<val_t>) {
                  if (is_default_value(member)) return;
               }

               if (!first) {
                  dump(", ", b, ix);
               }
               first = false;

               if (!ensure_space(ctx, b, ix + key.size() + 8)) [[unlikely]] {
                  return;
               }
               dump(key, b, ix);
               dump(": ", b, ix);

               serialize<YAML>::op<flow_context_on<Opts>()>(member, ctx, b, ix);
            }
         });
      }

      // Write flow-style mapping
      template <auto Opts, class T, class B>
      inline void write_flow_mapping(T&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }
         dump('{', b, ix);
         bool first = true;
         write_flow_mapping_members<Opts>(value, ctx, b, ix, first);
         dump('}', b, ix);
      }

      // Write a tagged variant's discriminator value (the meta::ids entry for `index`).
      template <auto Opts, class Variant, class B>
      inline void write_variant_tag_id(size_t index, is_context auto&& ctx, B&& b, auto& ix, int32_t indent_level)
      {
         using id_type = std::decay_t<decltype(ids_v<Variant>[0])>;
         if constexpr (std::integral<id_type>) {
            serialize<YAML>::op<Opts>(ids_v<Variant>[index], ctx, b, ix);
         }
         else {
            write_yaml_string<Opts>(sv{ids_v<Variant>[index]}, ctx, b, ix, indent_level);
         }
      }

      // Write a tagged variant alternative that holds an object as a block mapping, emitting the
      // discriminator entry (meta::tag: id) ahead of the object's own members so it round-trips.
      template <auto Opts, class Variant, class T, class B>
      inline void write_tagged_block_object(T&& inner, size_t index, is_context auto&& ctx, B&& b, auto& ix,
                                            int32_t indent_level, bool skip_first_indent)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});
         static constexpr sv tag = tag_v<Variant>;

         if (!skip_first_indent) {
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + tag.size() + 8)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }
         }
         dump(tag, b, ix);
         dump(": ", b, ix);
         write_variant_tag_id<Opts, Variant>(index, ctx, b, ix, indent_level);
         dump('\n', b, ix);

         // Remaining members are emitted at the mapping's indent (the tag occupied the first line).
         write_block_mapping<Opts>(inner, ctx, b, ix, indent_level, false);
      }

      // Write a tagged variant alternative that holds an object as a flow mapping
      // ({tag: id, key: value, ...}).
      template <auto Opts, class Variant, class T, class B>
      inline void write_tagged_flow_object(T&& inner, size_t index, is_context auto&& ctx, B&& b, auto& ix)
      {
         static constexpr sv tag = tag_v<Variant>;
         if (!ensure_space(ctx, b, ix + tag.size() + 8)) [[unlikely]] {
            return;
         }
         dump('{', b, ix);
         dump(tag, b, ix);
         dump(": ", b, ix);
         write_variant_tag_id<Opts, Variant>(index, ctx, b, ix, 0);
         bool first = false; // tag already written, so members need a leading ", "
         write_flow_mapping_members<Opts>(inner, ctx, b, ix, first);
         dump('}', b, ix);
      }

      // Write a tagged variant alternative whose body comes from a custom serializer (custom_write).
      // Like write_tagged_block_object, but the body is produced by serialize<YAML> rather than by
      // field reflection. The custom serializer is expected to emit an object body (e.g. a map);
      // it is driven at the variant's own indent so the tag and the body share one mapping.
      template <auto Opts, class Variant, class T, class B>
      inline void write_tagged_block_custom(T&& inner, size_t index, is_context auto&& ctx, B&& b, auto& ix,
                                            int32_t indent_level, bool skip_first_indent)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});
         static constexpr sv tag = tag_v<Variant>;

         if (!skip_first_indent) {
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + tag.size() + 8)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }
         }
         dump(tag, b, ix);
         dump(": ", b, ix);
         write_variant_tag_id<Opts, Variant>(index, ctx, b, ix, indent_level);
         dump('\n', b, ix);

         // The YAML writer threads indentation through explicit parameters, not the context, so a
         // custom serializer (which can only call serialize<YAML>) always emits its body relative to
         // column 0. Render the body, then re-indent every line to the mapping's column so the tag
         // and the body share one block mapping. Relative indentation within the body is preserved.
         std::string body;
         size_t body_ix = 0;
         serialize<YAML>::op<Opts>(inner, ctx, body, body_ix);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         const int32_t spaces = indent_level * indent_width;
         const std::string_view body_sv{body.data(), body_ix};
         size_t line_start = 0;
         while (line_start < body_sv.size()) {
            const size_t nl = body_sv.find('\n', line_start);
            const size_t line_end = (nl == std::string_view::npos) ? body_sv.size() : nl;
            const std::string_view line = body_sv.substr(line_start, line_end - line_start);
            if (!line.empty()) {
               if (!ensure_space(ctx, b, ix + size_t(spaces) + line.size() + 2)) [[unlikely]] {
                  return;
               }
               for (int32_t i = 0; i < spaces; ++i) {
                  b[ix++] = ' ';
               }
               dump(line, b, ix);
            }
            if (nl == std::string_view::npos) {
               break;
            }
            dump('\n', b, ix);
            line_start = nl + 1;
         }
      }
   } // namespace yaml

   // glaze_object_t and reflectable (structs)
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
            yaml::write_flow_mapping<Opts>(value, ctx, b, ix);
         }
         else {
            int32_t indent_level = 0;
            if constexpr (requires { ctx.indent_level; }) {
               indent_level = ctx.indent_level;
            }
            yaml::write_block_mapping<Opts>(value, ctx, b, ix, indent_level);
         }
      }
   };

   // writable_map_t (std::map, std::unordered_map, etc.)
   template <writable_map_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
            // Flow style
            if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
               return;
            }
            dump('{', b, ix);

            bool first = true;
            for (auto&& [k, v] : value) {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (!first) {
                  dump(", ", b, ix);
               }
               first = false;

               using key_t = std::remove_cvref_t<decltype(k)>;
               if constexpr (str_t<key_t>) {
                  yaml::write_yaml_string<Opts>(sv{k}, ctx, b, ix);
               }
               else {
                  serialize<YAML>::op<yaml::flow_context_on<Opts>()>(k, ctx, b, ix);
               }
               dump(": ", b, ix);
               serialize<YAML>::op<yaml::flow_context_on<Opts>()>(v, ctx, b, ix);
            }

            dump('}', b, ix);
         }
         else {
            // Block style
            int32_t indent_level = 0;
            if constexpr (requires { ctx.indent_level; }) {
               indent_level = ctx.indent_level;
            }
            yaml::write_block_mapping_nested<Opts>(value, ctx, b, ix, indent_level);
         }
      }
   };

   // Variant support
   template <is_variant T>
      requires(not custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         using V = std::remove_cvref_t<T>;
         // Tagged variants emit a discriminator entry (meta::tag) when holding an object, so the
         // output names which alternative it is and round-trips through the reader.
         if constexpr (check_write_type_info(Opts) && not tag_v<V>.empty()) {
            const size_t index = value.index();
            std::visit(
               [&](auto&& v) {
                  using Vt = std::remove_cvref_t<decltype(v)>;
                  // not custom_write: a custom alternative that also happens to be reflectable (e.g.
                  // a non-aggregate class under P2996 reflection) must take the custom branch below.
                  if constexpr ((glaze_object_t<Vt> || reflectable<Vt>) && not custom_write<Vt>) {
                     if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
                        yaml::write_tagged_flow_object<Opts, V>(v, index, ctx, b, ix);
                     }
                     else {
                        int32_t indent_level = 0;
                        if constexpr (requires { ctx.indent_level; }) {
                           indent_level = ctx.indent_level;
                        }
                        yaml::write_tagged_block_object<Opts, V>(v, index, ctx, b, ix, indent_level, false);
                     }
                  }
                  else if constexpr (custom_write<Vt>) {
                     // Custom alternative: merge the tag into the body the custom serializer emits.
                     if constexpr (yaml::check_flow_style(Opts) || yaml::check_flow_context(Opts)) {
                        ctx.error = error_code::feature_not_supported;
                        ctx.custom_error_message = "custom variant alternatives are not supported in YAML flow style";
                     }
                     else {
                        int32_t indent_level = 0;
                        if constexpr (requires { ctx.indent_level; }) {
                           indent_level = ctx.indent_level;
                        }
                        yaml::write_tagged_block_custom<Opts, V>(v, index, ctx, b, ix, indent_level, false);
                     }
                  }
                  else {
                     // Non-object alternatives (scalars, null) carry no tag, matching JSON.
                     serialize<YAML>::op<Opts>(v, ctx, b, ix);
                  }
               },
               value);
         }
         else {
            std::visit([&](auto&& v) { serialize<YAML>::op<Opts>(v, ctx, b, ix); }, value);
         }
      }
   };

   // Convenience functions

   template <auto Opts = yaml::yaml_opts{}, class T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_yaml(T&& value, Buffer& buffer) noexcept
   {
      return write<set_yaml<Opts>()>(std::forward<T>(value), buffer);
   }

   template <auto Opts = yaml::yaml_opts{}, class T>
   [[nodiscard]] expected<std::string, error_ctx> write_yaml(T&& value) noexcept
   {
      return write<set_yaml<Opts>()>(std::forward<T>(value));
   }

   template <auto Opts = yaml::yaml_opts{}, class T>
   [[nodiscard]] error_ctx write_file_yaml(T&& value, const sv file_path) noexcept
   {
      std::string buffer;
      auto ec = write<set_yaml<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) {
         return ec;
      }
      const auto file_ec = buffer_to_file(buffer, file_path);
      if (bool(file_ec)) {
         return {0, file_ec};
      }
      return {};
   }

} // namespace glz
