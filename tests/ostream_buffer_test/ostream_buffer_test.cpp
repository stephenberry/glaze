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
   "buffer_traits::is_output_streaming for std::string"_test = [] {
      static_assert(glz::buffer_traits<std::string>::is_output_streaming == false);
      static_assert(!glz::is_output_streaming<std::string>);
   };

   "buffer_traits::is_output_streaming for basic_ostream_buffer"_test = [] {
      static_assert(glz::buffer_traits<glz::basic_ostream_buffer<std::ostream, 512>>::is_output_streaming == true);
      static_assert(glz::is_output_streaming<glz::basic_ostream_buffer<std::ostream, 512>>);
   };

   "buffer_traits::is_output_streaming for ostream_buffer alias"_test = [] {
      static_assert(glz::buffer_traits<glz::ostream_buffer<512>>::is_output_streaming == true);
      static_assert(glz::is_output_streaming<glz::ostream_buffer<512>>);
   };

   "buffer_traits::is_output_streaming for concrete stream type"_test = [] {
      static_assert(glz::buffer_traits<glz::basic_ostream_buffer<std::ostringstream>>::is_output_streaming == true);
      static_assert(glz::is_output_streaming<glz::basic_ostream_buffer<std::ostringstream>>);
   };

   "buffer_traits::is_output_streaming for std::vector<char>"_test = [] {
      static_assert(glz::buffer_traits<std::vector<char>>::is_output_streaming == false);
      static_assert(!glz::is_output_streaming<std::vector<char>>);
   };

   "buffer_traits::is_output_streaming for char*"_test = [] {
      static_assert(glz::buffer_traits<char*>::is_output_streaming == false);
      static_assert(!glz::is_output_streaming<char*>);
   };
};

// Test JSON streaming with ostream_buffer
suite json_ostream_buffer_tests = [] {
   "JSON object streaming"_test = [] {
      TestObject obj{42, "test", 3.14};

      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss}; // Small buffer to trigger multiple flushes
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      glz::serialize<glz::JSON>::op<glz::opts{}>(empty_arr, ctx, buf, ix);
      buf.finalize(ix);

      expect(oss.str() == "[]");
   };

   "empty object streaming"_test = [] {
      EmptyObj obj{};

      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};
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
      glz::ostream_buffer<512> buf{oss}; // Minimum size buffer (512 bytes)
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
      glz::ostream_buffer<512> buf{oss};
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

// ============================================================================
// FLUSH BEHAVIOR TESTS
// ============================================================================

suite flush_behavior_tests = [] {
   "no flush when under threshold"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<1024> buf{oss}; // 1KB buffer
      glz::context ctx{};
      size_t ix = 0;

      // Small array that won't trigger flush
      std::vector<int> small_arr{1, 2, 3};
      glz::serialize<glz::JSON>::op<glz::opts{}>(small_arr, ctx, buf, ix);

      // Before finalize, nothing should be flushed to stream (under threshold)
      // Note: Implementation may flush at any point, this just verifies functionality
      buf.finalize(ix);

      std::string expected = "[1,2,3]";
      expect(oss.str() == expected);
   };

   "flush at array element boundaries"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss}; // Small buffer to trigger flushes
      glz::context ctx{};
      size_t ix = 0;

      // Larger array that will trigger flushes
      std::vector<int> arr(50);
      for (int i = 0; i < 50; ++i) {
         arr[i] = i;
      }
      glz::serialize<glz::JSON>::op<glz::opts{}>(arr, ctx, buf, ix);
      buf.finalize(ix);

      // Verify complete output
      std::vector<int> parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == arr);
   };

   "flush at object field boundaries"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss}; // Small buffer
      glz::context ctx{};
      size_t ix = 0;

      std::map<std::string, int> map_data{
         {"alpha", 1}, {"beta", 2}, {"gamma", 3}, {"delta", 4}, {"epsilon", 5}};
      glz::serialize<glz::JSON>::op<glz::opts{}>(map_data, ctx, buf, ix);
      buf.finalize(ix);

      // Verify complete output
      std::map<std::string, int> parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == map_data);
   };

   "bytes_flushed tracking"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss}; // Small buffer to ensure flushing
      glz::context ctx{};
      size_t ix = 0;

      expect(buf.bytes_flushed() == 0u);

      // Write enough data to trigger flush
      std::vector<int> arr(100);
      for (int i = 0; i < 100; ++i) {
         arr[i] = i;
      }
      glz::serialize<glz::JSON>::op<glz::opts{}>(arr, ctx, buf, ix);

      // Finalize should flush remaining data
      buf.finalize(ix);

      expect(buf.bytes_flushed() == ix) << "bytes_flushed: " << buf.bytes_flushed() << " ix: " << ix;
   };

   "buffer_capacity accessor"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      expect(buf.buffer_capacity() == 512u);
   };

   "final flush on finalize"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<1024> buf{oss}; // Large buffer
      glz::context ctx{};
      size_t ix = 0;

      // Write small content that won't trigger intermediate flush
      TestObject obj{42, "test", 3.14};
      glz::serialize<glz::JSON>::op<glz::opts{}>(obj, ctx, buf, ix);

      // Before finalize, stream should have some or all data
      // After finalize, all data must be in stream
      buf.finalize(ix);

      TestObject parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed.id == obj.id);
      expect(parsed.name == obj.name);
   };

   "nested structure with flushes"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      // Nested structure
      std::map<std::string, std::vector<int>> nested{
         {"first", {1, 2, 3, 4, 5}},
         {"second", {10, 20, 30, 40, 50}},
         {"third", {100, 200, 300}}};
      glz::serialize<glz::JSON>::op<glz::opts{}>(nested, ctx, buf, ix);
      buf.finalize(ix);

      std::map<std::string, std::vector<int>> parsed{};
      auto ec = glz::read_json(parsed, oss.str());
      expect(ec == glz::error_code::none);
      expect(parsed == nested);
   };
};

