#define GLZ_DISABLE_SIMD
#include "string_write_no_simd.hpp"

#include "glaze/json.hpp"

namespace no_simd
{
   void write_json_string(const std::string& input, std::string& output)
   {
      output.clear();
      (void)glz::write_json(input, output);
   }
}
