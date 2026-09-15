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

   // The comment enabled paths scan comments as well as strings, which is where the scanners that
   // have to report failure live
   {
      std::vector<char> jsonc{Data, Data + Size};
      [[maybe_unused]] auto maybe_smaller = glz::minify_jsonc(jsonc);
   }
   {
      std::vector<char> jsonc{Data, Data + Size};
      jsonc.push_back('\0');
      [[maybe_unused]] auto maybe_smaller = glz::minify_jsonc(jsonc);
   }

   // Only a string selects the null terminated path, where the run ends on the sentinel byte
   {
      std::string jsonc{Data, Data + Size};
      [[maybe_unused]] auto maybe_smaller = glz::minify_jsonc(jsonc);
   }

   return 0;
}
