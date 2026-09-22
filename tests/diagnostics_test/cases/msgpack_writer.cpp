// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Outer, 30, 1
//
// A struct with one member that has no writer for this format. The diagnostic has to
// name the member (index 1, key "o", type Opaque) instead of leaving the compiler to report an
// incomplete 'glz::to<30, Opaque>' from inside the member loop.
#include "shapes.hpp"

#include <glaze/msgpack.hpp>

int main()
{
   Outer v{};
   std::vector<std::byte> b;
   glz::write_msgpack(v, b);
}
