#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glaze/glaze.hpp>
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
   if (std::isfinite(s.value)) {
      auto str = glz::write_json(s).value_or(std::string{});
      [[maybe_unused]] auto restored = glz::read_json<S>(str);
      assert(restored);
      assert(restored.value().value == s.value);
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
