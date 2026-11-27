// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Outer, 1, 1
//
// BEVE's positional layout (`structs_as_arrays`) has its own members loop, separate from the keyed
// one, so each needs the check: a struct with one member that has no writer must be reported under
// BEVE's format id instead of failing from inside the loop.
#include "shapes.hpp"

#include <glaze/beve.hpp>

int main()
{
   constexpr auto opts = glz::opt_true<glz::opts{.format = glz::BEVE}, glz::structs_as_arrays_opt_tag{}>;
   Outer v{};
   std::vector<std::byte> b;
   (void)glz::write<opts>(v, b);
}
