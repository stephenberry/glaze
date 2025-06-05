// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp"
#include "glaze/reflection/enum_reflect.hpp"
#include "ut/ut.hpp"
#include <string>

using namespace ut;

// Test enums for different scenarios
enum class Color { Red, Green, Blue };
enum class Status : int { Pending = -1, Running = 0, Complete = 1 };
enum class Sparse { First = 1, Second = 5, Third = 10 };
enum TrafficLight { Stop, Caution, Go };
enum Direction : unsigned { North = 0, East = 1, South = 2, West = 3 };
enum class Boolean : bool { False = false, True = true };
enum Empty {};

// Concepts and type traits tests - compile-time verification
static_assert(glz::is_enum<Color>, "Color should satisfy Enum concept");
static_assert(glz::is_enum<Status>, "Status should satisfy Enum concept");
static_assert(glz::is_enum<TrafficLight>, "TrafficLight should satisfy Enum concept");
static_assert(!glz::is_enum<int>, "int should not satisfy Enum concept");

static_assert(glz::scoped_enum<Color>, "Color should be scoped enum");
static_assert(glz::scoped_enum<Status>, "Status should be scoped enum");
static_assert(!glz::scoped_enum<TrafficLight>, "TrafficLight should not be scoped enum");

static_assert(glz::unscoped_enum<TrafficLight>, "TrafficLight should be unscoped enum");
static_assert(glz::unscoped_enum<Direction>, "Direction should be unscoped enum");
static_assert(!glz::unscoped_enum<Color>, "Color should not be unscoped enum");

static_assert(glz::signed_enum<Status>, "Status should be signed enum");
static_assert(!glz::signed_enum<Direction>, "Direction should not be signed enum");
static_assert(!glz::signed_enum<Boolean>, "Boolean should not be signed enum");

static_assert(glz::unsigned_enum<Direction>, "Direction should be unsigned enum");
static_assert(glz::unsigned_enum<Boolean>, "Boolean should be unsigned enum");
static_assert(!glz::unsigned_enum<Status>, "Status should not be unsigned enum");

// Basic functionality tests
suite basic_functionality_tests = [] {
   "entries_color"_test = [] {
      constexpr auto color_entries = glz::enums<Color>;
      expect(color_entries.size() == 3) << "Color should have 3 enums\n";
      expect(color_entries[0].first == Color::Red) << "First entry should be Red\n";
      expect(color_entries[1].first == Color::Green) << "Second entry should be Green\n";
      expect(color_entries[2].first == Color::Blue) << "Third entry should be Blue\n";
      expect(color_entries[0].second == "Red") << "First entry name should be 'Red'\n";
      expect(color_entries[1].second == "Green") << "Second entry name should be 'Green'\n";
      expect(color_entries[2].second == "Blue") << "Third entry name should be 'Blue'\n";
   };
   
   "entries_status"_test = [] {
      constexpr auto status_entries = glz::enums<Status>;
      expect(status_entries.size() == 3) << "Status should have 3 enums\n";
      expect(status_entries[0].first == Status::Pending) << "First entry should be Pending\n";
      expect(status_entries[1].first == Status::Running) << "Second entry should be Running\n";
      expect(status_entries[2].first == Status::Complete) << "Third entry should be Complete\n";
   };
   
   "values_extraction"_test = [] {
      constexpr auto color_values = glz::values<Color>;
      expect(color_values.size() == 3) << "Color should have 3 values\n";
      expect(color_values[0] == Color::Red) << "First value should be Red\n";
      expect(color_values[1] == Color::Green) << "Second value should be Green\n";
      expect(color_values[2] == Color::Blue) << "Third value should be Blue\n";
   };
   
   "names_extraction"_test = [] {
      constexpr auto color_names = glz::names<Color>;
      expect(color_names.size() == 3) << "Color should have 3 names\n";
      expect(color_names[0] == "Red") << "First name should be 'Red'\n";
      expect(color_names[1] == "Green") << "Second name should be 'Green'\n";
      expect(color_names[2] == "Blue") << "Third name should be 'Blue'\n";
   };
   
   "min_max_values"_test = [] {
      expect(glz::min<Color> == Color::Red) << "Min Color should be Red\n";
      expect(glz::max<Color> == Color::Blue) << "Max Color should be Blue\n";
      expect(glz::min<Status> == Status::Pending) << "Min Status should be Pending\n";
      expect(glz::max<Status> == Status::Complete) << "Max Status should be Complete\n";
   };
   
   "count_values"_test = [] {
      expect(glz::count<Color> == 3) << "Color count should be 3\n";
      expect(glz::count<Status> == 3) << "Status count should be 3\n";
      expect(glz::count<TrafficLight> == 3) << "TrafficLight count should be 3\n";
   };
};

