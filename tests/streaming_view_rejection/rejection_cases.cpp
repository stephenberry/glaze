// Each GLZ_REJECT_CASE_* below must FAIL to compile. The build target for each case is driven by
// a ctest entry marked WILL_FAIL, so a guard that stops firing turns into a failing test rather
// than into silently wrong data at runtime.
//
// A streaming read that fills a non-owning view reports success and hands back bytes a later
// refill has overwritten. That cannot be detected at runtime -- the pointer stays inside the
// buffer's allocation -- so the readers that alias the input refuse at compile time instead. These
// cases cover the shapes a destination-type walk would have to re-enumerate to find: a view behind
// a tuple, behind a custom setter, or nested past any fixed depth bound.

import std;

import glaze.beve.read;
import glaze.core.common;
import glaze.core.custom;
import glaze.core.istream_buffer;
import glaze.core.meta_fwd;
import glaze.json.json_stream;
import glaze.json.ndjson;
import glaze.json.read;

struct lvl5_t
{
   std::string_view v{};
};
struct lvl4_t
{
   lvl5_t a{};
};
struct lvl3_t
{
   lvl4_t a{};
};
struct lvl2_t
{
   lvl3_t a{};
};
struct lvl1_t
{
   lvl2_t a{};
};
struct deeply_nested_view_t
{
   std::vector<std::map<std::string, lvl1_t>> entries{};
};

struct custom_setter_t
{
   std::string_view sv{};
   void set(std::string_view s) { sv = s; }
   std::string_view get() const { return sv; }
};
template <>
struct glz::meta<custom_setter_t>
{
   using T = custom_setter_t;
   static constexpr auto value = object("sv", glz::custom<&T::set, &T::get>);
};

struct holds_text_view_t
{
   glz::text_view tv{};
};

// A tagged variant's discriminator is exempt from the guard: it is turned into an alternative index
// inside the reader that parsed it and never reaches the caller (issue #2823). That exemption is the
// discriminator's alone -- an alternative that really does hold a view must still be rejected.
struct view_alternative_t
{
   std::string_view v{};
};
struct plain_alternative_t
{
   int n{};
};
using tagged_view_variant_t = std::variant<view_alternative_t, plain_alternative_t>;
template <>
struct glz::meta<tagged_view_variant_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr auto ids = std::array{"view", "plain"};
};

template <class T>
void stream_into()
{
   std::istringstream in{"{}"};
   glz::basic_istream_buffer<std::istringstream, 512> buf{in};
   T value{};
   [[maybe_unused]] auto ec = glz::read_json(value, buf);
}

int main()
{
#ifdef GLZ_REJECT_CASE_DIRECT_VIEW
   stream_into<std::vector<std::string_view>>();
#endif
#ifdef GLZ_REJECT_CASE_BEHIND_TUPLE
   stream_into<std::vector<std::tuple<std::string_view, std::string_view>>>();
#endif
#ifdef GLZ_REJECT_CASE_TEXT_VIEW
   stream_into<holds_text_view_t>();
#endif
#ifdef GLZ_REJECT_CASE_TAGGED_VARIANT_VIEW
   stream_into<tagged_view_variant_t>();
#endif
#ifdef GLZ_REJECT_CASE_PAST_ANY_DEPTH
   stream_into<deeply_nested_view_t>();
#endif
#ifdef GLZ_REJECT_CASE_CUSTOM_SETTER
   stream_into<custom_setter_t>();
#endif
#ifdef GLZ_REJECT_CASE_NDJSON
   {
      std::istringstream in{"[]\n"};
      glz::basic_istream_buffer<std::istringstream, 512> buf{in};
      std::vector<std::vector<std::string_view>> rows{};
      [[maybe_unused]] auto ec = glz::read_ndjson(rows, buf);
   }
#endif
#ifdef GLZ_REJECT_CASE_STREAM_READER
   {
      std::istringstream in{"{}"};
      glz::json_stream_reader<std::vector<std::string_view>> reader(in);
      std::vector<std::string_view> v{};
      [[maybe_unused]] auto e = reader.read_next(v);
   }
#endif
#ifdef GLZ_REJECT_CASE_BEVE_CONST_SPAN
   {
      std::istringstream in{""};
      glz::basic_istream_buffer<std::istringstream, 512> buf{in};
      std::span<const int> s{};
      [[maybe_unused]] auto ec = glz::read_beve(s, buf);
   }
#endif
   return 0;
}
