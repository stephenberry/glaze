// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp"
#include "glaze/reflection/enum_reflect.hpp"
#include "ut/ut.hpp"
#include <string>
#include <vector>

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
      constexpr auto color_values = glz::enum_values<Color>;
      expect(color_values.size() == 3) << "Color should have 3 values\n";
      expect(color_values[0] == Color::Red) << "First value should be Red\n";
      expect(color_values[1] == Color::Green) << "Second value should be Green\n";
      expect(color_values[2] == Color::Blue) << "Third value should be Blue\n";
   };
   
   "names_extraction"_test = [] {
      constexpr auto color_names = glz::enum_names<Color>;
      expect(color_names.size() == 3) << "Color should have 3 names\n";
      expect(color_names[0] == "Red") << "First name should be 'Red'\n";
      expect(color_names[1] == "Green") << "Second name should be 'Green'\n";
      expect(color_names[2] == "Blue") << "Third name should be 'Blue'\n";
   };
   
   "min_max_values"_test = [] {
      expect(glz::enum_min<Color> == Color::Red) << "Min Color should be Red\n";
      expect(glz::enum_max<Color> == Color::Blue) << "Max Color should be Blue\n";
      expect(glz::enum_min<Status> == Status::Pending) << "Min Status should be Pending\n";
      expect(glz::enum_max<Status> == Status::Complete) << "Max Status should be Complete\n";
   };
   
   "count_values"_test = [] {
      expect(glz::enum_count<Color> == 3) << "Color count should be 3\n";
      expect(glz::enum_count<Status> == 3) << "Status count should be 3\n";
      expect(glz::enum_count<TrafficLight> == 3) << "TrafficLight count should be 3\n";
   };
};

