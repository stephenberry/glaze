// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// A binary document may hold a zero byte anywhere, so a C string's strlen length would truncate it.
// GLZ_REJECT_FORMAT selects the format.

#include <string>

#include "glaze/beve.hpp"
#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"

struct holds_int
{
   int value{};
};

int main()
{
   holds_int value{};
   std::string buffer{};
   [[maybe_unused]] auto ec = glz::read<glz::opts{.format = GLZ_REJECT_FORMAT}>(value, buffer.data());
   return 0;
}
