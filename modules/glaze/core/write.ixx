// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.core.write;

import std;

import glaze.core.opts;
import glaze.core.buffer_traits;
import glaze.core.common;
import glaze.core.context;
import glaze.concepts.container_concepts;

import glaze.util.expected;
import glaze.util.string_literal;

namespace glz
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and the like
   export template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      using traits = buffer_traits<std::remove_cvref_t<Buffer>>;

      if constexpr (traits::is_resizable) {
         // A buffer could be size 1, to ensure we have sufficient memory we can't just check `empty()`
         if (buffer.size() < 2 * write_padding_bytes) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      std::size_t ix = 0; // overwrite index
      to<Opts.format, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);

      if (bool(ctx.error)) [[unlikely]] {
         return {ix, ctx.error, ctx.custom_error_message};
      }

      traits::finalize(buffer, ix);
      return {ix, error_code::none, ctx.custom_error_message};
   }

   export template <auto& Partial, auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer)
   {
      using traits = buffer_traits<std::remove_cvref_t<Buffer>>;

      if constexpr (traits::is_resizable) {
         // A buffer could be size 1, to ensure we have sufficient memory we can't just check `empty()`
         if (buffer.size() < 2 * write_padding_bytes) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      context ctx{};
      std::size_t ix = 0;
      serialize_partial<Opts.format>::template op<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);

      if (bool(ctx.error)) [[unlikely]] {
         return {ix, ctx.error, ctx.custom_error_message};
      }

      traits::finalize(buffer, ix);
      return {ix, error_code::none, ctx.custom_error_message};
   }

   export template <auto& Partial, auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer)
   {
      context ctx{};
      std::size_t ix = 0;
      serialize_partial<Opts.format>::template op<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);
      if (bool(ctx.error)) [[unlikely]] {
         return {ix, ctx.error, ctx.custom_error_message};
      }
      return {ix, error_code::none, ctx.custom_error_message};
   }

   export template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer)
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   export template <auto Opts, class T>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<std::string, error_ctx> write(T&& value)
   {
      std::string buffer{};
      context ctx{};
      const auto res = write<Opts>(std::forward<T>(value), buffer, ctx);
      if (res) [[unlikely]] {
         return glz::unexpected(res);
      }
      return {buffer};
   }

   export template <auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer&& buffer, is_context auto&& ctx)
   {
      std::size_t ix = 0;
      to<Opts.format, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if (bool(ctx.error)) [[unlikely]] {
         return {ix, ctx.error, ctx.custom_error_message};
      }
      return {ix, error_code::none, ctx.custom_error_message};
   }

   export template <auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer&& buffer)
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   // requires file_name to be null terminated
   export [[nodiscard]] inline error_code buffer_to_file(auto&& buffer, const sv file_name)
   {
      auto file = std::ofstream(file_name.data(), std::ios::out);
      if (!file) {
         return error_code::file_open_failure;
      }
      file.write(buffer.data(), buffer.size());
      return {};
   }
}
