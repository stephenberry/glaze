// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// A glz::custom getter that cannot be invoked on its parent must be reported by the getter's own
// static_assert. The block writers look through glz::custom getters to lay a value out by what the
// getter returns; an uninvocable getter resolves to itself, and before it was excluded from that
// unwrapping, TOML recursed on it forever at runtime (a stack overflow instead of a diagnostic).
// GLZ_REJECT_FORMAT selects the format, and GLZ_REJECT_POSITION where the custom type appears.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "glaze/toml.hpp"
#include "glaze/yaml.hpp"

struct uninvocable_getter
{
   int v{};
};

template <>
struct glz::meta<uninvocable_getter>
{
   using T = uninvocable_getter;
   // The getter's extra parameters can never be supplied, so it is not invocable on T
   static constexpr auto value = glz::custom<[](T& s, const int& i) { s.v = i; }, [](T& s, int, int) { return s.v; }>;
};

struct member_holder
{
   uninvocable_getter value{};
};

struct sequence_holder
{
   std::vector<uninvocable_getter> values{{}};
};

struct map_value_holder
{
   std::map<std::string, uninvocable_getter> values{{"key", {}}};
};

struct optional_holder
{
   std::optional<uninvocable_getter> value{uninvocable_getter{}};
};

int main()
{
   [[maybe_unused]] GLZ_REJECT_POSITION value{};
   [[maybe_unused]] std::string buffer{};
   [[maybe_unused]] auto ec = glz::write<glz::opts{.format = GLZ_REJECT_FORMAT}>(value, buffer);
   return 0;
}
