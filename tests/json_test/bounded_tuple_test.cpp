// Glaze Library
// For the license information refer to glaze.hpp

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "glaze/json/write.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct empty_glaze_array
{};

template <>
struct glz::meta<empty_glaze_array>
{
   static constexpr auto value = array();
};

struct nested_glaze_array
{
   std::tuple<> value{};
};

template <>
struct glz::meta<nested_glaze_array>
{
   static constexpr auto value = array(&nested_glaze_array::value);
};

struct failing_glaze_array
{
   std::string value{};
   std::tuple<> next{};
};

template <>
struct glz::meta<failing_glaze_array>
{
   static constexpr auto value = array(&failing_glaze_array::value, &failing_glaze_array::next);
};

struct pretty_inline_arrays : glz::opts
{
   bool prettify = true;
   bool new_lines_in_arrays = false;
};

template <auto Opts, class T>
void check_capacities(const T& value, std::string_view expected)
{
   constexpr char guard = '\x7f';
   for (size_t capacity = 0; capacity <= 48; ++capacity) {
      std::vector<char> storage(capacity + 16, guard);
      std::span<char> buffer(storage.data(), capacity);
      const auto result = glz::write<Opts>(value, buffer);

      expect(result.count <= capacity) << "capacity " << capacity << " count " << result.count;
      expect(std::all_of(storage.begin() + capacity, storage.end(), [](char c) { return c == guard; }))
         << "guard bytes changed at capacity " << capacity;
      if (result) {
         expect(result.ec == glz::error_code::buffer_overflow);
      }
      else {
         expect(std::string_view(storage.data(), result.count) == expected);
      }
   }

   std::array<char, 512> buffer{};
   const auto result = glz::write<Opts>(value, buffer);
   expect(not result);
   expect(std::string_view(buffer.data(), result.count) == expected);

   std::string output{};
   expect(not glz::write<Opts>(value, output));
   expect(output == expected);
}

template <class T>
void check_modes(const T& value, std::string_view compact, std::string_view pretty, std::string_view inline_pretty)
{
   check_capacities<glz::opts{}>(value, compact);
   check_capacities<glz::opts{.prettify = true}>(value, pretty);
   check_capacities<pretty_inline_arrays{}>(value, inline_pretty);
}

template <class T>
void check_member_failure(const T& value)
{
   std::array<char, 32> buffer{};
   buffer.fill('\x7f');
   const auto result = glz::write_json(value, buffer);
   expect(result.ec == glz::error_code::buffer_overflow);
   expect(result.count == 1u);
   expect(buffer[0] == '[');
   expect(std::all_of(buffer.begin() + 1, buffer.end(), [](char c) { return c == '\x7f'; }));
}

suite bounded_tuple_tests = [] {
   "empty tuple and metadata arrays respect buffer capacity"_test = [] {
      check_modes(std::tuple<>{}, "[]", "[]", "[]");
      check_modes(empty_glaze_array{}, "[]", "[]", "[]");
      check_modes(glz::arr{}, "[]", "[]", "[]");
   };

   "nested tuple and metadata arrays bound indentation and closing brackets"_test = [] {
      const std::tuple<std::tuple<>> tuple{};
      const glz::tuple<std::tuple<>> native_tuple{};
      const nested_glaze_array metadata_array{};
      const glz::arr<std::tuple<>> array{};
      const std::tuple<std::tuple<std::tuple<>>> deeply_nested{};
      check_modes(tuple, "[[]]", "[\n   []\n]", "[[]]");
      check_modes(native_tuple, "[[]]", "[\n   []\n]", "[[]]");
      check_modes(metadata_array, "[[]]", "[\n   []\n]", "[[]]");
      check_modes(array, "[[]]", "[\n   []\n]", "[[]]");
      check_modes(deeply_nested, "[[[]]]", "[\n   [\n      []\n   ]\n]", "[[[]]]");
   };

   "failed tuple members stop separators later members and closing brackets"_test = [] {
      const std::string text(80, 'x');
      const auto compact = "[\"" + text + "\",[]]";
      const auto pretty = "[\n   \"" + text + "\",\n   []\n]";
      const auto inline_pretty = "[\"" + text + "\", []]";
      const auto tuple = std::tuple{text, std::tuple<>{}};
      const auto native_tuple = glz::tuple{text, std::tuple<>{}};
      const auto metadata_array = failing_glaze_array{text};
      const auto array = glz::arr{text, std::tuple<>{}};

      check_member_failure(tuple);
      check_member_failure(native_tuple);
      check_member_failure(metadata_array);
      check_member_failure(array);
      check_modes(tuple, compact, pretty, inline_pretty);
      check_modes(native_tuple, compact, pretty, inline_pretty);
      check_modes(metadata_array, compact, pretty, inline_pretty);
      check_modes(array, compact, pretty, inline_pretty);
   };

   "empty tuple closing bracket must fit in a fixed array"_test = [] {
      std::array<char, 1> buffer{};
      const auto result = glz::write_json(std::tuple<>{}, buffer);
      expect(result.ec == glz::error_code::buffer_overflow);
      expect(result.count == 1u);
   };

   "prettified tuple closing newline and bracket must both fit"_test = [] {
      std::array<char, 8> buffer{};
      buffer.fill('\x7f');
      const auto result = glz::write<glz::opts{.prettify = true}>(std::tuple<std::tuple<>>{}, buffer);
      expect(result.ec == glz::error_code::buffer_overflow);
      expect(result.count == 7u);
      expect(buffer[7] == '\x7f');
   };

   "tuple writes preserve C array capacity"_test = [] {
      char buffer[1]{};
      const auto result = glz::write_json(std::tuple<>{}, buffer);
      expect(result.ec == glz::error_code::buffer_overflow);
      expect(result.count <= sizeof(buffer));
   };
};

int main() { return 0; }
