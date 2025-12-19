#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glaze/glaze.hpp>
#include <glaze/msgpack.hpp>
#include <vector>

struct S
{
   std::string value{};
};

void test(const uint8_t* Data, size_t Size)
{
   S s{{Data, Data + Size}};

   // MessagePack handles arbitrary byte sequences in strings
   std::string buffer{};
   auto write_ec = glz::write_msgpack(s, buffer);
   if (write_ec) {
      return;
   }
   auto restored = glz::read_msgpack<S>(buffer);
   assert(restored);
   assert(restored.value().value == s.value);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   test(Data, Size);
   return 0;
}
