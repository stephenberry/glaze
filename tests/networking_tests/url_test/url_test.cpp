// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/net/url.hpp"

#include <vector>

#include "ut/ut.hpp"

using namespace ut;

suite url_decode_tests = [] {
   "basic"_test = [] {
      expect(glz::url_decode("") == "");
      expect(glz::url_decode("hello") == "hello");
      expect(glz::url_decode("hello%20world") == "hello world");
      expect(glz::url_decode("hello+world") == "hello world");
   };

   "percent_encoding"_test = [] {
      expect(glz::url_decode("%2F") == "/");
      expect(glz::url_decode("%2f") == "/");  // lowercase
      expect(glz::url_decode("path%2Fto%2Ffile") == "path/to/file");
      expect(glz::url_decode("a%3Db%26c%3Dd") == "a=b&c=d");
   };

   "invalid_sequences"_test = [] {
      expect(glz::url_decode("%") == "%");
      expect(glz::url_decode("%2") == "%2");
      expect(glz::url_decode("%GG") == "%GG");
      expect(glz::url_decode("100%") == "100%");
   };

   "buffer_overload"_test = [] {
      std::string buffer;
      glz::url_decode("hello%20world", buffer);
      expect(buffer == "hello world");

      glz::url_decode("foo%2Fbar", buffer);
      expect(buffer == "foo/bar");
   };
};

suite parse_urlencoded_tests = [] {
   "basic"_test = [] {
      expect(glz::parse_urlencoded("").empty());

      auto single = glz::parse_urlencoded("key=value");
      expect(single["key"] == "value");

      auto multi = glz::parse_urlencoded("a=1&b=2&c=3");
      expect(multi.size() == 3u);
      expect(multi["a"] == "1");
      expect(multi["b"] == "2");
      expect(multi["c"] == "3");
   };

   "encoding"_test = [] {
      auto result = glz::parse_urlencoded("name=John%20Doe&city=New+York");
      expect(result["name"] == "John Doe");
      expect(result["city"] == "New York");

      // Encoded keys
      auto encoded_key = glz::parse_urlencoded("encoded%20key=value");
      expect(encoded_key["encoded key"] == "value");
   };

   "edge_cases"_test = [] {
      auto empty_val = glz::parse_urlencoded("key=");
      expect(empty_val["key"] == "");

      auto no_val = glz::parse_urlencoded("flag");
      expect(no_val["flag"] == "");

      auto dup = glz::parse_urlencoded("a=1&a=2");
      expect(dup["a"] == "2");  // last wins

      auto trail = glz::parse_urlencoded("a=1&");
      expect(trail.size() == 1u);

      auto empty_key = glz::parse_urlencoded("=value&a=1");
      expect(empty_key.size() == 1u);  // empty key is skipped
      expect(empty_key["a"] == "1");
   };

   "buffer_overload"_test = [] {
      std::unordered_map<std::string, std::string> output;
      std::string key_buf, val_buf;

      glz::parse_urlencoded("a=1&b=2", output, key_buf, val_buf);
      expect(output["a"] == "1");
      expect(output["b"] == "2");

      glz::parse_urlencoded("x=10", output);
      expect(output["x"] == "10");
   };

   "realistic"_test = [] {
      auto form = glz::parse_urlencoded("username=john&password=s3cr3t%21&email=john%40example.com");
      expect(form["username"] == "john");
      expect(form["password"] == "s3cr3t!");
      expect(form["email"] == "john@example.com");
   };
};

suite split_target_tests = [] {
   "basic"_test = [] {
      auto [p1, q1] = glz::split_target("/api/users");
      expect(p1 == "/api/users");
      expect(q1.empty());

      auto [p2, q2] = glz::split_target("/api/users?limit=10");
      expect(p2 == "/api/users");
      expect(q2 == "limit=10");

      auto [p3, q3] = glz::split_target("/search?q=hello&page=1");
      expect(p3 == "/search");
      expect(q3 == "q=hello&page=1");
   };

   "edge_cases"_test = [] {
      auto [p1, q1] = glz::split_target("");
      expect(p1.empty());

      auto [p2, q2] = glz::split_target("/?debug=1");
      expect(p2 == "/");
      expect(q2 == "debug=1");

      auto [p3, q3] = glz::split_target("/path?");
      expect(p3 == "/path");
      expect(q3.empty());
   };

   "constexpr"_test = [] {
      constexpr auto result = glz::split_target("/test?foo=bar");
      static_assert(result.path == "/test");
      static_assert(result.query_string == "foo=bar");
   };
};

suite integration_tests = [] {
   "full_workflow"_test = [] {
      auto [path, query_string] = glz::split_target("/api/users?name=John%20Doe&age=30");
      expect(path == "/api/users");

      auto params = glz::parse_urlencoded(query_string);
      expect(params["name"] == "John Doe");
      expect(params["age"] == "30");
   };
};

int main() {}
