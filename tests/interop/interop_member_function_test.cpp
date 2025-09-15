// Member function discovery and invocation test
// Tests runtime function discovery patterns for Julia interop
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "glaze/interop/interop.hpp"
#include "boost/ut.hpp"

using namespace boost::ut;

// Calculator with various function signatures
struct Calculator
{
   double value = 0.0;
   std::string last_operation;

   // Different return types
   double add(double a, double b)
   {
      value = a + b;
      last_operation = "add";
      return value;
   }

   int multiply_int(int a, int b) { return a * b; }

   std::string describe() const { return "Calculator with value: " + std::to_string(value); }

   // Void return
   void reset()
   {
      value = 0.0;
      last_operation = "reset";
   }

   // Vector return
   std::vector<double> generate_sequence(size_t n)
   {
      std::vector<double> result;
      for (size_t i = 0; i < n; ++i) {
         result.push_back(value + i);
      }
      return result;
   }

   // Optional return
   std::optional<double> safe_divide(double a, double b)
   {
      if (b == 0.0) return std::nullopt;
      return a / b;
   }

   // Multiple parameters
   double polynomial(double x, double a, double b, double c) { return a * x * x + b * x + c; }

   // Const member function
   bool is_positive() const { return value > 0; }

   // Async operation
   std::shared_future<double> compute_async(double x)
   {
      return std::async(std::launch::async,
                        [x]() {
                           std::this_thread::sleep_for(std::chrono::milliseconds(10));
                           return x * x;
                        })
         .share();
   }
};

template <>
struct glz::meta<Calculator>
{
   using T = Calculator;
   static constexpr auto value =
      object("value", &T::value, "last_operation", &T::last_operation, "add", &T::add, "multiply_int", &T::multiply_int,
             "describe", &T::describe, "reset", &T::reset, "generate_sequence", &T::generate_sequence, "safe_divide",
             &T::safe_divide, "polynomial", &T::polynomial, "is_positive", &T::is_positive, "compute_async",
             &T::compute_async);
};

// Helper to find member by name
const glz_member_info* find_member(const glz_type_info* info, const std::string& name)
{
   for (size_t i = 0; i < info->member_count; ++i) {
      if (std::string(info->members[i].name) == name) {
         return &info->members[i];
      }
   }
   return nullptr;
}

