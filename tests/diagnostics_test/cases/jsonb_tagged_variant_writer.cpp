// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 3, 1
// TRACE: {"o"}
//
// A tagged variant's object alternative is written inline by `write_reflected_body`, not through
// `to<JSONB, Inner>::op`, so only the check in the variant writer covers this route.
#include "shapes.hpp"

#include <array>
#include <glaze/jsonb.hpp>
#include <string_view>
#include <variant>

using tagged = std::variant<Inner, int>;

template <>
struct glz::meta<tagged>
{
   static constexpr std::string_view tag = "kind";
   static constexpr std::array<std::string_view, 2> ids{"inner", "number"};
};

int main()
{
   tagged v{Inner{}};
   std::vector<std::byte> b;
   (void)glz::write_jsonb(v, b);
}
