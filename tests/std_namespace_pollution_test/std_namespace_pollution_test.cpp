// Glaze Library
// For the license information refer to glaze.hpp

// Regression test for https://github.com/stephenberry/glaze/issues/2534
//
// When a translation unit hoists std::hash into the global namespace
// (commonly via a stray `using namespace std;` in some upstream header
// that's transitively included before glaze) Clang's parser used to
// misread `obj.hash <` inside ordered_small_map<T> as the start of a
// template-id, because `hash` was visible at global scope as the
// std::hash class template. This file is purely a compilation test:
// if it builds, the parens-fix in ordered_small_map.hpp is in place.

#include <functional> // brings std::hash

using namespace std; // hoists std::hash into the global namespace

#include "glaze/glaze.hpp"

#include "ut/ut.hpp"

using namespace ut;

suite std_namespace_pollution_tests = [] {
   "ordered_small_map round-trip under polluted namespace"_test = [] {
      // glz::generic is backed by ordered_small_map; exercising
      // read/write forces ordered_small_map.hpp to be fully
      // instantiated, which is what triggered the original parse error.
      glz::generic v;
      auto ec = glz::read_json(v, R"({"a":1,"b":2,"c":3})");
      expect(!ec);

      std::string out;
      auto wec = glz::write_json(v, out);
      expect(!wec);
      expect(out == R"({"a":1,"b":2,"c":3})");
   };
};

int main() { return 0; }
