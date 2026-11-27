// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 400, 1
// TRACE: {"o"}
//
// An inline table is written by `write_inline_object`, not by `to<TOML, Inner>::op`, so a check at
// the object entry point alone would miss this route. `inline_table_t` forces the inline syntax that
// takes it.
#include "shapes.hpp"

#include <glaze/toml.hpp>

int main()
{
   std::vector<Inner> rows{};
   glz::inline_table_t wrapper{rows};
   std::string b;
   (void)glz::write_toml(wrapper, b);
}
