// Glaze Library
// For the license information refer to glaze.hpp
// glz::optionalize<T> generates its members with C++26 reflection, so this target is only built under
// glaze_ENABLE_REFLECTION26

#include "glaze/reflection/optionalize.hpp"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/json/patch.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace
{
   struct window_config
   {
      int width = 800;
      int height = 600;
      std::string title = "default";

      bool operator==(const window_config&) const = default;
   };

   struct app_config
   {
      std::string name = "app";
      int threads = 4;
      window_config window{};
      std::vector<std::string> plugins{"core", "net"};
      std::optional<std::string> theme{};

      bool operator==(const app_config&) const = default;
   };

   using partial_app_config = glz::optionalize<app_config>;
   using partial_window_config = glz::optionalize<window_config>;
   using shallow_app_config = glz::optionalize_shallow<app_config>;

   static_assert(std::same_as<decltype(partial_app_config::name), std::optional<std::string>>);
   static_assert(std::same_as<decltype(partial_app_config::threads), std::optional<int>>);
   static_assert(std::same_as<decltype(partial_app_config::plugins), std::optional<std::vector<std::string>>>);
   // Not double wrapped
   static_assert(std::same_as<decltype(partial_app_config::theme), std::optional<std::string>>);
   static_assert(std::same_as<decltype(partial_app_config::window), std::optional<partial_window_config>>);
   static_assert(std::same_as<decltype(shallow_app_config::window), std::optional<window_config>>);
   static_assert(std::same_as<glz::optionalize<app_config>, partial_app_config>);
   static_assert(glz::reflectable<partial_app_config>);
   static_assert(glz::member_names<partial_app_config> == glz::member_names<app_config>);

   // The keys of these would change or lose their meaning in a generated type
   struct renamed
   {
      int value{};
   };

   struct local_meta
   {
      int value{};
      struct glaze
      {
         static constexpr auto value = glz::object("v", &local_meta::value);
      };
   };

   struct base_member
   {
      int value{};
   };
   struct derived : base_member
   {
      int other{};
   };

   struct array_member
   {
      int values[3]{};
   };
   struct reference_member
   {
      int& value;
   };
   struct const_member
   {
      const int value{};
   };
   struct bit_field_member
   {
      unsigned value : 3 {};
   };
   struct unnamed_member
   {
      union {
         int a;
         float b;
      };
   };
   struct skip_member
   {
      glz::skip value{};
   };
   struct hidden_member
   {
      glz::hidden value{};
   };
   class not_aggregate
   {
     public:
      not_aggregate() = default;
      int value{};
   };

   template <class T>
   concept can_optionalize = requires { typename glz::optionalize<T>; };

   static_assert(can_optionalize<app_config>);
   static_assert(!can_optionalize<int>);
   static_assert(!can_optionalize<std::string>);
   static_assert(!can_optionalize<std::vector<int>>);
   static_assert(!can_optionalize<local_meta>);
   static_assert(!can_optionalize<derived>);
   static_assert(!can_optionalize<array_member>);
   static_assert(!can_optionalize<reference_member>);
   static_assert(!can_optionalize<const_member>);
   static_assert(!can_optionalize<bit_field_member>);
   static_assert(!can_optionalize<unnamed_member>);
   static_assert(!can_optionalize<skip_member>);
   static_assert(!can_optionalize<hidden_member>);
   static_assert(!can_optionalize<not_aggregate>);
   static_assert(!can_optionalize<const app_config>);

   // A nested type that cannot be optionalized is a value, read and replaced whole
   struct holds_values
   {
      renamed by_meta{};
      array_member by_array{};
      int count = 0;
   };

   // Nothing here has a default constructor, which a partial must not need
   struct no_default
   {
      no_default() = delete;
      explicit no_default(int value) : value(value) {}
      int value;
   };

   struct holds_no_default
   {
      no_default required;
      int count = 0;
   };

   struct move_only_config
   {
      std::unique_ptr<int> ptr{};
   };

   struct empty_config
   {};

   struct holds_empty
   {
      empty_config nothing{};
      int count = 0;
   };

   struct level_3
   {
      int value = 3;
      int untouched = 33;
   };
   struct level_2
   {
      level_3 next{};
      int untouched = 22;
   };
   struct level_1
   {
      level_2 next{};
      int untouched = 11;
   };
}