// Contiguous enum tests
suite contiguous_tests = [] {
   "is_contiguous_check"_test = [] {
      expect(glz::is_contiguous<Color>) << "Color should be contiguous\n";
      expect(glz::is_contiguous<Status>) << "Status should be contiguous\n";
      expect(glz::is_contiguous<TrafficLight>) << "TrafficLight should be contiguous\n";
      expect(glz::is_contiguous<Direction>) << "Direction should be contiguous\n";
      expect(not glz::is_contiguous<Sparse>) << "Sparse should not be contiguous\n";
      expect(glz::is_contiguous<Boolean>) << "Boolean should be contiguous\n";
   };
   
   "contiguous_enum_concept"_test = [] {
      expect(glz::ContiguousEnum<Color>) << "Color should satisfy ContiguousEnum concept\n";
      expect(glz::ContiguousEnum<TrafficLight>) << "TrafficLight should satisfy ContiguousEnum concept\n";
      expect(not glz::ContiguousEnum<Sparse>) << "Sparse should not satisfy ContiguousEnum concept\n";
   };
};

// Contains functionality tests
suite contains_tests = [] {
   "contains_enum_value"_test = [] {
      expect(glz::contains<Color>(Color::Red)) << "Color should contain Red\n";
      expect(glz::contains<Color>(Color::Green)) << "Color should contain Green\n";
      expect(glz::contains<Color>(Color::Blue)) << "Color should contain Blue\n";
      
      expect(glz::contains<Status>(Status::Pending)) << "Status should contain Pending\n";
      expect(glz::contains<Status>(Status::Running)) << "Status should contain Running\n";
      expect(glz::contains<Status>(Status::Complete)) << "Status should contain Complete\n";
   };
   
   "contains_underlying_value"_test = [] {
      expect(glz::contains<Color>(0)) << "Color should contain underlying value 0 (Red)\n";
      expect(glz::contains<Color>(1)) << "Color should contain underlying value 1 (Green)\n";
      expect(glz::contains<Color>(2)) << "Color should contain underlying value 2 (Blue)\n";
      expect(not glz::contains<Color>(3)) << "Color should not contain underlying value 3\n";
      
      expect(glz::contains<Status>(-1)) << "Status should contain underlying value -1 (Pending)\n";
      expect(glz::contains<Status>(0)) << "Status should contain underlying value 0 (Running)\n";
      expect(glz::contains<Status>(1)) << "Status should contain underlying value 1 (Complete)\n";
      expect(not glz::contains<Status>(2)) << "Status should not contain underlying value 2\n";
   };
   
   "contains_string_name"_test = [] {
      expect(glz::contains<Color>("Red")) << "Color should contain name 'Red'\n";
      expect(glz::contains<Color>("Green")) << "Color should contain name 'Green'\n";
      expect(glz::contains<Color>("Blue")) << "Color should contain name 'Blue'\n";
      expect(not glz::contains<Color>("Yellow")) << "Color should not contain name 'Yellow'\n";
      
      expect(glz::contains<Status>("Pending")) << "Status should contain name 'Pending'\n";
      expect(glz::contains<Status>("Running")) << "Status should contain name 'Running'\n";
      expect(glz::contains<Status>("Complete")) << "Status should contain name 'Complete'\n";
      expect(not glz::contains<Status>("Failed")) << "Status should not contain name 'Failed'\n";
   };
};

