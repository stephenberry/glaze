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
      const auto& input = buffer;
      [[maybe_unused]] auto beautiful = glz::prettify_json(input);
   }

   // null terminated
   {
      buffer.push_back('\0');
      const auto& input = buffer;
      [[maybe_unused]] auto beautiful = glz::prettify_json(input);
   }

   // The comment enabled path is a different scan: read_jsonc_comment only runs here, and it is
   // the scanner that has to report an unterminated comment rather than hand an empty view to a
   // memcpy. Fuzzed as its own input because a document rejected by the plain path may be accepted
   // by this one.
   {
      const std::vector<char> jsonc{Data, Data + Size};
      [[maybe_unused]] auto beautiful = glz::prettify_jsonc(jsonc);
   }

   // Only a self-terminating buffer selects the null terminated scan. A vector carrying a
   // trailing NUL does not: that NUL is just a byte inside the document.
   {
      const std::string jsonc{Data, Data + Size};
      [[maybe_unused]] auto beautiful = glz::prettify_jsonc(jsonc);
   }

   return 0;
}
