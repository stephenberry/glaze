// Glaze Library
// For the license information refer to glaze.hpp

// meta<T>::skip must mean the same thing in every format that writes keyed objects: a key skipped on
// serialize is absent from the output, a key skipped on parse is consumed without touching the member,
// and a skipped member's reader and writer are never instantiated, so skipping is also how a member
// with no representation in a format is kept out of it.

#include <string>
#include <string_view>
#include <vector>

#include <ut/ut.hpp>

#include "glaze/beve.hpp"
#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/json.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"
#include "glaze/toml.hpp"
#include "glaze/yaml.hpp"

using namespace ut;

// "secret" is skipped on serialize only, and sits between two written members
struct write_skipped
{
   int a = 1;
   int secret = 2;
   int b = 3;
};

template <>
struct glz::meta<write_skipped>
{
   static constexpr bool skip(const std::string_view key, const meta_context& ctx)
   {
      return ctx.op == operation::serialize && key == "secret";
   }
};

// The same keys with no skip, to read back what write_skipped wrote
struct write_skipped_mirror
{
   int a = -1;
   int secret = -1;
   int b = -1;
};

struct point
{
   int x{};
   int y{};
};

// Every "ignored" key is skipped on parse only. Object and array members are included because TOML
// writes them as [table] and [[array]] sections whose bodies follow the header, and a member after
// them checks that reading resumes in the right place.
struct read_skipped
{
   int a{};
   int ignored{};
   point ignored_table{};
   std::vector<point> ignored_tables{};
   point kept{};
};

template <>
struct glz::meta<read_skipped>
{
   static constexpr bool skip(const std::string_view key, const meta_context& ctx)
   {
      return ctx.op == operation::parse && key.starts_with("ignored");
   }
};

// The same keys with no skip, to produce input that carries the ignored keys
struct read_skipped_source
{
   int a = 5;
   int ignored = 7;
   point ignored_table{1, 2};
   std::vector<point> ignored_tables{{3, 4}, {5, 6}};
   point kept{8, 9};
};

// Has no reader or writer in any format
struct unserializable
{
   static constexpr bool glaze_reflect = false;
   int value{};
};

// "opaque" is skipped both ways, so this struct is readable and writable in every format even though
// its member is not. That only compiles if the skipped member's reader and writer are never
// instantiated.
struct holds_unserializable
{
   int a = 1;
   unserializable opaque{};
   int b = 2;
};

template <>
struct glz::meta<holds_unserializable>
{
   static constexpr bool skip(const std::string_view key, const meta_context&) { return key == "opaque"; }
};

template <uint32_t Format>
void check_meta_skip(const std::string_view format_name)
{
   static constexpr glz::opts Opts{.format = Format};

   {
      std::string buffer{};
      expect(not glz::write<Opts>(write_skipped{}, buffer)) << format_name;
      write_skipped_mirror mirror{};
      expect(not glz::read<Opts>(mirror, buffer)) << format_name;
      expect(mirror.a == 1) << format_name;
      expect(mirror.secret == -1) << format_name << ": a key skipped on serialize was written";
      expect(mirror.b == 3) << format_name;
   }

   {
      std::string buffer{};
      expect(not glz::write<Opts>(read_skipped_source{}, buffer)) << format_name;
      read_skipped value{};
      // error_on_unknown_keys is on by default: a skipped key is known, just not read
      expect(not glz::read<Opts>(value, buffer)) << format_name;
      expect(value.a == 5) << format_name;
      expect(value.ignored == 0) << format_name << ": a key skipped on parse was read";
      expect(value.ignored_table.x == 0) << format_name;
      expect(value.ignored_tables.empty()) << format_name;
      expect(value.kept.x == 8 && value.kept.y == 9) << format_name;
   }

   {
      std::string buffer{};
      expect(not glz::write<Opts>(holds_unserializable{.a = 3, .opaque = {}, .b = 4}, buffer)) << format_name;
      holds_unserializable value{};
      expect(not glz::read<Opts>(value, buffer)) << format_name;
      expect(value.a == 3) << format_name;
      expect(value.b == 4) << format_name;
   }
}

suite meta_skip_in_every_format = [] {
   "json"_test = [] { check_meta_skip<glz::JSON>("json"); };
   "beve"_test = [] { check_meta_skip<glz::BEVE>("beve"); };
   "cbor"_test = [] { check_meta_skip<glz::CBOR>("cbor"); };
   "msgpack"_test = [] { check_meta_skip<glz::MSGPACK>("msgpack"); };
   "toml"_test = [] { check_meta_skip<glz::TOML>("toml"); };
   "yaml"_test = [] { check_meta_skip<glz::YAML>("yaml"); };
   "jsonb"_test = [] { check_meta_skip<glz::JSONB>("jsonb"); };
   "bson"_test = [] { check_meta_skip<glz::BSON>("bson"); };
};

suite meta_skip_beve = [] {
   // beve_size counts members separately from the writer, so it has to leave out the same keys
   "beve_size leaves out a key skipped on serialize"_test = [] {
      std::string buffer{};
      expect(not glz::write_beve(write_skipped{}, buffer));
      expect(glz::beve_size(write_skipped{}) == buffer.size());
   };

   // A positional layout has no keys for meta::skip to name, and a reader locates members by position,
   // so dropping one on write would shift every member after it.
   "structs_as_arrays keeps a key skipped on serialize"_test = [] {
      std::string buffer{};
      expect(not glz::write_beve_untagged(write_skipped{.a = 4, .secret = 5, .b = 6}, buffer));
      expect(glz::beve_size_untagged(write_skipped{}) == buffer.size());
      write_skipped value{0, 0, 0};
      expect(not glz::read_beve_untagged(value, buffer));
      expect(value.a == 4);
      expect(value.secret == 5);
      expect(value.b == 6);
   };
};

int main() { return 0; }
