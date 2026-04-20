#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <glaze/jsonb.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct nested
{
   int n = 0;
   std::string label{};
   std::vector<double> data{};
};

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   std::optional<std::string> maybe{};
   std::map<std::string, int> dict{};
   std::vector<nested> kids{};
   std::variant<int, std::string, double> alt{};
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // Use a vector with explicit bounds rather than std::string to avoid SSO masking
   // bounds-check bugs.
   std::vector<char> buffer{Data, Data + Size};
   const auto& input = buffer;

   // 1. Drive the typed reader: exercises header parsing, type dispatch, container
   //    traversal, hash key lookup, depth guard, INT bounds checking.
   {
      [[maybe_unused]] auto s = glz::read_jsonb<my_struct>(input);
   }

   // 2. Drive the JSONB→JSON converter: separate code path with its own depth
   //    tracking and TEXT/TEXTRAW/TEXTJ/TEXT5 emission paths.
   {
      std::string json_output{};
      [[maybe_unused]] auto ec = glz::jsonb_to_json(input, json_output);
   }

   // 3. Drive the schemaless reader (glz::generic) — covers paths the typed reader
   //    short-circuits on type mismatch.
   {
      [[maybe_unused]] auto s = glz::read_jsonb<glz::generic>(input);
   }

   return 0;
}
