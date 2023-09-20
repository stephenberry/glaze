// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <optional>
#include <string>

namespace glz
{
   namespace detail
   {
      struct source_info final
      {
         size_t line{};
         size_t column{};
         std::string context;
         size_t index{};
      };

      inline std::optional<source_info> get_source_info(const std::string_view buffer, const size_t index)
      {
         if (index >= buffer.size()) {
            return std::nullopt;
         }

         using V = std::decay_t<decltype(buffer[0])>;
         const auto start = std::begin(buffer) + index;
         const auto line_number = size_t(std::count(std::begin(buffer), start, static_cast<V>('\n')) + 1);
         const auto rstart = std::rbegin(buffer) + buffer.size() - index - 1;
         const auto prev_new_line = std::find((std::min)(rstart + 1, std::rend(buffer)), std::rend(buffer), static_cast<V>('\n'));
         const auto column = size_t(std::distance(rstart, prev_new_line));
         const auto next_new_line = std::find((std::min)(start + 1, std::end(buffer)), std::end(buffer), static_cast<V>('\n'));

         if constexpr (std::same_as<V, std::byte>) {
            std::string context{
               reinterpret_cast<const char*>(buffer.data()) + (prev_new_line == std::rend(buffer) ? 0 : index - column + 1),
               reinterpret_cast<const char*>(&(*next_new_line))};
            return source_info{line_number, column, context, index};
         }
         else {
            std::string context{std::begin(buffer) + (prev_new_line == std::rend(buffer) ? 0 : index - column + 1), next_new_line};
            return source_info{line_number, column, context, index};
         }
      }

      inline std::string generate_error_string(const std::string_view error, const source_info& info,
                                               const std::string_view filename = "")
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
   } // namespace detail
} // namespace test
