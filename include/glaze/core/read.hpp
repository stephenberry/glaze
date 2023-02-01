// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/util/validate.hpp"
#include "glaze/api/std/span.hpp"

#include <span>

namespace glz
{
   template <opts Opts>
   inline auto read_iterators(detail::contiguous auto&& buffer)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      
      auto b = reinterpret_cast<const char*>(buffer.data());
      auto e = reinterpret_cast<const char*>(buffer.data()); // to be incrementd
      
      using Buffer = std::decay_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> || is_specialization_v<Buffer, std::basic_string_view> || span<Buffer> || Opts.format == binary) {
         e += buffer.size();
         
         if (b == e) {
            throw std::runtime_error("No input provided to read");
         }
      }
      else {
         // if not a std::string or a std::string_view, check that the last character is a null character
         // this is not required for binary specification reading, because we require the data to be properly formatted
         if (buffer.empty()) {
            throw std::runtime_error("No input provided to read");
         }
         e += buffer.size() - 1;
         if (*e != '\0') {
            throw std::runtime_error("Data must be null terminated");
         }
      }
      
      return std::pair{ b, e };
   }
   
   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <opts Opts>
   inline void read(auto& value, detail::contiguous auto&& buffer, is_context auto&& ctx)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);
      
      auto b = reinterpret_cast<const char*>(buffer.data());
      auto e = reinterpret_cast<const char*>(buffer.data()); // to be incrementd
      
      using Buffer = std::decay_t<decltype(buffer)>;
      if constexpr (is_specialization_v<Buffer, std::basic_string> || is_specialization_v<Buffer, std::basic_string_view> || span<Buffer> || Opts.format == binary) {
         e += buffer.size();
         
         if (b == e) {
            throw std::runtime_error("No input provided to read");
         }
      }
      else {
         // if not a std::string or a std::string_view, check that the last character is a null character
         // this is not required for binary specification reading, because we require the data to be properly formatted
         if (buffer.empty()) {
            throw std::runtime_error("No input provided to read");
         }
         e += buffer.size() - 1;
         if (*e != '\0') {
            throw std::runtime_error("Data must be null terminated");
         }
      }
      
      if constexpr (Opts.format == binary) {
         // binary exceptions are not formatted
         detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
      }
      else {
         try {
            detail::read<Opts.format>::template op<Opts>(value, ctx, b, e);
         }
         catch (const std::exception& e) {
            
            auto index = std::distance(reinterpret_cast<const char*>(buffer.data()), b);
            auto info = detail::get_source_info(buffer, index);
            std::string error = e.what();
            if (info) {
               error = detail::generate_error_string(error, *info);
            }
            throw std::runtime_error(error);
         }
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
