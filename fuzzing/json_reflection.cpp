#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <vector>

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};

   // non-null terminated
   {
      my_struct obj{};
      [[maybe_unused]] auto ec =
         glz::read<glz::opts{.null_terminated = false}>(obj, std::string_view{buffer.data(), Size});
   }

   // null terminated
   {
      buffer.push_back('\0');
      [[maybe_unused]] auto s = glz::read_json<my_struct>(std::string_view{buffer.data(), Size});
      if (s) {
         // hooray! valid json found
      }
   }

   return 0;
}