// Contiguous enum tests
suite contiguous_tests = [] {
   "is_contiguous_check"_test = [] {
      expect(glz::enum_is_contiguous<Color>) << "Color should be contiguous\n";
      expect(glz::enum_is_contiguous<Status>) << "Status should be contiguous\n";
      expect(glz::enum_is_contiguous<TrafficLight>) << "TrafficLight should be contiguous\n";
      expect(glz::enum_is_contiguous<Direction>) << "Direction should be contiguous\n";
      expect(not glz::enum_is_contiguous<Sparse>) << "Sparse should not be contiguous\n";
      expect(glz::enum_is_contiguous<Boolean>) << "Boolean should be contiguous\n";
   };
   
   "contiguous_enum_check"_test = [] {
      expect(glz::enum_is_contiguous<Color>) << "Color should be contiguous enum\n";
      expect(glz::enum_is_contiguous<TrafficLight>) << "TrafficLight should be contiguous enum\n";
      expect(not glz::enum_is_contiguous<Sparse>) << "Sparse should not be contiguous enum\n";
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
      auto red = glz::enum_cast<Color>(0);
      expect(red.has_value()) << "Cast from 0 should succeed\n";
      expect(red.value() == Color::Red) << "Cast from 0 should give Red\n";
      
      auto green = glz::enum_cast<Color>(1);
      expect(green.has_value()) << "Cast from 1 should succeed\n";
      expect(green.value() == Color::Green) << "Cast from 1 should give Green\n";
      
      auto invalid = glz::enum_cast<Color>(5);
      expect(not invalid.has_value()) << "Cast from 5 should fail\n";
      
      auto pending = glz::enum_cast<Status>(-1);
      expect(pending.has_value()) << "Cast from -1 should succeed\n";
      expect(pending.value() == Status::Pending) << "Cast from -1 should give Pending\n";
   };
   
   "cast_from_string"_test = [] {
      auto red = glz::enum_cast<Color>("Red");
      expect(red.has_value()) << "Cast from 'Red' should succeed\n";
      expect(red.value() == Color::Red) << "Cast from 'Red' should give Red\n";
      
      auto green = glz::enum_cast<Color>("Green");
      expect(green.has_value()) << "Cast from 'Green' should succeed\n";
      expect(green.value() == Color::Green) << "Cast from 'Green' should give Green\n";
      
      auto invalid = glz::enum_cast<Color>("Yellow");
      expect(not invalid.has_value()) << "Cast from 'Yellow' should fail\n";
      
      auto pending = glz::enum_cast<Status>("Pending");
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
      expect(glz::enum_is_contiguous<Boolean>) << "Boolean should be contiguous\n";
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
      expect(not glz::enum_is_contiguous<Sparse>) << "Sparse should not be contiguous\n";
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

// ============== NEW FEATURE TESTS ==============

// Bitflag enum for testing
enum class Permissions : unsigned {
   None = 0,
   Read = 1 << 0,
   Write = 1 << 1,
   Execute = 1 << 2,
   All = Read | Write | Execute
};

// Enable bitwise operators for Permissions
constexpr Permissions operator|(Permissions a, Permissions b) {
   return static_cast<Permissions>(
      static_cast<unsigned>(a) | static_cast<unsigned>(b)
   );
}

constexpr Permissions operator&(Permissions a, Permissions b) {
   return static_cast<Permissions>(
      static_cast<unsigned>(a) & static_cast<unsigned>(b)
   );
}

constexpr Permissions operator~(Permissions a) {
   return static_cast<Permissions>(~static_cast<unsigned>(a));
}

constexpr Permissions& operator|=(Permissions& a, Permissions b) {
   a = a | b;
   return a;
}

constexpr Permissions& operator&=(Permissions& a, Permissions b) {
   a = a & b;
   return a;
}

// Another bitflag enum for testing
enum class FileMode : std::uint16_t {
   None = 0,
   UserRead = 0400,
   UserWrite = 0200,
   UserExecute = 0100,
   GroupRead = 040,
   GroupWrite = 020,
   GroupExecute = 010,
   OtherRead = 04,
   OtherWrite = 02,
   OtherExecute = 01
};

constexpr FileMode operator|(FileMode a, FileMode b) {
   return static_cast<FileMode>(
      static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b)
   );
}

constexpr FileMode operator&(FileMode a, FileMode b) {
   return static_cast<FileMode>(
      static_cast<std::uint16_t>(a) & static_cast<std::uint16_t>(b)
   );
}

constexpr FileMode operator~(FileMode a) {
   return static_cast<FileMode>(~static_cast<std::uint16_t>(a));
}

constexpr FileMode& operator|=(FileMode& a, FileMode b) {
   a = a | b;
   return a;
}

constexpr FileMode& operator&=(FileMode& a, FileMode b) {
   a = a & b;
   return a;
}

// Bitflag enum tests
suite bitflag_tests = [] {
   "is_bitflag_detection"_test = [] {
      expect(glz::is_bitflag<Permissions>) << "Permissions should be detected as bitflag enum\n";
      expect(glz::is_bitflag<FileMode>) << "FileMode should be detected as bitflag enum\n";
      expect(not glz::is_bitflag<Color>) << "Color should not be detected as bitflag enum\n";
      expect(not glz::is_bitflag<Status>) << "Status should not be detected as bitflag enum\n";
   };
   
   "bitflag_concept"_test = [] {
      expect(glz::bit_flag_enum<Permissions>) << "Permissions should satisfy bit_flag_enum concept\n";
      expect(glz::bit_flag_enum<FileMode>) << "FileMode should satisfy bit_flag_enum concept\n";
      expect(not glz::bit_flag_enum<Color>) << "Color should not satisfy bit_flag_enum concept\n";
   };
   
   "contains_bitflag"_test = [] {
      auto perms = Permissions::Read | Permissions::Write;
      expect(glz::contains_bitflag(perms, Permissions::Read)) << "Combined permissions should contain Read\n";
      expect(glz::contains_bitflag(perms, Permissions::Write)) << "Combined permissions should contain Write\n";
      expect(not glz::contains_bitflag(perms, Permissions::Execute)) << "Combined permissions should not contain Execute\n";
      
      expect(glz::contains_bitflag(Permissions::All, Permissions::Read)) << "All should contain Read\n";
      expect(glz::contains_bitflag(Permissions::All, Permissions::Write)) << "All should contain Write\n";
      expect(glz::contains_bitflag(Permissions::All, Permissions::Execute)) << "All should contain Execute\n";
   };
   
   "to_string_bitflag"_test = [] {
      auto none = glz::enum_to_string_bitflag(Permissions::None);
      expect(none == "None") << "None should convert to 'None'\n";
      
      auto read = glz::enum_to_string_bitflag(Permissions::Read);
      expect(read == "Read") << "Read should convert to 'Read'\n";
      
      auto read_write = glz::enum_to_string_bitflag(Permissions::Read | Permissions::Write);
      expect(read_write == "Read | Write") << "Read|Write should convert to 'Read | Write'\n";
      
      auto all = glz::enum_to_string_bitflag(Permissions::All);
      // Note: All is defined as Read | Write | Execute, so it will be expanded
      expect(all == "Read | Write | Execute | All") << "All should list all contained flags\n";
   };
};

// Next/Previous value tests
suite navigation_tests = [] {
   "next_value"_test = [] {
      auto next_red = glz::enum_next_value(Color::Red);
      expect(next_red.has_value()) << "Red should have next value\n";
      expect(next_red.value() == Color::Green) << "Next after Red should be Green\n";
      
      auto next_green = glz::enum_next_value(Color::Green);
      expect(next_green.has_value()) << "Green should have next value\n";
      expect(next_green.value() == Color::Blue) << "Next after Green should be Blue\n";
      
      auto next_blue = glz::enum_next_value(Color::Blue);
      expect(not next_blue.has_value()) << "Blue should not have next value\n";
   };
   
   "prev_value"_test = [] {
      auto prev_blue = glz::enum_prev_value(Color::Blue);
      expect(prev_blue.has_value()) << "Blue should have previous value\n";
      expect(prev_blue.value() == Color::Green) << "Previous before Blue should be Green\n";
      
      auto prev_green = glz::enum_prev_value(Color::Green);
      expect(prev_green.has_value()) << "Green should have previous value\n";
      expect(prev_green.value() == Color::Red) << "Previous before Green should be Red\n";
      
      auto prev_red = glz::enum_prev_value(Color::Red);
      expect(not prev_red.has_value()) << "Red should not have previous value\n";
   };
   
   "next_value_sparse"_test = [] {
      auto next_first = glz::enum_next_value(Sparse::First);
      expect(next_first.has_value()) << "First should have next value\n";
      expect(next_first.value() == Sparse::Second) << "Next after First should be Second\n";
      
      auto next_second = glz::enum_next_value(Sparse::Second);
      expect(next_second.has_value()) << "Second should have next value\n";
      expect(next_second.value() == Sparse::Third) << "Next after Second should be Third\n";
   };
};

// Circular navigation tests
suite circular_navigation_tests = [] {
   "next_value_circular"_test = [] {
      auto next_red = glz::enum_next_value_circular(Color::Red);
      expect(next_red == Color::Green) << "Next after Red should be Green\n";
      
      auto next_green = glz::enum_next_value_circular(Color::Green);
      expect(next_green == Color::Blue) << "Next after Green should be Blue\n";
      
      auto next_blue = glz::enum_next_value_circular(Color::Blue);
      expect(next_blue == Color::Red) << "Next after Blue should wrap to Red\n";
   };
   
   "prev_value_circular"_test = [] {
      auto prev_blue = glz::enum_prev_value_circular(Color::Blue);
      expect(prev_blue == Color::Green) << "Previous before Blue should be Green\n";
      
      auto prev_green = glz::enum_prev_value_circular(Color::Green);
      expect(prev_green == Color::Red) << "Previous before Green should be Red\n";
      
      auto prev_red = glz::enum_prev_value_circular(Color::Red);
      expect(prev_red == Color::Blue) << "Previous before Red should wrap to Blue\n";
   };
   
   "circular_navigation_status"_test = [] {
      auto next_complete = glz::enum_next_value_circular(Status::Complete);
      expect(next_complete == Status::Pending) << "Next after Complete should wrap to Pending\n";
      
      auto prev_pending = glz::enum_prev_value_circular(Status::Pending);
      expect(prev_pending == Status::Complete) << "Previous before Pending should wrap to Complete\n";
   };
};

// For each utility test
suite for_each_tests = [] {
   "for_each_color"_test = [] {
      std::vector<Color> colors;
      glz::enum_for_each<Color>([&colors](Color c) {
         colors.push_back(c);
      });
      
      expect(colors.size() == 3) << "for_each should iterate over all 3 colors\n";
      expect(colors[0] == Color::Red) << "First color should be Red\n";
      expect(colors[1] == Color::Green) << "Second color should be Green\n";
      expect(colors[2] == Color::Blue) << "Third color should be Blue\n";
   };
   
   "for_each_count"_test = [] {
      int count = 0;
      glz::enum_for_each<Status>([&count](Status) {
         count++;
      });
      expect(count == 3) << "for_each should call function 3 times for Status\n";
   };
   
   "for_each_names"_test = [] {
      std::string combined;
      glz::enum_for_each<Color>([&combined](Color c) {
         if (!combined.empty()) combined += ", ";
         combined += std::string(glz::enum_name(c));
      });
      expect(combined == "Red, Green, Blue") << "for_each should iterate in order\n";
   };
};

// Edge case: single value enum within range
enum class SingleInRange { OnlyValue = 0 };

// Improved is_contiguous test
suite improved_contiguous_tests = [] {
   "is_contiguous_efficiency"_test = [] {
      expect(glz::enum_is_contiguous<Color>) << "Color should be contiguous (0,1,2)\n";
      expect(glz::enum_is_contiguous<Status>) << "Status should be contiguous (-1,0,1)\n";
      expect(not glz::enum_is_contiguous<Sparse>) << "Sparse should not be contiguous (1,5,10)\n";
      expect(glz::enum_is_contiguous<Direction>) << "Direction should be contiguous (0,1,2,3)\n";
      expect(glz::enum_is_contiguous<Boolean>) << "Boolean should be contiguous (false,true)\n";
      
      expect(glz::enum_is_contiguous<SingleInRange>) << "Single value enum should be contiguous\n";
   };
};

// Validation tests
suite validation_tests = [] {
   "validate_enum_reflection"_test = [] {
      // These should compile without issues
      glz::validate_enum_reflection<Color>();
      glz::validate_enum_reflection<Status>();
      glz::validate_enum_reflection<TrafficLight>();
      glz::validate_enum_reflection<Sparse>();
      glz::validate_enum_reflection<Direction>();
      glz::validate_enum_reflection<Boolean>();
      
      // Test that it works for our new enums
      glz::validate_enum_reflection<Permissions>();
      glz::validate_enum_reflection<FileMode>();
      
      expect(true) << "All enum validations should pass\n";
   };
   
   "empty_enum_count"_test = [] {
      // Empty enum should have count of 0
      expect(glz::enum_count<Empty> == 0) << "Empty enum should have count of 0\n";
      
      // This would fail validation if we called validate_enum_reflection<Empty>()
      // but we won't call it to avoid compilation error in the test
   };
};

// ============== TESTS FOR ADDITIONAL IMPROVEMENTS ==============

// Container wrapper tests
suite container_wrapper_tests = [] {
   "enum_array_basic"_test = [] {
      glz::enum_array<Color, int> scores;
      scores[Color::Red] = 100;
      scores[Color::Green] = 200;
      scores[Color::Blue] = 300;
      
      expect(scores[Color::Red] == 100) << "Red should have score 100\n";
      expect(scores[Color::Green] == 200) << "Green should have score 200\n";
      expect(scores[Color::Blue] == 300) << "Blue should have score 300\n";
   };
   
   "enum_array_initialization"_test = [] {
      glz::enum_array<Color, int> scores(42);
      
      expect(scores[Color::Red] == 42) << "Red should be initialized to 42\n";
      expect(scores[Color::Green] == 42) << "Green should be initialized to 42\n";
      expect(scores[Color::Blue] == 42) << "Blue should be initialized to 42\n";
   };
   
   "enum_array_at"_test = [] {
      glz::enum_array<Color, std::string> names;
      names[Color::Red] = "Rouge";
      names[Color::Green] = "Vert";
      names[Color::Blue] = "Bleu";
      
      expect(names.at(Color::Red) == "Rouge") << "at() should work for Red\n";
      expect(names.at(Color::Blue) == "Bleu") << "at() should work for Blue\n";
   };
   
   "enum_bitset_basic"_test = [] {
      glz::enum_bitset<Color> active;
      active.set(Color::Red);
      active.set(Color::Blue);
      
      expect(active.test(Color::Red)) << "Red should be set\n";
      expect(!active.test(Color::Green)) << "Green should not be set\n";
      expect(active.test(Color::Blue)) << "Blue should be set\n";
      expect(active.count() == 2) << "Should have 2 bits set\n";
   };
   
   "enum_bitset_initializer_list"_test = [] {
      glz::enum_bitset<Color> active{Color::Red, Color::Blue};
      
      expect(active.test(Color::Red)) << "Red should be set from initializer\n";
      expect(!active.test(Color::Green)) << "Green should not be set\n";
      expect(active.test(Color::Blue)) << "Blue should be set from initializer\n";
   };
   
   "enum_bitset_to_enum_string"_test = [] {
      glz::enum_bitset<Color> active{Color::Red, Color::Blue};
      auto str = active.to_enum_string();
      
      expect(str == "Red|Blue") << "Should convert to 'Red|Blue'\n";
   };
   
   "enum_bitset_operations"_test = [] {
      glz::enum_bitset<Status> flags;
      flags.set(Status::Pending);
      flags.set(Status::Running);
      flags.flip(Status::Running);
      flags.reset(Status::Pending);
      
      expect(!flags.test(Status::Pending)) << "Pending should be reset\n";
      expect(!flags.test(Status::Running)) << "Running should be flipped off\n";
      expect(!flags.test(Status::Complete)) << "Complete should never have been set\n";
      expect(flags.none()) << "All flags should be off\n";
   };
};

// Advanced bitflag tests
suite advanced_bitflag_tests = [] {
   "cast_bitflag_single"_test = [] {
      auto perm = glz::enum_cast_bitflag<Permissions>("Read");
      expect(perm.has_value()) << "Should parse single flag\n";
      expect(*perm == Permissions::Read) << "Should parse to Read\n";
      
      auto write = glz::enum_cast_bitflag<Permissions>("Write");
      expect(write.has_value()) << "Should parse Write\n";
      expect(*write == Permissions::Write) << "Should be Write\n";
   };
   
   "cast_bitflag_multiple"_test = [] {
      auto perms = glz::enum_cast_bitflag<Permissions>("Read|Write");
      expect(perms.has_value()) << "Should parse multiple flags\n";
      expect(*perms == (Permissions::Read | Permissions::Write)) << "Should be Read|Write\n";
      
      auto all = glz::enum_cast_bitflag<Permissions>("Read|Write|Execute");
      expect(all.has_value()) << "Should parse all flags\n";
      expect(*all == (Permissions::Read | Permissions::Write | Permissions::Execute)) << "Should be all permissions\n";
   };
   
   "cast_bitflag_whitespace"_test = [] {
      auto perms = glz::enum_cast_bitflag<Permissions>("Read | Write | Execute");
      expect(perms.has_value()) << "Should handle whitespace\n";
      expect(*perms == (Permissions::Read | Permissions::Write | Permissions::Execute)) << "Should parse with spaces\n";
   };
   
   "cast_bitflag_invalid"_test = [] {
      auto invalid = glz::enum_cast_bitflag<Permissions>("InvalidFlag");
      expect(!invalid.has_value()) << "Should fail for invalid flag\n";
      
      auto partial = glz::enum_cast_bitflag<Permissions>("Read|Invalid|Write");
      expect(!partial.has_value()) << "Should fail if any flag is invalid\n";
   };
};

// Extended navigation tests
suite extended_navigation_tests = [] {
   "next_value_with_count"_test = [] {
      auto next2 = glz::enum_next_value(Color::Red, 2);
      expect(next2.has_value()) << "Should get value 2 steps away\n";
      expect(*next2 == Color::Blue) << "2 steps from Red should be Blue\n";
      
      auto prev1 = glz::enum_next_value(Color::Blue, -1);
      expect(prev1.has_value()) << "Should get value -1 steps away\n";
      expect(*prev1 == Color::Green) << "-1 step from Blue should be Green\n";
   };
   
   "next_value_out_of_bounds"_test = [] {
      auto beyond = glz::enum_next_value(Color::Blue, 5);
      expect(!beyond.has_value()) << "5 steps from Blue should be out of bounds\n";
      
      auto before = glz::enum_next_value(Color::Red, -5);
      expect(!before.has_value()) << "-5 steps from Red should be out of bounds\n";
   };
   
   "distance_between_enums"_test = [] {
      auto dist1 = glz::distance(Color::Red, Color::Blue);
      expect(dist1.has_value()) << "Should calculate distance\n";
      expect(*dist1 == 2) << "Distance from Red to Blue should be 2\n";
      
      auto dist2 = glz::distance(Color::Blue, Color::Red);
      expect(dist2.has_value()) << "Should calculate reverse distance\n";
      expect(*dist2 == -2) << "Distance from Blue to Red should be -2\n";
      
      auto dist3 = glz::distance(Status::Pending, Status::Complete);
      expect(dist3.has_value()) << "Should work for Status enum\n";
      expect(*dist3 == 2) << "Distance from Pending to Complete should be 2\n";
   };
};

// Additional utility tests
suite additional_utility_tests = [] {
   "enum_size_bits"_test = [] {
      // Color has 3 values, needs 2 bits (can represent 0-3)
      expect(glz::enum_size_bits<Color> == 2) << "Color should need 2 bits\n";
      
      // Direction has 4 values, needs 2 bits (can represent 0-3)
      expect(glz::enum_size_bits<Direction> == 2) << "Direction should need 2 bits\n";
      
      // Boolean has 2 values, needs 1 bit
      expect(glz::enum_size_bits<Boolean> == 1) << "Boolean should need 1 bit\n";
      
      // Empty has 0 values
      expect(glz::enum_size_bits<Empty> == 0) << "Empty should need 0 bits\n";
   };
   
   "from_string_nocase"_test = [] {
      auto red1 = glz::from_string_nocase<Color>("red");
      expect(red1.has_value()) << "Should match 'red' case-insensitive\n";
      expect(*red1 == Color::Red) << "Should be Red\n";
      
      auto red2 = glz::from_string_nocase<Color>("RED");
      expect(red2.has_value()) << "Should match 'RED' case-insensitive\n";
      expect(*red2 == Color::Red) << "Should be Red\n";
      
      auto red3 = glz::from_string_nocase<Color>("ReD");
      expect(red3.has_value()) << "Should match 'ReD' case-insensitive\n";
      expect(*red3 == Color::Red) << "Should be Red\n";
      
      auto invalid = glz::from_string_nocase<Color>("purple");
      expect(!invalid.has_value()) << "Should not match 'purple'\n";
   };
};

// Test enum with manually defined bitwise operators
enum class TestFlags : unsigned {
   None = 0,
   Flag1 = 1,
   Flag2 = 2,
   Flag3 = 4
};

// Define bitwise operators for TestFlags (no macro needed)
constexpr TestFlags operator|(TestFlags a, TestFlags b) noexcept {
   return static_cast<TestFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr TestFlags operator&(TestFlags a, TestFlags b) noexcept {
   return static_cast<TestFlags>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
constexpr TestFlags operator^(TestFlags a, TestFlags b) noexcept {
   return static_cast<TestFlags>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
}
constexpr TestFlags operator~(TestFlags a) noexcept {
   return static_cast<TestFlags>(~static_cast<unsigned>(a));
}
constexpr TestFlags& operator|=(TestFlags& a, TestFlags b) noexcept {
   return a = a | b;
}
constexpr TestFlags& operator&=(TestFlags& a, TestFlags b) noexcept {
   return a = a & b;
}
constexpr TestFlags& operator^=(TestFlags& a, TestFlags b) noexcept {
   return a = a ^ b;
}

suite bitwise_operator_tests = [] {
   "bitwise_operations"_test = [] {
      auto combined = TestFlags::Flag1 | TestFlags::Flag2;
      expect(static_cast<unsigned>(combined) == 3) << "OR should work\n";
      
      auto masked = combined & TestFlags::Flag1;
      expect(masked == TestFlags::Flag1) << "AND should work\n";
      
      auto xored = TestFlags::Flag1 ^ TestFlags::Flag3;
      expect(static_cast<unsigned>(xored) == 5) << "XOR should work\n";
      
      auto inverted = ~TestFlags::Flag1;
      expect(static_cast<unsigned>(inverted) == ~1u) << "NOT should work\n";
      
      auto flags = TestFlags::None;
      flags |= TestFlags::Flag1;
      flags |= TestFlags::Flag2;
      expect(static_cast<unsigned>(flags) == 3) << "Compound OR should work\n";
   };
};

// ============== STRUCT-ENUM INTEGRATION TESTS ==============

// Test structures for comprehensive enum integration
struct SimpleEnumStruct {
   Color color{Color::Red};
   Status status{Status::Pending};
   int value{42};
};

struct VectorEnumStruct {
   std::vector<Color> colors;
   std::vector<Status> statuses;
   std::string name;
};

struct MapEnumStruct {
   std::map<std::string, Color> color_map;
   std::map<Color, int> score_map;
   std::map<Status, std::string> status_messages;
};

struct NestedEnumStruct {
   struct Inner {
      Color color{Color::Green};
      TrafficLight light{Stop};
   } inner;
   
   VectorEnumStruct vectors;
   std::optional<Status> optional_status;
};

struct ArrayEnumStruct {
   std::array<Color, 3> color_array{Color::Red, Color::Green, Color::Blue};
   std::array<Status, 2> status_array{Status::Pending, Status::Running};
   Direction direction{North};
};

struct ComplexEnumStruct {
   Color primary_color{Color::Red};
   std::vector<Color> secondary_colors;
   std::map<std::string, Status> task_statuses;
   std::optional<Direction> direction;
   std::array<TrafficLight, 2> traffic_lights{Stop, Go};
   TestFlags flags{TestFlags::Flag1 | TestFlags::Flag2};
};

// glz::meta for the test structures
template <>
struct glz::meta<SimpleEnumStruct> {
   using T = SimpleEnumStruct;
   static constexpr auto value = object(&T::color, &T::status, &T::value);
};

template <>
struct glz::meta<VectorEnumStruct> {
   using T = VectorEnumStruct;
   static constexpr auto value = object(&T::colors, &T::statuses, &T::name);
};

template <>
struct glz::meta<MapEnumStruct> {
   using T = MapEnumStruct;
   static constexpr auto value = object(&T::color_map, &T::score_map, &T::status_messages);
};

template <>
struct glz::meta<NestedEnumStruct::Inner> {
   using T = NestedEnumStruct::Inner;
   static constexpr auto value = object(&T::color, &T::light);
};

template <>
struct glz::meta<NestedEnumStruct> {
   using T = NestedEnumStruct;
   static constexpr auto value = object(&T::inner, &T::vectors, &T::optional_status);
};

template <>
struct glz::meta<ArrayEnumStruct> {
   using T = ArrayEnumStruct;
   static constexpr auto value = object(&T::color_array, &T::status_array, &T::direction);
};

template <>
struct glz::meta<ComplexEnumStruct> {
   using T = ComplexEnumStruct;
   static constexpr auto value = object(&T::primary_color, 
                                       &T::secondary_colors,
                                       &T::task_statuses,
                                       &T::direction,
                                       &T::traffic_lights,
                                       &T::flags);
};

suite struct_enum_tests = [] {
   "simple_struct_with_enums"_test = [] {
      SimpleEnumStruct s;
      s.color = Color::Blue;
      s.status = Status::Complete;
      s.value = 100;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(s, json));
      expect(json == R"({"color":"Blue","status":"Complete","value":100})") << "Simple struct should serialize enums as strings\n";
      
      SimpleEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color == Color::Blue) << "Color should be Blue\n";
      expect(parsed.status == Status::Complete) << "Status should be Complete\n";
      expect(parsed.value == 100) << "Value should be 100\n";
   };
   
   "vector_of_enums_in_struct"_test = [] {
      VectorEnumStruct v;
      v.colors = {Color::Red, Color::Green, Color::Blue};
      v.statuses = {Status::Pending, Status::Running, Status::Complete};
      v.name = "test";
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(v, json));
      expect(json == R"({"colors":["Red","Green","Blue"],"statuses":["Pending","Running","Complete"],"name":"test"})") 
         << "Vector of enums should serialize as array of strings\n";
      
      VectorEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.colors.size() == 3) << "Should have 3 colors\n";
      expect(parsed.colors[0] == Color::Red) << "First color should be Red\n";
      expect(parsed.colors[1] == Color::Green) << "Second color should be Green\n";
      expect(parsed.colors[2] == Color::Blue) << "Third color should be Blue\n";
      expect(parsed.statuses.size() == 3) << "Should have 3 statuses\n";
      expect(parsed.name == "test") << "Name should be 'test'\n";
   };
   
   "map_with_enum_keys_and_values"_test = [] {
      MapEnumStruct m;
      m.color_map = {{"primary", Color::Red}, {"secondary", Color::Blue}};
      m.score_map = {{Color::Red, 100}, {Color::Green, 200}};
      m.status_messages = {{Status::Pending, "Waiting"}, {Status::Complete, "Done"}};
      
      std::string json;
      expect(not glz::write_json(m, json));
      
      // Parse it back
      MapEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color_map["primary"] == Color::Red) << "Primary should be Red\n";
      expect(parsed.color_map["secondary"] == Color::Blue) << "Secondary should be Blue\n";
      expect(parsed.score_map[Color::Red] == 100) << "Red score should be 100\n";
      expect(parsed.score_map[Color::Green] == 200) << "Green score should be 200\n";
      expect(parsed.status_messages[Status::Pending] == "Waiting") << "Pending message should be 'Waiting'\n";
      expect(parsed.status_messages[Status::Complete] == "Done") << "Complete message should be 'Done'\n";
   };
   
   "nested_struct_with_enums"_test = [] {
      NestedEnumStruct n;
      n.inner.color = Color::Blue;
      n.inner.light = Go;
      n.vectors.colors = {Color::Red, Color::Blue};
      n.vectors.statuses = {Status::Running};
      n.vectors.name = "nested";
      n.optional_status = Status::Complete;
      
      std::string json;
      expect(not glz::write_json(n, json));
      
      NestedEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.inner.color == Color::Blue) << "Inner color should be Blue\n";
      expect(parsed.inner.light == Go) << "Inner light should be Go\n";
      expect(parsed.vectors.colors.size() == 2) << "Should have 2 colors\n";
      expect(parsed.vectors.colors[0] == Color::Red) << "First color should be Red\n";
      expect(parsed.vectors.statuses[0] == Status::Running) << "First status should be Running\n";
      expect(parsed.optional_status.has_value()) << "Optional status should have value\n";
      expect(*parsed.optional_status == Status::Complete) << "Optional status should be Complete\n";
   };
   
   "array_of_enums_in_struct"_test = [] {
      ArrayEnumStruct a;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(a, json));
      expect(json == R"({"color_array":["Red","Green","Blue"],"status_array":["Pending","Running"],"direction":"North"})") 
         << "Array of enums should serialize correctly\n";
      
      ArrayEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color_array[0] == Color::Red) << "First color should be Red\n";
      expect(parsed.color_array[1] == Color::Green) << "Second color should be Green\n";
      expect(parsed.color_array[2] == Color::Blue) << "Third color should be Blue\n";
      expect(parsed.status_array[0] == Status::Pending) << "First status should be Pending\n";
      expect(parsed.status_array[1] == Status::Running) << "Second status should be Running\n";
      expect(parsed.direction == North) << "Direction should be North\n";
   };
   
   "complex_struct_comprehensive"_test = [] {
      ComplexEnumStruct c;
      c.primary_color = Color::Green;
      c.secondary_colors = {Color::Red, Color::Blue};
      c.task_statuses = {{"task1", Status::Running}, {"task2", Status::Complete}};
      c.direction = East;
      c.traffic_lights[0] = Caution;
      c.traffic_lights[1] = Go;
      c.flags = TestFlags::Flag1 | TestFlags::Flag3;
      
      std::string json;
      expect(not glz::write_json(c, json));
      
      ComplexEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.primary_color == Color::Green) << "Primary color should be Green\n";
      expect(parsed.secondary_colors.size() == 2) << "Should have 2 secondary colors\n";
      expect(parsed.secondary_colors[0] == Color::Red) << "First secondary should be Red\n";
      expect(parsed.task_statuses["task1"] == Status::Running) << "Task1 should be Running\n";
      expect(parsed.task_statuses["task2"] == Status::Complete) << "Task2 should be Complete\n";
      expect(parsed.direction.has_value() && *parsed.direction == East) << "Direction should be East\n";
      expect(parsed.traffic_lights[0] == Caution) << "First light should be Caution\n";
      expect(parsed.traffic_lights[1] == Go) << "Second light should be Go\n";
      expect(parsed.flags == (TestFlags::Flag1 | TestFlags::Flag3)) << "Flags should be Flag1 | Flag3\n";
   };
   
   "empty_containers_with_enums"_test = [] {
      VectorEnumStruct v;
      v.name = "empty";
      
      std::string json;
      expect(not glz::write_json(v, json));
      expect(json == R"({"colors":[],"statuses":[],"name":"empty"})") << "Empty vectors should serialize as empty arrays\n";
      
      VectorEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.colors.empty()) << "Colors should be empty\n";
      expect(parsed.statuses.empty()) << "Statuses should be empty\n";
      expect(parsed.name == "empty") << "Name should be 'empty'\n";
   };
   
   "null_optional_enum_in_struct"_test = [] {
      NestedEnumStruct n;
      n.inner.color = Color::Red;
      n.vectors.name = "test";
      // optional_status is not set (null)
      
      std::string json;
      expect(not glz::write_json(n, json));
      
      // Parse it back to check round-trip works
      NestedEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(!parsed.optional_status.has_value()) << "Optional status should not have value\n";
      
      // Now test with a value
      n.optional_status = Status::Running;
      expect(not glz::write_json(n, json));
      ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully with value\n";
      expect(parsed.optional_status.has_value()) << "Optional status should have value\n";
      expect(*parsed.optional_status == Status::Running) << "Optional status should be Running\n";
   };
   
   "backward_compatibility_numeric_enums"_test = [] {
      // Test that we can still parse numeric enum values
      // Status: Pending = -1, Running = 0, Complete = 1
      std::string json = R"({"color":1,"status":1,"value":42})";
      
      SimpleEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse numeric enum values successfully\n";
      expect(parsed.color == Color::Green) << "Color 1 should be Green\n";
      expect(parsed.status == Status::Complete) << "Status 1 should be Complete\n";
      expect(parsed.value == 42) << "Value should be 42\n";
   };
   
   "mixed_numeric_and_string_enums"_test = [] {
      // Test parsing a vector with mixed numeric and string enum values
      // Status: Pending = -1, Running = 0, Complete = 1
      std::string json = R"({"colors":[0,"Green",2],"statuses":["Pending",0,"Complete"],"name":"mixed"})";
      
      VectorEnumStruct parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse mixed enum representations successfully\n";
      expect(parsed.colors.size() == 3) << "Should have 3 colors\n";
      expect(parsed.colors[0] == Color::Red) << "First color (0) should be Red\n";
      expect(parsed.colors[1] == Color::Green) << "Second color ('Green') should be Green\n";
      expect(parsed.colors[2] == Color::Blue) << "Third color (2) should be Blue\n";
      expect(parsed.statuses.size() == 3) << "Should have 3 statuses\n";
      expect(parsed.statuses[0] == Status::Pending) << "First status should be Pending\n";
      expect(parsed.statuses[1] == Status::Running) << "Second status (0) should be Running\n";
      expect(parsed.statuses[2] == Status::Complete) << "Third status should be Complete\n";
   };
};

