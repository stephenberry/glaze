// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"

#include <filesystem>

namespace glz
{
   // Register this with an object to allow loading a file when a valid file path is given as a string
   // If the file does not exist, the string is handled as a glz::raw_json
   // This enables file including of unknown structures that will be decoded in the future when state is known
   struct raw_or_file final
   {
      bool reflection_helper{};
      raw_json value = R"("")";
   };

   namespace detail
   {
      template <>
      struct from_json<raw_or_file>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr auto Opts = ws_handled_off<Options>();
            auto& v = value.value;
            // check if we are decoding a string, which could be a file path
            if (*it == '"') {
               read<json>::op<Opts>(v.str, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               
               namespace fs = std::filesystem;
               const auto path = relativize_if_not_absolute(fs::path(ctx.current_file).parent_path(),
                                                                 fs::path{v.str});
               
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
                  
                  const auto ecode = validate_json(v.str);
                  if (ecode) [[unlikely]] {
                     ctx.error = error_code::includer_error;
                     auto& error_msg = error_buffer();
                     error_msg = glz::format_error(ecode, v.str);
                     ctx.includer_error = error_msg;
                     return;
                  }
               }
            }
            else {
               read<json>::op<Opts>(v, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }
      };
      
      template <>
      struct to_json<raw_or_file>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            dump(value.value.str, b, ix);
         }
      };
   }
}
