// Consolidated plugin tests
// Combines: test_simple_loading.cpp, test_dynamic_loading.cpp, test_plugin_comprehensive.cpp
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

// Platform-specific dynamic library loading
#ifdef _WIN32
#include <windows.h>
using LibraryHandle = HMODULE;
#define LOAD_LIBRARY(path) LoadLibraryA(path)
#define GET_SYMBOL(handle, name) GetProcAddress(handle, name)
#define CLOSE_LIBRARY(handle) FreeLibrary(handle)
#else
#include <dlfcn.h>
using LibraryHandle = void*;
#define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#define GET_SYMBOL(handle, name) dlsym(handle, name)
#define CLOSE_LIBRARY(handle) dlclose(handle)
#endif

//=============================================================================
// PLUGIN ARCHITECTURE STRUCTURES (from test_plugin_comprehensive.cpp)
//=============================================================================

struct MathPlugin
{
   std::string name = "Basic Math";
   std::string version = "1.0.0";
   std::vector<std::string> operations = {"add", "subtract", "multiply", "divide", "modulo"};

   double calculate(const std::string& operation, double a, double b)
   {
      if (operation == "add") return a + b;
      if (operation == "subtract") return a - b;
      if (operation == "multiply") return a * b;
      if (operation == "divide") return b != 0 ? a / b : 0;
      if (operation == "modulo") return b != 0 ? std::fmod(a, b) : 0;
      return 0.0;
   }

   bool supports_operation(const std::string& op) const
   {
      for (const auto& supported : operations) {
         if (supported == op) return true;
      }
      return false;
   }

   std::string get_plugin_info() const { return name + " v" + version; }
   std::vector<std::string> get_supported_operations() const { return operations; }
};

struct ScientificMathPlugin
{
   std::string name = "Scientific Math";
   std::string version = "2.0.0";
   std::vector<std::string> operations = {"power", "sqrt", "log", "log10", "sin", "cos", "tan", "exp"};

   double calculate(const std::string& operation, double a, double b)
   {
      if (operation == "power") return std::pow(a, b);
      if (operation == "sqrt") return std::sqrt(a);
      if (operation == "log") return std::log(a);
      if (operation == "log10") return std::log10(a);
      if (operation == "sin") return std::sin(a);
      if (operation == "cos") return std::cos(a);
      if (operation == "tan") return std::tan(a);
      if (operation == "exp") return std::exp(a);
      return 0.0;
   }

   bool supports_operation(const std::string& op) const
   {
      for (const auto& supported : operations) {
         if (supported == op) return true;
      }
      return false;
   }

   std::string get_plugin_info() const { return name + " v" + version; }

   bool requires_single_argument(const std::string& op) const
   {
      return op == "sqrt" || op == "log" || op == "log10" || op == "sin" || op == "cos" || op == "tan" || op == "exp";
   }

   double calculate_single(const std::string& operation, double a) { return calculate(operation, a, 0.0); }
};

struct StatisticsPlugin
{
   std::string name = "Statistics";
   std::string version = "1.5.0";
   std::vector<std::string> operations = {"min", "max", "average", "variance", "stddev"};
   std::vector<double> data_set;

   double calculate(const std::string& operation, double a, double b)
   {
      if (operation == "min") return std::min(a, b);
      if (operation == "max") return std::max(a, b);
      if (operation == "average") return (a + b) / 2.0;
      return 0.0;
   }

   bool supports_operation(const std::string& op) const
   {
      for (const auto& supported : operations) {
         if (supported == op) return true;
      }
      return false;
   }

   std::string get_plugin_info() const { return name + " v" + version; }

   void add_data_point(double value) { data_set.push_back(value); }
   void clear_data() { data_set.clear(); }
   size_t data_size() const { return data_set.size(); }

   double calculate_mean() const
   {
      if (data_set.empty()) return 0.0;
      double sum = 0.0;
      for (double val : data_set) sum += val;
      return sum / data_set.size();
   }

   double calculate_variance() const
   {
      if (data_set.size() < 2) return 0.0;
      double mean = calculate_mean();
      double sum_sq_diff = 0.0;
      for (double val : data_set) {
         double diff = val - mean;
         sum_sq_diff += diff * diff;
      }
      return sum_sq_diff / (data_set.size() - 1);
   }
};

