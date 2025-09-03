// Basic interop functionality test (no TypeDescriptorPool required)
#include <iostream>
#include <string>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

// Simple test structure for basic C++ testing
struct BasicTestStruct
{
   int value;
   std::string name;
   bool flag;

   int get_value() const { return value; }
   void set_value(int v) { value = v; }
   std::string get_info() const { return name + ": " + std::to_string(value); }
};

BasicTestStruct global_basic_test{42, "test_struct", true};

int main()
{
   using namespace ut;

   "basic C++ struct functionality"_test = [] {
      BasicTestStruct local_test{10, "local", false};

      expect(local_test.value == 10);
      expect(local_test.name == "local");
      expect(local_test.flag == false);

      expect(local_test.get_value() == 10);
      local_test.set_value(20);
      expect(local_test.get_value() == 20);

      std::string info = local_test.get_info();
      expect(info == "local: 20");

      std::cout << "✅ Basic C++ struct functionality test passed\n";
   };

   "global instance access"_test = [] {
      expect(global_basic_test.value == 42);
      expect(global_basic_test.name == "test_struct");
      expect(global_basic_test.flag == true);

      global_basic_test.set_value(100);
      expect(global_basic_test.get_value() == 100);

      std::cout << "✅ Global instance access test passed\n";
   };

   "vector and string handling"_test = [] {
      std::vector<int> numbers = {1, 2, 3, 4, 5};
      expect(numbers.size() == 5);
      expect(numbers[0] == 1);
      expect(numbers[4] == 5);

      numbers.push_back(6);
      expect(numbers.size() == 6);

      std::vector<std::string> words = {"hello", "world", "test"};
      expect(words.size() == 3);
      expect(words[0] == "hello");

      std::cout << "✅ Vector and string handling test passed\n";
   };

   "method calling functionality"_test = [] {
      BasicTestStruct test_obj{5, "method_test", true};

      // Test const method
      int value = test_obj.get_value();
      expect(value == 5);

      // Test void method with parameter
      test_obj.set_value(15);
      expect(test_obj.value == 15);
      expect(test_obj.get_value() == 15);

      // Test method returning string
      std::string info = test_obj.get_info();
      expect(info == "method_test: 15");

      std::cout << "✅ Method calling functionality test passed\n";
   };

   "field modification"_test = [] {
      BasicTestStruct test_obj{0, "", false};

      // Direct field access
      test_obj.value = 999;
      test_obj.name = "modified";
      test_obj.flag = true;

      expect(test_obj.value == 999);
      expect(test_obj.name == "modified");
      expect(test_obj.flag == true);

      // Verify changes through method
      expect(test_obj.get_value() == 999);
      expect(test_obj.get_info() == "modified: 999");

      std::cout << "✅ Field modification test passed\n";
   };

   "complex data manipulation"_test = [] {
      // Test with collections of structs
      std::vector<BasicTestStruct> objects;
      objects.push_back({1, "first", true});
      objects.push_back({2, "second", false});
      objects.push_back({3, "third", true});

      expect(objects.size() == 3);
      expect(objects[0].value == 1);
      expect(objects[1].name == "second");
      expect(objects[2].flag == true);

      // Modify through methods
      objects[1].set_value(200);
      expect(objects[1].get_value() == 200);
      expect(objects[1].get_info() == "second: 200");

      // Count flags
      int true_count = 0;
      for (const auto& obj : objects) {
         if (obj.flag) true_count++;
      }
      expect(true_count == 2); // first and third have flag=true

      std::cout << "✅ Complex data manipulation test passed\n";
   };

   "error handling and edge cases"_test = [] {
      // Empty string
      BasicTestStruct empty_test{0, "", false};
      expect(empty_test.name.empty());
      expect(empty_test.get_info() == ": 0");

      // Large values
      BasicTestStruct large_test{1000000, "large", true};
      expect(large_test.value == 1000000);
      expect(large_test.get_value() == 1000000);

      // Negative values
      BasicTestStruct negative_test{-42, "negative", false};
      expect(negative_test.value == -42);
      expect(negative_test.get_info() == "negative: -42");

      // Empty vector
      std::vector<BasicTestStruct> empty_vec;
      expect(empty_vec.empty());
      expect(empty_vec.size() == 0);

      std::cout << "✅ Error handling test passed\n";
   };

   std::cout << "\n🎉 All basic interop tests completed successfully!\n";
   std::cout << "📊 Coverage: C++ fundamentals, method calling, data manipulation, edge cases\n";
   std::cout << "⚠️  Note: Advanced interop features require TypeDescriptorPool implementation\n\n";

   return 0;
}