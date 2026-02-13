// YAML conformance tests generated from https://github.com/yaml/yaml-test-suite.
// Concrete-struct variant: tests parse behavior into explicit C++ structs.

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct player_stats
{
   std::string name{};
   int hr{};
   double avg{};
};

struct escaped_slash_t
{
   std::string escaped_slash{};
};

template <>
struct glz::meta<escaped_slash_t>
{
   using T = escaped_slash_t;
   static constexpr auto value = object("escaped slash", &T::escaped_slash);
};

struct foo_bar_t
{
   std::string foo{};
   std::string bar{};
};

struct x_colon_t
{
   std::string x{};
};

struct folded_literal_t
{
   std::string folded{};
   std::string literal{};
};

struct weird_key_t
{
   std::string qfoo{};
   int bar{};
};

template <>
struct glz::meta<weird_key_t>
{
   using T = weird_key_t;
   static constexpr auto value = object("?foo", &T::qfoo, "bar", &T::bar);
};

struct key_seq_t
{
   std::vector<std::string> key{};
};

struct inner_bf_t
{
   std::string bar{};
};

struct foo_nested_t
{
   inner_bf_t foo{};
};

struct inner_cd_t
{
   std::string c{};
};

struct inner_fg_t
{
   std::string f{};
};

struct a_nested_t
{
   inner_cd_t b{};
   inner_fg_t e{};
};

struct deep_nested_t
{
   a_nested_t a{};
   std::string h{};
};

struct quote_pair_t
{
   std::string single{};
   std::string dbl{};
};

template <>
struct glz::meta<quote_pair_t>
{
   using T = quote_pair_t;
   static constexpr auto value = object(&T::single, "double", &T::dbl);
};

struct chomp_t
{
   std::string strip{};
   std::string clip{};
   std::string keep{};
};

struct seq_holder_t
{
   std::vector<std::string> a{};
};

struct one_three_t
{
   int one{};
   int three{};
};

struct foo_bar_num_t
{
   int bar{};
};

struct baz_foo_t
{
   foo_bar_num_t foo{};
   int baz{};
};

struct teams_t
{
   std::vector<std::string> american{};
   std::vector<std::string> national{};
};

struct hr_rbi_t
{
   std::vector<std::string> hr{};
   std::vector<std::string> rbi{};
};

struct stats_t
{
   int hr{};
   int rbi{};
   double avg{};
};

struct block_mapping_t
{
   std::string key{};
};

struct block_mapping_root_t
{
   block_mapping_t block_mapping{};
};

template <>
struct glz::meta<block_mapping_root_t>
{
   using T = block_mapping_root_t;
   static constexpr auto value = object("block mapping", &T::block_mapping);
};

struct null_pair_t
{
   std::optional<std::string> a{};
   std::optional<std::string> b{};
};

struct anchored_alias_t
{
   std::string anchored{};
   std::string alias{};
};

struct key_colons_t
{
   std::string value{};
};

template <>
struct glz::meta<key_colons_t>
{
   using T = key_colons_t;
   static constexpr auto value = object("key ends with two colons::", &T::value);
};

struct foo_tab_t
{
   std::string foo{};
};

struct key1_key2_t
{
   std::string key1{};
   std::string key2{};
};

struct action_t
{
   std::string time{};
   std::string player{};
   std::string action{};
};

struct colors_t
{
   std::string sky{};
   std::string sea{};
};

struct seq_map_t
{
   std::vector<std::string> sequence{};
   colors_t mapping{};
};

struct matches_pct_t
{
   int matches_pct{};
};

template <>
struct glz::meta<matches_pct_t>
{
   using T = matches_pct_t;
   static constexpr auto value = object("matches %", &T::matches_pct);
};

struct one_a_t
{
   std::string a{};
};

struct zh7c_t
{
   std::string a{};
   std::string c{};
};

