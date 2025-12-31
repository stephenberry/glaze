// Glaze Library
// For the license information refer to glaze.hpp

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "glaze/beve.hpp"
#include "glaze/cbor.hpp"
#include "glaze/core/ostream_buffer.hpp"
#include "glaze/csv.hpp"
#include "glaze/json.hpp"
#include "glaze/msgpack.hpp"
#include "glaze/toml.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test struct for object serialization
struct TestObject
{
   int id{};
   std::string name{};
   double value{};
};

template <>
struct glz::meta<TestObject>
{
   using T = TestObject;
   static constexpr auto value = object("id", &T::id, "name", &T::name, "value", &T::value);
};

// Nested object for testing (must be at file scope for reflection)
struct NestedObj
{
   TestObject inner{};
   std::vector<int> values{};
};

// Empty object for testing
struct EmptyObj
{};

// Verify byte_output_stream concept
suite byte_output_stream_concept_tests = [] {
   "byte_output_stream accepts std::ostream"_test = [] {
      static_assert(glz::byte_output_stream<std::ostream>);
   };

   "byte_output_stream accepts std::ofstream"_test = [] {
      static_assert(glz::byte_output_stream<std::ofstream>);
   };

   "byte_output_stream accepts std::ostringstream"_test = [] {
      static_assert(glz::byte_output_stream<std::ostringstream>);
   };

   "byte_output_stream rejects std::wostream"_test = [] {
      static_assert(!glz::byte_output_stream<std::wostream>);
   };

   "byte_output_stream rejects std::wofstream"_test = [] {
      static_assert(!glz::byte_output_stream<std::wofstream>);
   };

   "byte_output_stream rejects std::wostringstream"_test = [] {
      static_assert(!glz::byte_output_stream<std::wostringstream>);
   };
};

// Verify buffer_traits static properties
suite buffer_traits_tests = [] {
   "buffer_traits::is_streaming for std::string"_test = [] {
      static_assert(glz::buffer_traits<std::string>::is_streaming == false);
      static_assert(!glz::is_streaming<std::string>);
   };

   "buffer_traits::is_streaming for basic_ostream_buffer"_test = [] {
      static_assert(glz::buffer_traits<glz::basic_ostream_buffer<std::ostream, 32>>::is_streaming == true);
      static_assert(glz::is_streaming<glz::basic_ostream_buffer<std::ostream, 32>>);
   };

   "buffer_traits::is_streaming for ostream_buffer alias"_test = [] {
      static_assert(glz::buffer_traits<glz::ostream_buffer<32>>::is_streaming == true);
      static_assert(glz::is_streaming<glz::ostream_buffer<32>>);
   };

   "buffer_traits::is_streaming for concrete stream type"_test = [] {
      static_assert(glz::buffer_traits<glz::basic_ostream_buffer<std::ostringstream>>::is_streaming == true);
      static_assert(glz::is_streaming<glz::basic_ostream_buffer<std::ostringstream>>);
   };

   "buffer_traits::is_streaming for std::vector<char>"_test = [] {
      static_assert(glz::buffer_traits<std::vector<char>>::is_streaming == false);
      static_assert(!glz::is_streaming<std::vector<char>>);
   };

   "buffer_traits::is_streaming for char*"_test = [] {
      static_assert(glz::buffer_traits<char*>::is_streaming == false);
      static_assert(!glz::is_streaming<char*>);
   };
};

// Test JSON streaming with ostream_buffer
suite json_ostream_buffer_tests = [] {
   "JSON object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      std::string expected = R"({"id":42,"name":"test","value":3.14})";
      expect(oss.str() == expected) << "Expected: " << expected << "\nGot: " << oss.str();
   };

   "JSON array streaming"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      std::string expected = "[1,2,3,4,5]";
      expect(oss.str() == expected) << "Expected: " << expected << "\nGot: " << oss.str();
   };

   "JSON map streaming"_test = [] {
      std::map<std::string, int> map_data{{"a", 1}, {"b", 2}, {"c", 3}};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(map_data, ctx, buf, ix);
      buf.finalize(ix);

      // Maps serialize in sorted order
      std::string expected = R"({"a":1,"b":2,"c":3})";
      expect(oss.str() == expected) << "Expected: " << expected << "\nGot: " << oss.str();
   };

   "JSON nested object streaming"_test = [] {
      NestedObj obj{{1, "nested", 2.5}, {10, 20, 30}};

      std::ostringstream oss;
      glz::ostream_buffer<64> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify it parses back correctly
      NestedObj parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.inner.id == 1);
      expect(parsed.inner.name == "nested");
      expect(parsed.values.size() == 3u);
   };

   "JSON large array streaming triggers flush"_test = [] {
      // Create array larger than buffer to ensure flush is triggered
      std::vector<int> large_arr(100);
      for (int i = 0; i < 100; ++i) {
         large_arr[i] = i;
      }

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss}; // Small buffer to trigger multiple flushes
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(large_arr, ctx, buf, ix);
      buf.finalize(ix);

      // Verify the output is valid JSON
      std::vector<int> parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.size() == 100u);
      expect(parsed[0] == 0);
      expect(parsed[99] == 99);
   };
};

