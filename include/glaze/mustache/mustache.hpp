// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/write.hpp"

namespace glz
{
   template <opts Opts = opts{}, class T, class Template, class Buffer>
   [[nodiscard]] error_ctx mustache(T&& value, Template&& tmp, Buffer& buffer)
   {
      context ctx{};

      if (tmp.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return {ctx.error, ctx.custom_error_message, 0, ctx.includer_error};
      }

      auto p = read_iterators<Opts, false>(tmp);
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
                  bool unescaped = false;
                  bool is_section = false;
                  bool is_inverted_section = false;
                  bool is_comment = false;
                  [[maybe_unused]] bool is_partial = false;

                  if (it != end) {
                     if (*it == '{') {
                        ++it;
                        unescaped = true;
                     }
                     else if (*it == '&') {
                        ++it;
                        unescaped = true;
                     }
                     else if (*it == '!') {
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
                     return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
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
                     return {ctx.error, "Sections are not yet supported", size_t(it - start), ctx.includer_error};
                  }

                  static constexpr auto N = reflect<T>::size;
                  static constexpr auto HashInfo = detail::hash_info<T>;

                  const auto index =
                     detail::decode_hash_with_size<MUSTACHE, T, HashInfo, HashInfo.type>::op(start, end, key.size());

                  if (index >= N) [[unlikely]] {
                     ctx.error = error_code::unknown_key;
                     return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
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
                           static constexpr auto Length = TargetKey.size();
                           if ((Length == key.size()) && detail::comparitor<TargetKey>(start)) [[likely]] {
                              if constexpr (detail::reflectable<T> && N > 0) {
                                 detail::write<Opts.format>::template op<RawOpts>(get_member(value, get<I>(to_tuple(value))), ctx, buffer, ix);
                              }
                              else if constexpr (detail::glaze_object_t<T> && N > 0) {
                                 detail::write<Opts.format>::template op<RawOpts>(get_member(value, get<I>(reflect<T>::values)), ctx, buffer, ix);
                              }
                           }
                           else {
                              ctx.error = error_code::unknown_key;
                           }
                        },
                        index);

                     if (bool(ctx.error)) [[unlikely]] {
                        return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
                     }
                     
                     buffer.resize(ix);
                  }

                  skip_whitespace();

                  // Handle closing braces
                  if (unescaped) {
                     if (*it == '}') {
                        ++it;
                        if (it != end && *it == '}') {
                           ++it;
                           if (it != end && *it == '}') {
                              ++it;
                              break;
                           }
                        }
                     }
                     ctx.error = error_code::syntax_error;
                     return {ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
                  }
                  else {
                     if (*it == '}') {
                        ++it;
                        if (it != end && *it == '}') {
                           ++it;
                           break;
                        }
                        else {
                           buffer.append("}");
                        }
                        break;
                     }
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
         return {ctx.error, ctx.custom_error_message, size_t(it - outer_start), ctx.includer_error};
      }

      return {};
   }
   
   template <opts Opts = opts{}, class T, class Buffer>
      requires(requires { std::decay_t<T>::glaze_mustache; })
   [[nodiscard]] error_ctx mustache(T&& value, Buffer& buffer)
   {
      return mustache(std::forward<T>(value), sv{std::decay_t<T>::glaze_mustache}, buffer);
   }
   
   template <opts Opts = opts{}, class T, class Template>
   [[nodiscard]] expected<std::string, error_ctx> mustache(T&& value, Template&& tmp)
   {
      std::string buffer{};
      auto ec = mustache(std::forward<T>(value), std::forward<Template>(tmp), buffer);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return {buffer};
   }

   template <opts Opts = opts{}, class T>
      requires(requires { std::decay_t<T>::glaze_mustache; })
   [[nodiscard]] expected<std::string, error_ctx> mustache(T&& value)
   {
      std::string buffer{};
      auto ec = mustache(std::forward<T>(value), sv{std::decay_t<T>::glaze_mustache}, buffer);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return {buffer};
   }
}
