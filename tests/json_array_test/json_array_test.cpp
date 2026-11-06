// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json/json_array.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "glaze/core/istream_buffer.hpp"
#include "glaze/core/ostream_buffer.hpp"
#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace
{
   struct header
   {
      int version{};
      std::string kind{};
   };

   struct record
   {
      int id{};
      std::string name{};
      bool operator==(const record&) const = default;
   };

   struct only_a
   {
      int a{};
   };

   struct nested_a
   {
      only_a in{};
   };

   struct wrapped_partial
   {
      only_a in{};
      int after{};
   };

   struct failing_value
   {};

   // Two array alternatives, so the variant is not auto-deducible and each level tries both.
   struct node;
   using two_arrays = std::variant<std::vector<node>, std::list<node>>;
   struct node
   {
      two_arrays child{};
   };

   struct alt_a
   {
      int a{};
   };

   struct alt_b
   {
      int b{};
   };

   constexpr glz::opts bounded{.null_terminated = false};

   struct strict_opts : glz::opts
   {
      bool validate_trailing_whitespace = true;
   };

   struct quoted_opts : glz::opts
   {
      bool quoted_num = true;
   };

   struct pretty_opts : glz::opts
   {
      bool prettify = true;
      uint8_t indentation_width = 1;
   };

   struct tabbed_opts : glz::opts
   {
      bool prettify = true;
      char indentation_char = '\t';
      uint8_t indentation_width = 1;
   };

   struct one_line_opts : glz::opts
   {
      bool prettify = true;
      bool new_lines_in_arrays = false;
   };

   // What a read of the whole array reports for the same bytes, under the same bounded parse.
   template <class T>
   glz::error_ctx whole_read(std::string_view json)
   {
      std::vector<T> out{};
      return glz::read<bounded>(out, json);
   }
}

namespace glz
{
   template <>
   struct to<JSON, failing_value>
   {
      template <auto Opts>
      static void op(auto&&, is_context auto&& ctx, auto&& b, auto& ix)
      {
         dump("partial output", b, ix);
         ctx.error = error_code::syntax_error;
         ctx.custom_error_message = "failing_value";
      }
   };
}

template <>
struct glz::meta<wrapped_partial>
{
   using T = wrapped_partial;
   static constexpr auto value = object("in", glz::partial_read<&T::in>, "after", &T::after);
};

// Dangling is a compile error: a temporary buffer cannot back a reader, and a window onto a stream
// cannot be treated as the whole document.
static_assert(std::constructible_from<glz::json_array_reader<>, std::string&>);
static_assert(std::constructible_from<glz::json_array_reader<>, const std::string&>);
static_assert(std::constructible_from<glz::json_array_reader<>, std::string_view>);
static_assert(not std::constructible_from<glz::json_array_reader<>, std::string>);
static_assert(not std::constructible_from<glz::json_array_reader<>, glz::basic_istream_buffer<std::ifstream>&>);
template <class Buffer>
concept array_writable = requires { typename glz::json_array_writer<Buffer>; };
static_assert(array_writable<std::string>);
static_assert(array_writable<std::vector<char>>);
static_assert(not array_writable<glz::basic_ostream_buffer<std::ofstream>>);
static_assert(not array_writable<std::array<char, 8>>);
static_assert(not array_writable<std::span<char>>);

