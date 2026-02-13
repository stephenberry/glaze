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

struct plain_quoted_t
{
   std::string plain{};
   std::string quoted{};
};

struct top1_key1_t
{
   std::string key1{};
};

struct top1_top2_t
{
   top1_key1_t top1{};
   std::string top2{};
};

struct folding_chomping_t
{
   std::string Folding{};
   std::string Chomping{};
};

struct tab_string_t
{
   std::string tab{};
};

struct seq_mapping_t
{
   std::string a{};
   std::vector<std::string> seq{};
   std::string c{};
};

struct foo_int_t
{
   int foo{};
};

struct wanted_t
{
   std::string wanted{};
};

struct name_accomplishment_stats_t
{
   std::string name{};
   std::string accomplishment{};
   std::string stats{};
};

struct foo_bar_baz_t
{
   std::string foo{};
   std::string bar{};
   std::string baz{};
};

struct many_whitespace_strings_t
{
   std::string a{};
   std::string b{};
   std::string c{};
   std::string d{};
   std::string e{};
   std::string f{};
   std::string g{};
   std::string h{};
};

struct binary_description_t
{
   std::string canonical{};
   std::string generic{};
   std::string description{};
};

struct adjacent_readable_empty_t
{
   std::string adjacent{};
   std::string readable{};
   std::optional<std::string> empty{};
};

struct foo_bar_int_t
{
   int foo{};
   int bar{};
};

struct foo_bar_text_t
{
   int foo{};
   int bar{};
   std::string text{};
};

struct int_lists_t
{
   std::vector<int> foo{};
   std::vector<int> bar{};
};

struct time_user_warning_t
{
   std::string Time{};
   std::string User{};
   std::string Warning{};
};

struct a_b_string_t
{
   std::string a{};
   std::string b{};
};

struct foo_string_bar_int_t
{
   std::string foo{};
   int bar{};
};

template <class T>
static void expect_yaml_matches_json_case(const std::string& yaml, const std::string& expected_json)
{
   T parsed{};
   auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
   expect(!ec) << glz::format_error(ec, yaml);
   if (ec) return;

   T expected{};
   auto expected_ec = glz::read_json(expected, expected_json);
   expect(!expected_ec) << glz::format_error(expected_ec, expected_json);
   if (expected_ec) return;

   std::string actual_json{};
   std::string expected_json_out{};
   (void)glz::write_json(parsed, actual_json);
   (void)glz::write_json(expected, expected_json_out);
   expect(actual_json == expected_json_out) << "expected: " << expected_json_out << "\nactual: " << actual_json;
}

