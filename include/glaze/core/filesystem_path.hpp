// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   // True when the path stores its native name as UTF-16, which is the case on Windows.
   template <class T>
   inline constexpr bool wide_native_path =
      sizeof(typename std::remove_cvref_t<decltype(std::declval<const T&>().native())>::value_type) == 2;

   [[nodiscard]] inline bool utf16_to_utf8(const auto& in, std::string& out)
   {
      using unit_t = std::make_unsigned_t<std::remove_cvref_t<decltype(in[0])>>;
      const auto unit = [&](size_t i) { return static_cast<uint32_t>(static_cast<unit_t>(in[i])); };

      out.clear();
      out.reserve(in.size());

      for (size_t i = 0; i < in.size(); ++i) {
         uint32_t cp = unit(i);
         if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 >= in.size()) {
               return false; // unpaired high surrogate
            }
            const uint32_t low = unit(++i);
            if (low < 0xDC00 || low > 0xDFFF) {
               return false;
            }
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
         }
         else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return false; // unpaired low surrogate
         }

         char buf[4];
         out.append(buf, static_cast<size_t>(code_point_to_utf8(cp, buf)));
      }
      return true;
   }

   [[nodiscard]] inline bool utf8_to_utf16(std::string_view in, std::wstring& out)
   {
      out.clear();
      out.reserve(in.size());

      for (size_t i = 0; i < in.size();) {
         const auto lead = static_cast<uint8_t>(in[i]);
         uint32_t cp{};
         size_t len{};
         if (lead < 0x80) {
            cp = lead;
            len = 1;
         }
         else if ((lead & 0xE0) == 0xC0) {
            cp = lead & 0x1F;
            len = 2;
         }
         else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0F;
            len = 3;
         }
         else if ((lead & 0xF8) == 0xF0) {
            cp = lead & 0x07;
            len = 4;
         }
         else {
            return false;
         }

         if (i + len > in.size()) {
            return false; // truncated sequence
         }

         for (size_t j = 1; j < len; ++j) {
            const auto cont = static_cast<uint8_t>(in[i + j]);
            if ((cont & 0xC0) != 0x80) {
               return false;
            }
            cp = (cp << 6) | static_cast<uint32_t>(cont & 0x3F);
         }
         i += len;

         // rejects overlong forms, surrogates, and values above U+10FFFF
         static constexpr uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
         if (cp < minimum[len] || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            return false;
         }

         if (cp > 0xFFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
         }
         else {
            out.push_back(static_cast<wchar_t>(cp));
         }
      }
      return true;
   }

   // Assigns a UTF-8 string to a path. On Windows, assigning the narrow string directly would decode it with the
   // ANSI code page, so it is converted to the UTF-16 native form first. Returns false for malformed UTF-8.
   template <class T>
   [[nodiscard]] bool assign_utf8_path(T& path, const std::string& utf8)
   {
      if constexpr (wide_native_path<T>) {
         std::wstring wide{};
         if (!utf8_to_utf16(utf8, wide)) {
            return false;
         }
         path = T(std::move(wide));
      }
      else {
         path = utf8;
      }
      return true;
   }
}