suite use_case_tests = [] {
   "a header decides how the rest is read"_test = [] {
      const auto read_doc = [](const std::string& buffer, std::vector<record>& records, std::vector<int>& numbers) {
         glz::json_array_reader reader{buffer};
         header h{};
         expect(!reader.read(h));
         return h.kind == "records" ? reader.read_rest(records) : reader.read_rest(numbers);
      };

      std::vector<record> records{};
      std::vector<int> numbers{};
      expect(
         !read_doc(R"([{"version":2,"kind":"records"},{"id":1,"name":"a"},{"id":2,"name":"b"}])", records, numbers));
      expect(records == std::vector<record>{{1, "a"}, {2, "b"}});
      expect(numbers.empty());

      records.clear();
      expect(!read_doc(R"([{"version":2,"kind":"numbers"},1,2,3])", records, numbers));
      expect(records.empty());
      expect(numbers == std::vector<int>{1, 2, 3});
   };

   "later elements are read one at a time, and skipped"_test = [] {
      const std::string buffer = R"([1,{"not":"an int"},[3,3],"four",5])";
      glz::json_array_reader reader{buffer};
      int first{};
      expect(!reader.read(first));
      expect(first == 1);
      expect(!reader.skip());
      expect(!reader.skip());
      std::string fourth{};
      expect(!reader.read(fourth));
      expect(fourth == "four");
      int fifth{};
      const auto ec = reader.read(fifth);
      expect(!ec);
      expect(fifth == 5);
      expect(ec.count == buffer.size() - 1) << "count is the position just past the element";
      expect(reader.skip() == glz::error_code::end_reached);
   };

   "the bytes already read are not scanned again"_test = [] {
      std::string buffer = R"([11,22,33,44])";
      glz::json_array_reader reader{buffer};
      int v{};
      const auto ec = reader.read(v);
      expect(!ec);
      expect(v == 11);
      expect(!reader.read(v));
      expect(v == 22);

      // Nothing still to be read lives before count, so wrecking it cannot change the outcome.
      const auto consumed = reader.read(v).count;
      expect(v == 33);
      buffer.replace(0, consumed, consumed, '?');

      std::vector<int> rest{};
      expect(!reader.read_rest(rest));
      expect(rest == std::vector<int>{44});
   };

   "loop until the array closes"_test = [] {
      const std::string buffer = "[1,2,3]";
      glz::json_array_reader reader{buffer};
      int sum{};
      int v{};
      glz::error_ctx ec{};
      while (!(ec = reader.read(v))) {
         sum += v;
      }
      expect(ec == glz::error_code::end_reached);
      expect(ec.count == buffer.size());
      expect(sum == 6);
   };
};

