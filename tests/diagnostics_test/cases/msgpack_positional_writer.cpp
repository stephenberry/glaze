// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Outer, 30, 1
//
// MSGPACK's positional layout (`structs_as_arrays`) has no keys and its own members loop, separate
// from the keyed one, so each needs the check: a struct with one member that has no writer must be
// reported under MSGPACK's format id instead of failing from inside the loop.
#include "shapes.hpp"

#include <glaze/msgpack.hpp>

int main()
{
   constexpr auto opts = glz::opt_true<glz::opts{.format = glz::MSGPACK}, glz::structs_as_arrays_opt_tag{}>;
   Outer v{};
   std::vector<std::byte> b;
   (void)glz::write<opts>(v, b);
}