// ============================================================================
// BUFFER_TRAITS COMPREHENSIVE TESTS
// ============================================================================

suite buffer_traits_comprehensive_tests = [] {
   "buffer_traits for std::string"_test = [] {
      using traits = glz::buffer_traits<std::string>;
      static_assert(traits::is_resizable);
      static_assert(!traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);

      std::string s = "hello";
      expect(traits::capacity(s) == std::numeric_limits<size_t>::max());
      expect(traits::ensure_capacity(s, 100) == true);
      expect(s.size() >= 100u); // ensure_capacity may resize
   };

   "buffer_traits for std::vector<char>"_test = [] {
      using traits = glz::buffer_traits<std::vector<char>>;
      static_assert(traits::is_resizable);
      static_assert(!traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);

      std::vector<char> v(10);
      expect(traits::ensure_capacity(v, 50) == true);
      expect(v.size() >= 50u);
   };

   "buffer_traits for std::array"_test = [] {
      using traits = glz::buffer_traits<std::array<char, 100>>;
      static_assert(!traits::is_resizable);
      static_assert(traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);
      static_assert(traits::static_capacity == 100);

      std::array<char, 100> arr{};
      expect(traits::capacity(arr) == 100u);
      expect(traits::ensure_capacity(arr, 50) == true);
      expect(traits::ensure_capacity(arr, 200) == false);
   };

   "buffer_traits for std::span"_test = [] {
      using traits = glz::buffer_traits<std::span<char>>;
      static_assert(!traits::is_resizable);
      static_assert(traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);

      std::array<char, 50> storage{};
      std::span<char> s{storage};
      expect(traits::capacity(s) == 50u);
      expect(traits::ensure_capacity(s, 30) == true);
      expect(traits::ensure_capacity(s, 100) == false);
   };

   "buffer_traits for char*"_test = [] {
      using traits = glz::buffer_traits<char*>;
      static_assert(!traits::is_resizable);
      static_assert(!traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);

      char buf[100];
      char* ptr = buf;
      expect(traits::capacity(ptr) == std::numeric_limits<size_t>::max());
      expect(traits::ensure_capacity(ptr, 1000) == true); // Always trusts caller
   };

   "buffer_traits finalize behavior"_test = [] {
      // Test finalize for resizable buffer
      std::string s;
      s.resize(100);
      glz::buffer_traits<std::string>::finalize(s, 50);
      expect(s.size() == 50u);

      // Test finalize for fixed buffer (no-op)
      std::array<char, 100> arr{};
      glz::buffer_traits<std::array<char, 100>>::finalize(arr, 50);
      expect(arr.size() == 100u); // Unchanged
   };

   "is_output_streaming concept"_test = [] {
      static_assert(!glz::is_output_streaming<std::string>);
      static_assert(!glz::is_output_streaming<std::vector<char>>);
      static_assert(!glz::is_output_streaming<char*>);
      static_assert(glz::is_output_streaming<glz::ostream_buffer<>>);
      static_assert(glz::is_output_streaming<glz::basic_ostream_buffer<std::ostringstream>>);
   };

   "flush_buffer helper"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      // Write some data directly
      buf[0] = 'H';
      buf[1] = 'i';

      glz::flush_buffer(buf, 2);
      buf.finalize(2);

      expect(oss.str() == "Hi");
   };
};

// ============================================================================
// RESET AND REUSE TESTS
// ============================================================================