// ============== PURE REFLECTION TESTS (NO glz::meta) ==============

// Pure reflection structs - NO glz::meta definitions!
struct PureReflectSimple {
   Color color{Color::Red};
   Status status{Status::Pending};
   int32_t count{42};
   double value{3.14};
};

struct PureReflectNested {
   struct InnerData {
      TrafficLight light{Stop};
      Direction direction{North};
      bool active{true};
   };
   
   InnerData data;
   Color primary_color{Color::Blue};
   std::optional<Status> optional_status;
};

struct PureReflectContainers {
   std::vector<Color> color_list;
   std::array<Status, 3> status_array{Status::Pending, Status::Running, Status::Complete};
   std::map<std::string, Direction> directions;
   std::optional<TestFlags> flags;
};

struct PureReflectComplex {
   Color foreground{Color::Green};
   Color background{Color::Blue};
   std::vector<TrafficLight> lights;
   std::map<Color, std::string> color_names;
   std::array<Direction, 4> compass{North, East, South, West};
   TestFlags permissions{TestFlags::Flag1};
   std::optional<Sparse> sparse_value;
};

struct PureReflectMixed {
   // Mix of enum and non-enum types
   std::string name{"test"};
   Color theme{Color::Blue};
   int32_t version{1};
   Status state{Status::Running};
   std::vector<double> scores{1.0, 2.0, 3.0};
   Direction heading{East};
   bool enabled{true};
};

