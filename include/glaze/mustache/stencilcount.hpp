// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/read.hpp"
#include "glaze/core/refl.hpp"
#include "glaze/core/write.hpp"
#include "glaze/format/format_to.hpp"

namespace glz
{
   template <opts Opts = opts{}, class T, class Template>
   expected<std::string, error_ctx> stencilcount(T&& value, Template&& tmp) noexcept
   {
      std::string result{};

      context ctx{};

      if (tmp.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
         return unexpected(error_ctx{ctx.error, ctx.custom_error_message, 0, ctx.includer_error});
      }

      auto [it, end] = read_iterators<Opts, false>(ctx, tmp);
      auto start = it;
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

                  auto s = it;
                  while (it != end && *it != '}') {
                     ++it;
                  }

                  sv key{s, size_t(it - s)};

                  static constexpr auto num_members = refl<T>.N;

                  decltype(auto) frozen_map = [&]() -> decltype(auto) {
                     using V = decay_keep_volatile_t<decltype(value)>;
                     if constexpr (detail::reflectable<T> && num_members > 0) {
#if ((defined _MSC_VER) && (!defined __clang__))
                        static thread_local auto cmap = detail::make_map<V, Opts.use_hash_comparison>();
#else
                        static thread_local constinit auto cmap = detail::make_map<V, Opts.use_hash_comparison>();
#endif
                        // We want to run this populate outside of the while loop
                        populate_map(value, cmap); // Function required for MSVC to build
                        return cmap;
                     }
                     else if constexpr (detail::glaze_object_t<T> && num_members > 0) {
                        static constexpr auto cmap = detail::make_map<T, Opts.use_hash_comparison>();
                        return cmap;
                     }
                     else {
                        return nullptr;
                     }
                  }();

                  if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                     std::visit(
                        [&](auto&& member_ptr) {
                           static thread_local std::string temp{};
                           std::ignore =
                              write<opt_true<Opts, &opts::raw>>(detail::get_member(value, member_ptr), temp, ctx);
                           result.append(temp);
                        },
                        member_it->second);
                     if (bool(ctx.error)) [[unlikely]]
                        return unexpected(
                           error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
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
         return unexpected(error_ctx{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error});
      }

      return {result};
   }
}
