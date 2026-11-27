// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 450, 1
// TRACE: {"o"}
//
// A tagged variant's object alternative in flow style is laid out by `write_flow_mapping_members`,
// not by `to<YAML, Inner>::op`, so only a check in the helper covers this route.
#include "shapes.hpp"

#include <array>
#include <glaze/yaml.hpp>
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
   std::string b;
   constexpr glz::yaml::yaml_opts opts{.flow_style = true};
   (void)glz::write<opts>(v, b);
}