suite yaml_conformance_struct = [] {
   "229Q_struct"_test = [] {
      std::string yaml = R"yaml(-
  name: Mark McGwire
  hr:   65
  avg:  0.278
-
  name: Sammy Sosa
  hr:   63
  avg:  0.288
)yaml";
      std::vector<player_stats> parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.size() == 2u);
      expect(parsed[0].name == "Mark McGwire");
      expect(parsed[1].hr == 63);
   };

   "236B_struct_fail"_test = [] {
      std::string yaml = R"yaml(foo:
  bar
invalid
)yaml";
      foo_bar_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   "3UYS_struct"_test = [] {
      std::string yaml = R"yaml(escaped slash: "a\/b"
)yaml";
      escaped_slash_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.escaped_slash == "a/b");
   };

   "54T7_struct"_test = [] {
      std::string yaml = R"yaml({foo: you, bar: far}
)yaml";
      foo_bar_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.foo == "you");
      expect(parsed.bar == "far");
   };

   "58MP_struct"_test = [] {
      std::string yaml = R"yaml({x: :x}
)yaml";
      x_colon_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.x == ":x");
   };

   "5BVJ_struct"_test = [] {
      std::string yaml = R"yaml(literal: |
  some
  text
folded: >
  some
  text
)yaml";
      folded_literal_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.literal == "some\ntext\n");
      expect(parsed.folded == "some text\n");
   };

   "652Z_struct"_test = [] {
      std::string yaml = R"yaml({ ?foo: bar,
  bar: 42 }
)yaml";
      weird_key_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.qfoo == "bar");
      expect(parsed.bar == 42);
   };

   "8QBE_struct"_test = [] {
      std::string yaml = R"yaml(key:
 - item1
 - item2
)yaml";
      key_seq_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.key.size() == 2u);
      expect(parsed.key[0] == "item1");
   };

   "9FMG_struct"_test = [] {
      std::string yaml = R"yaml(a:
  b:
    c: d
  e:
    f: g
h: i
)yaml";
      deep_nested_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.a.b.c == "d");
      expect(parsed.a.e.f == "g");
      expect(parsed.h == "i");
   };

   "9J7A_struct"_test = [] {
      std::string yaml = R"yaml(foo:
  bar: baz
)yaml";
      foo_nested_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.foo.bar == "baz");
   };

   "9SHH_struct"_test = [] {
      std::string yaml = R"yaml(single: 'text'
double: "text"
)yaml";
      quote_pair_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.single == "text");
      expect(parsed.dbl == "text");
   };

   "A6F9_struct"_test = [] {
      std::string yaml = R"yaml(strip: |-
  text
clip: |
  text
keep: |+
  text
)yaml";
      chomp_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.strip == "text");
      expect(parsed.clip == "text\n");
      expect(parsed.keep == "text\n");
   };

   "CXX2_struct"_test = [] {
      std::string yaml = R"yaml(--- &anchor a: b
)yaml";
      one_a_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.a == "b");
   };

   "D88J_struct"_test = [] {
      std::string yaml = R"yaml(a: [b, c]
)yaml";
      seq_holder_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.a.size() == 2u);
      expect(parsed.a[1] == "c");
   };

   "J7VC_struct"_test = [] {
      std::string yaml = R"yaml(one: 2
three: 4
)yaml";
      one_three_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.one == 2);
      expect(parsed.three == 4);
   };

   "KMK3_struct"_test = [] {
      std::string yaml = R"yaml(foo:
  bar: 1
baz: 2
)yaml";
      baz_foo_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.foo.bar == 1);
      expect(parsed.baz == 2);
   };

   "PBJ2_struct"_test = [] {
      std::string yaml = R"yaml(american:
  - Boston Red Sox
  - Detroit Tigers
  - New York Yankees
national:
  - New York Mets
  - Chicago Cubs
  - Atlanta Braves
)yaml";
      teams_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.american.size() == 3u);
      expect(parsed.national[2] == "Atlanta Braves");
   };

   "7BUB_struct"_test = [] {
      std::string yaml = R"yaml(hr:
  - Mark McGwire
  - Sammy Sosa
rbi:
  - Sammy Sosa
  - Ken Griffey
)yaml";
      hr_rbi_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.hr[0] == "Mark McGwire");
      expect(parsed.rbi[1] == "Ken Griffey");
   };

   "SYW4_struct"_test = [] {
      std::string yaml = R"yaml(hr:  65    # Home runs
rbi: 147   # Runs Batted In
avg: 0.278 # Batting Average
)yaml";
      stats_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.hr == 65);
      expect(parsed.rbi == 147);
      expect(std::abs(parsed.avg - 0.278) < 1e-12);
   };

   "TE2A_struct"_test = [] {
      std::string yaml = R"yaml(block mapping:
  key: value
)yaml";
      block_mapping_root_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.block_mapping.key == "value");
   };

   "6KGN_struct"_test = [] {
      std::string yaml = R"yaml(---
a: &anchor
b: *anchor
)yaml";
      null_pair_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(!parsed.a.has_value() || parsed.a->empty());
      expect(!parsed.b.has_value() || parsed.b->empty());
   };

   "CUP7_struct"_test = [] {
      std::string yaml = R"yaml(anchored: !local &anchor value
alias: *anchor
)yaml";
      anchored_alias_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.anchored == "value");
      expect(parsed.alias == "value");
   };

   "8CWC_struct"_test = [] {
      std::string yaml = R"yaml(key ends with two colons::: value
)yaml";
      key_colons_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.value == "value");
   };

   "96NN_00_struct"_test = [] {
      std::string yaml = R"yaml(foo: |-
 	bar
)yaml";
      foo_tab_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.foo == "\tbar");
   };

   "9KBC_struct"_test = [] {
      std::string yaml = R"yaml(--- key1: value1
 key2: value2
)yaml";
      key1_key2_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.key1 == "value1");
      expect(parsed.key2 == "value2");
   };

   "U9NS_struct"_test = [] {
      std::string yaml = R"yaml(time: 20:03:20
player: Sammy Sosa
action: strike (miss)
)yaml";
      action_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.time == "20:03:20");
      expect(parsed.action == "strike (miss)");
   };

   "UDR7_struct"_test = [] {
      std::string yaml = R"yaml(sequence: [ one, two, ]
mapping: { sky: blue, sea: green }
)yaml";
      seq_map_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.sequence.size() == 2u);
      expect(parsed.mapping.sky == "blue");
      expect(parsed.mapping.sea == "green");
   };

   "UT92_struct"_test = [] {
      std::string yaml = R"yaml(---
{ matches
% : 20 }
...
---
# Empty
...
)yaml";
      matches_pct_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.matches_pct == 20);
   };

   "ZH7C_struct"_test = [] {
      std::string yaml = R"yaml(&a a: b
c: &d d
)yaml";
      zh7c_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.a == "b");
      expect(parsed.c == "d");
   };
};

int main() { return 0; }