suite shape_tests = [] {
   "empty arrays"_test = [] {
      for (const std::string buffer : {"[]", "[ ]", " \n[\t]\n "}) {
         glz::json_array_reader reader{buffer};
         int v{7};
         const auto ec = reader.read(v);
         expect(ec == glz::error_code::end_reached);
         expect(ec.count == buffer.find(']') + 1);
         expect(v == 7);
         expect(reader.read(v) == glz::error_code::end_reached) << "reading on stays at the end";

         std::vector<int> rest{};
         expect(!glz::json_array_reader{buffer}.read_rest(rest));
         expect(rest.empty());
      }
   };

   "whitespace around and between elements"_test = [] {
      const std::string buffer = " [ 1 ,\n\t2 ] \n ";
      glz::json_array_reader reader{buffer};
      int v{};
      expect(!reader.read(v));
      expect(v == 1);
      expect(!reader.read(v));
      expect(v == 2);
      const auto ec = reader.read(v);
      expect(ec == glz::error_code::end_reached);
      expect(ec.count == buffer.find(']') + 1) << "trailing bytes are left alone, as glz::read leaves them";
   };

   "what follows the array is only looked at when asked for"_test = [] {
      const std::string garbage = "[1] x";
      {
         glz::json_array_reader reader{garbage};
         std::vector<int> rest{};
         const auto ec = reader.read_rest(rest);
         expect(!ec);
         expect(ec.count == 3u);
      }

      static constexpr strict_opts strict{};
      {
         glz::json_array_reader<strict> reader{garbage};
         std::vector<int> rest{};
         const auto ec = reader.read_rest(rest);
         expect(ec == glz::error_code::syntax_error);
         expect(ec.count == 4u);
         expect(rest == std::vector<int>{1});
         expect(reader.read_rest(rest) == glz::error_code::syntax_error);
      }
      {
         const std::string clean = "[1] \n ";
         glz::json_array_reader<strict> reader{clean};
         std::vector<int> rest{};
         expect(!reader.read_rest(rest));
         int v{};
         expect(reader.read(v) == glz::error_code::end_reached);
      }
      {
         const std::string clean = "[1]";
         glz::json_array_reader<strict> reader{clean};
         std::vector<int> rest{};
         expect(!reader.read_rest(rest));
      }
   };

   "non-array input"_test = [] {
      for (const std::string buffer : {R"({"a":1})", "5", R"("str")", "null", "true", "]", ","}) {
         glz::json_array_reader reader{buffer};
         int v{};
         const auto ec = reader.read(v);
         expect(ec == glz::error_code::expected_bracket);
         expect(ec.count == 0u);
         expect(reader.read(v) == glz::error_code::expected_bracket) << "errors are final";
         expect(reader.skip() == glz::error_code::expected_bracket);
         std::vector<int> rest{};
         expect(reader.read_rest(rest) == glz::error_code::expected_bracket);
      }
   };

   "no input"_test = [] {
      int v{};
      {
         const std::string empty{};
         glz::json_array_reader reader{empty};
         expect(reader.read(v) == glz::error_code::no_read_input);
      }
      {
         glz::json_array_reader reader{std::string_view{}};
         expect(reader.read(v) == glz::error_code::no_read_input);
      }
      {
         const std::string blank = "  \n";
         glz::json_array_reader reader{blank};
         const auto ec = reader.read(v);
         expect(ec == glz::error_code::no_read_input);
         expect(ec.count == 3u);
      }
   };

   "a malformed separator fails on the call that reaches it"_test = [] {
      for (const std::string buffer : {"[1 2]", "[1;2]", "[1:2]", "[1}", "[1 "}) {
         glz::json_array_reader reader{buffer};
         int v{};
         expect(!reader.read(v)) << "the element before it is intact";
         expect(v == 1);
         const auto ec = reader.read(v);
         expect(ec.ec != glz::error_code::none);
         expect(ec.ec != glz::error_code::end_reached);
         expect(ec.ec == whole_read<int>(buffer).ec);
         expect(ec.count == whole_read<int>(buffer).count);
         expect(reader.read(v) == ec.ec) << "errors are final";
         expect(reader.read(v).count == ec.count);
      }
   };

   "a missing element is rejected whatever the element type"_test = [] {
      const auto check = []<class T>(const std::string_view buffer, const size_t good_elements, const size_t count) {
         glz::json_array_reader reader{buffer};
         T v{};
         for (size_t i = 0; i < good_elements; ++i) {
            expect(!reader.read(v)) << buffer;
         }
         const auto ec = reader.read(v);
         expect(ec == glz::error_code::syntax_error) << buffer;
         expect(ec.count == count) << buffer;
         expect(reader.read(v) == glz::error_code::syntax_error) << "errors are final";
      };
      const auto check_all = [&]<class T> {
         check.template operator()<T>("[1,]", 1, 3);
         check.template operator()<T>("[1 , ]", 1, 5);
         check.template operator()<T>("[,1]", 0, 1);
         check.template operator()<T>("[,]", 0, 1);
         check.template operator()<T>("[1,,2]", 1, 3);
      };
      check_all.template operator()<int>();
      check_all.template operator()<glz::skip>();
      check_all.template operator()<glz::raw_json>();
      check_all.template operator()<glz::raw_json_view>();
      check_all.template operator()<glz::generic>();
   };

   "reading past the end"_test = [] {
      const std::string buffer = "[1]";
      glz::json_array_reader reader{buffer};
      int v{};
      expect(!reader.read(v));
      for (int i = 0; i < 3; ++i) {
         const auto ec = reader.read(v);
         expect(ec == glz::error_code::end_reached);
         expect(ec.count == 3u);
      }
      expect(v == 1);
      expect(reader.skip() == glz::error_code::end_reached);
      std::vector<int> rest{};
      expect(!reader.read_rest(rest)) << "nothing is left, and the array closed";
      expect(rest.empty());
   };

   "a failing element stops the reader and reports its own byte"_test = [] {
      const std::string buffer = R"(["a",   "b", 3, "d"])";
      glz::json_array_reader reader{buffer};
      std::vector<std::string> out{};
      const auto ec = reader.read_rest(out);
      expect(ec == glz::error_code::expected_quote);
      expect(ec.count == buffer.find('3'));
      expect(out == std::vector<std::string>{"a", "b"}) << "the failed element is not kept";
      expect(reader.read_rest(out) == ec.ec);
      expect(out.size() == 2u);
   };
};

