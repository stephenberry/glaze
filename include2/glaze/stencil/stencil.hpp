// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/write.hpp"

namespace glz
{
   template <opts Opts = opts{}, class Template, class T, resizable Buffer>
   [[nodiscard]] error_ctx stencil(Template&& layout, T&& value, Buffer& buffer)
   {
      context ctx{};

      if (layout.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return {ctx.error, ctx.custom_error_message, 0};
      }

      auto p = read_iterators<Opts, false>(layout);
      auto it = p.first;
      auto end = p.second;
      auto outer_start = it;

      if (not bool(ctx.error)) [[likely]] {
         auto skip_whitespace = [&] {
            while (it < end && detail::whitespace_table[uint8_t(*it)]) {
               ++it;
            }
         };

         while (it < end) {
            if (*it == '{') {
               ++it;
               if (it != end && *it == '{') {
                  ++it;
                  bool is_section = false;
                  bool is_inverted_section = false;
                  bool is_comment = false;

                  if (it != end) {
                     if (*it == '!') {
                        ++it;
                        is_comment = true;
                     }
                     else if (*it == '#') {
                        ++it;
                        is_section = true;
                     }
                     else if (*it == '^') {
                        ++it;
                        is_inverted_section = true;
                     }
                  }

                  skip_whitespace();

                  auto start = it;
                  while (it != end && *it != '}' && *it != ' ' && *it != '\t') {
                     ++it;
                  }

                  if (it == end) {
                     ctx.error = error_code::unexpected_end;
                     return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                  }

                  const sv key{start, size_t(it - start)};

                  skip_whitespace();

                  if (is_comment) {
                     while (it < end && !(it + 1 < end && *it == '}' && *(it + 1) == '}')) {
                        ++it;
                     }
                     if (it + 1 < end) {
                        it += 2; // Skip '}}'
                     }
                     continue;
                  }

                  if (is_section || is_inverted_section) {
                     // Find the closing tag '{{/key}}'
                     std::string closing_tag = "{{/" + std::string(key) + "}}";
                     auto closing_pos = std::search(it, end, closing_tag.begin(), closing_tag.end());

                     if (closing_pos == end) {
                        ctx.error = error_code::unexpected_end;
                        return {ctx.error, "Closing tag not found for section", size_t(it - start)};
                     }

                     if (it + 1 < end) {
                        it += 2; // Skip '}}'
                     }

                     // Extract inner template between current position and closing tag
                     std::string_view inner_template(it, closing_pos);
                     it = closing_pos + closing_tag.size();

                     // Retrieve the value associated with 'key'
                     bool condition = false;
                     {
                        static constexpr auto N = reflect<T>::size;
                        static constexpr auto HashInfo = detail::hash_info<T>;

                        const auto index = detail::decode_hash_with_size<STENCIL, T, HashInfo, HashInfo.type>::op(
                           start, end, key.size());

                        if (index >= N) {
                           ctx.error = error_code::unknown_key;
                           return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                        }
                        else {
                           visit<N>(
                              [&]<size_t I>() {
                                 static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                                 if (TargetKey == key) [[likely]] {
                                    if constexpr (detail::bool_t<refl_t<T, I>>) {
                                       if constexpr (detail::reflectable<T>) {
                                          condition = bool(get_member(value, get<I>(to_tie(value))));
                                       }
                                       else if constexpr (detail::glaze_object_t<T>) {
                                          condition = bool(get_member(value, get<I>(reflect<T>::values)));
                                       }
                                    }
                                    else {
                                       // For non-boolean types
                                       ctx.error = error_code::syntax_error;
                                    }
                                 }
                                 else {
                                    ctx.error = error_code::unknown_key;
                                 }
                              },
                              index);
                        }
                     }

                     if (bool(ctx.error)) [[unlikely]] {
                        return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                     }

                     // If it's an inverted section, include inner content if condition is false
                     // Otherwise (regular section), include if condition is true
                     bool should_include = is_inverted_section ? !condition : condition;

                     if (should_include) {
                        // Recursively process the inner template
                        std::string inner_buffer;
                        auto inner_ec = stencil<Opts>(inner_template, value, inner_buffer);
                        if (inner_ec) {
                           return inner_ec;
                        }
                        buffer.append(inner_buffer);
                     }

                     skip_whitespace();
                     continue;
                  }

                  // Handle regular placeholder
                  static constexpr auto N = reflect<T>::size;
                  static constexpr auto HashInfo = detail::hash_info<T>;

                  const auto index =
                     detail::decode_hash_with_size<STENCIL, T, HashInfo, HashInfo.type>::op(start, end, key.size());

                  if (index >= N) [[unlikely]] {
                     ctx.error = error_code::unknown_key;
                     return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                  }
                  else [[likely]] {
                     size_t ix = buffer.size(); // overwrite index
                     if (buffer.empty()) {
                        buffer.resize(2 * write_padding_bytes);
                     }

                     static constexpr auto RawOpts = opt_true<Opts, &opts::raw>;

                     visit<N>(
                        [&]<size_t I>() {
                           static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                           if ((TargetKey.size() == key.size()) && detail::comparitor<TargetKey>(start)) [[likely]] {
                              if constexpr (detail::reflectable<T>) {
                                 detail::write<Opts.format>::template op<RawOpts>(
                                    get_member(value, get<I>(to_tie(value))), ctx, buffer, ix);
                              }
                              else if constexpr (detail::glaze_object_t<T>) {
                                 detail::write<Opts.format>::template op<RawOpts>(
                                    get_member(value, get<I>(reflect<T>::values)), ctx, buffer, ix);
                              }
                           }
                           else {
                              ctx.error = error_code::unknown_key;
                           }
                        },
                        index);

                     if (bool(ctx.error)) [[unlikely]] {
                        return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                     }

                     buffer.resize(ix);
                  }

                  skip_whitespace();

                  if (*it == '}') {
                     ++it;
                     if (it != end && *it == '}') {
                        ++it;
                        continue;
                     }
                     else {
                        buffer.append("}");
                     }
                  }
                  else {
                     ctx.error = error_code::syntax_error;
                     return {ctx.error, ctx.custom_error_message, size_t(it - start)};
                  }
               }
               else {
                  buffer.append("{");
                  // 'it' is already incremented past the first '{'
               }
            }
            else {
               buffer.push_back(*it);
               ++it;
            }
         }
      }

      if (bool(ctx.error)) [[unlikely]] {
         return {ctx.error, ctx.custom_error_message, size_t(it - outer_start)};
      }

      return {};
   }

   template <opts Opts = opts{}, class Template, class T>
   [[nodiscard]] expected<std::string, error_ctx> stencil(Template&& layout, T&& value)
   {
      std::string buffer{};
      auto ec = stencil(std::forward<Template>(layout), std::forward<T>(value), buffer);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return {buffer};
   }
}
