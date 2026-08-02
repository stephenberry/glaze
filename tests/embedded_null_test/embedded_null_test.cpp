#include <string>
#include <string_view>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace std::string_literals;

struct custom_string {
   std::string val;
   
   operator const char*() const { return val.c_str(); }
   const char* data() const { return val.data(); }
   size_t size() const { return val.size(); }
};

struct MyStruct {
   custom_string str;
};

template <>
struct glz::meta<MyStruct> {
   using T = MyStruct;
   static constexpr auto value = object("str", &T::str);
};

suite embedded_null_tests = [] {
   "all_formats_embedded_null"_test = [] {
      MyStruct obj{custom_string{"hello\0world"s}};
      
      std::string buffer;

      // JSON
      glz::write<glz::opts{.format = glz::JSON}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // YAML
      buffer.clear();
      glz::write<glz::opts{.format = glz::YAML}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // TOML
      buffer.clear();
      glz::write<glz::opts{.format = glz::TOML}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // CBOR
      buffer.clear();
      glz::write<glz::opts{.format = glz::CBOR}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // BSON
      buffer.clear();
      glz::write<glz::opts{.format = glz::BSON}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // BEVE
      buffer.clear();
      glz::write<glz::opts{.format = glz::BEVE}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);

      // JSONB
      buffer.clear();
      glz::write<glz::opts{.format = glz::JSONB}>(obj, buffer);
      expect(buffer.find("world") != std::string::npos);
   };
};

int main() {
   return 0;
}
