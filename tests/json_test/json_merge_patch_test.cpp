// Glaze Library
// For the license information refer to glaze.hpp

#include <cstdlib>

#include "glaze/json/patch.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite json_merge_patch_tests = [] {
   // ============================================================================
   // Basic Merge Operations
   // ============================================================================

   "merge_patch basic add"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1})");
      auto patch = glz::read_json<glz::generic>(R"({"b": 2})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch modify existing"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto patch = glz::read_json<glz::generic>(R"({"a": 99})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 99, "b": 2})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch remove with null"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto patch = glz::read_json<glz::generic>(R"({"b": null})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch nested merge"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({
         "a": {"b": 1, "c": 2}
      })");
      auto patch = glz::read_json<glz::generic>(R"({
         "a": {"b": 99, "d": 3}
      })");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({
         "a": {"b": 99, "c": 2, "d": 3}
      })");
      expect(glz::equal(*target, *expected));
   };

   // ============================================================================
   // Type Coercion Cases
   // ============================================================================

   "merge_patch array replacement"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"tags": [1, 2, 3]})");
      auto patch = glz::read_json<glz::generic>(R"({"tags": ["x"]})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"tags": ["x"]})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch non-object replaces"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1})");
      auto patch = glz::read_json<glz::generic>(R"(42)");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);
      expect(target->is_number());
      expect(target->get_number() == 42.0);
   };

   "merge_patch non-object target with object patch"_test = [] {
      auto target = glz::read_json<glz::generic>(R"(42)");
      auto patch = glz::read_json<glz::generic>(R"({"a": 1})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch string target with object patch"_test = [] {
      auto target = glz::read_json<glz::generic>(R"("hello")");
      auto patch = glz::read_json<glz::generic>(R"({"a": 1})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch array target with object patch"_test = [] {
      auto target = glz::read_json<glz::generic>(R"([1, 2, 3])");
      auto patch = glz::read_json<glz::generic>(R"({"a": 1})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(glz::equal(*target, *expected));
   };

   // ============================================================================
   // Edge Cases
   // ============================================================================

   "merge_patch empty patch is no-op"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto original = *target;
      auto patch = glz::read_json<glz::generic>(R"({})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);
      expect(glz::equal(*target, original));
   };

   "merge_patch remove all keys"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto patch = glz::read_json<glz::generic>(R"({"a": null, "b": null})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch empty string key"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"": 1, "a": 2})");
      auto patch = glz::read_json<glz::generic>(R"({"": 99})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"": 99, "a": 2})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch numeric string keys"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"0": "a", "1": "b"})");
      auto patch = glz::read_json<glz::generic>(R"({"1": "x", "2": "c"})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"0": "a", "1": "x", "2": "c"})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch unicode keys"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"日本語": 1, "中文": "hello"})");
      auto patch = glz::read_json<glz::generic>(R"({"日本語": 2, "emoji": "test"})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"日本語": 2, "中文": "hello", "emoji": "test"})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch deeply nested"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({
         "a": {"b": {"c": {"d": 1}}}
      })");
      auto patch = glz::read_json<glz::generic>(R"({
         "a": {"b": {"c": {"d": 99, "e": 2}}}
      })");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({
         "a": {"b": {"c": {"d": 99, "e": 2}}}
      })");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch null target"_test = [] {
      auto target = glz::read_json<glz::generic>(R"(null)");
      auto patch = glz::read_json<glz::generic>(R"({"a": 1})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(glz::equal(*target, *expected));
   };

   // ============================================================================
   // Max Depth Protection
   // ============================================================================

   "merge_patch max_depth exceeded"_test = [] {
      glz::generic target;
      target.data = glz::generic::object_t{};

      glz::generic patch;
      patch.data = glz::generic::object_t{};

      // Build a deeply nested patch exceeding max_recursive_depth_limit (256)
      glz::generic* current = &patch;
      for (size_t i = 0; i < glz::max_recursive_depth_limit + 10; ++i) {
         current->get_object()["nested"].data = glz::generic::object_t{};
         current = &current->get_object()["nested"];
      }

      auto ec = glz::merge_patch(target, patch);
      expect(ec.ec == glz::error_code::exceeded_max_recursive_depth);
   };

   "merge_patch within max_depth"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({})");
      auto patch = glz::read_json<glz::generic>(R"({"a": {"b": {"c": 1}}})");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);
   };

   // ============================================================================
   // Merge Diff Tests
   // ============================================================================

   "merge_diff generates correct patch"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "c": 3})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      // patch should contain {"b": null, "c": 3}
      expect(patch->is_object());
      auto& patch_obj = patch->get_object();
      expect(patch_obj.size() == 2u);
      expect(patch_obj.find("b") != patch_obj.end());
      expect(patch_obj["b"].is_null());
      expect(patch_obj.find("c") != patch_obj.end());
      expect(patch_obj["c"].get_number() == 3.0);
   };

   "merge_diff round-trip"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": {"x": 1}})");
      auto target = glz::read_json<glz::generic>(R"({"a": 2, "b": {"y": 2}, "c": 3})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);
      expect(glz::equal(result, *target));
   };

   "merge_diff identical documents"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      auto target = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      // patch should be empty object
      expect(patch->is_object());
      expect(patch->get_object().empty());
   };

   "merge_diff nested objects"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({"a": {"b": 1, "c": 2}})");
      auto target = glz::read_json<glz::generic>(R"({"a": {"b": 99, "d": 3}})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      // Verify round-trip
      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);
      expect(glz::equal(result, *target));
   };

   "merge_diff type change"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({"a": 1})");
      auto target = glz::read_json<glz::generic>(R"({"a": "string"})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);
      expect(glz::equal(result, *target));
   };

   "merge_diff null limitation"_test = [] {
      // Demonstrate that explicit null in target cannot be preserved
      auto source = glz::read_json<glz::generic>(R"({"a": 1})");
      auto target = glz::read_json<glz::generic>(R"({"a": null})");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      // The patch will contain {"a": null}, but this means "remove a"
      // not "set a to null"
      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);

      // Result will NOT have "a" at all, not have "a": null
      expect(!result.contains("a"));
      // This is the documented limitation of RFC 7386
   };

   // ============================================================================
   // Non-Mutating API (merge_patched)
   // ============================================================================

   "merge_patched non-mutating"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1})");
      auto patch = glz::read_json<glz::generic>(R"({"b": 2})");

      expect(target.has_value() && patch.has_value());

      auto result = glz::merge_patched(*target, *patch);
      expect(result.has_value());

      // Original unchanged
      expect(target->get_object()["a"].get_number() == 1.0);
      expect(target->get_object().find("b") == target->get_object().end());

      // Result has change
      expect(result->get_object()["a"].get_number() == 1.0);
      expect(result->get_object()["b"].get_number() == 2.0);
   };

   // ============================================================================
   // Convenience String Functions
   // ============================================================================

   "merge_patch from string"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(target.has_value());

      auto ec = glz::merge_patch(*target, R"({"b": 2})");
      expect(!ec);

      auto expected = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      expect(glz::equal(*target, *expected));
   };

   "merge_patch_json string to string"_test = [] {
      auto result = glz::merge_patch_json(R"({"a": 1})", R"({"b": 2})");
      expect(result.has_value());

      auto parsed = glz::read_json<glz::generic>(*result);
      expect(parsed.has_value());

      auto expected = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      expect(glz::equal(*parsed, *expected));
   };

   "merge_patched from strings"_test = [] {
      auto result = glz::merge_patched(R"({"a": 1})", R"({"b": 2})");
      expect(result.has_value());

      auto expected = glz::read_json<glz::generic>(R"({"a": 1, "b": 2})");
      expect(glz::equal(*result, *expected));
   };

   "merge_diff_json string to string"_test = [] {
      auto result = glz::merge_diff_json(R"({"a": 1, "b": 2})", R"({"a": 1, "c": 3})");
      expect(result.has_value());

      auto parsed = glz::read_json<glz::generic>(*result);
      expect(parsed.has_value());

      // Should contain {"b": null, "c": 3}
      expect(parsed->is_object());
      expect((*parsed)["b"].is_null());
      expect((*parsed)["c"].get_number() == 3.0);
   };

   "merge_patch invalid json"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({"a": 1})");
      expect(target.has_value());

      auto ec = glz::merge_patch(*target, R"({invalid json)");
      expect(ec); // Should fail
   };

   "merge_patch_json invalid target json"_test = [] {
      auto result = glz::merge_patch_json(R"({invalid)", R"({"b": 2})");
      expect(!result.has_value());
   };

   "merge_patch_json invalid patch json"_test = [] {
      auto result = glz::merge_patch_json(R"({"a": 1})", R"({invalid)");
      expect(!result.has_value());
   };

   // ============================================================================
   // RFC 7386 Appendix A Example
   // ============================================================================

   "rfc7386 appendix a example"_test = [] {
      auto target = glz::read_json<glz::generic>(R"({
         "title": "Goodbye!",
         "author": {
            "givenName": "John",
            "familyName": "Doe"
         },
         "tags": ["example", "sample"],
         "content": "This will be unchanged"
      })");

      auto patch = glz::read_json<glz::generic>(R"({
         "title": "Hello!",
         "phoneNumber": "+01-123-456-7890",
         "author": {
            "familyName": null
         },
         "tags": ["example"]
      })");

      expect(target.has_value() && patch.has_value());

      auto ec = glz::merge_patch(*target, *patch);
      expect(!ec);

      // Verify the result matches RFC 7386 Appendix A expected output
      expect(target->get_object()["title"].get_string() == "Hello!");
      expect(target->get_object()["phoneNumber"].get_string() == "+01-123-456-7890");
      expect(target->get_object()["content"].get_string() == "This will be unchanged");

      // author should have givenName but not familyName
      auto& author = target->get_object()["author"];
      expect(author.is_object());
      expect(author.get_object()["givenName"].get_string() == "John");
      expect(author.get_object().find("familyName") == author.get_object().end());

      // tags should be replaced entirely
      auto& tags = target->get_object()["tags"];
      expect(tags.is_array());
      expect(tags.get_array().size() == 1u);
      expect(tags.get_array()[0].get_string() == "example");
   };

   // ============================================================================
   // Complex Round-Trip Tests
   // ============================================================================

   "complex round-trip"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({
         "name": "test",
         "values": [1, 2, 3],
         "nested": {"a": 1, "b": 2},
         "removed": "will be gone"
      })");
      auto target = glz::read_json<glz::generic>(R"({
         "name": "modified",
         "values": [4, 5],
         "nested": {"a": 1, "c": 3},
         "new_field": true
      })");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);
      expect(glz::equal(result, *target));
   };

   "deeply nested round-trip"_test = [] {
      auto source = glz::read_json<glz::generic>(R"({
         "l1": {"l2": {"l3": {"l4": {"value": 1}}}}
      })");
      auto target = glz::read_json<glz::generic>(R"({
         "l1": {"l2": {"l3": {"l4": {"value": 2, "new": true}}}}
      })");

      expect(source.has_value() && target.has_value());

      auto patch = glz::merge_diff(*source, *target);
      expect(patch.has_value());

      auto result = *source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);
      expect(glz::equal(result, *target));
   };
};

