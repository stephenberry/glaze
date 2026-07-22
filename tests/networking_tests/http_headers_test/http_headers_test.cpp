#include "glaze/net/http_headers.hpp"

#include <ranges>
#include <string>
#include <string_view>
#include <ut/ut.hpp>
#include <vector>

using namespace ut;
using namespace std::string_view_literals;

std::vector<std::string> values_of(const glz::http_headers& fields, std::string_view name)
{
   std::vector<std::string> values{};
   for (std::string_view value : fields.values(name)) {
      values.emplace_back(value);
   }
   return values;
}

bool contains_value(const glz::http_headers& fields, std::string_view name, std::string_view expected_value)
{
   auto values = fields.values(name);
   return std::ranges::find(values, expected_value) != values.end();
}

// The checks are templated because 'requires' reports invalid
// expressions only during template substitution
template <class T = glz::http_headers>
consteval bool rvalue_overloads_are_deleted()
{
   static_assert(not requires(T fields) { std::move(fields).begin(); });
   static_assert(not requires(T fields) { std::move(fields).end(); });
   static_assert(not requires(T fields) { std::move(fields).cbegin(); });
   static_assert(not requires(T fields) { std::move(fields).cend(); });
   static_assert(not requires(T fields) { std::move(fields).front(); });
   static_assert(not requires(T fields) { std::move(fields).back(); });
   static_assert(not requires(T fields) { std::move(fields).find("A"); });
   static_assert(not requires(T fields) { std::move(fields).fields("A"); });
   static_assert(not requires(T fields) { std::move(fields).names(); });
   static_assert(not requires(T fields) { std::move(fields).values(); });
   static_assert(not requires(T fields) { std::move(fields).values("A"); });
   static_assert(not requires(T fields) { std::move(fields).first_value("A"); });
   static_assert(not requires(T fields) { std::move(fields).add("A", "1"); });
   static_assert(not requires(T fields) { std::move(fields).set("A", "1"); });

   static_assert(not requires(const T fields) { std::move(fields).begin(); });
   static_assert(not requires(const T fields) { std::move(fields).end(); });
   static_assert(not requires(const T fields) { std::move(fields).front(); });
   static_assert(not requires(const T fields) { std::move(fields).back(); });
   static_assert(not requires(const T fields) { std::move(fields).find("A"); });
   static_assert(not requires(const T fields) { std::move(fields).fields("A"); });

   return true;
}
static_assert(rvalue_overloads_are_deleted());

