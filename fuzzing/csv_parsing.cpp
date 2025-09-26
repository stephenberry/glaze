#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <vector>

struct my_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   if (Size < 1) return 0;

   const bool colwise = Data[0] & 0x1;
   Size -= 1;
   Data += 1;

   my_struct obj{};
   std::string_view input{(const char*)Data, Size};
   if (colwise) {
      [[maybe_unused]] auto parsed = glz::read_csv<glz::colwise>(obj, input);
   }
   else {
      [[maybe_unused]] auto parsed = glz::read_csv<glz::rowwise>(obj, input);
   }

   return 0;
}
