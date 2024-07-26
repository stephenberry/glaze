// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <filesystem>

#include "glaze/core/common.hpp"

namespace glz
{
   // Register this with an object to allow loading a file when a valid file path is given as a string
   // If the file does not exist, the string is handled as a glz::raw_json
   // This enables file including of unknown structures that will be decoded in the future when state is known
   struct raw_or_file final
   {
      std::string str = R"("")";
   };

   namespace detail
   {
      template <>
      struct from_json<raw_or_file>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr auto Opts = opt_true<ws_handled_off<Options>(), &opts::null_terminated>;
            auto& v = value;
            // check if we are decoding a string, which could be a file path
            if (*it == '"') {
               read<json>::op<Opts>(v.str, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               namespace fs = std::filesystem;
               const auto path = relativize_if_not_absolute(fs::path(ctx.current_file).parent_path(), fs::path{v.str});

               if (fs::exists(path) && fs::is_regular_file(path)) {
                  const auto string_path = path.string();
                  const auto ec = file_to_buffer(v.str, string_path);

                  if (bool(ec)) [[unlikely]] {
                     ctx.error = error_code::includer_error;
                     auto& error_msg = error_buffer();
                     error_msg = "file failed to open: " + string_path;
                     ctx.includer_error = error_msg;
                     return;
                  }

                  //const auto ecode = validate_jsonc(v.str);
                  // A custom validate_jsonc that sets null_terminated = true
                  context new_ctx{};
                  glz::skip skip_value{};
                  const auto ecode = glz::read<opts{.null_terminated = true, .comments = true, .validate_skipped = true, .validate_trailing_whitespace = true}>(skip_value, v.str, new_ctx);
                  if (ecode) [[unlikely]] {
                     ctx.error = error_code::includer_error;
                     auto& error_msg = error_buffer();
                     error_msg = glz::format_error(ecode, v.str);
                     ctx.includer_error = error_msg;
                     return;
                  }
               }
               else {
                  // The file path doesn't exist, so we want a string with quotes
                  // But, we skipped the quotes when first reading
                  // So, now we add back the quotes
                  v.str = "\"" + v.str + "\"";
               }
            }
            else {
               auto it_start = it;
               skip_value<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.str = {it_start, static_cast<size_t>(it - it_start)};
            }
         }
      };

      template <>
      struct to_json<raw_or_file>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            dump_maybe_empty(value.str, b, ix);
         }
      };
   }
}
