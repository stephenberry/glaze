// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// A binary document may hold a zero byte anywhere, so a C string's strlen length truncates it. Before
// binary formats rejected a C string, reading a uint64_t whose encoding held a zero byte failed at
// runtime with unexpected_end. GLZ_REJECT_FORMAT selects the format.

#include <cstdint>
#include <string>

#include "glaze/beve.hpp"
#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"

struct holds_u64
{
   uint64_t value{};
};

int main()
{
   [[maybe_unused]] holds_u64 value{};
   [[maybe_unused]] std::string buffer{};
   [[maybe_unused]] auto ec = glz::read<glz::opts{.format = GLZ_REJECT_FORMAT}>(value, buffer.data());
   return 0;
}