template <>
struct glz::meta<renamed>
{
   static constexpr auto value = glz::object("renamed_value", &renamed::value);
};

static_assert(!can_optionalize<renamed>);
static_assert(std::same_as<decltype(glz::optionalize<holds_values>::by_meta), std::optional<renamed>>);
static_assert(std::same_as<decltype(glz::optionalize<holds_values>::by_array), std::optional<array_member>>);

suite optionalize_read_write = [] {
   "missing keys stay nullopt"_test = [] {
      partial_app_config partial{};
      expect(not glz::read_json(partial, R"({"threads":8,"window":{"title":"mine"}})"));

      expect(partial.threads == 8);
      expect(not partial.name);
      expect(not partial.plugins);
      expect(not partial.theme);
      expect(partial.window.has_value());
      expect(partial.window->title == "mine");
      expect(not partial.window->width);
      expect(not partial.window->height);
   };

   "error_on_missing_keys accepts a partial document"_test = [] {
      partial_app_config partial{};
      expect(not glz::read<glz::opts{.error_on_missing_keys = true}>(partial, std::string{R"({"threads":8})"}));
      expect(partial.threads == 8);
   };

   "an unknown key is still an error"_test = [] {
      partial_app_config partial{};
      expect(glz::read_json(partial, R"({"thread_count":8})").ec == glz::error_code::unknown_key);
   };

   "a wrong value type is still an error"_test = [] {
      partial_app_config partial{};
      expect(bool(glz::read_json(partial, R"({"threads":"eight"})")));
   };

   "skip_null_members writes only what was set"_test = [] {
      partial_app_config partial{};
      expect(glz::write_json(partial).value_or("error") == "{}");

      expect(not glz::read_json(partial, R"({"threads":8,"window":{"title":"mine"},"plugins":["x"]})"));
      expect(glz::write<glz::opts{.skip_null_members = true}>(partial).value_or("error") ==
             R"({"threads":8,"window":{"title":"mine"},"plugins":["x"]})")
         << glz::write_json(partial).value_or("error");
   };

   "skip_null_members = false writes every key"_test = [] {
      partial_app_config partial{};
      partial.threads = 8;
      partial.window.emplace().width = 1;
      expect(
         glz::write<glz::opts{.skip_null_members = false}>(partial).value_or("error") ==
         R"({"name":null,"threads":8,"window":{"width":1,"height":null,"title":null},"plugins":null,"theme":null})");
   };

   "explicit null reads as not set"_test = [] {
      partial_app_config partial{};
      partial.threads = 1;
      expect(not glz::read_json(partial, R"({"threads":null})"));
      expect(not partial.threads);
   };

   "beve round trip"_test = [] {
      partial_app_config partial{};
      partial.threads = 8;
      partial.window.emplace().title = "mine";

      std::string buffer{};
      expect(not glz::write_beve(partial, buffer));

      partial_app_config result{};
      expect(not glz::read_beve(result, buffer));
      expect(result.threads == 8);
      expect(not result.name);
      expect(result.window.has_value());
      expect(result.window->title == "mine");
      expect(not result.window->width);
   };
};

