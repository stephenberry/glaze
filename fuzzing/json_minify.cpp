#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};

   // non-null terminated
   {
      [[maybe_unused]] auto maybe_smaller = glz::minify_json(buffer);
   }

   // null terminated
   {
      buffer.push_back('\0');
      [[maybe_unused]] auto maybe_smaller = glz::minify_json(buffer);
   }

   // The comment enabled path is a different scan: read_jsonc_comment only runs here, and it is
   // the scanner that has to report an unterminated comment rather than swallow the buffer. Fuzzed
   // as its own input because a document rejected by the plain path may be accepted by this one.
   {
      std::vector<char> jsonc{Data, Data + Size};
      [[maybe_unused]] auto maybe_smaller = glz::minify_jsonc(jsonc);
   }

   // Only a self-terminating buffer selects the null terminated scan, whose run ends on the
   // sentinel rather than on a bound in its loop condition. A vector carrying a trailing NUL does
   // not: that NUL is just a byte inside the document.
   {
      std::string jsonc{Data, Data + Size};
      [[maybe_unused]] auto maybe_smaller = glz::minify_jsonc(jsonc);
   }

   // Minifying only ever removes bytes, so a bounded output large enough for the input is large
   // enough for the result -- and one that is not has to say so rather than be written past.
   {
      std::vector<char> jsonc{Data, Data + Size};
      std::array<char, 64> bounded{};
      [[maybe_unused]] const auto ec = glz::minify_jsonc(jsonc, bounded);
   }

   return 0;
}