// ============================================================================
// Struct-based Merge Patch Tests
// ============================================================================

struct Person
{
   std::string name{};
   int age{};
   std::string city{};
};

struct Config
{
   std::string host = "localhost";
   int port = 8080;
   bool enabled = true;
   std::vector<std::string> tags{};
};

struct Nested
{
   std::string value{};
   std::optional<int> count{};
};

struct Parent
{
   std::string id{};
   Nested nested{};
};

suite struct_merge_patch_tests = [] {
   "struct merge_patch basic"_test = [] {
      Person person{.name = "Alice", .age = 30, .city = "NYC"};

      auto patch = glz::read_json<glz::generic>(R"({"age": 31})");
      expect(patch.has_value());

      auto ec = glz::merge_patch(person, *patch);
      expect(!ec);
      expect(person.name == "Alice");
      expect(person.age == 31);
      expect(person.city == "NYC");
   };

   "struct merge_patch multiple fields"_test = [] {
      Person person{.name = "Bob", .age = 25, .city = "LA"};

      auto patch = glz::read_json<glz::generic>(R"({"name": "Robert", "city": "San Francisco"})");
      expect(patch.has_value());

      auto ec = glz::merge_patch(person, *patch);
      expect(!ec);
      expect(person.name == "Robert");
      expect(person.age == 25);
      expect(person.city == "San Francisco");
   };

   "struct merge_patch from string"_test = [] {
      Person person{.name = "Charlie", .age = 40, .city = "Boston"};

      auto ec = glz::merge_patch(person, R"({"age": 41, "city": "Chicago"})");
      expect(!ec);
      expect(person.name == "Charlie");
      expect(person.age == 41);
      expect(person.city == "Chicago");
   };

   "struct merge_patch array replacement"_test = [] {
      Config config{.host = "localhost", .port = 8080, .enabled = true, .tags = {"dev", "test"}};

      auto patch = glz::read_json<glz::generic>(R"({"tags": ["prod"]})");
      expect(patch.has_value());

      auto ec = glz::merge_patch(config, *patch);
      expect(!ec);
      expect(config.host == "localhost");
      expect(config.port == 8080);
      expect(config.enabled == true);
      expect(config.tags.size() == 1u);
      expect(config.tags[0] == "prod");
   };

   "struct merge_patch nested object"_test = [] {
      Parent parent{.id = "123", .nested = {.value = "old", .count = 5}};

      auto patch = glz::read_json<glz::generic>(R"({"nested": {"value": "new"}})");
      expect(patch.has_value());

      auto ec = glz::merge_patch(parent, *patch);
      expect(!ec);
      expect(parent.id == "123");
      expect(parent.nested.value == "new");
      // Note: count will be reset because the nested object is replaced
   };

   "struct merge_patched non-mutating"_test = [] {
      Person original{.name = "Dave", .age = 35, .city = "Seattle"};

      auto patch = glz::read_json<glz::generic>(R"({"age": 36})");
      expect(patch.has_value());

      auto result = glz::merge_patched(original, *patch);
      expect(result.has_value());

      // Original unchanged
      expect(original.age == 35);

      // Result has change
      expect(result->name == "Dave");
      expect(result->age == 36);
      expect(result->city == "Seattle");
   };

   "struct merge_patched from string"_test = [] {
      Config config{.host = "localhost", .port = 8080, .enabled = true, .tags = {}};

      auto result = glz::merge_patched(config, R"({"port": 9000, "enabled": false})");
      expect(result.has_value());

      // Original unchanged
      expect(config.port == 8080);
      expect(config.enabled == true);

      // Result has changes
      expect(result->host == "localhost");
      expect(result->port == 9000);
      expect(result->enabled == false);
   };

   "struct merge_diff basic"_test = [] {
      Person source{.name = "Eve", .age = 28, .city = "Miami"};
      Person target{.name = "Eve", .age = 29, .city = "Miami"};

      auto patch = glz::merge_diff(source, target);
      expect(patch.has_value());

      // Patch should only contain age change
      expect(patch->is_object());
      auto& obj = patch->get_object();
      expect(obj.find("age") != obj.end());
      expect(obj["age"].get_number() == 29.0);
      // name and city should not be in patch (unchanged)
      expect(obj.find("name") == obj.end());
      expect(obj.find("city") == obj.end());
   };

   "struct merge_diff multiple changes"_test = [] {
      Config source{.host = "localhost", .port = 8080, .enabled = true, .tags = {"a", "b"}};
      Config target{.host = "production.example.com", .port = 443, .enabled = true, .tags = {"prod"}};

      auto patch = glz::merge_diff(source, target);
      expect(patch.has_value());

      expect(patch->is_object());
      auto& obj = patch->get_object();
      expect(obj["host"].get_string() == "production.example.com");
      expect(obj["port"].get_number() == 443.0);
      // enabled is unchanged, should not be in patch
      expect(obj.find("enabled") == obj.end());
   };

   "struct merge_diff_json"_test = [] {
      Person source{.name = "Frank", .age = 50, .city = "Denver"};
      Person target{.name = "Frank", .age = 51, .city = "Austin"};

      auto patch_json = glz::merge_diff_json(source, target);
      expect(patch_json.has_value());

      // Parse and verify
      auto patch = glz::read_json<glz::generic>(*patch_json);
      expect(patch.has_value());
      expect((*patch)["age"].get_number() == 51.0);
      expect((*patch)["city"].get_string() == "Austin");
   };

   "struct round-trip"_test = [] {
      Person source{.name = "Grace", .age = 45, .city = "Portland"};
      Person target{.name = "Grace", .age = 46, .city = "Seattle"};

      // Generate patch
      auto patch = glz::merge_diff(source, target);
      expect(patch.has_value());

      // Apply patch to source
      Person result = source;
      auto ec = glz::merge_patch(result, *patch);
      expect(!ec);

      // Result should match target
      expect(result.name == target.name);
      expect(result.age == target.age);
      expect(result.city == target.city);
   };

   "struct empty patch is no-op"_test = [] {
      Person person{.name = "Henry", .age = 60, .city = "Phoenix"};
      Person original = person;

      auto patch = glz::read_json<glz::generic>(R"({})");
      expect(patch.has_value());

      auto ec = glz::merge_patch(person, *patch);
      expect(!ec);
      expect(person.name == original.name);
      expect(person.age == original.age);
      expect(person.city == original.city);
   };
};

int main() { return 0; }
