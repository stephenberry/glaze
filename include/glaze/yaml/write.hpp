// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/buffer_traits.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/to.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
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
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
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
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
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

   // std::nullptr_t
   template <>
   struct to<YAML, std::nullptr_t>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(std::nullptr_t, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE void write_double_quoted_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE void write_single_quoted_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE void write_literal_block(std::string_view str, is_context auto&& ctx, B&& b, auto& ix,
                                                 int32_t indent_level, uint8_t indent_width)
      {
         if (!ensure_space(ctx, b, ix + str.size() + 64 + write_padding_bytes)) [[unlikely]] {
            return;
         }

         dump("|\n", b, ix);

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
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE void write_yaml_string(std::string_view str, is_context auto&& ctx, B&& b, auto& ix,
                                               int32_t indent_level = 0)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

         // Check if literal block style is appropriate (multiline strings)
         if (str.find('\n') != std::string_view::npos && str.size() > 40) {
            write_literal_block(str, ctx, b, ix, indent_level, indent_width);
            return;
         }

         // Check if string needs quoting
         if (yaml::needs_quoting(str)) {
            // Prefer single quotes if no single quotes in string
            if (str.find('\'') == std::string_view::npos) {
               write_single_quoted_string(str, ctx, b, ix);
            }
            else {
               write_double_quoted_string(str, ctx, b, ix);
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
   } // namespace yaml

   // str_t (strings)
   template <str_t T>
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         serialize<YAML>::op<Opts>(static_cast<std::underlying_type_t<std::decay_t<T>>>(value), ctx,
                                   std::forward<Args>(args)...);
      }
   };

   namespace yaml
   {
      // Helper to check if a type is "simple" (writes on same line)
      template <class T>
      consteval bool is_simple_type()
      {
         using V = std::remove_cvref_t<T>;
         return bool_t<V> || num_t<V> || str_t<V> || char_t<V> || std::is_enum_v<V>;
      }

      // Runtime check if a variant currently holds a simple type
      template <class T>
      GLZ_ALWAYS_INLINE bool variant_holds_simple_type([[maybe_unused]] const T& value)
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

      // Write block-style sequence
      template <auto Opts, class T, class B>
      GLZ_ALWAYS_INLINE void write_block_sequence(T&& value, is_context auto&& ctx, B&& b, auto& ix,
                                                  int32_t indent_level)
      {
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

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
            dump("- ", b, ix);

            if constexpr (is_simple_type<element_t>()) {
               serialize<YAML>::op<Opts>(element, ctx, b, ix);
               dump('\n', b, ix);
            }
            else if constexpr (is_or_wraps_variant<element_t>()) {
               // For variants, check at runtime if they hold a simple type
               if (variant_holds_simple_type(element)) {
                  serialize<YAML>::op<Opts>(element, ctx, b, ix);
                  dump('\n', b, ix);
               }
               else {
                  // Complex variant content (maps/arrays) uses flow style ({...}, [...]) rather than
                  // block style. This is a pragmatic choice: proper block-style output would require
                  // tracking indentation context through the variant visitor, which adds significant
                  // complexity. Flow style produces valid, parseable YAML that round-trips correctly.
                  serialize<YAML>::op<flow_context_on<Opts>()>(element, ctx, b, ix);
                  dump('\n', b, ix);
               }
            }
            else {
               // Complex type - write on next line with increased indent
               dump('\n', b, ix);
               serialize<YAML>::op<Opts>(element, ctx, b, ix);
            }
         }
      }

      // Write flow-style sequence
      template <auto Opts, class T, class B>
      GLZ_ALWAYS_INLINE void write_flow_sequence(T&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE void write_block_mapping_nested(T&& value, is_context auto&& ctx, B&& b, auto& ix,
                                                        int32_t indent_level);

      // Write block-style mapping
      template <auto Opts, class T, class B>
      GLZ_ALWAYS_INLINE void write_block_mapping(T&& value, is_context auto&& ctx, B&& b, auto& ix,
                                                 int32_t indent_level)
      {
         using V = std::remove_cvref_t<T>;
         constexpr auto N = reflect<V>::size;
         constexpr uint8_t indent_width = check_indent_width(yaml_opts{});

         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;

            using val_t = field_t<V, I>;
            static constexpr sv key = get<I>(reflect<V>::keys);

            // Skip null members if configured
            if constexpr (nullable_like<val_t>) {
               if constexpr (Opts.skip_null_members) {
                  auto&& member = get_member(value, get<I>(reflect<V>::values));
                  if (!member) {
                     return;
                  }
               }
            }

            // Write indentation
            const int32_t spaces = indent_level * indent_width;
            if (!ensure_space(ctx, b, ix + spaces + key.size() + 8)) [[unlikely]] {
               return;
            }
            for (int32_t i = 0; i < spaces; ++i) {
               b[ix++] = ' ';
            }

            // Write key
            dump(key, b, ix);
            dump(':', b, ix);

            // Get member value
            decltype(auto) member = get_member(value, get<I>(reflect<V>::values));

            if constexpr (is_simple_type<val_t>() || nullable_like<val_t>) {
               // Simple types go on same line
               dump(' ', b, ix);
               serialize<YAML>::op<Opts>(member, ctx, b, ix);
               dump('\n', b, ix);
            }
            else {
               // Complex types go on next line with increased indent
               dump('\n', b, ix);

               // Create a modified context with incremented indent level
               if constexpr (requires { ctx.indent_level; }) {
                  auto nested_ctx = ctx;
                  nested_ctx.indent_level = indent_level + 1;
                  serialize<YAML>::op<Opts>(member, nested_ctx, b, ix);
               }
               else {
                  // Pass indent through opts internal state - simplified approach
                  // For now, just write at next indent level
                  write_block_mapping_nested<Opts>(member, ctx, b, ix, indent_level + 1);
               }
            }
         });
      }

      // Helper for nested objects
      template <auto Opts, class T, class B>
      GLZ_ALWAYS_INLINE void write_block_mapping_nested(T&& value, is_context auto&& ctx, B&& b, auto& ix,
                                                        int32_t indent_level)
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
            // Handle variants by visiting and recursing with the same indent level
            std::visit([&](auto&& inner) { write_block_mapping_nested<Opts>(inner, ctx, b, ix, indent_level); }, value);
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
               if constexpr (is_simple_type<val_t>()) {
                  dump(' ', b, ix);
                  serialize<YAML>::op<Opts>(v, ctx, b, ix);
                  dump('\n', b, ix);
               }
               else if constexpr (is_or_wraps_variant<val_t>()) {
                  // For variants and types wrapping variants (like glz::generic),
                  // check at runtime if they hold a simple type
                  if (variant_holds_simple_type(v)) {
                     dump(' ', b, ix);
                     serialize<YAML>::op<Opts>(v, ctx, b, ix);
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

      // Write flow-style mapping
      template <auto Opts, class T, class B>
      GLZ_ALWAYS_INLINE void write_flow_mapping(T&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         using V = std::remove_cvref_t<T>;
         constexpr auto N = reflect<V>::size;

         if (!ensure_space(ctx, b, ix + 8)) [[unlikely]] {
            return;
         }
         dump('{', b, ix);

         bool first = true;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]]
               return;

            using val_t = field_t<V, I>;
            static constexpr sv key = get<I>(reflect<V>::keys);

            // Skip null members if configured
            if constexpr (nullable_like<val_t>) {
               if constexpr (Opts.skip_null_members) {
                  auto&& member = get_member(value, get<I>(reflect<V>::values));
                  if (!member) {
                     return;
                  }
               }
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

            decltype(auto) member = get_member(value, get<I>(reflect<V>::values));
            serialize<YAML>::op<flow_context_on<Opts>()>(member, ctx, b, ix);
         });

         dump('}', b, ix);
      }
   } // namespace yaml

   // glaze_object_t and reflectable (structs)
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
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
   struct to<YAML, T>
   {
      template <auto Opts, class B>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
      {
         std::visit([&](auto&& v) { serialize<YAML>::op<Opts>(v, ctx, b, ix); }, value);
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
