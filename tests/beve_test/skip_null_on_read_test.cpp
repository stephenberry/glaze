// Tests for skip_null_members_on_read option in BEVE format

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct simple_struct
{
   std::string name = "default";
   int age = 0;
   double score = 0.0;
};

template <>
struct glz::meta<simple_struct>
{
   using T = simple_struct;
   static constexpr auto value = glz::object("name", &T::name, "age", &T::age, "score", &T::score);
};

// Struct with optional fields for writing null values
struct optional_struct
{
   std::optional<std::string> name;
   std::optional<int> age;
   std::optional<double> score;
};

template <>
struct glz::meta<optional_struct>
{
   using T = optional_struct;
   static constexpr auto value = glz::object("name", &T::name, "age", &T::age, "score", &T::score);
};

// Custom options with skip_null_members_on_read enabled for BEVE
struct opts_skip_null_beve : glz::opts
{
   uint32_t format = glz::BEVE;
   bool skip_null_members_on_read = true;
};

suite skip_null_on_read_beve_tests = [] {
   "skip null fields"_test = [] {
      constexpr opts_skip_null_beve opts{};

      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      // Write with null values using optional struct
      optional_struct temp;
      temp.name = std::nullopt;
      temp.age = 30;
      temp.score = std::nullopt;

      std::string buffer;
      expect(!glz::write_beve(temp, buffer));

      auto ec = glz::read<opts>(obj, buffer);

      expect(!ec);
      expect(obj.name == "original"); // Null was skipped
      expect(obj.age == 30);
      expect(obj.score == 100.0); // Null was skipped
   };

   "skip all null fields"_test = [] {
      constexpr opts_skip_null_beve opts{};

      simple_struct obj{};
      obj.name = "original";
      obj.age = 25;
      obj.score = 100.0;

      optional_struct temp;
      temp.name = std::nullopt;
      temp.age = std::nullopt;
      temp.score = std::nullopt;

      std::string buffer;
      expect(!glz::write_beve(temp, buffer));

      auto ec = glz::read<opts>(obj, buffer);

      expect(!ec);
      // All fields retain original values
      expect(obj.name == "original");
      expect(obj.age == 25);
      expect(obj.score == 100.0);
   };
};

int main() {}
