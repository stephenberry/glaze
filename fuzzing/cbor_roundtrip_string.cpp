#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glaze/cbor.hpp>
#include <glaze/glaze.hpp>
#include <vector>

struct S
{
   std::string value{};
};

void test(const uint8_t* Data, size_t Size)
{
   S s{{Data, Data + Size}};

   // CBOR handles arbitrary byte sequences in strings, no escaping needed
   std::string buffer{};
   auto write_ec = glz::write_cbor(s, buffer);
   if (write_ec) {
      return;
   }
   auto restored = glz::read_cbor<S>(buffer);
   assert(restored);
   assert(restored.value().value == s.value);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   test(Data, Size);
   return 0;
}
