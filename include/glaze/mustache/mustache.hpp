// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/refl.hpp"
#include "glaze/core/write.hpp"

namespace glz
{
   template <opts Opts = opts{}, class T, class Template>
   expected<std::string, error_ctx> mustache(T&& value, Template&& tmp) noexcept
   {
      std::string result{};

      context ctx{};

      if (tmp.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return unexpected(error_ctx{ctx.error, ctx.custom_error_message, 0, ctx.includer_error});
      }

      auto p = read_iterators<Opts, false>(ctx, tmp);
      auto it = p.first;
      auto end = p.second;
      auto start = it;
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
               if (*it == '{') {
                  ++it;
                  skip_whitespace();

                  auto s = it;
                  while (it != end && *it != '}') {
                     ++it;
                  }

                  sv key{s, size_t(it - s)};
                  
                  static constexpr auto N = refl<T>.N;
                  static constexpr auto HashInfo = detail::hash_info<T>;
                                    
                  const auto index =
                     detail::decode_hash<T, HashInfo, HashInfo.type>::op(key.data(), key.data() + key.size());
                  
                  if (index >= N) [[unlikely]] {
                     ctx.error = error_code::unknown_key;
                     return unexpected(
                        error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
                  }
                  else {
                     static thread_local std::string temp{};
                     detail::jump_table<N>([&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(refl<T>.keys);
                        static constexpr auto Length = TargetKey.size();
                        if (((s + Length) < end) && compare<Length>(TargetKey.data(), s)) [[likely]] {
                           if constexpr (detail::reflectable<T> && N > 0) {
                              std::ignore =
                                 write<opt_true<Opts, &opts::raw>>(detail::get_member(value, get<I>(detail::to_tuple(value))), temp, ctx);
                           }
                           else if constexpr (detail::glaze_object_t<T> && N > 0) {
                              std::ignore =
                                 write<opt_true<Opts, &opts::raw>>(detail::get_member(value, get<I>(refl<T>.values)), temp, ctx);
                           }
                        }
                        else {
                           ctx.error = error_code::unknown_key;
                        }
                     }, index);
                     
                     if (bool(ctx.error)) [[unlikely]] {
                        return unexpected(
                           error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
                     }
                     
                     result.append(temp);
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
         return unexpected(error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
      }

      return {result};
   }

   template <opts Opts = opts{}, class T>
      requires(requires { std::decay_t<T>::glaze_mustache; })
   expected<std::string, error_ctx> mustache(T&& value) noexcept
   {
      return mustache(std::forward<T>(value), sv{std::decay_t<T>::glaze_mustache});
   }
}