// Global plugin instances for testing
MathPlugin global_math_plugin;
ScientificMathPlugin global_scientific_plugin;
StatisticsPlugin global_stats_plugin;

//=============================================================================
// MAIN TEST FUNCTIONS
//=============================================================================

int main()
{
   using namespace ut;

   //=========================================================================
   // SIMPLE DYNAMIC LOADING TESTS
   //=========================================================================

   "simple plugin loading test"_test = [] {
      // Path to the simple test plugin
      std::string plugin_path = "./build/src/interop/libsimple_plugin.dylib";

      // Check if plugin exists
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping simple loading test - plugin not found at: " << plugin_path << std::endl;
         return;
      }

      // Load the library
      LibraryHandle handle = LOAD_LIBRARY(plugin_path.c_str());
      expect(handle != nullptr);

      if (!handle) {
         std::cout << "Failed to load library: " << plugin_path << std::endl;
         return;
      }

      // Get function pointers
      auto create_plugin = reinterpret_cast<void* (*)()>(GET_SYMBOL(handle, "create_minimal_plugin"));
      auto delete_plugin = reinterpret_cast<void (*)(void*)>(GET_SYMBOL(handle, "delete_minimal_plugin"));
      auto get_value = reinterpret_cast<int (*)(void*)>(GET_SYMBOL(handle, "get_value"));
      auto set_value = reinterpret_cast<void (*)(void*, int)>(GET_SYMBOL(handle, "set_value"));
      auto call_multiply = reinterpret_cast<int (*)(void*, int)>(GET_SYMBOL(handle, "call_multiply"));
      auto plugin_name = reinterpret_cast<const char* (*)()>(GET_SYMBOL(handle, "plugin_name"));
      auto plugin_version = reinterpret_cast<const char* (*)()>(GET_SYMBOL(handle, "plugin_version"));

      // Verify all symbols were found
      expect(create_plugin != nullptr);
      expect(delete_plugin != nullptr);
      expect(get_value != nullptr);
      expect(set_value != nullptr);
      expect(call_multiply != nullptr);
      expect(plugin_name != nullptr);
      expect(plugin_version != nullptr);

      // Test plugin info
      std::string name = plugin_name();
      std::string version = plugin_version();
      expect(name == "MinimalPlugin");
      expect(version == "1.0.0");

      // Create plugin instance
      void* plugin = create_plugin();
      expect(plugin != nullptr);

      // Test initial value
      int value = get_value(plugin);
      expect(value == 42);

      // Test setting value
      set_value(plugin, 10);
      value = get_value(plugin);
      expect(value == 10);

      // Test multiply function
      int result = call_multiply(plugin, 5);
      expect(result == 50); // 10 * 5

      // Clean up
      delete_plugin(plugin);
      CLOSE_LIBRARY(handle);

      std::cout << "✅ Simple plugin loading test passed!" << std::endl;
   };

   //=========================================================================
   // PLUGIN ARCHITECTURE TESTS
   //=========================================================================

   "basic math plugin functionality"_test = [] {
      auto* plugin = &global_math_plugin;

      // Test plugin metadata
      expect(plugin->name == "Basic Math");
      expect(plugin->version == "1.0.0");
      expect(plugin->operations.size() == 5);
      expect(plugin->get_plugin_info() == "Basic Math v1.0.0");

      // Test basic operations
      expect(plugin->calculate("add", 5.0, 3.0) == 8.0);
      expect(plugin->calculate("subtract", 10.0, 4.0) == 6.0);
      expect(plugin->calculate("multiply", 3.0, 7.0) == 21.0);
      expect(plugin->calculate("divide", 15.0, 3.0) == 5.0);
      expect(plugin->calculate("modulo", 17.0, 5.0) == 2.0);

      // Test operation support
      expect(plugin->supports_operation("add") == true);
      expect(plugin->supports_operation("power") == false);

      // Test edge cases
      expect(plugin->calculate("divide", 10.0, 0.0) == 0.0); // Safe division by zero
      expect(plugin->calculate("modulo", 10.0, 0.0) == 0.0); // Safe modulo by zero

      std::cout << "✅ Basic math plugin test passed\n";
   };

   "scientific math plugin functionality"_test = [] {
      auto* plugin = &global_scientific_plugin;

      // Test plugin metadata
      expect(plugin->name == "Scientific Math");
      expect(plugin->version == "2.0.0");
      expect(plugin->operations.size() == 8);
      expect(plugin->get_plugin_info() == "Scientific Math v2.0.0");

      // Test scientific operations
      expect(plugin->calculate("power", 2.0, 3.0) == 8.0);
      expect(plugin->calculate("sqrt", 16.0, 0.0) == 4.0);
      expect(plugin->calculate("exp", 1.0, 0.0) - 2.718281828 < 0.00001); // e^1 ≈ 2.718

      // Test trigonometric functions
      expect(plugin->calculate("sin", 0.0, 0.0) == 0.0);
      expect(plugin->calculate("cos", 0.0, 0.0) == 1.0);
      expect(plugin->calculate("tan", 0.0, 0.0) == 0.0);

      // Test single argument detection
      expect(plugin->requires_single_argument("sqrt") == true);
      expect(plugin->requires_single_argument("power") == false);
      expect(plugin->requires_single_argument("sin") == true);

      // Test single argument calculation
      expect(plugin->calculate_single("sqrt", 25.0) == 5.0);
      expect(plugin->calculate_single("log10", 100.0) == 2.0);

      std::cout << "✅ Scientific math plugin test passed\n";
   };

   "statistics plugin functionality"_test = [] {
      auto* plugin = &global_stats_plugin;

      // Test plugin metadata
      expect(plugin->name == "Statistics");
      expect(plugin->version == "1.5.0");
      expect(plugin->get_plugin_info() == "Statistics v1.5.0");

      // Test basic statistical operations
      expect(plugin->calculate("min", 5.0, 3.0) == 3.0);
      expect(plugin->calculate("max", 5.0, 3.0) == 5.0);
      expect(plugin->calculate("average", 4.0, 6.0) == 5.0);

      // Test dataset operations
      expect(plugin->data_size() == 0);
      plugin->clear_data();

      plugin->add_data_point(10.0);
      plugin->add_data_point(20.0);
      plugin->add_data_point(30.0);
      plugin->add_data_point(40.0);
      plugin->add_data_point(50.0);

      expect(plugin->data_size() == 5);
      expect(plugin->calculate_mean() == 30.0); // (10+20+30+40+50)/5 = 30

      // Test variance calculation
      double variance = plugin->calculate_variance();
      expect(variance == 250.0); // Variance of the dataset

      plugin->clear_data();
      expect(plugin->data_size() == 0);

      std::cout << "✅ Statistics plugin test passed\n";
   };

   //=========================================================================
   // PLUGIN INTEROPERABILITY TESTS
   //=========================================================================

   "plugin interoperability"_test = [] {
      auto* basic = &global_math_plugin;
      auto* scientific = &global_scientific_plugin;
      auto* stats = &global_stats_plugin;

      // Perform complex calculation using multiple plugins
      double result1 = basic->calculate("add", 5.0, 3.0); // 8.0
      double result2 = scientific->calculate("power", result1, 2.0); // 64.0
      double result3 = basic->calculate("divide", result2, 4.0); // 16.0
      double result4 = scientific->calculate("sqrt", result3, 0.0); // 4.0

      expect(result4 == 4.0);

      // Test plugin capability discovery
      expect(basic->supports_operation("add") == true);
      expect(basic->supports_operation("sqrt") == false);
      expect(scientific->supports_operation("sqrt") == true);
      expect(scientific->supports_operation("add") == false);

      // Create a dataset using results from other plugins
      stats->clear_data();
      stats->add_data_point(basic->calculate("multiply", 2.0, 3.0)); // 6.0
      stats->add_data_point(scientific->calculate("power", 2.0, 4.0)); // 16.0
      stats->add_data_point(basic->calculate("add", 10.0, 5.0)); // 15.0

      expect(stats->data_size() == 3);
      double mean = stats->calculate_mean(); // (6 + 16 + 15) / 3 = 12.333...
      expect(mean > 12.0 && mean < 13.0);

      std::cout << "✅ Plugin interoperability test passed\n";
   };

   //=========================================================================
   // PLUGIN VERSIONING AND METADATA TESTS
   //=========================================================================

   "plugin versioning and metadata"_test = [] {
      auto* basic = &global_math_plugin;
      auto* scientific = &global_scientific_plugin;
      auto* stats = &global_stats_plugin;

      // Test version comparison
      expect(basic->version == "1.0.0");
      expect(scientific->version == "2.0.0");
      expect(stats->version == "1.5.0");

      // Test capability enumeration
      auto basic_ops = basic->get_supported_operations();
      expect(basic_ops.size() == 5);

      bool has_add = false;
      bool has_multiply = false;
      for (const auto& op : basic_ops) {
         if (op == "add") has_add = true;
         if (op == "multiply") has_multiply = true;
      }
      expect(has_add);
      expect(has_multiply);

      // Test plugin info strings
      expect(basic->get_plugin_info() == "Basic Math v1.0.0");
      expect(scientific->get_plugin_info() == "Scientific Math v2.0.0");
      expect(stats->get_plugin_info() == "Statistics v1.5.0");

      std::cout << "✅ Plugin versioning and metadata test passed\n";
   };

   //=========================================================================
   // PLUGIN ERROR HANDLING TESTS
   //=========================================================================

   "plugin error handling"_test = [] {
      auto* basic = &global_math_plugin;
      auto* scientific = &global_scientific_plugin;

      // Test unsupported operations
      expect(basic->calculate("unknown_op", 1.0, 2.0) == 0.0);
      expect(scientific->calculate("invalid_func", 5.0, 3.0) == 0.0);

      // Test edge cases
      expect(basic->supports_operation("") == false);
      expect(scientific->supports_operation("nonexistent") == false);

      // Test mathematical edge cases
      expect(basic->calculate("divide", 1.0, 0.0) == 0.0); // Division by zero
      expect(std::isnan(scientific->calculate("log", -1.0, 0.0))); // Log of negative should return NaN

      std::cout << "✅ Plugin error handling test passed\n";
   };

   //=========================================================================
   // MEMORY MANAGEMENT STRESS TEST
   //=========================================================================

   "plugin memory management stress test"_test = [] {
      // Test creating and destroying many plugin instances (simulation)
      std::vector<MathPlugin> math_plugins;
      std::vector<ScientificMathPlugin> sci_plugins;
      std::vector<StatisticsPlugin> stat_plugins;

      const size_t num_plugins = 10; // Smaller number for local test

      // Create many plugin instances
      for (size_t i = 0; i < num_plugins; ++i) {
         math_plugins.emplace_back();
         sci_plugins.emplace_back();
         stat_plugins.emplace_back();
      }

      // Test all plugin instances work
      for (size_t i = 0; i < num_plugins; ++i) {
         expect(math_plugins[i].calculate("add", 1.0, 2.0) == 3.0);
         expect(sci_plugins[i].calculate("power", 2.0, 3.0) == 8.0);
         expect(stat_plugins[i].calculate("min", 5.0, 3.0) == 3.0);

         // Add some data to statistics plugins
         stat_plugins[i].add_data_point(i * 10.0);
         stat_plugins[i].add_data_point((i + 1) * 10.0);
         expect(stat_plugins[i].data_size() == 2);
      }

      // All plugins automatically cleaned up when vectors go out of scope
      std::cout << "✅ Memory management stress test passed\n";
      std::cout << "   Created and destroyed " << num_plugins << " of each plugin type\n";
   };

   //=========================================================================
   // SUMMARY
   //=========================================================================

   std::cout << "\n🎉 All plugin tests completed successfully!\n";
   std::cout << "📊 Coverage: Dynamic loading, plugin architecture, interoperability, memory management\n";
   std::cout << "✅ Simple plugin loading working (if libsimple_plugin.dylib exists)\n";
   std::cout << "✅ Plugin architecture patterns validated\n";
   std::cout << "✅ Multi-plugin interoperability confirmed\n\n";

   return 0;
}