// Conversion tests
suite conversion_tests = [] {
   "to_underlying"_test = [] {
      expect(glz::to_underlying(Color::Red) == 0) << "Red should have underlying value 0\n";
      expect(glz::to_underlying(Color::Green) == 1) << "Green should have underlying value 1\n";
      expect(glz::to_underlying(Color::Blue) == 2) << "Blue should have underlying value 2\n";
      
      expect(glz::to_underlying(Status::Pending) == -1) << "Pending should have underlying value -1\n";
      expect(glz::to_underlying(Status::Running) == 0) << "Running should have underlying value 0\n";
      expect(glz::to_underlying(Status::Complete) == 1) << "Complete should have underlying value 1\n";
   };
   
   "cast_from_underlying"_test = [] {
      auto red = glz::cast_enum<Color>(0);
      expect(red.has_value()) << "Cast from 0 should succeed\n";
      expect(red.value() == Color::Red) << "Cast from 0 should give Red\n";
      
      auto green = glz::cast_enum<Color>(1);
      expect(green.has_value()) << "Cast from 1 should succeed\n";
      expect(green.value() == Color::Green) << "Cast from 1 should give Green\n";
      
      auto invalid = glz::cast_enum<Color>(5);
      expect(not invalid.has_value()) << "Cast from 5 should fail\n";
      
      auto pending = glz::cast_enum<Status>(-1);
      expect(pending.has_value()) << "Cast from -1 should succeed\n";
      expect(pending.value() == Status::Pending) << "Cast from -1 should give Pending\n";
   };
   
   "cast_from_string"_test = [] {
      auto red = glz::cast_enum<Color>("Red");
      expect(red.has_value()) << "Cast from 'Red' should succeed\n";
      expect(red.value() == Color::Red) << "Cast from 'Red' should give Red\n";
      
      auto green = glz::cast_enum<Color>("Green");
      expect(green.has_value()) << "Cast from 'Green' should succeed\n";
      expect(green.value() == Color::Green) << "Cast from 'Green' should give Green\n";
      
      auto invalid = glz::cast_enum<Color>("Yellow");
      expect(not invalid.has_value()) << "Cast from 'Yellow' should fail\n";
      
      auto pending = glz::cast_enum<Status>("Pending");
      expect(pending.has_value()) << "Cast from 'Pending' should succeed\n";
      expect(pending.value() == Status::Pending) << "Cast from 'Pending' should give Pending\n";
   };
   
   "to_string_conversion"_test = [] {
      expect(glz::enum_name(Color::Red) == "Red") << "Red should convert to 'Red'\n";
      expect(glz::enum_name(Color::Green) == "Green") << "Green should convert to 'Green'\n";
      expect(glz::enum_name(Color::Blue) == "Blue") << "Blue should convert to 'Blue'\n";
      
      expect(glz::enum_name(Status::Pending) == "Pending") << "Pending should convert to 'Pending'\n";
      expect(glz::enum_name(Status::Running) == "Running") << "Running should convert to 'Running'\n";
      expect(glz::enum_name(Status::Complete) == "Complete") << "Complete should convert to 'Complete'\n";
   };
};

// Index conversion tests
suite index_tests = [] {
   "enum_to_index"_test = [] {
      auto red_idx = glz::enum_to_index(Color::Red);
      expect(red_idx.has_value()) << "Red should have valid index\n";
      expect(red_idx.value() == 0) << "Red should have index 0\n";
      
      auto green_idx = glz::enum_to_index(Color::Green);
      expect(green_idx.has_value()) << "Green should have valid index\n";
      expect(green_idx.value() == 1) << "Green should have index 1\n";
      
      auto blue_idx = glz::enum_to_index(Color::Blue);
      expect(blue_idx.has_value()) << "Blue should have valid index\n";
      expect(blue_idx.value() == 2) << "Blue should have index 2\n";
   };
   
   "index_to_enum"_test = [] {
      auto color0 = glz::index_to_enum<Color>(0);
      expect(color0.has_value()) << "Index 0 should give valid Color\n";
      expect(color0.value() == Color::Red) << "Index 0 should give Red\n";
      
      auto color1 = glz::index_to_enum<Color>(1);
      expect(color1.has_value()) << "Index 1 should give valid Color\n";
      expect(color1.value() == Color::Green) << "Index 1 should give Green\n";
      
      auto color2 = glz::index_to_enum<Color>(2);
      expect(color2.has_value()) << "Index 2 should give valid Color\n";
      expect(color2.value() == Color::Blue) << "Index 2 should give Blue\n";
      
      auto invalid = glz::index_to_enum<Color>(5);
      expect(not invalid.has_value()) << "Index 5 should not give valid Color\n";
   };
};

