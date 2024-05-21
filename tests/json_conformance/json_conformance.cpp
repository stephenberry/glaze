#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"

using namespace boost::ut;

using sv = glz::sv;

struct empty_object
{};

struct bool_object
{
   bool b{};
};

struct int_object
{
   int i{};
};

struct nullable_object
{
   std::optional<int> i{};
};

template <glz::opts Opts>
inline void should_fail()
{
   "unclosed_array"_test = [] {
      constexpr sv s = R"(["Unclosed array")";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::deque<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::list<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "unquoted_key"_test = [] {
      constexpr sv s = R"({unquoted_key: "keys must be quoted"})";
      {
         std::map<std::string, std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::unordered_map<std::string, std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         empty_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "extra comma"_test = [] {
      constexpr sv s = R"(["extra comma",])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "double extra comma"_test = [] {
      constexpr sv s = R"(["double extra comma",,])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "missing value"_test = [] {
      constexpr sv s = R"([   , "<-- missing value"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   if constexpr (Opts.force_conformance) {
      // TODO: Add force_conformance testing after parse
      skip / "comma after close"_test = [] {
         constexpr sv s = R"(["Comma after the close"],)";
         {
            std::vector<std::string> v;
            expect(glz::read_json(v, s));
         }
      };

      skip / "extra close"_test = [] {
         constexpr sv s = R"(["Extra close"]])";
         {
            std::vector<std::string> v;
            expect(glz::read_json(v, s));
         }
      };

      skip / "extra value after close"_test = [] {
         constexpr sv s = R"({"b": true} "misplaced quoted value")";
         {
            bool_object obj{};
            expect(glz::read_json(obj, s));
         }
      };
   }

   "illegal expression"_test = [] {
      constexpr sv s = R"({"i": 1 + 2})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal invocation"_test = [] {
      constexpr sv s = R"({"i": alert()})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   // TODO: Support this error in all cases
   skip / "numbers cannot have leading zeroes"_test = [] {
      constexpr sv s = R"({"i": 013})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "numbers cannot be hex"_test = [] {
      constexpr sv s = R"({"i": 0x14})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal backslash escape: \x15"_test = [] {
      constexpr sv s = R"(["Illegal backslash escape: \x15"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal backslash escape: \017"_test = [] {
      constexpr sv s = R"(["Illegal backslash escape: \017"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "naked"_test = [] {
      constexpr sv s = R"([\naked])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "missing colon"_test = [] {
      constexpr sv s = R"({"i" null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "double colon"_test = [] {
      constexpr sv s = R"({"i":: null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "comma instead of colon"_test = [] {
      constexpr sv s = R"({"i", null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "colon instead of comma"_test = [] {
      constexpr sv s = R"(["Colon instead of comma": false])";
      {
         std::tuple<std::string, bool> v{};
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "bad value"_test = [] {
      constexpr sv s = R"(["Bad value", truth])";
      {
         std::tuple<std::string, bool> v{};
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "single quote"_test = [] {
      constexpr sv s = R"(['single quote'])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::json_t obj{};
         expect(glz::read_json(obj, s));
      }
   };

   // TODO: This should be an error
   skip / "0e"_test = [] {
      constexpr sv s = R"(0e)";
      {
         double v{};
         expect(glz::read_json(v, s));
      }
      {
         float v{};
         expect(glz::read_json(v, s));
      }
      {
         int v{};
         expect(glz::read_json(v, s));
      }
   };

   skip / "0e+"_test = [] {
      constexpr sv s = R"(0e+)";
      {
         double v{};
         expect(glz::read_json(v, s));
      }
      {
         float v{};
         expect(glz::read_json(v, s));
      }
      {
         int v{};
         expect(glz::read_json(v, s));
      }
   };
}

template <glz::opts Opts>
inline void should_pass()
{
   "bool_object"_test = [] {
      constexpr sv s = R"({"b": true})";
      {
         bool_object obj{};
         expect(!glz::read_json(obj, s));
         expect(obj.b == true);
      }
   };

   "int_object"_test = [] {
      constexpr sv s = R"({"i": 55})";
      {
         int_object obj{};
         expect(!glz::read_json(obj, s));
         expect(obj.i == 55);
      }
   };
}

suite json_conformance = [] {
   "error_on_unknown_keys = true"_test = [] {
      should_fail<glz::opts{}>();
      should_pass<glz::opts{}>();
   };

   "error_on_unknown_keys = false"_test = [] {
      should_fail<glz::opts{.error_on_unknown_keys = false}>();
      should_pass<glz::opts{.error_on_unknown_keys = false}>();
   };

   "force_conformance = true"_test = [] {
      should_fail<glz::opts{.force_conformance = true}>();
      should_pass<glz::opts{.force_conformance = true}>();
   };
};

int main()
{ // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
