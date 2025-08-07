// Final comprehensive unit test for std::span integration with buffer overflow protection
// Glaze Library
// For the license information refer to glaze.hpp

#include <array>
#include <span>
#include <string>
#include <vector>

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct SimpleStruct {
    int value = 42;
    std::string name = "test";
};

struct NestedStruct {
    SimpleStruct inner{};
    std::vector<int> numbers{1, 2, 3};
    bool flag = true;
};

template <>
struct glz::meta<SimpleStruct> {
    using T = SimpleStruct;
    static constexpr auto value = glz::object("value", &T::value, "name", &T::name);
};

template <>
struct glz::meta<NestedStruct> {
    using T = NestedStruct;
    static constexpr auto value = glz::object("inner", &T::inner, "numbers", &T::numbers, "flag", &T::flag);
};

suite final_span_integration_tests = [] {
   // Test 1: All byte span types work and return size information
   "all byte span types return size"_test = [] {
      SimpleStruct obj{123, "hello"};
      
      std::array<char, 256> char_buffer{};
      std::array<uint8_t, 256> uint8_buffer{};
      std::array<std::byte, 256> byte_buffer{};
      
      auto char_result = glz::write_json(obj, std::span{char_buffer});
      auto uint8_result = glz::write_json(obj, std::span{uint8_buffer});
      auto byte_result = glz::write_json(obj, std::span{byte_buffer});
      
      expect(char_result.has_value()) << "std::span<char> should succeed";
      expect(uint8_result.has_value()) << "std::span<uint8_t> should succeed";
      expect(byte_result.has_value()) << "std::span<std::byte> should succeed";
      
      // All should return the same size
      expect(char_result.value() == uint8_result.value());
      expect(char_result.value() == byte_result.value());
      expect(char_result.value() > 0);
      
      // Content should be identical
      std::string_view char_json(char_buffer.data(), char_result.value());
      std::string_view uint8_json(reinterpret_cast<char*>(uint8_buffer.data()), uint8_result.value());
      std::string_view byte_json(reinterpret_cast<char*>(byte_buffer.data()), byte_result.value());
      
      expect(char_json == uint8_json);
      expect(char_json == byte_json);
      
      // Verify JSON structure
      expect(char_json.find("\"value\":123") != std::string_view::npos);
      expect(char_json.find("\"name\":\"hello\"") != std::string_view::npos);
   };

   // Test 2: Buffer overflow is detected and reported correctly
   "buffer overflow detection works"_test = [] {
      SimpleStruct large_obj{99999, "this_is_a_very_long_name_that_will_not_fit_in_small_buffers"};
      
      // Test with insufficient buffer
      std::array<char, 20> small_buffer{};
      auto result = glz::write_json(large_obj, std::span{small_buffer});
      
      expect(!result.has_value()) << "Should fail with insufficient buffer";
      expect(result.error().ec == glz::error_code::unexpected_end) << "Should return unexpected_end error";
   };

   // Test 3: Edge case - exact buffer size vs one byte too small
   "exact buffer size edge cases"_test = [] {
      SimpleStruct obj{1, "x"};
      
      // Determine exact size needed
      std::array<char, 256> temp_buffer{};
      auto temp_result = glz::write_json(obj, std::span{temp_buffer});
      expect(temp_result.has_value());
      
      size_t exact_size = temp_result.value();
      std::string_view expected_json(temp_buffer.data(), exact_size);
      
      // Test with exact size - should succeed
      {
         std::vector<char> exact_buffer(exact_size);
         auto result = glz::write_json(obj, std::span{exact_buffer});
         
         expect(result.has_value()) << "Should succeed with exact buffer size";
         expect(result.value() == exact_size);
         
         std::string_view actual_json(exact_buffer.data(), result.value());
         expect(actual_json == expected_json) << "Content should match exactly";
      }
      
      // Test with one byte less - should fail with proper error
      if (exact_size > 1) {
         std::vector<char> small_buffer(exact_size - 1);
         auto result = glz::write_json(obj, std::span{small_buffer});
         
         expect(!result.has_value()) << "Should fail when buffer is one byte too small";
         expect(result.error().ec == glz::error_code::unexpected_end) << "Should return unexpected_end error";
      }
   };

   // Test 4: Complex nested structures with buffer overflow protection
   "complex structures with overflow protection"_test = [] {
      NestedStruct complex_obj;
      complex_obj.inner.value = 12345;
      complex_obj.inner.name = "nested_structure_test";
      complex_obj.numbers = {100, 200, 300, 400, 500};
      complex_obj.flag = false;
      
      // Test with sufficient buffer
      {
         std::array<char, 512> large_buffer{};
         auto result = glz::write_json(complex_obj, std::span{large_buffer});
         
         expect(result.has_value()) << "Should handle complex nested structures";
         expect(result.value() > 50) << "Should serialize to substantial size";
         
         std::string_view json(large_buffer.data(), result.value());
         expect(json.find("\"value\":12345") != std::string_view::npos);
         expect(json.find("\"name\":\"nested_structure_test\"") != std::string_view::npos);
         expect(json.find("\"numbers\":[100,200,300,400,500]") != std::string_view::npos);
         expect(json.find("\"flag\":false") != std::string_view::npos);
      }
      
      // Test with insufficient buffer for complex structure
      {
         std::array<char, 30> small_buffer{};
         auto result = glz::write_json(complex_obj, std::span{small_buffer});
         
         expect(!result.has_value()) << "Should fail with insufficient buffer for complex structure";
         expect(result.error().ec == glz::error_code::unexpected_end);
      }
   };

   // Test 5: Different span extents work correctly
   "different span extents work"_test = [] {
      SimpleStruct obj{777, "extent_test"};
      
      // Fixed extent span
      {
         std::array<char, 100> buffer{};
         std::span<char, 100> fixed_span{buffer};
         auto result = glz::write_json(obj, fixed_span);
         
         expect(result.has_value()) << "Fixed extent span should work";
         expect(result.value() > 0);
      }
      
      // Dynamic extent span
      {
         std::array<char, 100> buffer{};
         std::span<char> dynamic_span{buffer};
         auto result = glz::write_json(obj, dynamic_span);
         
         expect(result.has_value()) << "Dynamic extent span should work";
         expect(result.value() > 0);
      }
   };

   // Test 6: Verify backward compatibility with existing APIs
   "backward compatibility preserved"_test = [] {
      SimpleStruct obj{888, "compat_test"};
      
      // Test with std::string (should still work)
      {
         std::string buffer;
         auto result = glz::write_json(obj, buffer);
         
         expect(result == glz::error_code::none) << "std::string buffer should still work";
         expect(buffer.size() > 0);
         expect(buffer.find("\"value\":888") != std::string::npos);
      }
      
      // Test with std::vector (should still work)
      {
         std::vector<char> buffer;
         auto result = glz::write_json(obj, buffer);
         
         expect(result == glz::error_code::none) << "std::vector buffer should still work";
         expect(buffer.size() > 0);
      }
   };

   // Test 7: Empty and minimal data edge cases
   "empty and minimal data handling"_test = [] {
      // Empty string
      {
         std::string empty_str;
         std::array<char, 10> buffer{};
         auto result = glz::write_json(empty_str, std::span{buffer});
         
         expect(result.has_value());
         expect(result.value() == 2); // Should be '""'
         
         std::string_view json(buffer.data(), result.value());
         expect(json == "\"\"");
      }
      
      // Zero integer  
      {
         int zero = 0;
         std::array<char, 5> buffer{};
         auto result = glz::write_json(zero, std::span{buffer});
         
         expect(result.has_value());
         expect(result.value() == 1); // Should be '0'
         
         std::string_view json(buffer.data(), result.value());
         expect(json == "0");
      }
      
      // Empty vector
      {
         std::vector<int> empty_vec;
         std::array<char, 5> buffer{};
         auto result = glz::write_json(empty_vec, std::span{buffer});
         
         expect(result.has_value());
         expect(result.value() == 2); // Should be '[]'
         
         std::string_view json(buffer.data(), result.value());
         expect(json == "[]");
      }
   };
};

int main() {
   return 0;
}