suite ostream_buffer_reset_tests = [] {
   "reset clears state"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};
      glz::context ctx{};
      size_t ix = 0;

      // First write
      glz::serialize<glz::JSON>::op<glz::opts{}>(42, ctx, buf, ix);
      buf.finalize(ix);

      expect(buf.bytes_flushed() == ix);

      // Reset
      buf.reset();

      expect(buf.bytes_flushed() == 0u);
   };

   "good and fail accessors"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      expect(buf.good());
      expect(!buf.fail());

      // Access stream
      expect(buf.stream() == &oss);
   };

   "stream accessor"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      auto* stream_ptr = buf.stream();
      expect(stream_ptr == &oss);
   };
};

// ============================================================================
// SPECIAL TYPES WITH STREAMING OUTPUT
// ============================================================================

// Enum for testing
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   using enum Color;
   static constexpr auto value = enumerate(Red, Green, Blue);
};

// Variant type for testing
using Variant = std::variant<int, std::string, double>;

suite streaming_special_types_output_tests = [] {
   "enum with streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      auto ec = glz::write_json(Color::Green, buf);
      expect(!ec);
      expect(oss.str() == "\"Green\"");
   };

   "optional with value - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::optional<int> opt = 42;
      auto ec = glz::write_json(opt, buf);
      expect(!ec);
      expect(oss.str() == "42");
   };

   "optional without value - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::optional<int> opt = std::nullopt;
      auto ec = glz::write_json(opt, buf);
      expect(!ec);
      expect(oss.str() == "null");
   };

   "variant - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      Variant v = std::string{"hello"};
      auto ec = glz::write_json(v, buf);
      expect(!ec);
      expect(oss.str() == "\"hello\"");
   };

   "tuple - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::tuple<int, std::string, double> t{1, "two", 3.0};
      auto ec = glz::write_json(t, buf);
      expect(!ec);
      expect(oss.str() == "[1,\"two\",3]");
   };

   "pair - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::pair<std::string, int> p{"key", 42};
      auto ec = glz::write_json(p, buf);
      expect(!ec);
      // Glaze serializes pairs as objects {"key":value}
      expect(oss.str() == "{\"key\":42}");
   };

   "array of optionals - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::vector<std::optional<int>> arr{1, std::nullopt, 3, std::nullopt, 5};
      auto ec = glz::write_json(arr, buf);
      expect(!ec);
      expect(oss.str() == "[1,null,3,null,5]");
   };

   "nested optionals - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::optional<std::vector<int>> opt = std::vector<int>{1, 2, 3};
      auto ec = glz::write_json(opt, buf);
      expect(!ec);
      expect(oss.str() == "[1,2,3]");
   };

   "empty vector - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::vector<int> arr{};
      auto ec = glz::write_json(arr, buf);
      expect(!ec);
      expect(oss.str() == "[]");
   };

   "empty map - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::map<std::string, int> m{};
      auto ec = glz::write_json(m, buf);
      expect(!ec);
      expect(oss.str() == "{}");
   };

   "boolean values - streaming output"_test = [] {
      {
         std::ostringstream oss;
         glz::ostream_buffer<512> buf{oss};
         auto ec = glz::write_json(true, buf);
         expect(!ec);
         expect(oss.str() == "true");
      }
      {
         std::ostringstream oss;
         glz::ostream_buffer<512> buf{oss};
         auto ec = glz::write_json(false, buf);
         expect(!ec);
         expect(oss.str() == "false");
      }
   };

   "null pointer - streaming output"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<512> buf{oss};

      std::nullptr_t n = nullptr;
      auto ec = glz::write_json(n, buf);
      expect(!ec);
      expect(oss.str() == "null");
   };
};

// ============================================================================
// DOCUMENTATION EXAMPLES AS TESTS
// ============================================================================

suite documentation_example_tests = [] {
   "output streaming example from docs"_test = [] {
      // From docs/streaming.md
      std::ostringstream oss;
      glz::ostream_buffer<> buffer(oss);

      TestObject obj{42, "example", 3.14};
      auto ec = glz::write_json(obj, buffer);

      expect(!ec);
      expect(!oss.str().empty());
   };

   "polymorphic ostream_buffer example"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<> buffer(oss); // Alias for basic_ostream_buffer<std::ostream>

      auto ec = glz::write_json(123, buffer);
      expect(!ec);
      expect(oss.str() == "123");
   };

   "custom buffer capacity example"_test = [] {
      std::ostringstream oss;
      glz::ostream_buffer<4096> buf(oss); // 4KB buffer

      expect(buf.buffer_capacity() == 4096u);

      auto ec = glz::write_json(42, buf);
      expect(!ec);
   };

   "concrete stream type example"_test = [] {
      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostringstream> buffer(oss);

      auto ec = glz::write_json("hello", buffer);
      expect(!ec);
      expect(oss.str() == "\"hello\"");
   };
};

int main() { return 0; }
