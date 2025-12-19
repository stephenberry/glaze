#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glaze/glaze.hpp>
#include <glaze/msgpack.hpp>
#include <vector>

// must be outside test() to work in gcc<14
template <typename T>
struct Value
{
   T value{};
};

template <typename T>
void test(const uint8_t* Data, size_t Size)
{
   using S = Value<T>;
   S s{};

   if (Size >= sizeof(T)) {
      std::memcpy(&s.value, Data, sizeof(T));
   }
   else {
      return;
   }
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
   if (Size < 2) {
      return 0;
   }
   const auto action = Data[0];
   ++Data;
   --Size;

   switch (action & 0b11) {
   case 0:
      test<short>(Data, Size);
      test<unsigned short>(Data, Size);
      break;
   case 1:
      test<int>(Data, Size);
      test<unsigned int>(Data, Size);
      break;
   case 2:
      test<long>(Data, Size);
      test<unsigned long>(Data, Size);
      break;
   case 3:
      test<long long>(Data, Size);
      test<unsigned long long>(Data, Size);
      break;
   }

   return 0;
}
