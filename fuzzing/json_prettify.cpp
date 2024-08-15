#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
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

   return 0;
}
