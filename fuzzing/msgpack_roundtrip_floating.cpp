#include <cassert>
#include <cmath>
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
   // Only test finite values for equality (NaN != NaN by definition)
   if (std::isfinite(s.value)) {
      std::string buffer{};
      auto write_ec = glz::write_msgpack(s, buffer);
      if (write_ec) {
         return;
      }
      auto restored = glz::read_msgpack<S>(buffer);
      assert(restored);
      assert(restored.value().value == s.value);
   }
   else {
      // For non-finite values, just test that encoding/decoding doesn't crash
      std::string buffer{};
      auto write_ec = glz::write_msgpack(s, buffer);
      if (write_ec) {
         return;
      }
      [[maybe_unused]] auto restored = glz::read_msgpack<S>(buffer);
   }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   if (Size < 1 + sizeof(float)) {
      return 0;
   }
   const auto action = Data[0];
   ++Data;
   --Size;

   switch (action & 0b1) {
   case 0:
      test<float>(Data, Size);
      break;
   case 1:
      test<double>(Data, Size);
      break;
   }

   return 0;
}