template <class T = foo_bar_t>
static void expect_yaml_error_case(const std::string& yaml)
{
   T parsed{};
   auto ec = glz::read_yaml(parsed, yaml);
   expect(bool(ec));
}

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

   "4CQQ_struct"_test = [] {
      std::string yaml = R"yaml(plain:
  This unquoted scalar
  spans many lines.

quoted: "So does this
  quoted scalar.\n"
)yaml";
      std::string expected_json = R"json({
  "plain": "This unquoted scalar spans many lines.",
  "quoted": "So does this quoted scalar.\n"
}
)json";
      expect_yaml_matches_json_case<plain_quoted_t>(yaml, expected_json);
   };

   "4JVG_struct"_test = [] {
      std::string yaml = R"yaml(top1: &node1
  &k1 key1: val1
top2: &node2
  &v2 val2
)yaml";
      top1_top2_t parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      expect(parsed.top1.key1 == "val1");
      expect(parsed.top2 == "&v2 val2");
   };

   "5GBF_struct"_test = [] {
      std::string yaml = R"yaml(Folding:
  "Empty line
   	
  as a line feed"
Chomping: |
  Clipped empty lines
 

)yaml";
      std::string expected_json = R"json({
  "Folding": "Empty line\nas a line feed",
  "Chomping": "Clipped empty lines\n"
}
)json";
      expect_yaml_matches_json_case<folding_chomping_t>(yaml, expected_json);
   };

   "5NYZ_struct"_test = [] {
      std::string yaml = R"yaml(key:    # Comment
  value
)yaml";
      std::string expected_json = R"json({
  "key": "value"
}
)json";
      expect_yaml_matches_json_case<block_mapping_t>(yaml, expected_json);
   };

   "CPZ3_struct"_test = [] {
      std::string yaml = R"yaml(---
tab: "\tstring"
)yaml";
      std::string expected_json = R"json({
  "tab": "\tstring"
}
)json";
      expect_yaml_matches_json_case<tab_string_t>(yaml, expected_json);
   };

   "D9TU_struct"_test = [] {
      std::string yaml = R"yaml(foo: bar
)yaml";
      std::string expected_json = R"json({
  "foo": "bar"
}
)json";
      expect_yaml_matches_json_case<foo_tab_t>(yaml, expected_json);
   };

   "DC7X_struct"_test = [] {
      std::string yaml = R"yaml(a: b	
seq:	
 - a	
c: d	#X
)yaml";
      std::string expected_json = R"json({
  "a": "b",
  "seq": [
    "a"
  ],
  "c": "d"
}
)json";
      expect_yaml_matches_json_case<seq_mapping_t>(yaml, expected_json);
   };

   "DK95_02_struct"_test = [] {
      std::string yaml = R"yaml(foo: "bar
  	baz"
)yaml";
      std::string expected_json = R"json({
  "foo": "bar baz"
}
)json";
      expect_yaml_matches_json_case<foo_tab_t>(yaml, expected_json);
   };

   "DK95_03_struct"_test = [] {
      std::string yaml = R"yaml( 	
foo: 1
)yaml";
      std::string expected_json = R"json({
  "foo": 1
}
)json";
      expect_yaml_matches_json_case<foo_int_t>(yaml, expected_json);
   };

   "F8F9_struct"_test = [] {
      std::string yaml = R"yaml( # Strip
  # Comments:
strip: |-
  # text
  
 # Clip
  # comments:

clip: |
  # text
 
 # Keep
  # comments:

keep: |+
  # text

 # Trail
  # comments.
)yaml";
      std::string expected_json = R"json({
  "strip": "# text",
  "clip": "# text\n",
  "keep": "# text\n\n"
}
)json";
      expect_yaml_matches_json_case<chomp_t>(yaml, expected_json);
   };

   "H3Z8_struct"_test = [] {
      std::string yaml = R"yaml(---
wanted: love ♥ and peace ☮
)yaml";
      std::string expected_json = R"json({
  "wanted": "love ♥ and peace ☮"
}
)json";
      expect_yaml_matches_json_case<wanted_t>(yaml, expected_json);
   };

   "HMK4_struct"_test = [] {
      std::string yaml = R"yaml(name: Mark McGwire
accomplishment: >
  Mark set a major league
  home run record in 1998.
stats: |
  65 Home Runs
  0.278 Batting Average
)yaml";
      std::string expected_json = R"json({
  "name": "Mark McGwire",
  "accomplishment": "Mark set a major league home run record in 1998.\n",
  "stats": "65 Home Runs\n0.278 Batting Average\n"
}
)json";
      expect_yaml_matches_json_case<name_accomplishment_stats_t>(yaml, expected_json);
   };

   "J5UC_struct"_test = [] {
      std::string yaml = R"yaml(foo: blue
bar: arrr
baz: jazz
)yaml";
      std::string expected_json = R"json({
  "foo": "blue",
  "bar": "arrr",
  "baz": "jazz"
}
)json";
      expect_yaml_matches_json_case<foo_bar_baz_t>(yaml, expected_json);
   };

   "NAT4_struct"_test = [] {
      std::string yaml = R"yaml(---
a: '
  '
b: '  
  '
c: "
  "
d: "  
  "
e: '

  '
f: "

  "
g: '


  '
h: "


  "
)yaml";
      std::string expected_json = R"json({
  "a": " ",
  "b": " ",
  "c": " ",
  "d": " ",
  "e": "\n",
  "f": "\n",
  "g": "\n\n",
  "h": "\n\n"
}
)json";
      expect_yaml_matches_json_case<many_whitespace_strings_t>(yaml, expected_json);
   };

   "P94K_struct"_test = [] {
      std::string yaml = R"yaml(key:    # Comment
        # lines
  value


)yaml";
      std::string expected_json = R"json({
  "key": "value"
}
)json";
      expect_yaml_matches_json_case<block_mapping_t>(yaml, expected_json);
   };

   "QLJ7_struct"_test = [] {
      std::string yaml = R"yaml(%TAG !prefix! tag:example.com,2011:
--- !prefix!A
a: b
--- !prefix!B
c: d
--- !prefix!C
e: f
)yaml";
      std::string expected_json = R"json({
  "a": "b"
}
)json";
      expect_yaml_matches_json_case<one_a_t>(yaml, expected_json);
   };

   "565N_struct"_test = [] {
      std::string yaml = R"yaml(canonical: !!binary "\
 R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5\
 OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+\
 +f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC\
 AgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs="
generic: !!binary |
 R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5
 OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+
 +f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC
 AgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=
description:
 The binary value above is a tiny arrow encoded as a gif image.
)yaml";
      std::string expected_json = R"json({
  "canonical": "R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLCAgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=",
  "generic": "R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5\nOTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+\n+f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC\nAgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=\n",
  "description": "The binary value above is a tiny arrow encoded as a gif image."
}
)json";
      expect_yaml_matches_json_case<binary_description_t>(yaml, expected_json);
   };

   "5U3A_struct"_test = [] {
      std::string yaml = R"yaml(key: - a
     - b
)yaml";
      std::string expected_json = R"json({
  "key": [
    "a",
    "b"
  ]
}
)json";
      expect_yaml_matches_json_case<key_seq_t>(yaml, expected_json);
   };

   "C2DT_struct"_test = [] {
      std::string yaml = R"yaml({
"adjacent":value,
"readable": value,
"empty":
}
)yaml";
      std::string expected_json = R"json({
  "adjacent": "value",
  "readable": "value",
  "empty": null
}
)json";
      expect_yaml_matches_json_case<adjacent_readable_empty_t>(yaml, expected_json);
   };

   "DK95_04_struct"_test = [] {
      std::string yaml = R"yaml(foo: 1
	
bar: 2
)yaml";
      std::string expected_json = R"json({
  "foo": 1,
  "bar": 2
}
)json";
      expect_yaml_matches_json_case<foo_bar_int_t>(yaml, expected_json);
   };

   "H2RW_struct"_test = [] {
      std::string yaml = R"yaml(foo: 1

bar: 2
    
text: |
  a
    
  b

  c
 
  d
)yaml";
      std::string expected_json = R"json({
  "foo": 1,
  "bar": 2,
  "text": "a\n\nb\n\nc\n\nd\n"
}
)json";
      expect_yaml_matches_json_case<foo_bar_text_t>(yaml, expected_json);
   };

   "RLU9_struct"_test = [] {
      std::string yaml = R"yaml(foo:
- 42
bar:
  - 44
)yaml";
      std::string expected_json = R"json({
  "foo": [
    42
  ],
  "bar": [
    44
  ]
}
)json";
      expect_yaml_matches_json_case<int_lists_t>(yaml, expected_json);
   };

   "RZT7_struct"_test = [] {
      std::string yaml = R"yaml(---
Time: 2001-11-23 15:01:42 -5
User: ed
Warning:
  This is an error message
  for the log file
---
Time: 2001-11-23 15:02:31 -5
User: ed
Warning:
  A slightly different error
  message.
---
Date: 2001-11-23 15:03:17 -5
User: ed
Fatal:
  Unknown variable "bar"
Stack:
  - file: TopClass.py
    line: 23
    code: |
      x = MoreObject("345\n")
  - file: MoreClass.py
    line: 58
    code: |-
      foo = bar
)yaml";
      std::string expected_json = R"json({
  "Time": "2001-11-23 15:01:42 -5",
  "User": "ed",
  "Warning": "This is an error message for the log file"
}
)json";
      expect_yaml_matches_json_case<time_user_warning_t>(yaml, expected_json);
   };

   "W5VH_struct"_test = [] {
      std::string yaml = R"yaml(a: &:@*!$"<foo>: scalar a
b: *:@*!$"<foo>:
)yaml";
      std::string expected_json = R"json({
  "a": "scalar a",
  "b": "scalar a"
}
)json";
      expect_yaml_matches_json_case<a_b_string_t>(yaml, expected_json);
   };

   "Y79Y_001_struct"_test = [] {
      std::string yaml = R"yaml(foo: |
 	
bar: 1
)yaml";
      std::string expected_json = R"json({
  "foo": "\t\n",
  "bar": 1
}
)json";
      expect_yaml_matches_json_case<foo_string_bar_int_t>(yaml, expected_json);
   };

   "Z67P_struct"_test = [] {
      std::string yaml = R"yaml(literal: |2
  value
folded: !foo >1
 value
)yaml";
      std::string expected_json = R"json({
  "literal": "value\n",
  "folded": "value\n"
}
)json";
      expect_yaml_matches_json_case<folded_literal_t>(yaml, expected_json);
   };

};

