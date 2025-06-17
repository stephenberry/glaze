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
   my_struct obj{};
   std::string_view input_col{(const char*)Data, Size};
   [[maybe_unused]] auto parsed = glz::read_csv<glz::colwise>(obj, input_col);

   return 0;
}
