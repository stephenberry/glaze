// EXPECT: one of this object's members has no reader for this format
// TRACE: unsupported_reader<Outer, 1, 1
//
// A struct with one member that has no reader for this format. The diagnostic has to
// name the member (index 1, key "o", type Opaque) instead of leaving the compiler to report an
// incomplete 'glz::to<1, Opaque>' from inside the member loop.
#include "shapes.hpp"

#include <glaze/beve.hpp>

int main()
{
   Outer v{};
   std::vector<std::byte> b;
   glz::read_beve(v, b);
}
