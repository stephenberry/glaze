#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <glaze/yaml.hpp>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // The YAML reader is bounded by `end` and never consults a trailing sentinel, so it must
   // parse a buffer that has no '\0' after it. An exact-size heap buffer puts the ASAN
   // redzone immediately after the last byte, so any read past the end fails here.
   {
      const std::vector<char> buffer{Data, Data + Size};

      static constexpr glz::opts opts{.format = glz::YAML, .null_terminated = false};
      glz::generic yaml{};
      auto ec = glz::read<opts>(yaml, buffer);
      if (!ec) {
         [[maybe_unused]] auto s = yaml.size();
      }
   }

   // glz::yaml_opts has no null_terminated member at all, so this covers the default
   // read_yaml path, where check_null_terminated falls back to false.
   {
      const std::vector<char> buffer{Data, Data + Size};

      glz::generic yaml{};
      auto ec = glz::read_yaml(yaml, buffer);
      if (!ec) {
         [[maybe_unused]] auto s = yaml.size();
      }
   }

   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};
   buffer.push_back('\0');

   // const qualified input buffer
   {
      const auto& input = buffer;
      glz::generic yaml{};
      auto ec = glz::read_yaml(yaml, input);
      if (!ec) {
         [[maybe_unused]] auto s = yaml.size();
      }
   }

   // non-const input buffer
   {
      glz::generic yaml{};
      auto ec = glz::read_yaml(yaml, buffer);
      if (!ec) {
         [[maybe_unused]] auto s = yaml.size();
      }
   }

   return 0;
}