// Test BEVE streaming with ostream_buffer
suite beve_ostream_buffer_tests = [] {
   "BEVE array streaming"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::BEVE>::op<glz::opts{.format = glz::BEVE}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::vector<int> parsed{};
      auto ec = glz::read_beve(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == arr);
   };

   "BEVE object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::BEVE>::op<glz::opts{.format = glz::BEVE}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      TestObject parsed{};
      auto ec = glz::read_beve(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.id == obj.id);
      expect(parsed.name == obj.name);
   };

   "BEVE map streaming"_test = [] {
      std::map<std::string, int> map_data{{"x", 10}, {"y", 20}};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::BEVE>::op<glz::opts{.format = glz::BEVE}>(map_data, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::map<std::string, int> parsed{};
      auto ec = glz::read_beve(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == map_data);
   };
};

// Test CBOR streaming with ostream_buffer
suite cbor_ostream_buffer_tests = [] {
   "CBOR array streaming"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::CBOR>::op<glz::opts{.format = glz::CBOR}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::vector<int> parsed{};
      auto ec = glz::read_cbor(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == arr);
   };

   "CBOR object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::CBOR>::op<glz::opts{.format = glz::CBOR}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      TestObject parsed{};
      auto ec = glz::read_cbor(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.id == obj.id);
      expect(parsed.name == obj.name);
   };

   "CBOR map streaming"_test = [] {
      std::map<std::string, int> map_data{{"alpha", 1}, {"beta", 2}};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::CBOR>::op<glz::opts{.format = glz::CBOR}>(map_data, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::map<std::string, int> parsed{};
      auto ec = glz::read_cbor(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == map_data);
   };
};

// Test MsgPack streaming with ostream_buffer
suite msgpack_ostream_buffer_tests = [] {
   "MsgPack array streaming"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::MSGPACK>::op<glz::opts{.format = glz::MSGPACK}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::vector<int> parsed{};
      auto ec = glz::read_msgpack(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == arr);
   };

   "MsgPack object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::MSGPACK>::op<glz::opts{.format = glz::MSGPACK}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      TestObject parsed{};
      auto ec = glz::read_msgpack(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.id == obj.id);
      expect(parsed.name == obj.name);
   };

   "MsgPack map streaming"_test = [] {
      std::map<std::string, int> map_data{{"foo", 100}, {"bar", 200}};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::MSGPACK>::op<glz::opts{.format = glz::MSGPACK}>(map_data, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify roundtrip
      std::map<std::string, int> parsed{};
      auto ec = glz::read_msgpack(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == map_data);
   };
};

// Test TOML streaming with ostream_buffer
suite toml_ostream_buffer_tests = [] {
   "TOML object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::TOML>::op<glz::opts{.format = glz::TOML}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // TOML output should contain the keys
      expect(oss.str().find("id") != std::string::npos);
      expect(oss.str().find("name") != std::string::npos);
      expect(oss.str().find("value") != std::string::npos);
   };
};

// Test CSV streaming with ostream_buffer
suite csv_ostream_buffer_tests = [] {
   "CSV 2D array streaming"_test = [] {
      std::vector<std::vector<int>> csv_data{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::CSV>::op<glz::opts{.format = glz::CSV}>(csv_data, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      // Verify output contains expected structure
      auto output = oss.str();
      expect(output.find(',') != std::string::npos); // Contains commas
      expect(output.find('\n') != std::string::npos); // Contains newlines
   };

   "CSV 1D array streaming"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::CSV>::op<glz::opts{.format = glz::CSV}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(!oss.str().empty());
      expect(oss.str().find(',') != std::string::npos); // Contains commas
   };
};

// Test ostream_buffer edge cases
suite ostream_buffer_edge_cases = [] {
   "empty array streaming"_test = [] {
      std::vector<int> empty_arr{};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(empty_arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(oss.str() == "[]");
   };

   "empty object streaming"_test = [] {
      EmptyObj obj{};

      std::ostringstream oss;
      glz::ostream_buffer<32> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(obj, ctx, buf, ix);
      buf.finalize(ix);

      expect(oss.str() == "{}");
   };

   "small buffer with large content"_test = [] {
      // Test that small buffer handles large content through flushing
      std::string long_string(1000, 'x');

      std::ostringstream oss;
      glz::ostream_buffer<16> buf{oss}; // Very small buffer
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(long_string, ctx, buf, ix);
      buf.finalize(ix);

      // Verify the content is preserved
      std::string parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == long_string);
   };

   "multiple writes to same buffer"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<64> buf{oss};
      glz::context ctx{};

      // Write first object
      size_t ix1 = 0;
      glz::serialize<glz::JSON>::op<glz::opts{}>(42, ctx, buf, ix1);
      buf.finalize(ix1);

      // The buffer should be reusable for another write
      // (though typically you'd create a new buffer)
      expect(oss.str() == "42");
   };
};

int main() { return 0; }
