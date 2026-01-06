// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Structs for deserialization tests - must be at namespace scope for reflection
namespace lazy_beve_test
{
   struct User
   {
      std::string name{};
      int age{};
      bool active{};
   };

   struct Address
   {
      std::string city{};
      std::string country{};
   };

   struct Person
   {
      std::string name{};
      Address address{};
   };

   struct Item
   {
      int id{};
      std::string value{};
   };

   struct Numbers
   {
      int32_t int_val{42};
      double float_val{3.14};
      int64_t negative{-100};
      uint64_t big{9007199254740993ULL};
   };

   struct StringData
   {
      std::string simple{"hello"};
      std::string with_special{"hello\nworld"};
   };

   struct BoolData
   {
      bool exists{true};
   };

   struct MixedData
   {
      std::string str{"hello"};
      int num{42};
   };

   struct Container
   {
      User user;
      int version{1};
   };

   struct PeopleContainer
   {
      std::vector<Person> people;
   };

   struct PrimitiveData
   {
      int count{42};
      double ratio{3.14};
      std::string name{"test"};
      bool active{true};
   };

   struct ArrayData
   {
      std::vector<int> values{1, 2, 3, 4, 5};
   };

   struct SimpleData
   {
      std::string name{"test"};
      int age{30};
   };

   struct NumMapContainer
   {
      std::map<int, std::string> num_map{{1, "hello"}, {2, "world"}};
      std::string after{"after_map"};
   };
}

