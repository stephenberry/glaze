// Glaze Library
// For the license information refer to glaze.ixx

// Test for skip_default_members option
// Related to GitHub issue #855

import std;
import glaze;
import ut;

using namespace ut;

struct DefaultObj
{
   std::string name{};
   int age{};
   double score{};
   bool active{};
   std::vector<int> tags{};
   std::map<std::string, int> metadata{};
};

struct skip_defaults_opts : glz::opts
{
   bool skip_default_members = true;
};

suite skip_default_members_tests = [] {
   "all_defaults_skipped"_test = [] {
      DefaultObj obj{};

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({})") << buffer;
   };

   "non_default_string_included"_test = [] {
      DefaultObj obj{};
      obj.name = "Alice";

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"name":"Alice"})") << buffer;
   };

   "non_default_int_included"_test = [] {
      DefaultObj obj{};
      obj.age = 30;

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"age":30})") << buffer;
   };

   "non_default_double_included"_test = [] {
      DefaultObj obj{};
      obj.score = 9.5;

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"score":9.5})") << buffer;
   };

   "non_default_bool_included"_test = [] {
      DefaultObj obj{};
      obj.active = true;

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"active":true})") << buffer;
   };

   "non_default_vector_included"_test = [] {
      DefaultObj obj{};
      obj.tags = {1, 2, 3};

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"tags":[1,2,3]})") << buffer;
   };

   "non_default_map_included"_test = [] {
      DefaultObj obj{};
      obj.metadata = {{"key", 42}};

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"metadata":{"key":42}})") << buffer;
   };

   "mixed_defaults_and_non_defaults"_test = [] {
      DefaultObj obj{};
      obj.name = "Bob";
      obj.active = true;
      obj.tags = {10};

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);
      expect(buffer == R"({"name":"Bob","active":true,"tags":[10]})") << buffer;
   };

   "without_skip_default_all_written"_test = [] {
      DefaultObj obj{};

      std::string buffer{};
      auto ec = glz::write_json(obj, buffer);
      expect(!ec);
      // Without skip_default_members, all fields are written
      expect(buffer == R"({"name":"","age":0,"score":0,"active":false,"tags":[],"metadata":{}})") << buffer;
   };

   "roundtrip_with_defaults"_test = [] {
      DefaultObj obj{};
      obj.name = "Charlie";
      obj.age = 25;

      std::string buffer{};
      auto ec = glz::write<skip_defaults_opts{}>(obj, buffer);
      expect(!ec);

      DefaultObj obj2{};
      auto ec2 = glz::read_json(obj2, buffer);
      expect(!ec2);
      expect(obj2.name == "Charlie");
      expect(obj2.age == 25);
      // Defaults remain as defaults since they were not in JSON
      expect(obj2.score == 0.0);
      expect(obj2.active == false);
      expect(obj2.tags.empty());
      expect(obj2.metadata.empty());
   };
};

int main() { return 0; }
