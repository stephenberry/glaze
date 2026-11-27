// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 450, 1
// TRACE: {"o"}
//
// A nested object is written by `write_block_mapping` directly, so `to<YAML, Inner>::op` is never
// instantiated and its check alone would leave the old incomplete-type error. The report has to come
// from the helper that emits the members.
#include "shapes.hpp"

#include <glaze/yaml.hpp>

int main()
{
   Nested v{};
   std::string b;
   (void)glz::write_yaml(v, b);
}
