#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glaze/glaze.hpp>
#include <vector>

struct S
{
   std::string value{};
};

void test(const uint8_t* Data, size_t Size)
{
   S s{{Data, Data + Size}};

   // note - glaze does not escape control characters. see https://github.com/stephenberry/glaze/issues/812

   // replace control characters with space
   for (auto& c : s.value) {
      if (std::iscntrl(c) && c != '\b' && c != '\f' && c != '\n' && c != '\r' && c != '\t') {
         c = ' ';
      }
   }

   auto str = glz::write_json(s).value_or(std::string{});
   auto restored = glz::read_json<S>(str);
   assert(restored);

   assert(restored.value().value == s.value);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   test(Data, Size);
   return 0;
}