suite lazy_beve_tests = [] {
   "lazy_beve_read_basic"_test = [] {
      // Create BEVE data from a struct
      lazy_beve_test::User user{"John", 30, true};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(user, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value()) << "Failed to parse BEVE";

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().size() == 3u);

      // Access values lazily
      expect(doc["name"].is_string());
      expect(doc["name"].get<std::string>().value() == "John");

      expect(doc["age"].is_number());
      expect(doc["age"].get<int64_t>().value() == 30);

      expect(doc["active"].is_boolean());
      expect(doc["active"].get<bool>().value() == true);
   };

   "lazy_beve_read_array"_test = [] {
      // Create a generic array
      std::vector<int> arr{1, 2, 3, 4, 5};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 5u);
   };

   "lazy_beve_nested"_test = [] {
      lazy_beve_test::Person person{"Alice", {"New York", "USA"}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(person, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["name"].get<std::string>().value() == "Alice");
      expect(doc["address"]["city"].get<std::string>().value() == "New York");
      expect(doc["address"]["country"].get<std::string>().value() == "USA");
   };

   "lazy_beve_contains"_test = [] {
      std::map<std::string, int> data{{"a", 1}, {"b", 2}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().contains("a"));
      expect(doc.root().contains("b"));
      expect(!doc.root().contains("c"));
   };

   "lazy_beve_empty_object"_test = [] {
      std::map<std::string, int> empty_map{};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(empty_map, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_beve_empty_array"_test = [] {
      std::vector<int> empty_arr{};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(empty_arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_beve_null"_test = [] {
      std::optional<int> null_val = std::nullopt;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(null_val, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_null());
      expect(doc.root().empty());
   };

   "lazy_beve_number_types"_test = [] {
      lazy_beve_test::Numbers nums;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(nums, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["int_val"].get<int32_t>().value() == 42);
      expect(std::abs(doc["float_val"].get<double>().value() - 3.14) < 0.001);
      expect(doc["negative"].get<int64_t>().value() == -100);
      expect(doc["big"].get<uint64_t>().value() == 9007199254740993ULL);
   };

   "lazy_beve_string_view"_test = [] {
      lazy_beve_test::StringData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;

      auto simple_sv = doc["simple"].get<std::string_view>();
      expect(simple_sv.has_value());
      expect(simple_sv.value() == "hello");

      auto special_sv = doc["with_special"].get<std::string_view>();
      expect(special_sv.has_value());
      expect(special_sv.value() == "hello\nworld");
   };

   "lazy_beve_explicit_bool"_test = [] {
      lazy_beve_test::BoolData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // explicit bool conversion - true if not null
      expect(static_cast<bool>(*result));
      expect(static_cast<bool>((*result)["exists"]));

      std::optional<int> null_val = std::nullopt;
      std::vector<std::byte> null_buffer;
      auto null_ec = glz::write_beve(null_val, null_buffer);
      expect(null_ec == glz::error_code::none);
      auto null_result = glz::lazy_beve(null_buffer);
      expect(null_result.has_value());
      expect(!static_cast<bool>(*null_result)); // null is false
   };

   "lazy_beve_wrong_type_error"_test = [] {
      lazy_beve_test::MixedData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Trying to get string as number should fail
      auto num_result = doc["str"].get<int64_t>();
      expect(!num_result.has_value());
      expect(num_result.error().ec == glz::error_code::get_wrong_type);

      // Trying to get number as string should fail
      auto str_result = doc["num"].get<std::string>();
      expect(!str_result.has_value());
      expect(str_result.error().ec == glz::error_code::get_wrong_type);
   };

   "lazy_beve_progressive_scanning"_test = [] {
      std::map<std::string, int> data{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Sequential access should use progressive scanning
      expect(doc["a"].get<int64_t>().value() == 1);
      expect(doc["b"].get<int64_t>().value() == 2);
      expect(doc["c"].get<int64_t>().value() == 3);
      expect(doc["d"].get<int64_t>().value() == 4);
      expect(doc["e"].get<int64_t>().value() == 5);

      // Accessing earlier key should still work (wrap-around)
      expect(doc["a"].get<int64_t>().value() == 1);
      expect(doc["c"].get<int64_t>().value() == 3);

      // Non-existent key should return error
      auto missing = doc["z"];
      expect(missing.has_error());
   };

   "lazy_beve_reset_parse_pos"_test = [] {
      std::map<std::string, int> data{{"x", 10}, {"y", 20}, {"z", 30}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Access z first (advances parse_pos to end)
      expect(doc["z"].get<int64_t>().value() == 30);

      // Access x (should wrap around)
      expect(doc["x"].get<int64_t>().value() == 10);

      // Reset and access again
      doc.reset_parse_pos();
      expect(doc["y"].get<int64_t>().value() == 20);
   };

   // ============================================================================
   // indexed_lazy_beve_view tests
   // ============================================================================

   "indexed_lazy_beve_view_array_basic"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.is_array());
      expect(!indexed.is_object());
      expect(indexed.size() == 5u);
      expect(!indexed.empty());
   };

   "indexed_lazy_beve_view_object_basic"_test = [] {
      std::map<std::string, int> data{{"a", 1}, {"b", 2}, {"c", 3}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.is_object());
      expect(!indexed.is_array());
      expect(indexed.size() == 3u);
      expect(!indexed.empty());

      // Key lookup
      expect(indexed["a"].get<int64_t>().value() == 1);
      expect(indexed["b"].get<int64_t>().value() == 2);
      expect(indexed["c"].get<int64_t>().value() == 3);
      expect(indexed["missing"].has_error());

      // Contains
      expect(indexed.contains("a"));
      expect(indexed.contains("b"));
      expect(!indexed.contains("missing"));
   };

   "indexed_lazy_beve_view_object_iteration"_test = [] {
      std::map<std::string, int> data{{"x", 10}, {"y", 20}, {"z", 30}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      std::vector<std::pair<std::string_view, int64_t>> items;
      for (auto& item : indexed) {
         auto val = item.get<int64_t>();
         if (val) {
            items.emplace_back(item.key(), *val);
         }
      }

      expect(items.size() == 3u);
   };

   "indexed_lazy_beve_view_empty"_test = [] {
      std::vector<int> empty_arr{};
      std::vector<std::byte> buffer1;
      auto ec1 = glz::write_beve(empty_arr, buffer1);
      expect(ec1 == glz::error_code::none);
      auto result1 = glz::lazy_beve(buffer1);
      expect(result1.has_value());
      auto indexed1 = result1->root().index();
      expect(indexed1.empty());
      expect(indexed1.size() == 0u);

      std::map<std::string, int> empty_map{};
      std::vector<std::byte> buffer2;
      auto ec2 = glz::write_beve(empty_map, buffer2);
      expect(ec2 == glz::error_code::none);
      auto result2 = glz::lazy_beve(buffer2);
      expect(result2.has_value());
      auto indexed2 = result2->root().index();
      expect(indexed2.empty());
      expect(indexed2.size() == 0u);
   };

   // ============================================================================
   // raw_beve() and struct deserialization tests
   // ============================================================================

   "lazy_beve_raw_beve_basic"_test = [] {
      lazy_beve_test::User user{"Alice", 30, true};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(user, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // raw_beve() returns the entire BEVE for a value
      auto raw = result->root().raw_beve();
      expect(raw.size() == buffer.size());
   };

   "lazy_beve_deserialize_struct"_test = [] {
      lazy_beve_test::Container container{{"Alice", 30, true}, 1};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(container, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Navigate lazily to "user", then deserialize into struct
      auto user_view = (*result)["user"];
      expect(!user_view.has_error());

      // Get raw BEVE and deserialize
      auto user_beve = user_view.raw_beve();
      lazy_beve_test::User user{};
      auto read_ec = glz::read_beve(user, user_beve);

      expect(read_ec == glz::error_code::none);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   // ============================================================================
   // read_into<T>() tests
   // ============================================================================

   "lazy_beve_read_into_basic"_test = [] {
      lazy_beve_test::Container container{{"Alice", 30, true}, 1};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(container, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Navigate lazily to "user", then deserialize directly
      lazy_beve_test::User user{};
      auto read_ec = (*result)["user"].read_into(user);

      expect(!read_ec);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   "lazy_beve_read_into_nested"_test = [] {
      lazy_beve_test::PeopleContainer container{{{"Alice", {"New York", "USA"}}, {"Bob", {"London", "UK"}}}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(container, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Deserialize directly using read_into
      lazy_beve_test::Person alice{};
      auto ec1 = (*result)["people"][0].read_into(alice);
      expect(!ec1);
      expect(alice.name == "Alice");
      expect(alice.address.city == "New York");
      expect(alice.address.country == "USA");

      lazy_beve_test::Person bob{};
      auto ec2 = (*result)["people"][1].read_into(bob);
      expect(!ec2);
      expect(bob.name == "Bob");
      expect(bob.address.city == "London");
      expect(bob.address.country == "UK");
   };

   "lazy_beve_read_into_primitive"_test = [] {
      lazy_beve_test::PrimitiveData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // read_into works with primitive types too
      int count{};
      auto ec1 = (*result)["count"].read_into(count);
      expect(!ec1);
      expect(count == 42);

      double ratio{};
      auto ec2 = (*result)["ratio"].read_into(ratio);
      expect(!ec2);
      expect(std::abs(ratio - 3.14) < 0.001);

      std::string name{};
      auto ec3 = (*result)["name"].read_into(name);
      expect(!ec3);
      expect(name == "test");

      bool active{};
      auto ec4 = (*result)["active"].read_into(active);
      expect(!ec4);
      expect(active == true);
   };

   "lazy_beve_read_into_array"_test = [] {
      lazy_beve_test::ArrayData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // read_into can deserialize arrays
      std::vector<int> values{};
      auto read_ec = (*result)["values"].read_into(values);
      expect(!read_ec);
      expect(values.size() == 5u);
      expect(values[0] == 1);
      expect(values[4] == 5);
   };

   "lazy_beve_read_into_error_handling"_test = [] {
      lazy_beve_test::SimpleData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // read_into on missing key should return error
      lazy_beve_test::User user{};
      auto missing_view = (*result)["missing"];
      expect(missing_view.has_error());

      auto read_ec = missing_view.read_into(user);
      expect(bool(read_ec)); // Should have error
   };

   // ============================================================================
   // glz::read_beve overload for lazy_beve_view tests
   // ============================================================================

   "lazy_beve_read_beve_overload_basic"_test = [] {
      lazy_beve_test::Container container{{"Alice", 30, true}, 1};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(container, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Use glz::read_beve directly with lazy_beve_view
      lazy_beve_test::User user{};
      auto read_ec = glz::read_beve(user, (*result)["user"]);

      expect(!read_ec);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   "lazy_beve_read_beve_overload_primitives"_test = [] {
      lazy_beve_test::PrimitiveData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // glz::read_beve works with primitive types
      int count{};
      auto ec1 = glz::read_beve(count, (*result)["count"]);
      expect(!ec1);
      expect(count == 42);

      double ratio{};
      auto ec2 = glz::read_beve(ratio, (*result)["ratio"]);
      expect(!ec2);
      expect(std::abs(ratio - 3.14) < 0.001);

      std::string name{};
      auto ec3 = glz::read_beve(name, (*result)["name"]);
      expect(!ec3);
      expect(name == "test");

      bool active{};
      auto ec4 = glz::read_beve(active, (*result)["active"]);
      expect(!ec4);
      expect(active == true);
   };

   "lazy_beve_read_beve_overload_vector"_test = [] {
      lazy_beve_test::ArrayData data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // glz::read_beve works with containers
      std::vector<int> values{};
      auto read_ec = glz::read_beve(values, (*result)["values"]);
      expect(!read_ec);
      expect(values.size() == 5u);
      expect(values[0] == 1);
      expect(values[4] == 5);
   };

   // ============================================================================
   // Iterator tests
   // ============================================================================

   "lazy_beve_object_iteration"_test = [] {
      std::map<std::string, int> data{{"a", 1}, {"b", 2}, {"c", 3}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      int64_t sum = 0;
      size_t count = 0;
      for (auto& item : result->root()) {
         auto val = item.get<int64_t>();
         if (val) sum += *val;
         ++count;
      }
      expect(sum == 6);
      expect(count == 3u);
   };

   "lazy_beve_generic_array_iteration"_test = [] {
      // Use a tuple to create a generic array with different types
      std::tuple<int, std::string, double> data{42, "hello", 3.14};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      size_t count = 0;
      for ([[maybe_unused]] auto& item : result->root()) {
         ++count;
      }
      expect(count == 3u);
   };

   // ============================================================================
   // Typed array tests
   // ============================================================================

   "lazy_beve_typed_array_size"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.is_array());
      expect(doc.root().size() == 10u);
      expect(!doc.root().empty());
   };

   "lazy_beve_string_array"_test = [] {
      std::vector<std::string> arr{"hello", "world", "test"};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 3u);
   };

   // ============================================================================
   // Large data tests
   // ============================================================================

   "lazy_beve_large_object"_test = [] {
      std::map<std::string, int> data;
      for (int i = 0; i < 100; ++i) {
         data["key" + std::to_string(i)] = i;
      }
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().size() == 100u);

      // Access specific keys
      expect(doc["key0"].get<int64_t>().value() == 0);
      expect(doc["key50"].get<int64_t>().value() == 50);
      expect(doc["key99"].get<int64_t>().value() == 99);
   };

   "lazy_beve_large_array"_test = [] {
      std::vector<int> arr;
      for (int i = 0; i < 1000; ++i) {
         arr.push_back(i);
      }
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().size() == 1000u);
   };

   // ============================================================================
   // Document copy/move tests
   // ============================================================================

   "lazy_beve_document_copy"_test = [] {
      lazy_beve_test::User user{"Alice", 30, true};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(user, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Test copy
      auto doc_copy = *result;
      expect(doc_copy["name"].get<std::string>().value() == "Alice");
      expect(doc_copy["age"].get<int64_t>().value() == 30);
   };

   "lazy_beve_document_move"_test = [] {
      lazy_beve_test::User user{"Bob", 25, false};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(user, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Test move
      auto doc_moved = std::move(*result);
      expect(doc_moved["name"].get<std::string>().value() == "Bob");
      expect(doc_moved["age"].get<int64_t>().value() == 25);
   };

   // ============================================================================
   // Number-keyed map tests (critical - verifies skip.hpp fixes)
   // ============================================================================

   "lazy_beve_number_keyed_map_iteration"_test = [] {
      std::map<int, std::string> num_map{{1, "one"}, {2, "two"}, {3, "three"}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(num_map, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Iterate over number-keyed map
      size_t count = 0;
      for (auto& item : result->root()) {
         auto val = item.get<std::string_view>();
         expect(val.has_value());
         ++count;
      }
      expect(count == 3u);
   };

   "lazy_beve_number_keyed_map_skip"_test = [] {
      // Test that skipping over a number-keyed map works correctly
      lazy_beve_test::NumMapContainer data;
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      // Access "after" field - this should skip over num_map correctly
      auto after = (*result)["after"].get<std::string_view>();
      expect(after.has_value());
      expect(after.value() == "after_map");
   };

   "lazy_beve_number_keyed_map_indexed"_test = [] {
      std::map<int, std::string> num_map{{10, "ten"}, {20, "twenty"}, {30, "thirty"}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(num_map, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.size() == 3u);

      // Access by position (number keys don't support string lookup)
      expect(indexed[0].get<std::string_view>().value() == "ten");
      expect(indexed[1].get<std::string_view>().value() == "twenty");
      expect(indexed[2].get<std::string_view>().value() == "thirty");
   };

   // ============================================================================
   // Boolean array tests
   // ============================================================================

   "lazy_beve_bool_array"_test = [] {
      std::vector<bool> bools{true, false, true, true, false};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(bools, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 5u);
   };

   // ============================================================================
   // Floating point array tests
   // ============================================================================

   "lazy_beve_float_array"_test = [] {
      std::vector<float> floats{1.5f, 2.5f, 3.5f};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(floats, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 3u);
   };

   "lazy_beve_double_array"_test = [] {
      std::vector<double> doubles{1.1, 2.2, 3.3, 4.4};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(doubles, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 4u);
   };

   // ============================================================================
   // Indexed view random access tests
   // ============================================================================

   "indexed_lazy_beve_view_random_access"_test = [] {
      // Use a generic array (tuple) for indexed access - typed arrays don't have per-element tags
      std::tuple<int, int, int, int, int> arr{10, 20, 30, 40, 50};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(arr, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();

      // Random access by position
      expect(indexed[0].get<int64_t>().value() == 10);
      expect(indexed[2].get<int64_t>().value() == 30);
      expect(indexed[4].get<int64_t>().value() == 50);

      // Out of order access
      expect(indexed[4].get<int64_t>().value() == 50);
      expect(indexed[1].get<int64_t>().value() == 20);
      expect(indexed[3].get<int64_t>().value() == 40);
   };

   "indexed_lazy_beve_iterator_arithmetic"_test = [] {
      // Use object for iterator arithmetic test - more practical use case
      std::map<std::string, int> data;
      for (int i = 1; i <= 10; ++i) {
         data["k" + std::to_string(i)] = i;
      }
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.size() == 10u);

      auto it = indexed.begin();

      // Test iterator arithmetic
      it += 3;
      // Values depend on map ordering, just verify no crash
      expect((*it).get<int64_t>().has_value());

      it -= 2;
      expect((*it).get<int64_t>().has_value());

      auto it2 = it + 5;
      expect((*it2).get<int64_t>().has_value());

      auto dist = indexed.end() - indexed.begin();
      expect(dist == 10);
   };

   // ============================================================================
   // Error handling tests
   // ============================================================================

   "lazy_beve_empty_buffer"_test = [] {
      std::vector<std::byte> empty_buffer;
      auto result = glz::lazy_beve(empty_buffer);
      expect(!result.has_value());
   };

   "lazy_beve_truncated_buffer"_test = [] {
      lazy_beve_test::User user{"Alice", 30, true};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(user, buffer);
      expect(ec == glz::error_code::none);

      // Truncate the buffer
      buffer.resize(buffer.size() / 2);

      auto result = glz::lazy_beve(buffer);
      // May or may not parse initially, but accessing fields should fail gracefully
      if (result.has_value()) {
         // Try to access - should handle gracefully
         [[maybe_unused]] auto name = (*result)["name"];
         // Don't crash - that's the important part
      }
   };

   "lazy_beve_missing_key"_test = [] {
      std::map<std::string, int> data{{"a", 1}};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(data, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto missing = (*result)["nonexistent"];
      expect(missing.has_error());
      expect(missing.error() == glz::error_code::key_not_found);
   };

   // ============================================================================
   // Unsigned integer array tests
   // ============================================================================

   "lazy_beve_uint_array"_test = [] {
      std::vector<uint32_t> uints{100u, 200u, 300u};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(uints, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 3u);
   };

   "lazy_beve_uint64_array"_test = [] {
      std::vector<uint64_t> uints{1ULL << 40, 1ULL << 50, 1ULL << 60};
      std::vector<std::byte> buffer;
      auto ec = glz::write_beve(uints, buffer);
      expect(ec == glz::error_code::none);

      auto result = glz::lazy_beve(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 3u);
   };
};

int main() { return 0; }
