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
         size_t front_truncation{};
         size_t rear_truncation{};
      };

      // We convert to only single spaces for error messages in order to keep the source info
      // calculation more efficient and avoid needing to allocate more memory.
      inline void convert_tabs_to_single_spaces(std::string& input) noexcept
      {
         for (auto& c : input) {
            if (c == '\t') {
               c = ' ';
            }
         }
      }

      inline std::optional<source_info> get_source_info(const std::string_view buffer, const size_t index)
      {
         if (index >= buffer.size()) {
            return std::nullopt;
         }

         using V = std::decay_t<decltype(buffer[0])>;
         const auto start = std::begin(buffer) + index;
         const auto line = size_t(std::count(std::begin(buffer), start, static_cast<V>('\n')) + 1);
         const auto rstart = std::rbegin(buffer) + buffer.size() - index - 1;
         const auto prev_new_line =
            std::find((std::min)(rstart + 1, std::rend(buffer)), std::rend(buffer), static_cast<V>('\n'));
         const auto column = size_t(std::distance(rstart, prev_new_line));
         const auto next_new_line =
            std::find((std::min)(start + 1, std::end(buffer)), std::end(buffer), static_cast<V>('\n'));

         const auto offset = (prev_new_line == std::rend(buffer) ? 0 : index - column + 1);
         auto context_begin = std::begin(buffer) + offset;
         auto context_end = next_new_line;

         size_t front_truncation = 0;
         size_t rear_truncation = 0;

         if (std::distance(context_begin, context_end) > 64) {
            // reduce the context length so that we can more easily see errors, especially for non-prettified buffers
            if (column <= 32) {
               rear_truncation = 64;
               context_end = context_begin + rear_truncation;
            }
            else {
               front_truncation = column - 32;
               context_begin += front_truncation;
               if (std::distance(context_begin, context_end) > 64) {
                  rear_truncation = front_truncation + 64;
                  context_end = std::begin(buffer) + offset + rear_truncation;
               }
            }
         }

         std::string context = [&]() -> std::string {
            if constexpr (std::same_as<V, std::byte>) {
               return {reinterpret_cast<const char*>(&(*context_begin)),
                       reinterpret_cast<const char*>(&(*context_end))};
            }
            else {
               return {context_begin, context_end};
            }
         }();

         convert_tabs_to_single_spaces(context);
         return source_info{line, column, context, index, front_truncation, rear_truncation};
      }

      inline std::string generate_error_string(const std::string_view error, const source_info& info,
                                               const std::string_view filename = "")
      {
         std::string s{};
         s.reserve(error.size() + info.context.size() + filename.size() + 128);

         if (!filename.empty()) {
            s += filename;
            s += ":";
         }

         s += std::to_string(info.line) + ":" + std::to_string(info.column) + ": ";
         s += error;
         s += "\n";
         if (info.front_truncation) {
            if (info.rear_truncation) {
               s += "..." + info.context + "...\n   ";
            }
            else {
               s += "..." + info.context + "\n   ";
            }
         }
         else {
            s += "   " + info.context + "\n   ";
         }
         for (size_t i = 0; i < info.column - 1 - info.front_truncation; ++i) {
            s += " ";
         }
         s += "^";

         return s;
      }
   }
}
