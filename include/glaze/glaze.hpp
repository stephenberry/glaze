/*
  Glaze Library

  Copyright (c) 2019 - present, Stephen Berry

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  --- Optional exception to the license ---

  As an exception, if, as a result of your compiling your source code, portions
  of this Software are embedded into a machine-executable object form of such
  source code, you may redistribute such embedded portions in such object form
  without including the above copyright and permission notices.
 */

#pragma once

#include "glaze/binary.hpp"
#include "glaze/csv.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json.hpp"
#include "glaze/record/recorder.hpp"

namespace glz
{
   template <class T>
   inline parse_error read_file(T& value, const sv file_name, auto&& buffer) noexcept
   {
      context ctx{};
      ctx.current_file = file_name;

      std::filesystem::path path{file_name};

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      if (path.has_extension()) {
         const auto extension = path.extension().string();

         if (extension == ".json" || extension == ".jsonc") {
            return read<opts{}>(value, buffer, ctx);
         }
         else if (extension == ".beve") {
            return read<opts{.format = binary}>(value, buffer, ctx);
         }
         else {
            return {error_code::file_extension_not_supported};
         }
      }
      else {
         return {error_code::could_not_determine_extension};
      }
   }

   template <class T>
   [[nodiscard]] inline write_error write_file(T& value, const sv file_name, auto&& buffer) noexcept
   {
      context ctx{};
      ctx.current_file = file_name;

      std::filesystem::path path{file_name};

      if (path.has_extension()) {
         const auto extension = path.extension().string();

         if (extension == ".json") {
            write<opts{}>(value, buffer, ctx);
         }
         else if (extension == ".jsonc") {
            write<opts{.comments = true}>(value, buffer, ctx);
         }
         else if (extension == ".beve") {
            write<opts{.format = binary}>(value, buffer, ctx);
         }
         else {
            return {error_code::file_extension_not_supported};
         }
      }
      else {
         return {error_code::could_not_determine_extension};
      }

      std::ofstream file(std::string{file_name});

      if (file) {
         file << buffer;
      }
      else {
         return {error_code::file_open_failure};
      }

      return {};
   }
}
