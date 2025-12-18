#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/cbor.hpp>
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
      const auto& input = buffer;
      [[maybe_unused]] auto s = glz::read_cbor<my_struct>(input);

      std::string json_output{};
      [[maybe_unused]] auto ec = glz::cbor_to_json(input, json_output);
   }

   // null terminated
   {
      buffer.push_back('\0');
      const auto& input = buffer;
      [[maybe_unused]] auto s = glz::read_cbor<my_struct>(input);
   }

   return 0;
}
