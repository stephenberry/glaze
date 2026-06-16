// Glaze Library
// For the license information refer to glaze.hpp

// Symbol-absence guarantee for the size-optimized default.
//
// This translation unit is compiled with GLZ_DEFAULT_OPTIMIZATION_SIZE (plus -Os and
// section garbage collection) and links only glaze::glaze. It exercises the integer and
// float write paths and the error-formatting path entirely on default options. Because the
// build-wide default is `size`, nothing here ODR-uses the 40 KB integer `digit_quads` table.
// check_symbols.cmake then asserts (via nm) that the table symbol is absent from the binary,
// so any future internal call site that hardcodes the normal-mode large tables on a default
// path fails CI instead of silently re-bloating embedded builds.
//
// argc keeps the written values runtime-unknown so the optimizer cannot constant-fold the
// conversions away (which would make the absence check vacuous).

#include <string>
#include <string_view>
#include <vector>

#include "glaze/glaze.hpp"

int main(int argc, char**)
{
   std::string int_out{};
   (void)glz::write_json(std::vector<int64_t>{argc, argc * 1000000003LL, -argc}, int_out);

   std::string float_out{};
   (void)glz::write_json(static_cast<double>(argc) + 0.5, float_out);

   // Error path that previously pulled in the 40 KB integer table via format_error.
   std::string err{};
   int value{};
   const std::string_view bad = "not-a-number";
   if (auto ec = glz::read_json(value, bad)) {
      err = glz::format_error(ec, bad);
   }

   // Reference all outputs so none of the above is dead-code-eliminated wholesale.
   const std::size_t total = int_out.size() + float_out.size() + err.size();
   return static_cast<int>(total & 0u);
}