suite http_headers = [] {
   "default constructed headers are empty"_test = [] {
      glz::http_headers fields{};

      expect(fields.empty());
      expect(fields.size() == 0);
      expect(fields.begin() == fields.end());
   };

   "clear removes all fields"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"B", "2"}};

      fields.clear();

      expect(fields.empty());
      expect(fields.size() == 0U);
      expect(fields.begin() == fields.end());
      expect(fields.cbegin() == fields.cend());
   };

   "front and back return the first and last fields"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"B", "2"}, {"C", "3"}};

      expect(fields.front().name == "A");
      expect(fields.front().value == "1");
      expect(fields.back().name == "C");
      expect(fields.back().value == "3");

      const auto& const_fields = fields;
      expect(const_fields.front().name == "A");
      expect(const_fields.back().name == "C");
   };

   "initializer list preserves insertion order and duplicates"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"B", "2"}, {"C", "3"}, {"D", "4"}, {"A", "11"}};
      auto repeated_record_values = values_of(fields, "A");

      expect(fields.size() == 5U);

      expect(repeated_record_values.size() == 2U);
      if (repeated_record_values.size() == 2U) {
         expect(repeated_record_values[0] == "1");
         expect(repeated_record_values[1] == "11");
      }

      expect(fields.first_value("B") == "2");
      expect(fields.first_value("C") == "3");
      expect(fields.first_value("D") == "4");
   };

   "adding a field preserves insertion order for iteration"_test = [] {
      glz::http_headers fields{};
      fields.add("A", "1");
      fields.add("B", "2");
      fields.add("C", "3");

      std::vector<glz::http_headers::value_type> all_fields(fields.begin(), fields.end());

      expect(all_fields.size() == 3U);
      if (all_fields.size() == 3U) {
         expect(all_fields[0].name == "A");
         expect(all_fields[0].value == "1");
         expect(all_fields[1].name == "B");
         expect(all_fields[1].value == "2");
         expect(all_fields[2].name == "C");
         expect(all_fields[2].value == "3");
      }
   };

   "contains and find are case-insensitive"_test = [] {
      glz::http_headers fields{};

      fields.add("Content-Length", "123");

      expect(fields.contains("content-length"));
      expect(fields.contains("CONTENT-LENGTH"));
      expect(not fields.contains("Connection"));

      const auto it = fields.find("CONTENT-length");
      expect(it != fields.end());
      if (it != fields.end()) {
         expect(it->value == "123");
      }
   };

   "contains_token matches a single token case-insensitively"_test = [] {
      glz::http_headers fields{{"Connection", "UpGrAde"}};

      expect(fields.contains_token("Connection", "upgrade"));
      expect(fields.contains_token("CONNECTION", "UPGRADE"));
      expect(not fields.contains_token("Connection", "close"));
   };

   "contains_token finds tokens anywhere in a comma-separated list"_test = [] {
      glz::http_headers fields{
         {"Accept-Encoding", "gzip, deflate, br, zstd"},
      };

      expect(fields.contains_token("Accept-Encoding", "gzip"));
      expect(fields.contains_token("Accept-Encoding", "deflate"));
      expect(fields.contains_token("Accept-Encoding", "zstd"));
      expect(not fields.contains_token("Accept-Encoding", "identity"));
   };

   "contains_token ignores optional whitespace around tokens"_test = [] {
      glz::http_headers fields{{"Connection", "  keep-alive  ,\tupgrade\t"}};

      expect(fields.contains_token("Connection", "keep-alive"));
      expect(fields.contains_token("Connection", "upgrade"));
   };

   "contains_token does not match a substring of a token"_test = [] {
      glz::http_headers fields{
         {"Accept-Encoding", "gzipped, deflated"},
      };

      expect(not fields.contains_token("Accept-Encoding", "gzip"));
      expect(not fields.contains_token("Accept-Encoding", "deflate"));
      expect(fields.contains_token("Accept-Encoding", "gzipped"));
   };

   "contains_token returns false for an empty token"_test = [] {
      glz::http_headers fields{
         {"Connection", "keep-alive"},
      };

      expect(not fields.contains_token("Connection", ""));
   };

   "contains_token returns false when the field is missing"_test = [] {
      glz::http_headers fields{
         {"Connection", "keep-alive"},
      };

      expect(not fields.contains_token("Upgrade", "websocket"));
   };

   "contains_token searches every occurrence of a repeated field"_test = [] {
      glz::http_headers fields{};

      fields.add("Connection", "keep-alive");
      fields.add("X", "x");
      fields.add("connection", "upgrade");

      expect(fields.contains_token("Connection", "keep-alive"));
      expect(fields.contains_token("Connection", "upgrade"));
      expect(not fields.contains_token("Connection", "close"));
   };

   "contains_token skips empty list elements"_test = [] {
      glz::http_headers fields{
         {"Accept-Encoding", "gzip,,br,"},
      };

      expect(fields.contains_token("Accept-Encoding", "gzip"));
      expect(fields.contains_token("Accept-Encoding", "br"));
   };

   "contains_token returns false on an empty field value"_test = [] {
      glz::http_headers fields{
         {"Connection", ""},
      };

      expect(not fields.contains_token("Connection", "keep-alive"));
   };

   "header contains_token matches tokens in its own value"_test = [] {
      const glz::http_header field{"Accept-Encoding", "gzip, br"};

      expect(field.contains_token("gzip"));
      expect(field.contains_token("BR"));
      expect(not field.contains_token("zstd"));
      expect(not field.contains_token(""));
   };

   "erasing a field removes all matching occurrences"_test = [] {
      glz::http_headers fields{};

      fields.add("Set-Cookie", "a=1");
      fields.add("Set-Cookie", "b=2");
      fields.add("X", "x");

      auto cookies = values_of(fields, "SET-COOKIE");

      expect(fields.contains("set-cookie"));
      expect(cookies.size() == 2U);

      fields.erase("set-cookie");

      expect(not fields.contains("Set-Cookie"));
      expect(fields.contains("X"));
      expect(fields.size() == 1U);
   };

   "erasing a missing field leaves headers unchanged"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"B", "2"}};

      fields.erase("Missing");

      expect(fields.size() == 2U);
      expect(fields.first_value("A") == "1");
      expect(fields.first_value("B") == "2");
   };

   "fields view iterates matching pairs in insertion order"_test = [] {
      glz::http_headers fields{};
      fields.add("Set-Cookie", "a=1");
      fields.add("X", "x");
      fields.add("set-cookie", "b=2");
      fields.add("Y", "y");
      fields.add("SET-COOKIE", "c=3");

      auto range = fields.fields("set-cookie");

      std::vector<std::string> collected_names;
      std::vector<std::string> collected_values;

      for (auto&& [name, value] : range) {
         collected_names.emplace_back(name);
         collected_values.emplace_back(value);
      }

      expect(collected_values.size() == 3U);
      if (collected_values.size() == 3U) {
         expect(collected_values[0] == "a=1");
         expect(collected_values[1] == "b=2");
         expect(collected_values[2] == "c=3");
      }

      expect(collected_names.size() == 3U);
      if (collected_names.size() == 3U) {
         expect(collected_names[0] == "Set-Cookie");
         expect(collected_names[1] == "set-cookie");
         expect(collected_names[2] == "SET-COOKIE");
      }
   };

   "fields view on const works and is case-insensitive"_test = [] {
      glz::http_headers mutable_fields{};
      mutable_fields.add("Set-Cookie", "a=1");
      mutable_fields.add("set-cookie", "b=2");
      mutable_fields.add("X", "x");

      const auto& fields = mutable_fields;
      auto range = fields.fields("SET-COOKIE");

      std::vector<std::string> collected_values;
      for (auto&& field : range) {
         collected_values.emplace_back(field.value);
      }

      expect(collected_values.size() == 2U);
      if (collected_values.size() == 2U) {
         expect(collected_values[0] == "a=1");
         expect(collected_values[1] == "b=2");
      }
   };

   "fields view is empty when key does not exist"_test = [] {
      glz::http_headers fields{};
      fields.add("A", "1");
      fields.add("B", "2");

      auto range = fields.fields("Missing");

      expect(std::ranges::empty(range));
      expect(std::ranges::distance(range) == 0);
   };

   "names view returns every name in insertion order"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"b", "2"}, {"A", "3"}};

      std::vector<std::string> names{};
      for (std::string_view name : fields.names()) {
         names.emplace_back(name);
      }

      expect(names.size() == 3U);
      if (names.size() == 3U) {
         expect(names[0] == "A");
         expect(names[1] == "b");
         expect(names[2] == "A");
      }
   };

   "values view without a name returns every value in insertion order"_test = [] {
      glz::http_headers fields{{"A", "1"}, {"B", "2"}, {"A", "3"}};

      std::vector<std::string> values{};
      for (std::string_view value : fields.values()) {
         values.emplace_back(value);
      }

      expect(values.size() == 3U);
      if (values.size() == 3U) {
         expect(values[0] == "1");
         expect(values[1] == "2");
         expect(values[2] == "3");
      }
   };

   "values view returns only values in insertion order"_test = [] {
      glz::http_headers fields{};
      fields.add("Set-Cookie", "a=1");
      fields.add("X", "x");
      fields.add("set-cookie", "b=2");
      fields.add("SET-COOKIE", "c=3");

      auto cookies = values_of(fields, "set-cookie");

      expect(cookies.size() == 3U);
      if (cookies.size() == 3U) {
         expect(cookies[0] == "a=1");
         expect(cookies[1] == "b=2");
         expect(cookies[2] == "c=3");
      }
   };

   "values view on const returns only values"_test = [] {
      glz::http_headers mutable_fields{};
      mutable_fields.add("Set-Cookie", "a=1");
      mutable_fields.add("set-cookie", "b=2");
      mutable_fields.add("X", "x");

      const auto& fields = mutable_fields;
      auto cookies = values_of(fields, "SET-COOKIE");

      expect(cookies.size() == 2U);
      if (cookies.size() == 2U) {
         expect(cookies[0] == "a=1");
         expect(cookies[1] == "b=2");
      }
   };

   "value views on const are compatible with ranges algorithms"_test = [] {
      glz::http_headers mutable_fields{};
      mutable_fields.add("Set-Cookie", "a=1");
      mutable_fields.add("set-cookie", "b=22");
      mutable_fields.add("SET-COOKIE", "ccc");
      mutable_fields.add("X", "x");

      const auto& fields = mutable_fields;
      auto value_views = fields.values("set-cookie");

      expect(not std::ranges::empty(value_views));
      expect(std::ranges::distance(value_views) == 3);
   };

   "serializes headers separated with crlf"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
         {"A", "a"},
         {"B", "b"},
         {"C", "c"},
      };

      glz::http_headers growing_fields{};

      for (const auto& [header_name, header_value] : fields) {
         growing_fields.add(header_name, header_value);

         auto fields_str = growing_fields.serialize();

         std::string_view clean_str = fields_str;
         clean_str.remove_suffix("\r\n\r\n"sv.size());

         auto split_headers = std::views::split(clean_str, "\r\n"sv);
         auto str_headers_count = std::ranges::distance(split_headers);

         expect(growing_fields.size() == static_cast<glz::http_headers::size_type>(str_headers_count));
      }
   };

   "serializes empty headers as a lone crlf terminator"_test = [] {
      glz::http_headers fields{};
      expect(fields.serialize() == "\r\n");
   };

   "serializes multiple fields"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
         {"A", "a"},
         {"B", "b"},
         {"c", "c"},
      };

      expect(fields.serialize() == "Set-Cookie: a=1\r\nA: a\r\nB: b\r\nc: c\r\n\r\n");
   };

   "serializes repeated fields as separate lines"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
         {"set-cookie", "b=1"},
      };

      expect(fields.serialize() == "Set-Cookie: a=1\r\nset-cookie: b=1\r\n\r\n");
   };

   "serializes repeated fields in insertion order"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
         {"Set-Cookie", "b=22"},
         {"Set-Cookie", "ccc"},
      };

      expect(fields.serialize() == "Set-Cookie: a=1\r\nSet-Cookie: b=22\r\nSet-Cookie: ccc\r\n\r\n");
   };

   "serializes fields with empty names as-is"_test = [] {
      glz::http_headers fields{
         {"", "ghost"},
         {"A", "1"},
      };

      expect(fields.serialize() == ": ghost\r\nA: 1\r\n\r\n");
   };

   "serializes a field with an empty value"_test = [] {
      // Empty field values are valid under RFC 9112
      glz::http_headers fields{
         {"X-Empty", ""},
      };

      expect(fields.serialize() == "X-Empty: \r\n\r\n");
   };

   "set replaces the value of an existing field in place"_test = [] {
      glz::http_headers fields{
         {"A", "1"},
         {"Set-Cookie", "a=1"},
         {"B", "2"},
      };

      auto it = fields.set("Set-Cookie", "c=3");

      expect(fields.count("Set-Cookie") == 1U);
      expect(fields.first_value("Set-Cookie") == "c=3");
      expect(it == std::next(fields.begin()));

      std::vector<glz::http_headers::value_type> all_fields(fields.begin(), fields.end());

      expect(all_fields.size() == 3U);
      if (all_fields.size() == 3U) {
         expect(all_fields[0].name == "A");
         expect(all_fields[1].name == "Set-Cookie");
         expect(all_fields[1].value == "c=3");
         expect(all_fields[2].name == "B");
      }
   };

   "set replaces the stored name casing"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
      };

      fields.set("SET-COOKIE", "c=3");

      expect(fields.count("set-cookie") == 1U);
      expect(fields.front().name == "SET-COOKIE");
      expect(fields.serialize() == "SET-COOKIE: c=3\r\n\r\n");
   };

   "set adds the field at the end when it does not exist"_test = [] {
      glz::http_headers fields{{"A", "1"}};

      auto it = fields.set("Set-Cookie", "a=1");

      expect(fields.size() == 2U);
      expect(fields.first_value("Set-Cookie") == "a=1");
      expect(fields.back().name == "Set-Cookie");
      expect(it == std::prev(fields.end()));
   };

   "set collapses duplicates into the first occurrence"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a"},
         {"X", "x"},
         {"Set-Cookie", "b=2"},
      };

      expect(fields.count("Set-Cookie") == 2U);

      fields.set("Set-Cookie", "c=3");

      expect(fields.count("Set-Cookie") == 1U);
      expect(fields.first_value("Set-Cookie") == "c=3");

      std::vector<glz::http_headers::value_type> all_fields(fields.begin(), fields.end());

      expect(all_fields.size() == 2U);
      if (all_fields.size() == 2U) {
         expect(all_fields[0].name == "Set-Cookie");
         expect(all_fields[0].value == "c=3");
         expect(all_fields[1].name == "X");
      }
   };

   "set matches case-insensitively and stores the new spelling"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
         {"SET-COOKIE", "b=2"},
      };

      fields.set("set-cookie", "c=3");

      expect(fields.count("Set-Cookie") == 1U);
      expect(fields.size() == 1U);
      expect(fields.front().name == "set-cookie");
      expect(fields.front().value == "c=3");
   };

   "contains is case-insensitive and fields enumerates all occurrences"_test = [] {
      glz::http_headers fields{};

      fields.add("Upgrade", "websocket");
      fields.add("upgrade", "h2c");

      expect(fields.contains("UPGRADE"));
      expect(fields.count("UpGrAdE") == 2U);

      std::vector<std::string> values{};
      for (const auto& value : fields.values("UPGRADE")) {
         values.emplace_back(value);
      }

      expect(values.size() == 2U);
      if (values.size() == 2U) {
         expect(values[0] == "websocket");
         expect(values[1] == "h2c");
      }
   };

   "first_value returns the field value"_test = [] {
      glz::http_headers fields{
         {"Content-Length", "123123"},
      };

      expect(fields.first_value("Content-Length") == "123123");
   };

   "first_value returns nullopt when the field is missing"_test = [] {
      glz::http_headers fields{};

      auto content_length = fields.first_value("Content-Length");
      expect(not content_length.has_value());
   };

   "append copies and returns size of both objects"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
      };

      glz::http_headers other_fields{
         {"Hello-World", "aero"},
      };

      fields.append(other_fields);

      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "Hello-World", "aero"));
      expect(fields.size() == 2U);

      expect(contains_value(other_fields, "Hello-World", "aero"));
   };

   "append moves fields and clears source"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
      };

      glz::http_headers other_fields{
         {"Hello-World", "aero"},
      };

      fields.append(std::move(other_fields));

      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "Hello-World", "aero"));
      expect(fields.size() == 2U);

      expect(other_fields.empty());
      expect(other_fields.begin() == other_fields.end());
   };

   "append copies nothing from empty object"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
      };

      glz::http_headers other_fields{};

      auto fields_size_before_append = fields.size();
      fields.append(other_fields);

      expect(contains_value(fields, "Host", "example.com"));
      expect(fields_size_before_append == fields.size());
   };

   "append copies initialized object to empty"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
      };

      glz::http_headers other_fields{};

      auto fields_size_before_append = fields.size();
      other_fields.append(fields);

      expect(contains_value(other_fields, "Host", "example.com"));
      expect(other_fields.size() == fields_size_before_append);
   };

   "append moves initialized object to empty"_test = [] {
      glz::http_headers fields{};

      glz::http_headers other_fields{
         {"Host", "example.com"},
         {"Hello-World", "aero"},
      };

      auto other_fields_size_before_append = other_fields.size();
      fields.append(std::move(other_fields));

      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "Hello-World", "aero"));
      expect(fields.size() == other_fields_size_before_append);

      expect(other_fields.empty());
   };

   "append copies nothing when both objects empty"_test = [] {
      glz::http_headers fields{};
      glz::http_headers other_fields{};

      fields.append(other_fields);

      expect(fields.empty());
      expect(other_fields.empty());
   };

   "append preserves duplicate header values on copy"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
      };

      glz::http_headers other_fields{
         {"Set-Cookie", "b=2"},
      };

      fields.append(other_fields);

      expect(contains_value(fields, "Set-Cookie", "a=1"));
      expect(contains_value(fields, "Set-Cookie", "b=2"));
      expect(fields.size() == 2U);

      auto cookies = values_of(fields, "Set-Cookie");

      expect(cookies.size() == 2U);
      if (cookies.size() == 2U) {
         expect(cookies[0] == "a=1");
         expect(cookies[1] == "b=2");
      }

      expect(contains_value(other_fields, "Set-Cookie", "b=2"));
      expect(other_fields.size() == 1U);
   };

   "append preserves duplicate header values on move"_test = [] {
      glz::http_headers fields{
         {"Set-Cookie", "a=1"},
      };

      glz::http_headers other_fields{
         {"Set-Cookie", "b=2"},
      };

      fields.append(std::move(other_fields));

      expect(contains_value(fields, "Set-Cookie", "a=1"));
      expect(contains_value(fields, "Set-Cookie", "b=2"));
      expect(fields.size() == 2U);

      auto cookies = values_of(fields, "Set-Cookie");

      expect(cookies.size() == 2U);
      if (cookies.size() == 2U) {
         expect(cookies[0] == "a=1");
         expect(cookies[1] == "b=2");
      }

      expect(other_fields.empty());
   };

   "append finds merged values case-insensitively"_test = [] {
      glz::http_headers fields{
         {"host", "example.com"},
      };

      glz::http_headers other_fields{
         {"Host", "example.org"},
      };

      fields.append(other_fields);

      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "HOST", "example.org"));

      auto host_values = values_of(fields, "HOST");

      expect(host_values.size() == 2U);
      if (host_values.size() == 2U) {
         expect(host_values[0] == "example.com");
         expect(host_values[1] == "example.org");
      }
   };

   "append keeps first value from existing fields when appending"_test = [] {
      glz::http_headers fields{
         {"X-Test", "first"},
      };

      glz::http_headers other_fields{
         {"X-Test", "second"},
      };

      fields.append(other_fields);

      expect(fields.first_value("X-Test") == "first");
      expect(contains_value(fields, "X-Test", "first"));
      expect(contains_value(fields, "X-Test", "second"));
   };

   "append preserves insertion order when appending"_test = [] {
      glz::http_headers fields{
         {"A", "1"},
         {"B", "2"},
      };

      glz::http_headers other_fields{
         {"C", "3"},
         {"D", "4"},
      };

      fields.append(other_fields);

      std::vector<glz::http_headers::value_type> all_fields(fields.begin(), fields.end());

      expect(all_fields.size() == 4U);
      if (all_fields.size() == 4U) {
         expect(all_fields[0].name == "A");
         expect(all_fields[0].value == "1");
         expect(all_fields[1].name == "B");
         expect(all_fields[1].value == "2");
         expect(all_fields[2].name == "C");
         expect(all_fields[2].value == "3");
         expect(all_fields[3].name == "D");
         expect(all_fields[3].value == "4");
      }
   };

   "self append does nothing on copy"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
         {"Hello-World", "aero"},
      };

      auto fields_size_before_append = fields.size();
      fields.append(fields);

      expect(fields.size() == fields_size_before_append);
      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "Hello-World", "aero"));
   };

   "self append does nothing on move"_test = [] {
      glz::http_headers fields{
         {"Host", "example.com"},
         {"Hello-World", "aero"},
      };

      auto fields_size_before_append = fields.size();
      fields.append(std::move(fields));

      expect(fields.size() == fields_size_before_append);
      expect(contains_value(fields, "Host", "example.com"));
      expect(contains_value(fields, "Hello-World", "aero"));
   };
};

int main() {}
