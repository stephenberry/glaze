// EXPECT: one of this object's members has no reader for this format
// TRACE: unsupported_reader<Inner, 3, 1
// TRACE: {"o"}
//
// A tagged variant's object alternative is read through `read_object_body`, not through
// `from<JSONB, Inner>::op`, so only the check in the variant reader covers this route.
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
   tagged v{};
   std::vector<std::byte> b;
   (void)glz::read_jsonb(v, b);
}
