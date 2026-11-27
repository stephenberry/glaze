// EXPECT: one of this object's members has no reader for this format
// TRACE: unsupported_reader<Inner, 400, 1
// TRACE: {"o"}
//
// The read side reaches `from<TOML, Inner>::op`, which carries the check, while the nested table
// reader continues after the failed assert and also prints the old incomplete-type error. The
// diagnostic is still the first error and the one the case pins.
#include "shapes.hpp"

#include <glaze/toml.hpp>

int main()
{
   Nested v{};
   std::string s = R"(head = 1)";
   (void)glz::read_toml(v, s);
}
