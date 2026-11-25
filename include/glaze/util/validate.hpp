// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cstdint>
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

      // A byte that a terminal would act on rather than print (C0 controls and DEL).
      constexpr bool is_control_char(const char c) noexcept
      {
         const auto u = static_cast<uint8_t>(c);
         return u < 0x20 || u == 0x7F;
      }

      // Every control character in the context, tabs and carriage returns included, becomes a single
      // space. Each byte then takes one column, so the caret lines up, and nothing in the context
      // can move the terminal's cursor.
      inline void convert_to_printable_spaces(std::string& input) noexcept
      {
         for (auto& c : input) {
            if (is_control_char(c)) {
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
            if (index > text.size()) {
               return {.context = "", .index = index};
            }

            // An error at the end of the input (a truncated document, a missing closing bracket)
            // has no character of its own. Point just past the last character that is not
            // trailing whitespace, so the message still shows the line where the input stops.
            // The reported index is left unchanged.
            size_t pos = index;
            if (pos == text.size()) {
               const size_t last = text.find_last_not_of(" \t\r\n");
               if (last == text.npos) {
                  return {.context = "", .index = index};
               }
               pos = last + 1;
            }

            // The line the error sits on starts just after the newline before it, or at the
            // beginning when there is none. Both the column and the context start from that one
            // position. Searching from pos - 1 leaves pos itself out, so a newline *at* the
            // error belongs to the line it ends rather than the one it opens.
            const size_t prev_newline = pos ? text.rfind('\n', pos - 1) : text.npos;
            const size_t offset = (prev_newline == text.npos) ? 0 : prev_newline + 1;
            const size_t column = pos - offset + 1;
            const size_t line = size_t(std::count(text.begin(), text.begin() + pos, '\n') + 1);

            // The context runs to the end of the line. Searching from pos itself means that a
            // newline at the error ends the context, instead of pulling in the line after it. An
            // error on an empty line therefore has an empty context, but still a line and column.
            size_t context_begin = offset;
            size_t context_end = (std::min)(text.find('\n', pos), text.size());

            // Binary input (BEVE, CBOR, MessagePack, BSON) is often held in a std::string, so it
            // reaches this text path, where its line and column mean nothing. Without the format
            // this is a best-effort guess: a line holding control characters other than tab and
            // carriage return is taken to be binary, and only the index is reported. Binary data
            // without such bytes is shown as text, with any control characters made spaces. The
            // byte at the error is exempt, because a text reader that rejects a raw control
            // character reports the error at that character.
            for (size_t i = offset; i < context_end; ++i) {
               const char c = text[i];
               if (i != pos && c != '\t' && c != '\r' && is_control_char(c)) {
                  return {.context = "", .index = index};
               }
            }

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
            convert_to_printable_spaces(context);
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
         // Line numbers start at 1, so line 0 means the location is unknown.
         if (info.line == 0) {
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
