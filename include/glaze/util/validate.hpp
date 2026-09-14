// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "glaze/core/write_chars.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   namespace detail
   {
      struct source_info final
      {
         size_t line{};
         size_t column{};
         std::string context{};
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

      // `glz::format_error(ec, buffer)` is the call that follows a failed read, so it has to accept
      // every buffer a read accepts: data() and size() and nothing else. See GitHub issue #2854.
      inline source_info get_source_info(const contiguous auto& buffer, const size_t index)
      {
         using V = std::decay_t<decltype(*buffer.data())>;

         if constexpr (std::same_as<V, std::byte>) {
            return {.context = "", .index = index};
         }
         else {
            const std::string_view text{reinterpret_cast<const char*>(buffer.data()), buffer.size()};
            if (index >= text.size()) {
               return {.context = "", .index = index};
            }

            // The line the error sits on starts just after the newline before it, or at the
            // beginning when there is none. Both the column and the context start from that one
            // position. Searching from index - 1 leaves index itself out, so a newline *at* the
            // error belongs to the line it ends rather than the one it opens.
            const size_t prev_newline = index ? text.rfind('\n', index - 1) : text.npos;
            const size_t offset = (prev_newline == text.npos) ? 0 : prev_newline + 1;
            const size_t column = index - offset + 1;
            const size_t line = size_t(std::count(text.begin(), text.begin() + index, '\n') + 1);

            size_t context_begin = offset;
            size_t context_end = (std::min)(text.find('\n', index + 1), text.size());

            size_t front_truncation = 0;
            size_t rear_truncation = 0;

            if (context_end - context_begin > 64) {
               // reduce the context length so that we can more easily see errors, especially for non-prettified buffers
               if (column <= 32) {
                  rear_truncation = 64;
                  context_end = context_begin + rear_truncation;
               }
               else {
                  front_truncation = column - 32;
                  context_begin += front_truncation;
                  if (context_end - context_begin > 64) {
                     rear_truncation = front_truncation + 64;
                     context_end = offset + rear_truncation;
                  }
               }
            }

            std::string context{text.substr(context_begin, context_end - context_begin)};
            convert_tabs_to_single_spaces(context);
            return {line, column, context, index, front_truncation, rear_truncation};
         }
      }

      // A buffer that can report its size but not its bytes cannot show context, but it can still
      // say where the error was. This overload keeps format_error callable for any sized buffer, so
      // a failed read is always reportable rather than failing to compile inside format_error
      // itself.
      inline source_info get_source_info(const has_size auto& buffer, const size_t index)
         requires(!contiguous<std::remove_cvref_t<decltype(buffer)>>)
      {
         return {.context = "", .index = index};
      }

      template <class B>
         requires(!has_size<B>)
      inline source_info get_source_info(const B* buffer, const size_t index)
      {
         return get_source_info(sv{buffer}, index);
      }

      inline std::string generate_error_string(const std::string_view error, const source_info& info,
                                               const std::string_view filename = "")
      {
         std::string b{};
         b.resize(error.size() + info.context.size() + filename.size() + 128);
         size_t ix{};

         if (not filename.empty()) {
            dump_not_empty(filename, b, ix);
            dump(':', b, ix);
         }

         glz::context ctx{};

         // Diagnostic numbers (index/line/column) are always formatted with size-optimized
         // integer conversion: error formatting is never a throughput path, and this keeps
         // glz::format_error from pulling in the 40 KB integer table regardless of the
         // optimization level used elsewhere in the program.
         if (info.context.empty()) {
            dump("index ", b, ix);
            write_chars::op<opts_size{}>(info.index, ctx, b, ix);
            dump(": ", b, ix);
            dump_maybe_empty(error, b, ix);
         }
         else {
            write_chars::op<opts_size{}>(info.line, ctx, b, ix);
            dump(':', b, ix);
            write_chars::op<opts_size{}>(info.column, ctx, b, ix);
            dump(": ", b, ix);
            dump_maybe_empty(error, b, ix);
            dump('\n', b, ix);
            if (info.front_truncation) {
               if (info.rear_truncation) {
                  dump("...", b, ix);
                  dump_maybe_empty(info.context, b, ix);
                  dump("...\n   ", b, ix);
               }
               else {
                  dump("...", b, ix);
                  dump_maybe_empty(info.context, b, ix);
                  dump("\n   ", b, ix);
               }
            }
            else {
               dump("   ", b, ix);
               dump_maybe_empty(info.context, b, ix);
               dump("\n   ", b, ix);
            }
            dumpn(' ', info.column - 1 - info.front_truncation, b, ix);
            dump('^', b, ix);
         }

         b.resize(ix);
         return b;
      }
   }
}
