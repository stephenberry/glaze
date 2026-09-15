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

   // The comment enabled paths scan comments as well as strings, which is where the scanners that
   // have to report failure live
   {
      const std::vector<char> jsonc{Data, Data + Size};
      [[maybe_unused]] auto beautiful = glz::prettify_jsonc(jsonc);
   }
   {
      std::vector<char> jsonc{Data, Data + Size};
      jsonc.push_back('\0');
      const auto& input = jsonc;
      [[maybe_unused]] auto beautiful = glz::prettify_jsonc(input);
   }

   // Only a string selects the null terminated path, where the run ends on the sentinel byte
   {
      std::string jsonc{Data, Data + Size};
      [[maybe_unused]] auto beautiful = glz::prettify_jsonc(jsonc);
   }

   return 0;
}