int main()
{
   using namespace boost::ut;

   // Register the type
   glz::register_type<Calculator>("Calculator");

   "function discovery"_test = [] {
      auto* type_info = glz_get_type_info("Calculator");
      expect(type_info != nullptr);

      // Count functions vs data members
      size_t function_count = 0;
      size_t data_count = 0;

      for (size_t i = 0; i < type_info->member_count; ++i) {
         auto* member = &type_info->members[i];
         if (member->kind == 1) { // MEMBER_FUNCTION
            function_count++;
            std::cout << "  Found function: " << member->name << "\n";
         }
         else {
            data_count++;
            std::cout << "  Found data: " << member->name << "\n";
         }
      }

      expect(function_count == 9); // All member functions
      expect(data_count == 2); // value and last_operation

      std::cout << "✅ Function discovery test passed\n";
   };

   "function invocation with return value"_test = [] {
      Calculator calc;
      calc.value = 10.0;

      auto* type_info = glz_get_type_info("Calculator");
      auto* add_member = find_member(type_info, "add");
      expect(add_member != nullptr);
      expect(add_member->kind == 1); // Function

      // Prepare arguments
      double arg1 = 5.0;
      double arg2 = 3.0;
      void* args[] = {&arg1, &arg2};

      // Prepare result buffer
      double result;

      // Call function
      void* ret = glz_call_member_function_with_type(&calc, "Calculator", add_member, args, &result);

      expect(ret == &result);
      expect(result == 8.0);
      expect(calc.value == 8.0);
      expect(calc.last_operation == "add");

      std::cout << "✅ Function invocation with return value test passed\n";
   };

   "void function invocation"_test = [] {
      Calculator calc;
      calc.value = 42.0;

      auto* type_info = glz_get_type_info("Calculator");
      auto* reset_member = find_member(type_info, "reset");
      expect(reset_member != nullptr);

      // Call void function
      glz_call_member_function_with_type(&calc, "Calculator", reset_member, nullptr, nullptr);

      expect(calc.value == 0.0);
      expect(calc.last_operation == "reset");

      std::cout << "✅ Void function invocation test passed\n";
   };

   "const function invocation"_test = [] {
      Calculator calc;
      calc.value = 10.0;

      auto* type_info = glz_get_type_info("Calculator");
      auto* is_positive_member = find_member(type_info, "is_positive");
      expect(is_positive_member != nullptr);

      // Check type descriptor for const
      auto* func_desc = is_positive_member->type;
      expect(func_desc->index == GLZ_TYPE_FUNCTION);
      expect(func_desc->data.function.is_const == 1);

      // Call const function
      bool result;
      glz_call_member_function_with_type(&calc, "Calculator", is_positive_member, nullptr, &result);

      expect(result == true);

      // Test with negative value
      calc.value = -5.0;
      glz_call_member_function_with_type(&calc, "Calculator", is_positive_member, nullptr, &result);

      expect(result == false);

      std::cout << "✅ Const function invocation test passed\n";
   };

   "vector return type"_test = [] {
      Calculator calc;
      calc.value = 10.0;

      auto* type_info = glz_get_type_info("Calculator");
      auto* gen_member = find_member(type_info, "generate_sequence");
      expect(gen_member != nullptr);

      size_t n = 5;
      void* args[] = {&n};

      // Result will be a vector
      std::vector<double> result;

      glz_call_member_function_with_type(&calc, "Calculator", gen_member, args, &result);

      expect(result.size() == 5);
      expect(result[0] == 10.0);
      expect(result[1] == 11.0);
      expect(result[2] == 12.0);
      expect(result[3] == 13.0);
      expect(result[4] == 14.0);

      std::cout << "✅ Vector return type test passed\n";
   };

   "optional return type"_test = [] {
      Calculator calc;

      auto* type_info = glz_get_type_info("Calculator");
      auto* divide_member = find_member(type_info, "safe_divide");
      expect(divide_member != nullptr);

      // Test successful division
      double a = 10.0;
      double b = 2.0;
      void* args[] = {&a, &b};

      std::optional<double> result;
      glz_call_member_function_with_type(&calc, "Calculator", divide_member, args, &result);

      expect(result.has_value());
      expect(result.value() == 5.0);

      // Test division by zero
      b = 0.0;
      glz_call_member_function_with_type(&calc, "Calculator", divide_member, args, &result);

      expect(!result.has_value());

      std::cout << "✅ Optional return type test passed\n";
   };

   "multiple parameters"_test = [] {
      Calculator calc;

      auto* type_info = glz_get_type_info("Calculator");
      auto* poly_member = find_member(type_info, "polynomial");
      expect(poly_member != nullptr);

      // Check parameter count
      auto* func_desc = poly_member->type;
      expect(func_desc->data.function.param_count == 4);

      // Test polynomial: 2x² + 3x + 1 at x=2
      double x = 2.0;
      double a = 2.0;
      double b = 3.0;
      double c = 1.0;
      void* args[] = {&x, &a, &b, &c};

      double result;
      glz_call_member_function_with_type(&calc, "Calculator", poly_member, args, &result);

      // 2*4 + 3*2 + 1 = 8 + 6 + 1 = 15
      expect(result == 15.0);

      std::cout << "✅ Multiple parameters test passed\n";
   };

   "async function return"_test = [] {
      Calculator calc;

      auto* type_info = glz_get_type_info("Calculator");
      auto* async_member = find_member(type_info, "compute_async");
      expect(async_member != nullptr);

      // Check return type is shared_future
      auto* func_desc = async_member->type;
      expect(func_desc->data.function.return_type->index == GLZ_TYPE_SHARED_FUTURE);

      double x = 5.0;
      void* args[] = {&x};

      // Call async function - returns a shared_future wrapper
      void* future_wrapper = glz_call_member_function_with_type(&calc, "Calculator", async_member, args, nullptr);

      expect(future_wrapper != nullptr);

      // Check if ready
      bool ready = glz_shared_future_is_ready(future_wrapper);

      // Wait for completion
      glz_shared_future_wait(future_wrapper);

      // Get result
      auto* result_ptr =
         glz_shared_future_get(future_wrapper, func_desc->data.function.return_type->data.shared_future.value_type);

      expect(result_ptr != nullptr);
      double result = *static_cast<double*>(result_ptr);
      expect(result == 25.0); // 5² = 25

      // Clean up
      delete static_cast<double*>(result_ptr);
      glz_shared_future_destroy(future_wrapper, func_desc->data.function.return_type->data.shared_future.value_type);

      std::cout << "✅ Async function return test passed\n";
   };

   "function type descriptors"_test = [] {
      auto* type_info = glz_get_type_info("Calculator");

      // Check each function's type descriptor
      for (size_t i = 0; i < type_info->member_count; ++i) {
         auto* member = &type_info->members[i];
         if (member->kind == 1) { // Function
            auto* desc = member->type;
            expect(desc != nullptr);
            expect(desc->index == GLZ_TYPE_FUNCTION);

            // Verify function has proper descriptor
            if (std::string(member->name) == "add") {
               expect(desc->data.function.param_count == 2);
               expect(desc->data.function.return_type != nullptr);
               expect(desc->data.function.is_const == 0);
            }
            else if (std::string(member->name) == "is_positive") {
               expect(desc->data.function.param_count == 0);
               expect(desc->data.function.is_const == 1);
            }
         }
      }

      std::cout << "✅ Function type descriptors test passed\n";
   };

   "error handling"_test = [] {
      Calculator calc;
      auto* type_info = glz_get_type_info("Calculator");
      auto* add_member = find_member(type_info, "add");

      // Test null instance
      double a = 1.0, b = 2.0;
      void* args[] = {&a, &b};
      double result;

      auto* ret = glz_call_member_function_with_type(nullptr, "Calculator", add_member, args, &result);
      expect(ret == nullptr);

      // Test null member
      ret = glz_call_member_function_with_type(&calc, "Calculator", nullptr, args, &result);
      expect(ret == nullptr);

      // Test wrong type name
      ret = glz_call_member_function_with_type(&calc, "WrongType", add_member, args, &result);
      expect(ret == nullptr);

      std::cout << "✅ Error handling test passed\n";
   };

   std::cout << "\n🎉 All member function tests completed successfully!\n";
   std::cout << "📊 Coverage: discovery, invocation, return types, async, error handling\n";
   std::cout << "✅ Complete member function support for Julia interop\n\n";

   return 0;
}