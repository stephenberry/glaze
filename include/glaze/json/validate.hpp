// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_format.hpp"
#include "glaze/json/read.hpp"

namespace glz
{
   namespace detail {
      template <opts Opts>
      inline void validate_json(is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         using enum json_type;

         int64_t depth = 0;

         auto skip_whitespace = [&] {
            if constexpr (Opts.null_terminated) {
               while (whitespace_table[static_cast<uint8_t>(*it)]) {
                  ++it;
               }
            }
            else {
               while ((it < end) && whitespace_table[static_cast<uint8_t>(*it)]) {
                  ++it;
               }
            }
         };

         skip_whitespace();
         
         std::string string{};
         double number{};

         while (true) {
            if constexpr (not Opts.null_terminated) {
               if (it >= end) break;
            }
            else {
               if (*it == '\0') break;
            }

            switch (json_types[static_cast<uint8_t>(*it)]) {
               case String: {
                  from<JSON, std::string>::template op<Opts>(string, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  skip_whitespace();
                  break;
               }
               case Comma: {
                  ++it;
                  skip_whitespace();
                  break;
               }
               case Number: {
                  from<JSON, double>::template op<Opts>(number, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  skip_whitespace();
                  break;
               }
               case Colon: {
                  ++it;
                  skip_whitespace();
                  break;
               }
               case Array_Start: {
                  ++it;
                  ++depth;
                  skip_whitespace();
                  break;
               }
               case Array_End: {
                  ++it;
                  if (depth <= 0) {
                     ctx.error = error_code::syntax_error; // Unmatched array end
                     return;
                  }
                  --depth;
                  skip_whitespace();
                  break;
               }
               case Null: {
                  if ((it + 4) > end) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  if (not comparitor<chars<"null">>(it)) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  it += 4;
                  skip_whitespace();
                  break;
               }
               case Bool: {
                  if (*it == 't') {
                     if ((it + 4) > end) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     if (not comparitor<chars<"true">>(it)) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     it += 4;
                  } else {
                     if ((it + 5) > end) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     if (not comparitor<chars<"false">>(it)) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     it += 5;
                  }
                  if (it > end) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  skip_whitespace();
                  break;
               }
               case Object_Start: {
                  ++it;
                  ++depth;
                  skip_whitespace();
                  break;
               }
               case Object_End: {
                  ++it;
                  if (depth <= 0) {
                     ctx.error = error_code::syntax_error; // Unmatched object end
                     return;
                  }
                  --depth;
                  skip_whitespace();
                  break;
               }
               case Comment: {
                  if constexpr (Opts.comments) {
                     // TODO: use this for JSONC validation
                  }
                  else {
                     [[fallthrough]];
                  }
               }
               default: {
                  // Unknown token or syntax error
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            
            if (depth == 0) {
               skip_whitespace();
               if constexpr (Opts.null_terminated) {
                  if (*it != '\0') {
                     ctx.error = error_code::syntax_error; // Extra tokens after main value
                     return;
                  }
               }
               else {
                  if (it < end) {
                     ctx.error = error_code::syntax_error; // Extra tokens after main value
                     return;
                  }
               }
               break; // Exit the loop as the main value has been fully parsed
            }

            if (size_t(depth) > max_recursive_depth_limit) {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         if (depth != 0) {
            ctx.error = error_code::syntax_error; // Mismatched brackets/braces
            return;
         }

         // If we reach here without errors, the JSON is valid
      }

      template <opts Opts, class In>
      [[nodiscard]] error_ctx validate_json(is_context auto&& ctx, In&& in) noexcept
      {
         if constexpr (resizable<In>) {
            in.resize(in.size() + padding_bytes);
         }
         
         auto [it, end] = read_iterators<Opts, true>(in);
         if (bool(ctx.error)) [[unlikely]] {
            return {ctx.error, ctx.custom_error_message, 0};
         }
         
         auto* start = it;
         
         if constexpr (resizable<In>) {
            static constexpr auto O = is_padded_on<Opts>();
            if constexpr (string_t<In>) {
               validate_json<opt_true<O, &opts::null_terminated>>(ctx, it, end);
            }
            else {
               validate_json<opt_false<O, &opts::null_terminated>>(ctx, it, end);
            }
         }
         else {
            if constexpr (string_t<In>) {
               validate_json<opt_true<Opts, &opts::null_terminated>>(ctx, it, end);
            }
            else {
               validate_json<opt_false<Opts, &opts::null_terminated>>(ctx, it, end);
            }
         }
         
         error_ctx result{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
         if constexpr (resizable<In>) {
            in.resize(in.size() - padding_bytes);
         }
         return result;
      }
   }

   template <opts Opts = opts{}>
   [[nodiscard]] error_ctx validate_json(resizable auto& in) noexcept
   {
      context ctx{};
      return detail::validate_json<Opts>(ctx, in);
   }
   
   template <opts Opts = opts{}>
   [[nodiscard]] error_ctx validate_json(const sv& in) noexcept
   {
      context ctx{};
      return detail::validate_json<Opts>(ctx, in);
   }
}
