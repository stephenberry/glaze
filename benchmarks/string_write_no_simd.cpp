#define GLZ_DISABLE_SIMD
#include "glaze/json.hpp"
#include "string_write_no_simd.hpp"

namespace no_simd
{
   void write_json_string(const std::string& input, std::string& output)
   {
      output.clear();
      (void)glz::write_json(input, output);
   }
}