suite pure_reflection_enum_tests = [] {
   "pure_reflect_simple_struct"_test = [] {
      PureReflectSimple obj;
      obj.color = Color::Green;
      obj.status = Status::Complete;
      obj.count = 100;
      obj.value = 2.718;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      // Verify the JSON contains enum names, not numbers
      expect(json.find("\"Green\"") != std::string::npos) << "Should serialize enum as 'Green'\n";
      expect(json.find("\"Complete\"") != std::string::npos) << "Should serialize enum as 'Complete'\n";
      expect(json.find("\"count\":100") != std::string::npos) << "Should contain count field\n";
      
      PureReflectSimple parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color == Color::Green) << "Color should be Green\n";
      expect(parsed.status == Status::Complete) << "Status should be Complete\n";
      expect(parsed.count == 100) << "Count should be 100\n";
      expect(std::abs(parsed.value - 2.718) < 0.001) << "Value should be 2.718\n";
   };
   
   "pure_reflect_nested_struct"_test = [] {
      PureReflectNested obj;
      obj.data.light = Go;
      obj.data.direction = South;
      obj.data.active = false;
      obj.primary_color = Color::Red;
      obj.optional_status = Status::Running;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      // Check nested structure serialization
      expect(json.find("\"Go\"") != std::string::npos) << "Should serialize TrafficLight as 'Go'\n";
      expect(json.find("\"South\"") != std::string::npos) << "Should serialize Direction as 'South'\n";
      expect(json.find("\"Red\"") != std::string::npos) << "Should serialize Color as 'Red'\n";
      expect(json.find("\"Running\"") != std::string::npos) << "Should serialize Status as 'Running'\n";
      
      PureReflectNested parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.data.light == Go) << "Light should be Go\n";
      expect(parsed.data.direction == South) << "Direction should be South\n";
      expect(parsed.data.active == false) << "Active should be false\n";
      expect(parsed.primary_color == Color::Red) << "Primary color should be Red\n";
      expect(parsed.optional_status.has_value() && *parsed.optional_status == Status::Running) 
         << "Optional status should be Running\n";
   };
   
   "pure_reflect_containers"_test = [] {
      PureReflectContainers obj;
      obj.color_list = {Color::Red, Color::Green, Color::Blue};
      obj.directions = {{"north", North}, {"south", South}};
      obj.flags = TestFlags::Flag2 | TestFlags::Flag3;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      // Verify container serialization
      expect(json.find("[\"Red\",\"Green\",\"Blue\"]") != std::string::npos) 
         << "Should serialize color vector correctly\n";
      expect(json.find("[\"Pending\",\"Running\",\"Complete\"]") != std::string::npos) 
         << "Should serialize status array correctly\n";
      
      PureReflectContainers parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color_list.size() == 3) << "Should have 3 colors\n";
      expect(parsed.color_list[0] == Color::Red) << "First color should be Red\n";
      expect(parsed.color_list[1] == Color::Green) << "Second color should be Green\n";
      expect(parsed.color_list[2] == Color::Blue) << "Third color should be Blue\n";
      expect(parsed.status_array[0] == Status::Pending) << "First status should be Pending\n";
      expect(parsed.directions["north"] == North) << "North direction should map correctly\n";
      expect(parsed.directions["south"] == South) << "South direction should map correctly\n";
      expect(parsed.flags.has_value() && *parsed.flags == (TestFlags::Flag2 | TestFlags::Flag3)) 
         << "Flags should be Flag2 | Flag3\n";
   };
   
   "pure_reflect_complex_struct"_test = [] {
      PureReflectComplex obj;
      obj.lights = {Stop, Caution, Go};
      obj.color_names = {{Color::Red, "rouge"}, {Color::Blue, "bleu"}};
      obj.sparse_value = Sparse::Second;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      PureReflectComplex parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.foreground == Color::Green) << "Foreground should be Green\n";
      expect(parsed.background == Color::Blue) << "Background should be Blue\n";
      expect(parsed.lights.size() == 3) << "Should have 3 lights\n";
      expect(parsed.lights[0] == Stop) << "First light should be Stop\n";
      expect(parsed.lights[1] == Caution) << "Second light should be Caution\n";
      expect(parsed.lights[2] == Go) << "Third light should be Go\n";
      expect(parsed.color_names[Color::Red] == "rouge") << "Red should map to 'rouge'\n";
      expect(parsed.color_names[Color::Blue] == "bleu") << "Blue should map to 'bleu'\n";
      expect(parsed.compass[0] == North) << "First compass direction should be North\n";
      expect(parsed.compass[3] == West) << "Last compass direction should be West\n";
      expect(parsed.sparse_value.has_value() && *parsed.sparse_value == Sparse::Second) 
         << "Sparse value should be Second\n";
   };
   
   "pure_reflect_mixed_types"_test = [] {
      PureReflectMixed obj;
      obj.name = "reflection_test";
      obj.theme = Color::Red;
      obj.version = 42;
      obj.state = Status::Complete;
      obj.scores = {10.5, 20.3, 30.1};
      obj.heading = West;
      obj.enabled = false;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      // Verify mixed type serialization
      expect(json.find("\"reflection_test\"") != std::string::npos) << "Should contain name\n";
      expect(json.find("\"Red\"") != std::string::npos) << "Should serialize theme as 'Red'\n";
      expect(json.find("\"Complete\"") != std::string::npos) << "Should serialize state as 'Complete'\n";
      expect(json.find("\"West\"") != std::string::npos) << "Should serialize heading as 'West'\n";
      expect(json.find("42") != std::string::npos) << "Should contain version number\n";
      
      PureReflectMixed parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.name == "reflection_test") << "Name should match\n";
      expect(parsed.theme == Color::Red) << "Theme should be Red\n";
      expect(parsed.version == 42) << "Version should be 42\n";
      expect(parsed.state == Status::Complete) << "State should be Complete\n";
      expect(parsed.scores.size() == 3) << "Should have 3 scores\n";
      expect(parsed.heading == West) << "Heading should be West\n";
      expect(parsed.enabled == false) << "Enabled should be false\n";
   };
   
   "pure_reflect_empty_containers"_test = [] {
      PureReflectContainers obj;
      // Leave color_list empty
      obj.directions.clear();
      // status_array has default values
      // flags is not set (null optional)
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      PureReflectContainers parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse successfully\n";
      expect(parsed.color_list.empty()) << "Color list should be empty\n";
      expect(parsed.directions.empty()) << "Directions should be empty\n";
      expect(parsed.status_array[0] == Status::Pending) << "Default status values should be preserved\n";
      expect(!parsed.flags.has_value()) << "Flags should be null\n";
   };
   
   "pure_reflect_numeric_backward_compat"_test = [] {
      // Test that pure reflection structs can still parse numeric enum values
      std::string json = R"({"color":1,"status":0,"count":99,"value":1.23})";
      
      PureReflectSimple parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Should parse numeric enum values\n";
      expect(parsed.color == Color::Green) << "Color 1 should be Green\n";
      expect(parsed.status == Status::Running) << "Status 0 should be Running\n";
      expect(parsed.count == 99) << "Count should be 99\n";
   };
   
   "pure_reflect_field_names_automatic"_test = [] {
      // Verify that field names are automatically derived
      PureReflectSimple obj;
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      // Check that the automatically derived field names are present
      expect(json.find("\"color\"") != std::string::npos) << "Should have 'color' field\n";
      expect(json.find("\"status\"") != std::string::npos) << "Should have 'status' field\n";
      expect(json.find("\"count\"") != std::string::npos) << "Should have 'count' field\n";
      expect(json.find("\"value\"") != std::string::npos) << "Should have 'value' field\n";
   };
   
   "pure_reflect_round_trip_all_enums"_test = [] {
      // Comprehensive round-trip test with all enum types
      PureReflectComplex obj;
      obj.foreground = Color::Red;
      obj.background = Color::Green;
      obj.lights = {Stop, Caution, Go, Stop, Go};
      obj.color_names = {{Color::Red, "red"}, {Color::Green, "green"}, {Color::Blue, "blue"}};
      obj.compass = {South, West, North, East};
      obj.permissions = TestFlags::Flag1 | TestFlags::Flag2 | TestFlags::Flag3;
      obj.sparse_value = Sparse::Third;
      
      std::string json;
      constexpr glz::opts opts{.enum_as_string = true};
      expect(not glz::write<opts>(obj, json));
      
      PureReflectComplex parsed;
      auto ec = glz::read_json(parsed, json);
      expect(!ec) << "Round trip should succeed\n";
      expect(parsed.foreground == obj.foreground) << "Foreground should match\n";
      expect(parsed.background == obj.background) << "Background should match\n";
      expect(parsed.lights == obj.lights) << "Lights should match\n";
      expect(parsed.color_names == obj.color_names) << "Color names should match\n";
      expect(parsed.compass == obj.compass) << "Compass should match\n";
      expect(parsed.permissions == obj.permissions) << "Permissions should match\n";
      expect(parsed.sparse_value == obj.sparse_value) << "Sparse value should match\n";
   };
};

int main() {}