suite yaml_conformance_struct_failures = [] {
   "3HFZ_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
key: value
... invalid
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4EJS_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
a:
	b:
		c: value
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4HVU_struct_fail"_test = [] {
      std::string yaml = R"yaml(key:
   - ok
   - also ok
  - wrong
)yaml";
      expect_yaml_error_case(yaml);
   };

   "6JTT_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
[ [ a, b, c ]
)yaml";
      expect_yaml_error_case(yaml);
   };

   "6S55_struct_fail"_test = [] {
      std::string yaml = R"yaml(key:
 - bar
 - baz
 invalid
)yaml";
      expect_yaml_error_case(yaml);
   };

   "7MNF_struct_fail"_test = [] {
      std::string yaml = R"yaml(top1:
  key1: val1
top2
)yaml";
      expect_yaml_error_case(yaml);
   };

   "8XDJ_struct_fail"_test = [] {
      std::string yaml = R"yaml(key: word1
#  xxx
  word2
)yaml";
      expect_yaml_error_case(yaml);
   };

   "9CWY_struct_fail"_test = [] {
      std::string yaml = R"yaml(key:
 - item1
 - item2
invalid
)yaml";
      expect_yaml_error_case(yaml);
   };

   "9HCY_struct_fail"_test = [] {
      std::string yaml = R"yaml(!foo "bar"
%TAG ! tag:example.com,2000:app/
---
!foo "bar"
)yaml";
      expect_yaml_error_case(yaml);
   };

   "9MMA_struct_fail"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
)yaml";
      expect_yaml_error_case(yaml);
   };

   "BF9H_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
