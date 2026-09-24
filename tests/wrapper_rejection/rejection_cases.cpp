// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// glz::invoke is a read-only, JSON-only wrapper. Before every format rejected it, a format without a
// specialization reflected the wrapper itself: writing produced an empty object and reading wrote into
// the wrapper's internals, both without complaint. GLZ_REJECT_FORMAT selects the format, and
// GLZ_REJECT_WRITE or GLZ_REJECT_READ the direction.

#include <string>

#include "glaze/beve.hpp"
#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/json.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"
#include "glaze/toml.hpp"
#include "glaze/yaml.hpp"

struct holds_invoke
{
   int y{};
   void add_one() { ++y; }
};

template <>
struct glz::meta<holds_invoke>
{
   using T = holds_invoke;
   static constexpr auto value = object("y", &T::y, "add_one", invoke<&T::add_one>);
};

int main()
{
   [[maybe_unused]] holds_invoke value{};
   [[maybe_unused]] std::string buffer{};
#ifdef GLZ_REJECT_WRITE
   [[maybe_unused]] auto ec = glz::write<glz::opts{.format = GLZ_REJECT_FORMAT}>(value, buffer);
#endif
#ifdef GLZ_REJECT_READ
   [[maybe_unused]] auto ec = glz::read<glz::opts{.format = GLZ_REJECT_FORMAT}>(value, buffer);
#endif
   return 0;
}
