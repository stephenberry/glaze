// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Minified JSONC only works with /**/ style comments, so we only supports this

#include "glaze/json/json_format.hpp"

namespace glz
{
   namespace detail
   {
      // We can use unchecked dumping to the output because we know minifying will not make the output any larger
      template <opts Opts>
      inline void minify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto&& ix) noexcept
      {
         using enum json_type;

         auto ws_start = it;
         uint64_t ws_size{};

         auto skip_expected_whitespace = [&] {
            auto new_ws_start = it;
            if (ws_size && ws_size < size_t(end - it)) [[likely]] {
               skip_matching_ws(ws_start, it, ws_size);
            }

            while (whitespace_table[*it]) {
               ++it;
            }
            ws_start = new_ws_start;
            ws_size = size_t(it - new_ws_start);
         };

         auto skip_whitespace = [&] {
            while (whitespace_table[*it]) {
               ++it;
            }
         };

         skip_whitespace();

         while (true) {
            switch (json_types[size_t(*it)]) {
            case String: {
               const auto value = read_json_string<Opts>(it, end);
               dump_unchecked(value, b, ix); // we couldn't have gotten here without a quote
               skip_whitespace();
               break;
            }
            case Comma: {
               dump_unchecked<','>(b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Number: {
               const auto value = read_json_number(it);
               dump_unchecked(value, b, ix); // we couldn't have gotten here without one valid character
               skip_whitespace();
               break;
            }
            case Colon: {
               dump_unchecked<':'>(b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Array_Start: {
               dump_unchecked<'['>(b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Array_End: {
               dump_unchecked<']'>(b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Null: {
               dump_unchecked<"null">(b, ix);
               it += 4;
               skip_whitespace();
               break;
            }
            case Bool: {
               if (*it == 't') {
                  dump_unchecked<"true">(b, ix);
                  it += 4;
                  skip_whitespace();
                  break;
               }
               else {
                  dump_unchecked<"false">(b, ix);
                  it += 5;
                  skip_whitespace();
                  break;
               }
            }
            case Object_Start: {
               dump_unchecked<'{'>(b, ix);
               ++it;
               skip_expected_whitespace();
               break;
            }
            case Object_End: {
               dump_unchecked<'}'>(b, ix);
               ++it;
               skip_whitespace();
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto value = read_jsonc_comment(it, end);
                  if (value.size()) [[likely]] {
                     dump_unchecked(value, b, ix);
                  }
                  skip_whitespace();
                  break;
               }
               else {
                  [[fallthrough]];
               }
            }
               [[unlikely]] default:
               {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
      }

      template <opts Opts, contiguous In, output_buffer Out>
      inline void minify_json(is_context auto&& ctx, In&& in, Out&& out) noexcept
      {
         if (in.empty()) {
            return;
         }

         if constexpr (resizable<Out>) {
            out.resize(in.size() + padding_bytes);
         }
         size_t ix = 0;
         auto [it, end] = read_iterators<Opts>(ctx, in);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         minify_json<opt_true<Opts, &opts::is_padded>>(ctx, it, end, out, ix);
         if constexpr (resizable<Out>) {
            out.resize(ix);
         }
      }
   }

   // We don't return errors from minifying even though they are handled because the error case
   // should not happen since we minify auto-generated JSON.
   // The detail version can be used if error context is needed

   template <opts Opts = opts{}>
   inline void minify_json(const auto& in, auto& out) noexcept
   {
      context ctx{};
      detail::minify_json<Opts>(ctx, in, out);
   }

   template <opts Opts = opts{}>
   inline std::string minify_json(const auto& in) noexcept
   {
      context ctx{};
      std::string out{};
      detail::minify_json<Opts>(ctx, in, out);
      return out;
   }

   template <opts Opts = opts{}>
   inline void minify_jsonc(const auto& in, auto& out) noexcept
   {
      context ctx{};
      detail::minify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
   }

   template <opts Opts = opts{}>
   inline std::string minify_jsonc(const auto& in) noexcept
   {
      context ctx{};
      std::string out{};
      detail::minify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return out;
   }
}