plain: a
       b # end of scalar
       c
)yaml";
      expect_yaml_error_case(yaml);
   };

   "CML9_struct_fail"_test = [] {
      std::string yaml = R"yaml(key: [ word1
#  xxx
  word2 ]
)yaml";
      expect_yaml_error_case(yaml);
   };

   "CQ3W_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
key: "missing closing quote
)yaml";
      expect_yaml_error_case(yaml);
   };

   "DK4H_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
[ key
  : value ]
)yaml";
      expect_yaml_error_case(yaml);
   };

   "G7JE_struct_fail"_test = [] {
      std::string yaml = R"yaml(a\nb: 1
c
 d: 1
)yaml";
      expect_yaml_error_case(yaml);
   };

   "G9HC_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
seq:
&anchor
- a
- b
)yaml";
      expect_yaml_error_case(yaml);
   };

   "GDY7_struct_fail"_test = [] {
      std::string yaml = R"yaml(key: value
this is #not a: key
)yaml";
      expect_yaml_error_case(yaml);
   };

   "H7J7_struct_fail"_test = [] {
      std::string yaml = R"yaml(key: &x
!!map
  a: b
)yaml";
      expect_yaml_error_case(yaml);
   };

   "H7TQ_struct_fail"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2 foo
---
)yaml";
      expect_yaml_error_case(yaml);
   };

   "JY7Z_struct_fail"_test = [] {
      std::string yaml = R"yaml(key1: "quoted1"
key2: "quoted2" no key: nor value
key3: "quoted3"
)yaml";
      expect_yaml_error_case(yaml);
   };

   "LHL4_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
!invalid{}tag scalar
)yaml";
      expect_yaml_error_case(yaml);
   };

   "Q4CL_struct_fail"_test = [] {
      std::string yaml = R"yaml(key1: "quoted1"
key2: "quoted2" trailing content
key3: "quoted3"
)yaml";
      expect_yaml_error_case(yaml);
   };

   "RHX7_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
key: value
%YAML 1.2
---
)yaml";
      expect_yaml_error_case(yaml);
   };

   "S4GJ_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
folded: > first line
  second line
)yaml";
      expect_yaml_error_case(yaml);
   };

   "SF5V_struct_fail"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
