#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};
   buffer.push_back('\0');

   auto maybe_smaller = glz::minify_json(buffer);

   return 0;
}
