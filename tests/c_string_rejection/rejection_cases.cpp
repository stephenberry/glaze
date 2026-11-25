// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// A binary document may hold a zero byte anywhere, so a C string's strlen length would truncate it.
// GLZ_REJECT_FORMAT selects the format.

import std;

import glaze.beve;
import glaze.bson;
import glaze.cbor;
import glaze.jsonb;
import glaze.msgpack;

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
