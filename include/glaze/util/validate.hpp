// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>
#include <optional>

namespace glz
{
   namespace detail
   {
      struct source_info
      {
         size_t line{};
         size_t column{};
         std::string context;
      };

      inline std::optional<source_info> get_source_info(const auto& buffer,
                                                        const std::size_t index)
      {
         if (index >= buffer.size()) {
            return std::nullopt;
         }
         
         using V = std::decay_t<decltype(buffer[0])>;
         const std::size_t r_index = buffer.size() - index - 1;
         const auto start = std::begin(buffer) + index;
         const auto count = std::count(std::begin(buffer), start, static_cast<V>('\n'));
         const auto rstart = std::rbegin(buffer) + r_index;
         const auto pnl = std::find(rstart, std::rend(buffer), static_cast<V>('\n'));
         const auto dist = std::distance(rstart, pnl);
         const auto nnl = std::find(start, std::end(buffer), static_cast<V>('\n'));
         
         if constexpr (std::same_as<V, std::byte>) {
            std::string context{
               reinterpret_cast<const char*>(buffer.data()) +
                  (pnl == std::rend(buffer) ? 0 : index - dist + 1),
               reinterpret_cast<const char*>(&(*nnl))};
            return source_info{static_cast<std::size_t>(count + 1),
                              static_cast<std::size_t>(dist), context};
         }
         else {
            std::string context{
               std::begin(buffer) +
                  (pnl == std::rend(buffer) ? 0 : index - dist + 1),
               nnl};
            return source_info{static_cast<std::size_t>(count + 1),
                              static_cast<std::size_t>(dist), context};
         }
      }

      inline std::string generate_error_string(const sv error,
                                               const source_info& info,
                                               const sv filename = "")
      {
         std::string s{};
         
         if (!filename.empty()) {
            s += filename;
            s += ":";
         }
         
         s += std::to_string(info.line) + ":" + std::to_string(info.column) + ": ";
         s += error;
         s += "\n";
         s += "   " + info.context + "\n   ";
         for (size_t i = 0; i < info.column - 1; ++i) {
            s += " ";
         }
         s += "^\n";
         
         // TODO: use std::format when available
         /*
         auto it = std::back_inserter(s);
         if (!filename.empty()) {
            fmt::format_to(it, FMT_COMPILE("{}:"), filename);
         }
         
         fmt::format_to(it, FMT_COMPILE("{}:{}: {}\n"), info.line, info.column, error);
         fmt::format_to(it, FMT_COMPILE("   {}\n   "), info.context);
         fmt::format_to(it, FMT_COMPILE("{: <{}}^\n"), "", info.column - 1);*/
         return s;
      }
   }  // namespace detail
}  // namespace test