// Boolean enum tests
suite boolean_enum_tests = [] {
   "boolean_enum_basic"_test = [] {
      constexpr auto bool_entries = glz::enums<Boolean>;
      expect(bool_entries.size() == 2) << "Boolean should have 2 enums\n";
      expect(bool_entries[0].first == Boolean::False) << "First entry should be False\n";
      expect(bool_entries[1].first == Boolean::True) << "Second entry should be True\n";
   };
   
   "boolean_enum_values"_test = [] {
      expect(glz::to_underlying(Boolean::False) == false) << "Boolean::False should have underlying value false\n";
      expect(glz::to_underlying(Boolean::True) == true) << "Boolean::True should have underlying value true\n";
      expect(glz::is_contiguous<Boolean>) << "Boolean should be contiguous\n";
   };
};

// Unscoped enum tests
suite unscoped_enum_tests = [] {
   "traffic_light_basic"_test = [] {
      constexpr auto traffic_entries = glz::enums<TrafficLight>;
      expect(traffic_entries.size() == 3) << "TrafficLight should have 3 enums\n";
      expect(traffic_entries[0].first == Stop) << "First entry should be Stop\n";
      expect(traffic_entries[1].first == Caution) << "Second entry should be Caution\n";
      expect(traffic_entries[2].first == Go) << "Third entry should be Go\n";
   };
   
   "direction_basic"_test = [] {
      constexpr auto dir_entries = glz::enums<Direction>;
      expect(dir_entries.size() == 4) << "Direction should have 4 enums\n";
      expect(dir_entries[0].first == North) << "First entry should be North\n";
      expect(dir_entries[1].first == East) << "Second entry should be East\n";
      expect(dir_entries[2].first == South) << "Third entry should be South\n";
      expect(dir_entries[3].first == West) << "Fourth entry should be West\n";
   };
};

// Sparse enum tests
suite sparse_enum_tests = [] {
   "sparse_enum_basic"_test = [] {
      constexpr auto sparse_entries = glz::enums<Sparse>;
      expect(sparse_entries.size() == 3) << "Sparse should have 3 enums\n";
      expect(sparse_entries[0].first == Sparse::First) << "First entry should be First\n";
      expect(sparse_entries[1].first == Sparse::Second) << "Second entry should be Second\n";
      expect(sparse_entries[2].first == Sparse::Third) << "Third entry should be Third\n";
   };
   
   "sparse_enum_values"_test = [] {
      expect(glz::to_underlying(Sparse::First) == 1) << "First should have underlying value 1\n";
      expect(glz::to_underlying(Sparse::Second) == 5) << "Second should have underlying value 5\n";
      expect(glz::to_underlying(Sparse::Third) == 10) << "Third should have underlying value 10\n";
      expect(not glz::is_contiguous<Sparse>) << "Sparse should not be contiguous\n";
   };
   
   "sparse_enum_contains"_test = [] {
      expect(glz::contains<Sparse>(Sparse::First)) << "Sparse should contain First\n";
      expect(glz::contains<Sparse>(1)) << "Sparse should contain underlying value 1\n";
      expect(not glz::contains<Sparse>(2)) << "Sparse should not contain underlying value 2\n";
      expect(not glz::contains<Sparse>(3)) << "Sparse should not contain underlying value 3\n";
      expect(glz::contains<Sparse>(5)) << "Sparse should contain underlying value 5\n";
   };
};

// Edge case tests
suite edge_case_tests = [] {
   "invalid_enum_value_to_string"_test = [] {
      // Test with an enum value that might be invalid
      auto invalid_color = static_cast<Color>(99);
      auto str_result = glz::enum_name(invalid_color);
      expect(str_result.empty()) << "Invalid enum value should return empty string\n";
   };
   
   "invalid_enum_to_index"_test = [] {
      auto invalid_color = static_cast<Color>(99);
      auto idx_result = glz::enum_to_index(invalid_color);
      expect(not idx_result.has_value()) << "Invalid enum value should not have valid index\n";
   };
};

int main() {}
