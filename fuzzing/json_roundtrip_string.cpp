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
      // std::iscntrl has undefined behavior for negative values, so widen through unsigned char.
      if (std::iscntrl(static_cast<unsigned char>(c)) && c != '\b' && c != '\f' && c != '\n' && c != '\r' &&
          c != '\t') {
         c = ' ';
      }
   }

   // Control characters are ASCII, so the substitution above cannot change whether the string is
   // well formed UTF-8.
   const bool well_formed = glz::validate_utf8(s.value.data(), s.value.size());

   auto str = glz::write_json(s).value_or(std::string{});
   auto restored = glz::read_json<S>(str);

   if (well_formed) {
      assert(restored);
      assert(restored.value().value == s.value);
   }
   else {
      // Writing is deliberately not validated while reading always is, so a std::string holding
      // malformed UTF-8 serializes and then fails to parse back. Assert that asymmetry rather than
      // skipping these inputs, so the round trip contract stays pinned in both directions.
      assert(!restored);
      assert(restored.error().ec == glz::error_code::invalid_utf8);
   }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   test(Data, Size);
   return 0;
}
