// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   template <class Buffer>
   concept byte_buffer = nano::ranges::input_range<std::decay_t<Buffer>> &&
   std::same_as<std::byte, nano::ranges::range_value_t<std::decay_t<Buffer>>>;
   
   template <class Buffer>
   concept char_buffer = nano::ranges::input_range<std::decay_t<Buffer>> &&
   std::same_as<char, nano::ranges::range_value_t<std::decay_t<Buffer>>>;
   
   template <class Buffer>
   concept string_viewable = std::convertible_to<std::decay_t<Buffer>, std::string_view> &&
   !nano::ranges::input_range<std::decay_t<Buffer>>;
   
   template <opts Opts, byte_buffer Buffer>
   inline void read(auto& value, Buffer&& buffer, auto&& ctx)
   {
      auto b = std::ranges::begin(buffer);
      auto e = std::ranges::end(buffer);
      if (b == e) {
         throw std::runtime_error("No input provided to read");
      }
      try {
         detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
      }
      catch (const std::exception& e) {
         // TODO: Implement good error message
         throw std::runtime_error("read error:" + std::string(e.what()));
      }
   }
   
   template <opts Opts, byte_buffer Buffer>
   inline void read(auto& value, Buffer&& buffer) {
      context ctx{};
      read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
   
   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <opts Opts, char_buffer Buffer>
   inline void read(auto& value, Buffer&& buffer, auto&& ctx)
   {
      auto b = std::ranges::begin(buffer);
      auto e = std::ranges::end(buffer);
      if (b == e) {
         throw std::runtime_error("No input provided to read");
      }
      try {
         detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
      }
      catch (const std::exception& e) {
         auto index = std::distance(std::ranges::begin(buffer), b);
         auto info = detail::get_source_info(buffer, index);
         std::string error = e.what();
         if (info) {
            error = detail::generate_error_string(error, *info);
         }
         throw std::runtime_error(error);
      }
   }
   
   template <opts Opts, char_buffer Buffer>
   inline void read(auto& value, Buffer&& buffer)
   {
      context ctx{};
      read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }

   // For reading json from std::ofstream, std::cout, or other streams
   template <opts Opts>
   inline void read(auto& value, detail::stream_t auto& is, auto&& ctx)
   {
      std::istreambuf_iterator<char> b{is}, e{};
      if (b == e) {
         throw std::runtime_error("No input provided to read");
      }
      detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
   }
   
   template <opts Opts>
   inline void read(auto& value, detail::stream_t auto& is)
   {
      context ctx{};
      read<Opts>(value, is, ctx);
   }

   template <opts Opts, class T, string_viewable Buffer>
   inline void read(T& value, Buffer&& buffer, auto&& ctx)
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         throw std::runtime_error("No input provided to read");
      }
      read<Opts>(value, str, ctx);
   }
   
   template <opts Opts, class T, string_viewable Buffer>
   inline void read(T& value, Buffer&& buffer)
   {
      context ctx{};
      read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
}
