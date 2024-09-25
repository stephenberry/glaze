// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace glz
{
   inline std::vector<unsigned char> base64_decode(const std::string_view input)
   {
      static constexpr std::string_view base64_chars =
         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
         "abcdefghijklmnopqrstuvwxyz"
         "0123456789+/";

      std::vector<unsigned char> decoded_data;
      static constexpr std::array<int, 256> decode_table = [] {
         std::array<int, 256> t;
         t.fill(-1);

         for (int i = 0; i < 64; ++i) {
            t[base64_chars[i]] = i;
         }
         return t;
      }();

      int val = 0, valb = -8;
      for (unsigned char c : input) {
         if (decode_table[c] == -1) break; // Stop decoding at padding '=' or invalid characters
         val = (val << 6) + decode_table[c];
         valb += 6;
         if (valb >= 0) {
            decoded_data.push_back((val >> valb) & 0xFF);
            valb -= 8;
         }
      }

      return decoded_data;
   }
}