suite parity_tests = [] {
   "errors match a whole read, code and position"_test = [] {
      for (const std::string_view buffer :
           {"[1,2,3]",    "[]",         " [ 1 , 2 ] ", "[1 2]",   "[1,2",  "[1,",     "[",
            "[ ",         R"({"a":1})", "5",           "",        "  ",    "[1,x,3]", "[1;2]",
            "[1]garbage", "[1,2,3",     "[1,2,3 ",     "[1,2,3,", "[[1]]", "[-]",     "[1e]"}) {
         glz::json_array_reader reader{buffer};
         std::vector<int> out{};
         const auto ec = reader.read_rest(out);
         const auto whole = whole_read<int>(buffer);
         expect(ec.ec == whole.ec) << buffer;
         expect(ec.count == whole.count) << buffer;
      }
   };

   "every truncation of a document matches a whole read"_test = [] {
      const std::string_view doc = R"( [ {"id":1,"name":"a\"b"} , {"id":22,"name":"c"}, {"id":-3,"name":""} ] )";
      const auto last = doc.rfind(']');
      for (size_t n = 0; n <= doc.size(); ++n) {
         const auto buffer = doc.substr(0, n);
         glz::json_array_reader reader{buffer};
         std::vector<record> out{};
         const auto ec = reader.read_rest(out);
         const auto whole = whole_read<record>(buffer);
         expect(ec.ec == whole.ec) << buffer;
         expect(ec.count == whole.count) << buffer;
         expect(n > last ? !ec : bool(ec)) << buffer;
      }
   };

   "values match a whole read"_test = [] {
      const std::string buffer = R"([{"id":1,"name":"a"},{"id":2,"name":"b\né"}])";
      std::vector<record> whole{};
      expect(!glz::read_json(whole, buffer));
      std::vector<record> incremental{};
      expect(!glz::json_array_reader{buffer}.read_rest(incremental));
      expect(incremental == whole);
   };

   "format_error points at the same place as a whole read"_test = [] {
      const std::string buffer = "[\n  1,\n  2 3,\n  4\n]";
      glz::json_array_reader reader{buffer};
      int v{};
      expect(!reader.read(v));
      expect(!reader.read(v));
      const auto ec = reader.read(v);
      expect(bool(ec));
      expect(ec.count == buffer.find('3'));

      std::vector<int> out{};
      const auto whole = glz::read_json(out, buffer);
      const auto message = glz::format_error(ec, buffer);
      expect(message == glz::format_error(whole, buffer)) << message;
      expect(message.find("3:5") != std::string::npos) << message;
   };

   "format_error at the very end of the buffer"_test = [] {
      const std::string buffer = "[1,\n2";
      glz::json_array_reader reader{buffer};
      std::vector<int> out{};
      const auto ec = reader.read_rest(out);
      expect(ec == glz::error_code::unexpected_end);
      expect(ec.count == buffer.size());
      expect(glz::format_error(ec, buffer) == glz::format_error(whole_read<int>(buffer), buffer));
   };
};

suite bounds_tests = [] {
   // Each view stops short of bytes that would change the answer if they were looked at.
   "nothing past the end of a view is read"_test = [] {
      const auto expect_truncated = [](const std::string_view backing, const size_t size) {
         const auto view = backing.substr(0, size);
         glz::json_array_reader reader{view};
         glz::generic out{};
         const auto ec = reader.read(out);
         const auto whole = whole_read<glz::generic>(view);
         expect(bool(ec) && ec.ec != glz::error_code::end_reached) << view;
         expect(ec.ec == whole.ec) << view;
         expect(ec.count == whole.count) << view;
         expect(ec.count <= size) << view;
      };
      expect_truncated("[23]", 2); // the number is cut short, not completed from what follows
      expect_truncated(R"(["abc"])", 3);
      expect_truncated("[true]", 3);
      expect_truncated("[true]", 5);
      expect_truncated(R"([{"a":1}])", 7);
      expect_truncated("[[1,2]]", 3);
   };

   "a number that ends with the buffer is not handed back as complete"_test = [] {
      const std::string_view backing = "[1,234]";
      const auto view = backing.substr(0, 4); // "[1,2"
      glz::json_array_reader reader{view};
      int v{};
      expect(!reader.read(v));
      expect(v == 1);
      const auto ec = reader.read(v);
      expect(ec == glz::error_code::unexpected_end);
      expect(ec.count == view.size());
   };

   "a separator and a closing bracket past the end are not seen"_test = [] {
      const std::string_view backing = "[1 ,2 ]";
      {
         const auto view = backing.substr(0, 3); // "[1 "
         glz::json_array_reader reader{view};
         int v{};
         expect(!reader.read(v)) << "the space shows the number ended";
         expect(reader.read(v) == glz::error_code::unexpected_end);
      }
      {
         const auto view = backing.substr(0, 6); // "[1 ,2 "
         glz::json_array_reader reader{view};
         std::vector<int> out{};
         expect(reader.read_rest(out) == glz::error_code::unexpected_end);
         expect(out == std::vector<int>{1, 2});
      }
   };

   "exactly sized allocations, with no terminator or padding"_test = [] {
      const std::string_view doc = R"([{"id":1,"name":"abc"},2,"three",[4],null,true,false,-1.5e3])";
      for (size_t n = 0; n <= doc.size(); ++n) {
         const auto bytes = std::make_unique_for_overwrite<char[]>(n);
         std::copy_n(doc.data(), n, bytes.get());
         const std::string_view view{bytes.get(), n};

         glz::json_array_reader reader{view};
         std::vector<glz::generic> out{};
         const auto ec = reader.read_rest(out);
         expect((n == doc.size()) == !ec) << view;
         expect(ec.count <= n) << view;

         glz::json_array_reader skipping{view};
         glz::error_ctx skipped{};
         while (!(skipped = skipping.skip())) {
         }
         expect(skipped.count <= n) << view;
         expect((n == doc.size()) == (skipped == glz::error_code::end_reached)) << view;
      }
   };

   "other contiguous buffers"_test = [] {
      const std::string_view doc = "[1,2]";
      {
         const std::vector<char> buffer(doc.begin(), doc.end());
         glz::json_array_reader reader{buffer};
         std::vector<int> out{};
         expect(!reader.read_rest(out));
         expect(out == std::vector<int>{1, 2});
      }
      {
         const std::vector<uint8_t> buffer(doc.begin(), doc.end());
         glz::json_array_reader reader{buffer};
         std::vector<int> out{};
         expect(!reader.read_rest(out));
         expect(out == std::vector<int>{1, 2});
      }
      {
         glz::json_array_reader reader{std::span<const char>{doc.data(), doc.size()}};
         std::vector<int> out{};
         expect(!reader.read_rest(out));
         expect(out == std::vector<int>{1, 2});
      }
   };
};

