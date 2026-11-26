// This file must FAIL to compile, with the message its ctest entry expects.
//
// glz::cast has no BSON element encoding, so a cast member reaches the serialize<BSON> dispatcher with
// a primitive. Before the dispatcher rejected that, the member was written as its raw payload with no
// type byte or key, producing a malformed document without any error.

#include <string>

#include "glaze/bson.hpp"
#include "glaze/core/cast.hpp"

struct holds_cast
{
   int a{};
};

template <>
struct glz::meta<holds_cast>
{
   static constexpr auto value = object("a", cast<&holds_cast::a, double>);
};

int main()
{
   [[maybe_unused]] holds_cast value{};
   [[maybe_unused]] std::string buffer{};
   [[maybe_unused]] auto ec = glz::write_bson(value, buffer);
   return 0;
}