suite optionalize_apply_partial = [] {
   "end to end: partial document onto a full config"_test = [] {
      const std::string user_config = R"({"threads":8,"window":{"title":"mine"},"plugins":["only"],"theme":"dark"})";

      partial_app_config partial{};
      expect(not glz::read_json(partial, user_config));

      // What the user set survives a round trip, and nothing the user left out appears
      const auto written = glz::write_json(partial).value_or("error");
      expect(written == user_config) << written;

      app_config config{};
      config.name = "kept";
      config.window.width = 1920;
      glz::apply_partial(config, partial);

      expect(config.name == "kept");
      expect(config.threads == 8);
      expect(config.window.width == 1920);
      expect(config.window.height == 600);
      expect(config.window.title == "mine");
      // A container is a single value: it is replaced, not appended to
      expect(config.plugins == std::vector<std::string>{"only"});
      expect(config.theme == "dark");
   };

   "an empty partial changes nothing"_test = [] {
      app_config config{};
      config.name = "kept";
      const auto before = config;
      glz::apply_partial(config, partial_app_config{});
      expect(config == before);
   };

   "an engaged empty value overrides"_test = [] {
      app_config config{};
      partial_app_config partial{};
      expect(not glz::read_json(partial, R"({"name":"","plugins":[]})"));
      glz::apply_partial(config, partial);
      expect(config.name.empty());
      expect(config.plugins.empty());
   };

   "nullopt cannot clear an already optional member"_test = [] {
      app_config config{};
      config.theme = "dark";
      partial_app_config partial{};
      expect(not glz::read_json(partial, R"({"theme":null})"));
      glz::apply_partial(config, partial);
      expect(config.theme == "dark");
   };

   "shallow replaces a nested aggregate whole"_test = [] {
      app_config config{};
      config.window.width = 1920;

      shallow_app_config partial{};
      expect(not glz::read_json(partial, R"({"window":{"title":"mine"}})"));
      expect(not partial.name);
      glz::apply_partial(config, partial);

      expect(config.window.title == "mine");
      expect(config.window.width == 800);
   };

   "deep nesting keeps every sibling"_test = [] {
      level_1 config{};
      config.next.next.untouched = 1;
      config.next.untouched = 2;
      config.untouched = 3;
      glz::optionalize<level_1> partial{};
      expect(not glz::read_json(partial, R"({"next":{"next":{"value":7}}})"));
      glz::apply_partial(config, partial);

      expect(config.next.next.value == 7);
      expect(config.next.next.untouched == 1);
      expect(config.next.untouched == 2);
      expect(config.untouched == 3);
   };

   "nested values that cannot be optionalized are replaced whole"_test = [] {
      holds_values config{};
      config.by_array.values[1] = 5;

      glz::optionalize<holds_values> partial{};
      expect(not glz::read_json(partial, R"({"by_meta":{"renamed_value":3},"by_array":{"values":[7,8,9]}})"));
      expect(not partial.count);
      glz::apply_partial(config, partial);

      expect(config.by_meta.value == 3);
      expect(config.by_array.values[0] == 7);
      expect(config.by_array.values[1] == 8);
      expect(config.count == 0);
   };

   "the target does not need a default constructor"_test = [] {
      holds_no_default config{no_default{1}, 2};
      glz::optionalize<holds_no_default> partial{};
      expect(not partial.required);
      partial.count = 3;
      glz::apply_partial(config, partial);
      expect(config.required.value == 1);
      expect(config.count == 3);

      partial.required.emplace(9);
      glz::apply_partial(config, partial);
      expect(config.required.value == 9);
   };

   "move only members move out of an rvalue partial"_test = [] {
      move_only_config config{};
      glz::optionalize<move_only_config> partial{};
      partial.ptr = std::make_unique<int>(5);
      glz::apply_partial(config, std::move(partial));
      expect(config.ptr != nullptr);
      expect(*config.ptr == 5);
   };

   "a const partial is copied from"_test = [] {
      app_config config{};
      partial_app_config mutable_partial{};
      mutable_partial.name = "copied";
      const auto& partial = mutable_partial;
      glz::apply_partial(config, partial);
      expect(config.name == "copied");
      expect(partial.name == "copied");
   };

   "an rvalue partial moves"_test = [] {
      app_config config{};
      partial_app_config partial{};
      partial.plugins = std::vector<std::string>{"a", "b"};
      const auto* data = partial.plugins->data();
      glz::apply_partial(config, std::move(partial));
      expect(config.plugins.data() == data);
   };

   "empty nested aggregate"_test = [] {
      holds_empty config{};
      glz::optionalize<holds_empty> partial{};
      expect(not glz::read_json(partial, R"({"nothing":{},"count":2})"));
      expect(partial.nothing.has_value());
      glz::apply_partial(config, partial);
      expect(config.count == 2);
   };

   "usable at compile time"_test = [] {
      static constexpr auto result = [] {
         level_3 config{};
         glz::optionalize<level_3> partial{};
         partial.value = 9;
         glz::apply_partial(config, partial);
         return config;
      }();
      static_assert(result.value == 9);
      static_assert(result.untouched == 33);
   };

   "a written partial is also a valid JSON merge patch"_test = [] {
      partial_app_config partial{};
      expect(not glz::read_json(partial, R"({"threads":8,"window":{"title":"mine"},"plugins":["only"]})"));

      app_config via_helper{};
      glz::apply_partial(via_helper, partial);

      app_config via_patch{};
      expect(not glz::merge_patch(via_patch, glz::write_json(partial).value_or("error")));

      expect(via_helper == via_patch);
   };
};

int main() {}