suite element_tests = [] {
   "options reach the element"_test = [] {
      {
         const std::string buffer = R"([{"id":1,"name":"a","extra":0}])";
         record r{};
         const auto strict = glz::json_array_reader{buffer}.read(r);
         expect(strict == glz::error_code::unknown_key);
         std::vector<record> whole{};
         expect(strict.count == glz::read_json(whole, buffer).count) << glz::format_error(strict, buffer);

         static constexpr glz::opts lenient{.error_on_unknown_keys = false};
         glz::json_array_reader<lenient> reader{buffer};
         expect(!reader.read(r));
         expect(r.name == "a");
         expect(reader.read(r) == glz::error_code::end_reached);
      }
      {
         const std::string buffer = R"(["1",2])";
         static constexpr quoted_opts quoted{};
         glz::json_array_reader<quoted> reader{buffer};
         int v{};
         expect(!reader.read(v));
         expect(v == 1);
         expect(bool(reader.read(v))) << "an unquoted number is an error under quoted_num";
      }
   };

   "comments are whitespace between elements too when enabled"_test = [] {
      const std::string buffer = "/*a*/[/*b*/1/*c*/,//d\n2/*e*/]/*f*/";
      static constexpr glz::opts jsonc{.comments = true};
      glz::json_array_reader<jsonc> reader{buffer};
      std::vector<int> out{};
      expect(!reader.read_rest(out));
      expect(out == std::vector<int>{1, 2});

      glz::json_array_reader plain{buffer};
      expect(bool(plain.read_rest(out)));

      const std::string unterminated = "[1/*";
      glz::json_array_reader<jsonc> broken{unterminated};
      int v{};
      const auto first = broken.read(v);
      const auto second = broken.read(v);
      expect(bool(first) || bool(second)) << "an unterminated comment is not a clean close";
      expect(second.ec != glz::error_code::end_reached);
   };

   "minified input"_test = [] {
      static constexpr glz::opts minified{.minified = true};
      const std::string buffer = R"([{"id":1,"name":"a"},{"id":2,"name":"b"}])";
      std::vector<record> out{};
      expect(!glz::json_array_reader<minified>{buffer}.read_rest(out));
      expect(out.size() == 2u);

      for (const std::string_view truncated : {"", "[", "[1", "[1,"}) {
         glz::json_array_reader<minified> reader{truncated};
         std::vector<int> ints{};
         expect(bool(reader.read_rest(ints))) << truncated;
      }
   };

   "element types with their own opinions"_test = [] {
      const std::string buffer = R"(["view",{"raw":[1,2]},{"a":1},[1,"x"],null,3,{"k":true},[[1],[2,2]],2.5,"var"])";
      glz::json_array_reader reader{buffer};

      std::string_view view{};
      expect(!reader.read(view));
      expect(view == "view");
      expect(view.data() >= buffer.data() && view.data() < buffer.data() + buffer.size()) << "points into the buffer";

      glz::raw_json raw{};
      expect(!reader.read(raw));
      expect(raw.str == R"({"raw":[1,2]})");

      glz::generic any{};
      expect(!reader.read(any));
      expect(any.is_object());

      std::tuple<int, std::string> tup{};
      expect(!reader.read(tup));
      expect(std::get<1>(tup) == "x");

      std::optional<int> absent{5};
      expect(!reader.read(absent));
      expect(!absent.has_value());

      std::optional<int> present{};
      expect(!reader.read(present));
      expect(present == 3);

      std::map<std::string, bool> map{};
      expect(!reader.read(map));
      expect(map.at("k"));

      std::vector<std::vector<int>> nested{};
      expect(!reader.read(nested));
      expect(nested.size() == 2u);
      expect(nested[1].size() == 2u);

      std::variant<std::string, double> number{};
      expect(!reader.read(number));
      expect(std::get<double>(number) == 2.5);

      std::variant<std::string, double> text{};
      expect(!reader.read(text));
      expect(std::get<std::string>(text) == "var");

      expect(reader.skip() == glz::error_code::end_reached);
   };

   "partial read of an element resumes at the next one"_test = [] {
      static constexpr glz::opts partial{.error_on_unknown_keys = false, .partial_read = true};
      const std::string buffer = R"([{"a":1,"b":[2,2],"c":"]"} , {"z":0,"a":5,"b":6} , 9])";
      glz::json_array_reader<partial> reader{buffer};

      only_a first{};
      expect(!reader.read(first));
      expect(first.a == 1);

      only_a second{};
      expect(!reader.read(second));
      expect(second.a == 5);

      int last{};
      expect(!reader.read(last));
      expect(last == 9);
      expect(reader.read(last) == glz::error_code::end_reached);
   };

   "a partial read of a truncated element still reports the truncation"_test = [] {
      static constexpr glz::opts partial{.error_on_unknown_keys = false, .partial_read = true};
      const std::string buffer = R"([{"a":1,"b":2)";
      glz::json_array_reader<partial> reader{buffer};
      only_a first{};
      const auto ec = reader.read(first);
      expect(ec == glz::error_code::unexpected_end);
      expect(ec.count <= buffer.size());
   };

   "the partial_read wrapper resumes at the next element without the option"_test = [] {
      const std::string buffer = R"([{"in":{"a":1,"b":2},"after":3},9])";
      glz::json_array_reader reader{buffer};
      wrapped_partial first{};
      expect(!reader.read(first));
      expect(first.in.a == 1);
      int last{};
      expect(!reader.read(last));
      expect(last == 9);
      expect(reader.read(last) == glz::error_code::end_reached);
   };

   "partial read of containers that stop without saying so"_test = [] {
      static constexpr glz::opts partial{.error_on_unknown_keys = false, .partial_read = true};
      const std::string buffer = R"([[1,2,3],[4,5,6],[7,8,9],[10,11],{"a":1,"b":2},12])";
      glz::json_array_reader<partial> reader{buffer};

      std::tuple<int> tuple{};
      expect(!reader.read(tuple));
      expect(std::get<0>(tuple) == 1);

      std::vector<int> sized(2);
      expect(!reader.read(sized));
      expect(sized == std::vector<int>{4, 5});

      std::vector<int> empty{};
      expect(!reader.read(empty));
      expect(empty.empty());

      std::array<int, 1> array{};
      expect(!reader.read(array));
      expect(array[0] == 10);

      std::map<std::string, int> map{};
      expect(!reader.read(map));
      expect(map.empty());

      int last{};
      expect(!reader.read(last));
      expect(last == 12);
      expect(reader.read(last) == glz::error_code::end_reached);
   };

   "depth does not accumulate across elements"_test = [] {
      // More elements than the recursion limit, each left a level down by a partial read.
      static constexpr glz::opts partial{.error_on_unknown_keys = false, .partial_read = true};
      const std::string_view element = R"({"in":{"a":1,"b":2},"c":3})";
      std::string buffer = "[";
      for (size_t i = 0; i < 2 * glz::max_recursive_depth_limit; ++i) {
         buffer += element;
         buffer += ',';
      }
      buffer += element;
      buffer += ']';

      glz::json_array_reader<partial> reader{buffer};
      nested_a v{};
      size_t count{};
      glz::error_ctx ec{};
      while (!(ec = reader.read(v))) {
         ++count;
      }
      expect(ec == glz::error_code::end_reached) << glz::format_error(ec, buffer);
      expect(count == 2 * glz::max_recursive_depth_limit + 1);
   };

   "the recursion limit still applies inside an element"_test = [] {
      std::string buffer(2 * glz::max_recursive_depth_limit, '[');
      glz::json_array_reader reader{buffer};
      glz::generic any{};
      expect(reader.read(any) == glz::error_code::exceeded_max_recursive_depth);
   };

   "an ambiguous nest inside an element cannot multiply the work of resolving it"_test = [] {
      // Unseeded, the speculation budget means no limit, and this runs for longer than anyone waits.
      const auto elapsed_ms = [](const size_t levels) {
         std::string buffer = "[";
         for (size_t i = 0; i < levels; ++i) buffer += R"([{"child":)";
         buffer += "[{]"; // malformed leaf
         for (size_t i = 0; i < levels; ++i) buffer += "}]";
         buffer += "]";

         const auto start = std::chrono::steady_clock::now();
         two_arrays out{};
         glz::json_array_reader reader{buffer};
         expect(reader.read(out) == glz::error_code::expected_quote);
         return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      };
      const auto shallow = elapsed_ms(24);
      const auto deep = elapsed_ms(96);
      expect(deep < 4.0 * shallow + 50.0) << shallow << " ms then " << deep << " ms";
   };

   "variant alternatives rejected in one element are not charged to the next"_test = [] {
      // Each element tries alt_a first and fails inside its object. That work is a small share of
      // the document, so it never comes near the budget, and the rejected level never counts.
      std::string buffer = "[";
      for (size_t i = 0; i < 2 * glz::max_recursive_depth_limit; ++i) {
         buffer += i ? R"(,{"b":1})" : R"({"b":1})";
      }
      buffer += "]";

      static constexpr glz::opts lenient{.error_on_unknown_keys = false};
      std::vector<std::variant<alt_a, alt_b>> out{};
      glz::json_array_reader<lenient> reader{buffer};
      const auto ec = reader.read_rest(out);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(out.size() == 2 * glz::max_recursive_depth_limit);
   };

   "a reader that takes the whole buffer leaves nothing to close"_test = [] {
      const std::string buffer = R"(["a"])";
      glz::json_array_reader reader{buffer};
      glz::text text{};
      expect(reader.read(text) == glz::error_code::unexpected_end);
   };

   "an array inside a document, found with lazy_json"_test = [] {
      const std::string buffer = R"({"skip":[0],"items":[{"version":1,"kind":"k"},1,2 3],"after":true})";
      const auto doc = glz::lazy_json(buffer);
      expect(doc.has_value());
      const auto items = (*doc)["items"].raw_json();

      glz::json_array_reader reader{items};
      header h{};
      expect(!reader.read(h));
      expect(h.kind == "k");
      std::vector<int> rest{};
      const auto ec = reader.read_rest(rest);
      expect(ec == glz::error_code::expected_bracket);
      expect(rest == std::vector<int>{1, 2});
      expect(items.data() + ec.count == buffer.data() + buffer.find('3')) << "count is relative to the view given";
   };

   "other containers for the rest"_test = [] {
      const std::string buffer = "[0,1,2]";
      {
         std::deque<int> out{};
         glz::json_array_reader reader{buffer};
         expect(!reader.skip());
         expect(!reader.read_rest(out));
         expect(out == std::deque<int>{1, 2});
      }
      {
         std::list<int> out{7};
         glz::json_array_reader reader{buffer};
         expect(!reader.read_rest(out));
         expect(out == std::list<int>{7, 0, 1, 2}) << "the rest is appended";
      }
      {
         const std::string flags = "[true,false]";
         std::vector<bool> out{};
         expect(!glz::json_array_reader{flags}.read_rest(out));
         expect(out == std::vector<bool>{true, false});
      }
   };
};