%YAML 1.2
---
)yaml";
      expect_yaml_error_case(yaml);
   };

   "SR86_struct_fail"_test = [] {
      std::string yaml = R"yaml(key1: &a value
key2: &b *a
)yaml";
      expect_yaml_error_case(yaml);
   };

   "ZVH3_struct_fail"_test = [] {
      std::string yaml = R"yaml(- key: value
 - item1
)yaml";
      expect_yaml_error_case(yaml);
   };

   "ZXT5_struct_fail"_test = [] {
      std::string yaml = R"yaml([ "key"
  :value ]
)yaml";
      expect_yaml_error_case(yaml);
   };

   "2CMS_struct_fail"_test = [] {
      std::string yaml = R"yaml(this
 is
  invalid: x
)yaml";
      expect_yaml_error_case(yaml);
   };

   "2G84_00_struct_fail"_test = [] {
      std::string yaml = R"yaml(--- |0
)yaml";
      expect_yaml_error_case(yaml);
   };

   "2G84_01_struct_fail"_test = [] {
      std::string yaml = R"yaml(--- |10
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4H7K_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
[ a, b, c ] ]
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4MUZ_00_struct_fail"_test = [] {
      std::string yaml = R"yaml({"foo"
: "bar"}
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4MUZ_01_struct_fail"_test = [] {
      std::string yaml = R"yaml({"foo"
: bar}
)yaml";
      expect_yaml_error_case(yaml);
   };

   "4MUZ_02_struct_fail"_test = [] {
      std::string yaml = R"yaml({foo
: bar}
)yaml";
      expect_yaml_error_case(yaml);
   };

   "55WF_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
"\."
)yaml";
      expect_yaml_error_case(yaml);
   };

   "5LLU_struct_fail"_test = [] {
      std::string yaml = R"yaml(block scalar: >
 
  
   
 invalid
)yaml";
      expect_yaml_error_case(yaml);
   };

   "5MUD_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
{ "foo"
  :bar }
)yaml";
      expect_yaml_error_case(yaml);
   };

   "62EZ_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
x: { y: z }in: valid
)yaml";
      expect_yaml_error_case(yaml);
   };

   "2SXE_struct_fail"_test = [] {
      std::string yaml = R"yaml(&a: key: &a value
foo:
  *a:
)yaml";
      expect_yaml_error_case(yaml);
   };

   "7W2P_struct_fail"_test = [] {
      std::string yaml = R"yaml(? a
? b
c:
)yaml";
      expect_yaml_error_case(yaml);
   };

   "A984_struct_fail"_test = [] {
      std::string yaml = R"yaml(a: b
 c
d:
 e
  f
)yaml";
      expect_yaml_error_case(yaml);
   };

   "AZ63_struct_fail"_test = [] {
      std::string yaml = R"yaml(one:
- 2
- 3
four: 5
)yaml";
      expect_yaml_error_case(yaml);
   };

   "BU8L_struct_fail"_test = [] {
      std::string yaml = R"yaml(key: &anchor
 !!map
  a: b
)yaml";
      expect_yaml_error_case(yaml);
   };

   "EHF6_struct_fail"_test = [] {
      std::string yaml = R"yaml(!!map {
  k: !!seq
  [ a, !!str b]
}
)yaml";
      expect_yaml_error_case(yaml);
   };

   "GH63_struct_fail"_test = [] {
      std::string yaml = R"yaml(? a
: 1.3
fifteen: d
)yaml";
      expect_yaml_error_case(yaml);
   };

   "L94M_struct_fail"_test = [] {
      std::string yaml = R"yaml(? !!str a
: !!int 47
? c
: !!str d
)yaml";
      expect_yaml_error_case(yaml);
   };

   "M5C3_struct_fail"_test = [] {
      std::string yaml = R"yaml(literal: |2
  value
folded:
   !foo
  >1
 value
)yaml";
      expect_yaml_error_case(yaml);
   };

   "RR7F_struct_fail"_test = [] {
      std::string yaml = R"yaml(a: 4.2
? d
: 23
)yaml";
      expect_yaml_error_case(yaml);
   };

   "ZWK4_struct_fail"_test = [] {
      std::string yaml = R"yaml(---
a: 1
? b
&anchor c: 3
)yaml";
      expect_yaml_error_case(yaml);
   };
};

int main() { return 0; }
