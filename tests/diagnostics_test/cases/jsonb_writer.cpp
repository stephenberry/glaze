// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Outer, 3, 1
//
// A struct with one member that has no writer for this format. The diagnostic has to
// name the member (index 1, key "o", type Opaque) instead of leaving the compiler to report an
// incomplete 'glz::to<3, Opaque>' from inside the member loop.
#include "shapes.hpp"

#include <glaze/jsonb.hpp>

int main()
{
   Outer v{};
   std::vector<std::byte> b;
   glz::write_jsonb(v, b);
}
