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
            while (detail::whitespace_table[uint8_t(*it)]) {
               ++it;
            }
         };

         while (it < end) {
            switch (*it) {
            case '{': {
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
                     while (it != end && !(*it == '}' && (it + 1 != end && *(it + 1) == '}'))) {
                        ++it;
                     }
                     if (it != end) {
                        it += 2; // Skip '}}'
                     }
                     break;
                  }

                  if (is_section || is_inverted_section) {
                     ctx.error = error_code::feature_not_supported;
                     return {ctx.error, "Sections are not yet supported", size_t(it - start)};
                  }

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
                                    get_member(value, get<I>(to_tuple(value))), ctx, buffer, ix);
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

                  // Handle closing braces assuming unescaped
                  if (*it == '}') {
                     ++it;
                     if (it != end && *it == '}') {
                        ++it;
                        break;
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
                  ++it;
               }
               break;
            }
            default: {
               buffer.push_back(*it);
               ++it;
            }
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
