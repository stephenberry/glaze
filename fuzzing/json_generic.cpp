#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // non-null terminated
   {
      const std::vector<char> buffer{Data, Data + Size};

      static constexpr glz::opts opts{.null_terminated = false};
      glz::generic json{};
      auto ec = glz::read<opts>(json, buffer);
      if (!ec) {
         [[maybe_unused]] auto s = json.size();
      }
   }

   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};
   buffer.push_back('\0');

   // const qualified input buffer
   {
      const auto& input = buffer;
      glz::generic json{};
      auto ec = glz::read_json(json, input);
      if (!ec) {
         [[maybe_unused]] auto s = json.size();
      }
   }

   // non-const input buffer
   {
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      if (!ec) {
         [[maybe_unused]] auto s = json.size();
      }
   }

   return 0;
}