suite writer_tests = [] {
   "the buffer holds a complete array at every step"_test = [] {
      std::string buffer{};
      glz::json_array_writer writer{buffer};
      expect(buffer == "[]") << "even with nothing appended";

      expect(!writer.append(1));
      expect(buffer == "[1]");

      const auto ec = writer.append(2);
      expect(!ec);
      expect(buffer == "[1,2]");
      expect(ec.count == buffer.size());

      expect(!writer.append("three"));
      expect(buffer == R"([1,2,"three"])");

      glz::generic parsed{};
      expect(!glz::read_json(parsed, buffer));
      expect(parsed.is_array());
   };

   "a writer that goes away leaves the array closed"_test = [] {
      std::string buffer{};
      {
         glz::json_array_writer writer{buffer};
         expect(!writer.append(1));
      }
      expect(buffer == "[1]");
      {
         std::string never_used{};
         glz::json_array_writer writer{never_used};
         (void)writer;
         expect(never_used == "[]");
      }
   };

   "what the buffer already holds is kept"_test = [] {
      std::string buffer = "data: ";
      glz::json_array_writer writer{buffer};
      expect(buffer == "data: []");
      expect(!writer.append(1));
      expect(!writer.append(2));
      expect(buffer == "data: [1,2]");
   };

   "the bytes match glz::write of a container"_test = [] {
      const auto check = []<auto Opts, class T>(const std::vector<T>& values) {
         std::string buffer{};
         glz::json_array_writer<std::string, Opts> writer{buffer};
         for (const auto& v : values) {
            expect(!writer.append(v));
         }
         std::string expected{};
         expect(!glz::write<Opts>(values, expected));
         expect(buffer == expected) << buffer;
      };
      const auto check_all = [&]<auto Opts> {
         check.template operator()<Opts>(std::vector<int>{});
         check.template operator()<Opts>(std::vector<int>{1});
         check.template operator()<Opts>(std::vector<int>{1, 2, 3});
         check.template operator()<Opts>(std::vector<std::string>{"a", "b\n"});
         check.template operator()<Opts>(std::vector<record>{{1, "a"}, {2, "b"}});
         check.template operator()<Opts>(std::vector<std::vector<int>>{{1, 2}, {}, {3}});
         check.template operator()<Opts>(std::vector<std::map<std::string, int>>{{{"k", 1}}, {}});
         check.template operator()<Opts>(std::vector<std::optional<int>>{std::nullopt, 1});
      };
      check_all.template operator()<glz::opts{}>();
      check_all.template operator()<glz::opts{.prettify = true}>();
      check_all.template operator()<pretty_opts{}>();
      check_all.template operator()<tabbed_opts{}>();
      check_all.template operator()<one_line_opts{}>();
   };

   "other resizable buffers"_test = [] {
      std::vector<char> buffer{};
      glz::json_array_writer writer{buffer};
      expect(!writer.append(1));
      expect(!writer.append(std::string(5000, 'x')));
      expect(!writer.append(2));
      const std::string_view text{buffer.data(), buffer.size()};
      expect(text == "[1,\"" + std::string(5000, 'x') + "\",2]");
   };

   "values larger than the padding, and buffers that reallocate"_test = [] {
      std::string buffer{};
      glz::json_array_writer writer{buffer};
      std::vector<std::string> expected{};
      for (size_t i = 0; i < 64; ++i) {
         expected.emplace_back(i * 100, 'a');
         expect(!writer.append(expected.back()));
      }
      std::vector<std::string> back{};
      expect(!glz::read_json(back, buffer));
      expect(back == expected);
   };

   "a failed append changes nothing"_test = [] {
      const auto check = []<auto Opts>(const std::string_view empty, const std::string_view one,
                                       const std::string_view two) {
         std::string buffer{};
         glz::json_array_writer<std::string, Opts> writer{buffer};

         const auto first = writer.append(failing_value{});
         expect(first == glz::error_code::syntax_error);
         expect(first.custom_error_message == "failing_value");
         expect(first.count == buffer.size());
         expect(buffer == empty) << buffer;

         expect(!writer.append(1));
         expect(buffer == one) << buffer;

         expect(writer.append(failing_value{}) == glz::error_code::syntax_error);
         expect(buffer == one) << buffer;

         expect(!writer.append(2)) << "a failure is not final, since the buffer is intact";
         expect(buffer == two) << buffer;
      };
      check.template operator()<glz::opts{}>("[]", "[1]", "[1,2]");
      check.template operator()<glz::opts{.prettify = true}>("[]", "[\n   1\n]", "[\n   1,\n   2\n]");
      check.template operator()<one_line_opts{}>("[]", "[1]", "[1, 2]");
   };

   "the failure is reported in terms of a whole buffer"_test = [] {
      std::string buffer = "prefix ";
      glz::json_array_writer writer{buffer};
      expect(!writer.append(1));
      const auto ec = writer.append(failing_value{});
      expect(ec.count == buffer.size());
      expect(!glz::format_error(ec, buffer).empty());
   };

   "reader and writer round trip"_test = [] {
      std::string buffer{};
      glz::json_array_writer writer{buffer};
      expect(!writer.append(header{2, "records"}));
      expect(!writer.append(record{1, "a"}));
      expect(!writer.append(record{2, "b"}));

      glz::json_array_reader reader{buffer};
      header h{};
      expect(!reader.read(h));
      expect(h.version == 2);
      std::vector<record> records{};
      expect(!reader.read_rest(records));
      expect(records == std::vector<record>{{1, "a"}, {2, "b"}});
   };
};

int main() { return 0; }
