// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"

namespace glz
{
   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <opts Opts>
   inline void read(auto& value, detail::contiguous auto&& buffer, is_context auto&& ctx)
   {
      auto b = buffer.data();
      auto e = buffer.data() + buffer.size();
      if (b == e) {
         throw std::runtime_error("No input provided to read");
      }
      try {
         detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
      }
      catch (const std::exception& e) {
         auto index = std::distance(buffer.data(), b);
         auto info = detail::get_source_info(buffer, index);
         std::string error = e.what();
         if (info) {
            error = detail::generate_error_string(error, *info);
         }
         throw std::runtime_error(error);
      }
   }
   
   template <opts Opts>
   inline void read(auto& value, detail::contiguous auto&& buffer)
   {
      context ctx{};
      read<Opts>(value, buffer, ctx);
   }
   
   template <class T>
   concept string_viewable = std::convertible_to<std::decay_t<T>, std::string_view> && !detail::has_data<T>;
   
   // for char array input
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
