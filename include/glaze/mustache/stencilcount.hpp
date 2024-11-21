// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/write.hpp"
#include "glaze/format/format_to.hpp"

namespace glz
{
   template <opts Opts = opts{}, class T, class Template>
   expected<std::string, error_ctx> stencilcount(T&& value, Template&& tmp)
   {
      std::string result{};

      context ctx{};

      if (tmp.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return unexpected(error_ctx{ctx.error, ctx.custom_error_message, 0, ctx.includer_error});
      }

      auto [it, end] = read_iterators<Opts, false>(tmp);
      auto outer_start = it;
      if (tmp.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
      }
      if (not bool(ctx.error)) [[likely]] {
         auto skip_whitespace = [&] {
            while (detail::whitespace_table[uint8_t(*it)]) {
               ++it;
            }
         };

         std::unordered_map<uint64_t, uint64_t> numbering{};
         uint64_t major_count{};
         uint64_t prev_count{};

         while (it < end) {
            switch (*it) {
            case '{': {
               ++it;
               if (*it == '{') {
                  ++it;
                  skip_whitespace();

                  uint64_t count{};
                  while (*it == '+') {
                     ++it;
                     ++count;
                  }

                  if (count < prev_count) {
                     numbering.clear();
                  }

                  if (count == 1) {
                     ++major_count;
                     format_to(result, major_count);
                     result.append(".");
                  }
                  else if (count > 1) {
                     format_to(result, major_count);

                     for (size_t i = 1; i < count; ++i) {
                        result.append(".");
                        auto& x = numbering[i];
                        if (i == (count - 1)) {
                           ++x;
                        }
                        format_to(result, x);
                     }
                  }

                  if (count > 0) {
                     prev_count = count;
                  }

                  if (*it == '}') {
                     ++it;
                     if (*it == '}') {
                        ++it;
                        break;
                     }
                     else {
                        result.append("}");
                     }
                     break;
                  }

                  auto start = it;
                  while (it != end && *it != '}' && *it != ' ' && *it != '\t') {
                     ++it;
                  }

                  const sv key{start, size_t(it - start)};

                  skip_whitespace();

                  static constexpr auto N = reflect<T>::size;
                  static constexpr auto HashInfo = detail::hash_info<T>;

                  const auto index =
                     detail::decode_hash_with_size<MUSTACHE, T, HashInfo, HashInfo.type>::op(start, end, key.size());

                  if (index < N) [[likely]] {
                     static thread_local std::string temp{};
                     detail::jump_table<N>(
                        [&]<size_t I>() {
                           static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                           static constexpr auto Length = TargetKey.size();
                           if ((Length == key.size()) && compare<Length>(TargetKey.data(), start)) [[likely]] {
                              if constexpr (detail::reflectable<T> && N > 0) {
                                 std::ignore = write<opt_true<Opts, &opts::raw>>(
                                    detail::get_member(value, get<I>(detail::to_tuple(value))), temp, ctx);
                              }
                              else if constexpr (detail::glaze_object_t<T> && N > 0) {
                                 std::ignore = write<opt_true<Opts, &opts::raw>>(
                                    detail::get_member(value, get<I>(reflect<T>::values)), temp, ctx);
                              }
                           }
                           else {
                              ctx.error = error_code::unknown_key;
                           }
                        },
                        index);

                     if (bool(ctx.error)) [[unlikely]] {
                        return unexpected(
                           error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
                     }

                     result.append(temp);
                  }
                  else {
                     // TODO: Is this an error?
                  }

                  skip_whitespace();

                  if (*it == '}') {
                     ++it;
                     if (*it == '}') {
                        ++it;
                        break;
                     }
                     else {
                        result.append("}");
                     }
                     break;
                  }
               }
               else {
                  result.push_back('{');
               }

               break;
            }
            default: {
               result.push_back(*it);
               ++it;
            }
            }
         }
      }

      if (bool(ctx.error)) [[unlikely]] {
         return unexpected(
            error_ctx{ctx.error, ctx.custom_error_message, size_t(it - outer_start), ctx.includer_error});
      }

      return {result};
   }
}
