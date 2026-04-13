// Bounded buffer overflow tests for CSV format
// These tests deliberately use undersized buffers to verify that ensure_space()
// properly returns buffer_overflow before any write occurs. GCC's optimizer
// emits false-positive -Wstringop-overflow warnings because it cannot prove
// the early-return path is always taken; suppressed via CMake for this target.

#include <array>
#include <span>
#include <string>
#include <vector>

#include "glaze/csv/read.hpp"
#include "glaze/csv/write.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace csv_bounded_buffer_tests
{
   struct simple_csv_row
   {
      std::vector<int> id{1, 2, 3};
      std::vector<std::string> name{"alice", "bob", "charlie"};
   };

   struct large_csv_row
   {
      std::vector<int> id{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      std::vector<std::string> name{"this_is_a_very_long_name_1", "this_is_a_very_long_name_2",
                                    "this_is_a_very_long_name_3", "this_is_a_very_long_name_4",
                                    "this_is_a_very_long_name_5", "this_is_a_very_long_name_6",
                                    "this_is_a_very_long_name_7", "this_is_a_very_long_name_8",
                                    "this_is_a_very_long_name_9", "this_is_a_very_long_name_10"};
   };

   struct csv_with_quotes
   {
      std::vector<std::string> data{"hello, world", "with \"quotes\"", "normal"};
   };
}

suite csv_bounded_buffer_overflow_tests = [] {
   using namespace csv_bounded_buffer_tests;
   "csv write to std::array with sufficient space succeeds"_test = [] {
      simple_csv_row obj{};
      std::array<char, 512> buffer{};

      auto result = glz::write_csv(obj, buffer);
      expect(not result) << "write should succeed with sufficient buffer";
      expect(result.count > 0) << "count should be non-zero";
      expect(result.count < buffer.size()) << "count should be less than buffer size";

      std::string_view csv(buffer.data(), result.count);
      expect(csv.find("id") != std::string_view::npos) << "CSV should contain header";
   };

   "csv write to std::array that is too small returns buffer_overflow"_test = [] {
      large_csv_row obj{};
      std::array<char, 10> buffer{};
      auto result = glz::write_csv(obj, buffer);
      expect(result.ec == glz::error_code::buffer_overflow) << "should return buffer_overflow error";
   };

   "csv write to std::span with sufficient space succeeds"_test = [] {
      simple_csv_row obj{};
      std::array<char, 512> storage{};
      std::span<char> buffer(storage);

      auto result = glz::write_csv(obj, buffer);
      expect(not result) << "write should succeed with sufficient buffer";
      expect(result.count > 0) << "count should be non-zero";
   };

   "csv write to std::span that is too small returns buffer_overflow"_test = [] {
      large_csv_row obj{};
      std::array<char, 5> storage{};
      std::span<char> buffer(storage);

      auto result = glz::write_csv(obj, buffer);
      expect(result.ec == glz::error_code::buffer_overflow) << "should return buffer_overflow error";
   };

   "csv write 2d array to bounded buffer works correctly"_test = [] {
      std::vector<std::vector<int>> matrix{{1, 2, 3}, {4, 5, 6}};
      std::array<char, 512> buffer{};

      auto result = glz::write<glz::opts_csv{.use_headers = false}>(matrix, buffer);
      expect(not result) << "write should succeed";
      expect(result.count > 0) << "count should be non-zero";
   };

   "csv resizable buffer still works as before"_test = [] {
      simple_csv_row obj{};
      std::string buffer;

      auto result = glz::write_csv(obj, buffer);
      expect(not result) << "write to resizable buffer should succeed";
      expect(buffer.size() > 0) << "buffer should have data";
   };

   "csv with quoted strings to bounded buffer"_test = [] {
      csv_with_quotes obj{};
      std::array<char, 512> buffer{};

      auto result = glz::write_csv(obj, buffer);
      expect(not result) << "write should succeed";
   };
};

int main() { return 0; }
