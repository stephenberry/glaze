// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <fstream>
#include <span>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"

namespace glz
{
   // For writing to a std::string, std::vector<char>, std::deque<char> and the like
   template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      if constexpr (resizable<Buffer>) {
         // A buffer could be size 1, to ensure we have sufficient memory we can't just check `empty()`
         if (buffer.size() < 2 * write_padding_bytes) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      size_t ix = 0; // overwrite index
      to<Opts.format, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (resizable<Buffer>) {
         buffer.resize(ix);
      }

      return {ctx.error, ctx.custom_error_message};
   }

   template <auto& Partial, auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer)
   {
      if constexpr (resizable<Buffer>) {
         // A buffer could be size 1, to ensure we have sufficient memory we can't just check `empty()`
         if (buffer.size() < 2 * write_padding_bytes) {
            buffer.resize(2 * write_padding_bytes);
         }
      }
      context ctx{};
      size_t ix = 0;
      serialize_partial<Opts.format>::template op<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);
      if constexpr (resizable<Buffer>) {
         buffer.resize(ix);
      }
      return {ctx.error, ctx.custom_error_message};
   }

   template <auto& Partial, auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<size_t, error_ctx> write(T&& value, Buffer& buffer)
   {
      context ctx{};
      size_t ix = 0;
      serialize_partial<Opts.format>::template op<Partial, Opts>(std::forward<T>(value), ctx, buffer, ix);
      if (bool(ctx.error)) [[unlikely]] {
         return glz::unexpected(error_ctx{.ec = ctx.error, .custom_error_message = ctx.custom_error_message});
      }
      return {ix};
   }

   template <auto Opts, class T, output_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] error_ctx write(T&& value, Buffer& buffer)
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   template <auto Opts, class T>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<std::string, error_ctx> write(T&& value)
   {
      std::string buffer{};
      context ctx{};
      const auto ec = write<Opts>(std::forward<T>(value), buffer, ctx);
      if (bool(ec)) [[unlikely]] {
         return glz::unexpected(ec);
      }
      return {buffer};
   }

   template <auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<size_t, error_ctx> write(T&& value, Buffer&& buffer, is_context auto&& ctx)
   {
      size_t ix = 0;
      to<Opts.format, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      if (bool(ctx.error)) [[unlikely]] {
         return glz::unexpected(error_ctx{ctx.error});
      }
      return {ix};
   }

   template <auto Opts, class T, raw_buffer Buffer>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<size_t, error_ctx> write(T&& value, Buffer&& buffer)
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer), ctx);
   }

   // Concept for byte-like types that can be used in buffers
   template <class T>
   concept byte_like = std::same_as<T, char> || std::same_as<T, unsigned char> || 
                       std::same_as<T, uint8_t> || std::same_as<T, std::byte>;

   // Overload for std::span of byte-like types to return size information  
   template <auto Opts, class T, byte_like ByteType, size_t Extent>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<size_t, error_ctx> write(T&& value, std::span<ByteType, Extent> buffer, is_context auto&& ctx)
   {
      size_t ix = 0;
      
      // Use the existing serialization but check for overflow after
      to<Opts.format, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), ctx, buffer, ix);
      
      // Check if serialization failed
      if (bool(ctx.error)) [[unlikely]] {
         return glz::unexpected(error_ctx{.ec = ctx.error, .custom_error_message = ctx.custom_error_message});
      }
      
      // Check if we wrote beyond the buffer (this indicates the dump functions failed to prevent overflow)
      if (ix > buffer.size()) [[unlikely]] {
         // This should not happen if dump functions work correctly, but let's be safe
         ctx.error = error_code::unexpected_end;
         return glz::unexpected(error_ctx{.ec = error_code::unexpected_end});
      }
      
      return {ix};
   }

   template <auto Opts, class T, byte_like ByteType, size_t Extent>
      requires write_supported<T, Opts.format>
   [[nodiscard]] glz::expected<size_t, error_ctx> write(T&& value, std::span<ByteType, Extent> buffer)
   {
      context ctx{};
      return write<Opts>(std::forward<T>(value), buffer, ctx);
   }

   // requires file_name to be null terminated
   [[nodiscard]] inline error_code buffer_to_file(auto&& buffer, const sv file_name)
   {
      auto file = std::ofstream(file_name.data(), std::ios::out);
      if (!file) {
         return error_code::file_open_failure;
      }
      file.write(buffer.data(), buffer.size());
      return {};
   }
}
