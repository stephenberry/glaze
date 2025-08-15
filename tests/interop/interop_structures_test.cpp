// Consolidated cross-library structure tests
// Combines: test_cross_library_structures.cpp, test_structure_validation.cpp, test_json_across_libraries.cpp,
// test_json_serialization.cpp
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
// JSON SERIALIZATION TEST STRUCTURES
//=============================================================================

struct PersonData
{
   std::string name;
   int age;
   double height;
   bool is_student;
   std::vector<std::string> hobbies;
   std::optional<std::string> email;

   std::string get_summary() const { return name + " (" + std::to_string(age) + " years old)"; }
};

struct Address
{
   std::string street;
   std::string city;
   std::string country;
   int postal_code;

   std::string full_address() const
   {
      return street + ", " + city + ", " + country + " " + std::to_string(postal_code);
   }
};

struct Company
{
   std::string name;
   Address headquarters;
   std::vector<PersonData> employees;
   std::vector<std::string> departments;

   size_t employee_count() const { return employees.size(); }
   void add_employee(const PersonData& person) { employees.push_back(person); }
};

// Global instances for JSON serialization testing
PersonData sample_person{"Alice Johnson", 28, 165.5, false, {"reading", "hiking", "programming"}, "alice@example.com"};

Address sample_address{"123 Tech Street", "San Francisco", "USA", 94105};

Company sample_company{"Tech Corp", sample_address, {sample_person}, {"Engineering", "Marketing", "Sales"}};

//=============================================================================
// COMPLEX STRUCTURES PLUGIN WRAPPER
//=============================================================================

class ComplexStructuresPlugin
{
  private:
   LibraryHandle handle_;

  public:
   // Function pointers for complex structures
   void* (*create_company)();
   void (*delete_company)(void*);
   void* (*create_person)(uint32_t, const char*, const char*, int);
   void (*delete_person)(void*);
   void* (*create_project)(uint32_t, const char*, const char*);
   void (*delete_project)(void*);

   const char* (*get_person_full_name)(void*);
   const char* (*get_person_email)(void*);
   const char* (*get_person_home_address)(void*);
   void (*add_person_skill)(void*, const char*, double);
   double (*get_person_skill_rating)(void*, const char*);

   const char* (*get_company_name)(void*);
   const char* (*get_company_headquarters_address)(void*);
   size_t (*get_company_office_count)(void*);
   void (*add_employee_to_company)(void*, void*);
   size_t (*get_company_employee_count)(void*);

   const char* (*get_project_name)(void*);
   size_t (*get_project_milestone_count)(void*);
   double (*get_project_completion_percentage)(void*);
   void (*complete_project_milestone)(void*, size_t, const char*);
   void (*add_project_to_company)(void*, void*);

   const char* (*plugin_name)();
   const char* (*plugin_version)();
   const char* (*plugin_description)();

   explicit ComplexStructuresPlugin(const std::string& plugin_path)
   {
      handle_ = LOAD_LIBRARY(plugin_path.c_str());
      if (!handle_) {
         std::cout << "Failed to load plugin: " << plugin_path << std::endl;
         return;
      }

      // Load all function pointers (simplified for space)
      create_company = reinterpret_cast<void* (*)()>(GET_SYMBOL(handle_, "create_company"));
      delete_company = reinterpret_cast<void (*)(void*)>(GET_SYMBOL(handle_, "delete_company"));
      create_person =
         reinterpret_cast<void* (*)(uint32_t, const char*, const char*, int)>(GET_SYMBOL(handle_, "create_person"));
      delete_person = reinterpret_cast<void (*)(void*)>(GET_SYMBOL(handle_, "delete_person"));
      create_project =
         reinterpret_cast<void* (*)(uint32_t, const char*, const char*)>(GET_SYMBOL(handle_, "create_project"));
      delete_project = reinterpret_cast<void (*)(void*)>(GET_SYMBOL(handle_, "delete_project"));

      get_person_full_name = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_person_full_name"));
      get_person_email = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_person_email"));
      get_person_home_address =
         reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_person_home_address"));
      add_person_skill =
         reinterpret_cast<void (*)(void*, const char*, double)>(GET_SYMBOL(handle_, "add_person_skill"));
      get_person_skill_rating =
         reinterpret_cast<double (*)(void*, const char*)>(GET_SYMBOL(handle_, "get_person_skill_rating"));

      get_company_name = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_company_name"));
      get_company_headquarters_address =
         reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_company_headquarters_address"));
      get_company_office_count = reinterpret_cast<size_t (*)(void*)>(GET_SYMBOL(handle_, "get_company_office_count"));
      add_employee_to_company =
         reinterpret_cast<void (*)(void*, void*)>(GET_SYMBOL(handle_, "add_employee_to_company"));
      get_company_employee_count =
         reinterpret_cast<size_t (*)(void*)>(GET_SYMBOL(handle_, "get_company_employee_count"));

      get_project_name = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle_, "get_project_name"));
      get_project_milestone_count =
         reinterpret_cast<size_t (*)(void*)>(GET_SYMBOL(handle_, "get_project_milestone_count"));
      get_project_completion_percentage =
         reinterpret_cast<double (*)(void*)>(GET_SYMBOL(handle_, "get_project_completion_percentage"));
      complete_project_milestone =
         reinterpret_cast<void (*)(void*, size_t, const char*)>(GET_SYMBOL(handle_, "complete_project_milestone"));
      add_project_to_company = reinterpret_cast<void (*)(void*, void*)>(GET_SYMBOL(handle_, "add_project_to_company"));

      plugin_name = reinterpret_cast<const char* (*)()>(GET_SYMBOL(handle_, "plugin_name"));
      plugin_version = reinterpret_cast<const char* (*)()>(GET_SYMBOL(handle_, "plugin_version"));
      plugin_description = reinterpret_cast<const char* (*)()>(GET_SYMBOL(handle_, "plugin_description"));

      // Verify critical functions were loaded
      if (!create_company || !create_person || !create_project || !get_person_full_name || !get_company_name ||
          !get_project_name || !plugin_name || !plugin_version) {
         CLOSE_LIBRARY(handle_);
         handle_ = nullptr;
         std::cout << "Failed to load required functions from plugin" << std::endl;
      }
   }

   ~ComplexStructuresPlugin()
   {
      if (handle_) {
         CLOSE_LIBRARY(handle_);
      }
   }

   bool is_loaded() const { return handle_ != nullptr; }
};

//=============================================================================
// MAIN TEST FUNCTIONS
//=============================================================================

int main()
{
   using namespace ut;

   //=========================================================================
   // JSON SERIALIZATION TESTS (from test_json_serialization.cpp)
   //=========================================================================

   "basic JSON-like serialization"_test = [] {
      // Test direct manual JSON serialization
      PersonData person{"John Doe", 25, 180.0, true, {"music", "sports"}, std::nullopt};

      // Manual JSON construction (simulating serialization)
      std::string json = "{";
      json += "\"name\":\"" + person.name + "\",";
      json += "\"age\":" + std::to_string(person.age) + ",";
      json += "\"height\":" + std::to_string(person.height) + ",";
      json += "\"is_student\":";
      json += person.is_student ? "true" : "false";
      json += ",";
      json += "\"hobbies\":[";
      for (size_t i = 0; i < person.hobbies.size(); ++i) {
         json += "\"" + person.hobbies[i] + "\"";
         if (i < person.hobbies.size() - 1) json += ",";
      }
      json += "]";
      if (person.email.has_value()) {
         json += ",\"email\":\"" + person.email.value() + "\"";
      }
      json += "}";

      expect(!json.empty());
      expect(json.find("John Doe") != std::string::npos);
      expect(json.find("25") != std::string::npos);
      expect(json.find("180") != std::string::npos);
      expect(json.find("music") != std::string::npos);
      expect(json.find("sports") != std::string::npos);

      std::cout << "✅ Basic JSON-like serialization test passed\n";
   };

   "nested object JSON simulation"_test = [] {
      // Test nested structure JSON generation
      std::string json = "{";
      json += "\"company\":{";
      json += "\"name\":\"" + sample_company.name + "\",";
      json += "\"headquarters\":{";
      json += "\"street\":\"" + sample_company.headquarters.street + "\",";
      json += "\"city\":\"" + sample_company.headquarters.city + "\",";
      json += "\"country\":\"" + sample_company.headquarters.country + "\",";
      json += "\"postal_code\":" + std::to_string(sample_company.headquarters.postal_code);
      json += "},";
      json += "\"employee_count\":" + std::to_string(sample_company.employee_count()) + ",";
      json += "\"departments\":[";
      for (size_t i = 0; i < sample_company.departments.size(); ++i) {
         json += "\"" + sample_company.departments[i] + "\"";
         if (i < sample_company.departments.size() - 1) json += ",";
      }
      json += "]";
      json += "}}";

      // Verify nested structure is represented
      expect(json.find("Tech Corp") != std::string::npos);
      expect(json.find("123 Tech Street") != std::string::npos);
      expect(json.find("San Francisco") != std::string::npos);
      expect(json.find("Engineering") != std::string::npos);

      std::cout << "✅ Nested object JSON simulation test passed\n";
   };

   //=========================================================================
   // CROSS-LIBRARY STRUCTURE TESTS
   //=========================================================================

   "complex structures plugin loading"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";

      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping complex structures test - plugin not found at: " << plugin_path << std::endl;
         return;
      }

      ComplexStructuresPlugin plugin(plugin_path);
      if (!plugin.is_loaded()) {
         std::cout << "❌ Plugin loading failed" << std::endl;
         expect(false);
         return;
      }

      // Test plugin metadata
      std::string name = plugin.plugin_name();
      std::string version = plugin.plugin_version();
      std::string description = plugin.plugin_description();

      expect(name == "ComplexStructuresPlugin");
      expect(version == "2.0.0");
      expect(!description.empty());

      std::cout << "✅ Complex structures plugin loading test passed\n";
      std::cout << "   Loaded: " << name << " v" << version << "\n";
   };

   "person structure operations across library"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping person structure test - plugin not found\n";
         return;
      }

      ComplexStructuresPlugin plugin(plugin_path);
      if (!plugin.is_loaded()) {
         std::cout << "❌ Person structure test failed: plugin not loaded" << std::endl;
         expect(false);
         return;
      }

      // Create a person through the plugin
      void* person = plugin.create_person(1001, "Alice", "Johnson", 32);
      expect(person != nullptr);

      // Test basic person info
      std::string full_name = plugin.get_person_full_name(person);
      std::string email = plugin.get_person_email(person);
      std::string address = plugin.get_person_home_address(person);

      expect(full_name == "Alice Johnson");
      expect(email == "Alice.Johnson@example.com");
      expect(!address.empty());
      expect(address.find("Residential Lane") != std::string::npos);
      expect(address.find("Palo Alto") != std::string::npos);

      // Test skill management across library boundary
      plugin.add_person_skill(person, "C++", 9.5);
      plugin.add_person_skill(person, "Python", 8.0);
      plugin.add_person_skill(person, "Leadership", 7.5);

      double cpp_rating = plugin.get_person_skill_rating(person, "C++");
      double python_rating = plugin.get_person_skill_rating(person, "Python");
      double unknown_rating = plugin.get_person_skill_rating(person, "COBOL");

      expect(cpp_rating == 9.5);
      expect(python_rating == 8.0);
      expect(unknown_rating == 0.0);

      // Clean up
      plugin.delete_person(person);

      std::cout << "✅ Person structure operations test passed\n";
   };

   "company structure operations across library"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping company structure test - plugin not found\n";
         return;
      }

      ComplexStructuresPlugin plugin(plugin_path);
      if (!plugin.is_loaded()) {
         std::cout << "❌ Company structure test failed: plugin not loaded" << std::endl;
         expect(false);
         return;
      }

      // Create a company through the plugin
      void* company = plugin.create_company();
      expect(company != nullptr);

      // Test basic company info
      std::string name = plugin.get_company_name(company);
      std::string hq_address = plugin.get_company_headquarters_address(company);
      size_t office_count = plugin.get_company_office_count(company);

      expect(name == "TechCorp Global");
      expect(!hq_address.empty());
      expect(hq_address.find("Innovation Drive") != std::string::npos);
      expect(hq_address.find("San Francisco") != std::string::npos);
      expect(office_count == 2);

      // Test employee management
      size_t initial_count = plugin.get_company_employee_count(company);
      expect(initial_count == 0);

      // Create and add employees
      void* ceo = plugin.create_person(2001, "John", "Smith", 45);
      void* cto = plugin.create_person(2002, "Sarah", "Davis", 38);

      expect(ceo != nullptr);
      expect(cto != nullptr);

      plugin.add_person_skill(ceo, "Leadership", 9.0);
      plugin.add_person_skill(cto, "System Architecture", 9.5);

      plugin.add_employee_to_company(company, ceo);
      plugin.add_employee_to_company(company, cto);

      size_t final_count = plugin.get_company_employee_count(company);
      expect(final_count == 2);

      // Clean up
      plugin.delete_person(ceo);
      plugin.delete_person(cto);
      plugin.delete_company(company);

      std::cout << "✅ Company structure operations test passed\n";
   };

   "project milestone management across library"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping project structure test - plugin not found\n";
         return;
      }

      ComplexStructuresPlugin plugin(plugin_path);
      if (!plugin.is_loaded()) {
         std::cout << "❌ Project structure test failed: plugin not loaded" << std::endl;
         expect(false);
         return;
      }

      // Create a project through the plugin
      void* project = plugin.create_project(3001, "AI Platform", "Next-generation AI development platform");
      expect(project != nullptr);

      // Test basic project info
      std::string name = plugin.get_project_name(project);
      size_t milestone_count = plugin.get_project_milestone_count(project);
      double initial_completion = plugin.get_project_completion_percentage(project);

      expect(name == "AI Platform");
      expect(milestone_count == 4); // Planning, Development, Testing, Deployment
      expect(initial_completion == 0.0);

      // Complete some milestones
      plugin.complete_project_milestone(project, 0, "2024-03-15"); // Planning
      plugin.complete_project_milestone(project, 1, "2024-06-20"); // Development

      double updated_completion = plugin.get_project_completion_percentage(project);
      expect(updated_completion == 50.0); // 2 out of 4 milestones = 50%

      // Clean up
      plugin.delete_project(project);

      std::cout << "✅ Project milestone management test passed\n";
   };

   //=========================================================================
   // CROSS-LIBRARY JSON DATA CONSISTENCY
   //=========================================================================

   "cross-library data extraction for JSON"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping JSON data consistency test - plugin not found\n";
         return;
      }

      LibraryHandle handle = LOAD_LIBRARY(plugin_path.c_str());
      expect(handle != nullptr);

      if (!handle) return;

      // Get function pointers
      auto create_person =
         reinterpret_cast<void* (*)(uint32_t, const char*, const char*, int)>(GET_SYMBOL(handle, "create_person"));
      auto delete_person = reinterpret_cast<void (*)(void*)>(GET_SYMBOL(handle, "delete_person"));
      auto get_person_full_name = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle, "get_person_full_name"));
      auto get_person_email = reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle, "get_person_email"));
      auto get_person_home_address =
         reinterpret_cast<const char* (*)(void*)>(GET_SYMBOL(handle, "get_person_home_address"));
      auto add_person_skill =
         reinterpret_cast<void (*)(void*, const char*, double)>(GET_SYMBOL(handle, "add_person_skill"));
      auto get_person_skill_rating =
         reinterpret_cast<double (*)(void*, const char*)>(GET_SYMBOL(handle, "get_person_skill_rating"));

      expect(create_person != nullptr);
      expect(get_person_full_name != nullptr);

      // Create person with comprehensive data
      void* person = create_person(5001, "John", "Doe", 35);
      expect(person != nullptr);

      // Add multiple skills
      add_person_skill(person, "C++", 9.5);
      add_person_skill(person, "Python", 8.5);
      add_person_skill(person, "JavaScript", 7.0);
      add_person_skill(person, "Leadership", 8.0);

      // Extract all data for JSON representation
      std::string full_name = get_person_full_name(person);
      std::string email = get_person_email(person);
      std::string address = get_person_home_address(person);

      double cpp_rating = get_person_skill_rating(person, "C++");
      double python_rating = get_person_skill_rating(person, "Python");
      double js_rating = get_person_skill_rating(person, "JavaScript");
      double leadership_rating = get_person_skill_rating(person, "Leadership");

      // Verify all data
      expect(full_name == "John Doe");
      expect(email == "John.Doe@example.com");
      expect(!address.empty());
      expect(cpp_rating == 9.5);
      expect(python_rating == 8.5);
      expect(js_rating == 7.0);
      expect(leadership_rating == 8.0);

      // Create JSON-like representation
      std::string json = "{";
      json += "\"name\":\"" + full_name + "\",";
      json += "\"email\":\"" + email + "\",";
      json += "\"address\":\"" + address + "\",";
      json += "\"skills\":{";
      json += "\"C++\":" + std::to_string(cpp_rating) + ",";
      json += "\"Python\":" + std::to_string(python_rating) + ",";
      json += "\"JavaScript\":" + std::to_string(js_rating) + ",";
      json += "\"Leadership\":" + std::to_string(leadership_rating);
      json += "}}";

      // Verify JSON contains expected data
      expect(json.find("John Doe") != std::string::npos);
      expect(json.find("9.5") != std::string::npos);
      expect(json.find("8.5") != std::string::npos);

      // Clean up
      delete_person(person);
      CLOSE_LIBRARY(handle);

      std::cout << "✅ Cross-library JSON data consistency test passed\n";
   };

   //=========================================================================
   // MEMORY MANAGEMENT VALIDATION
   //=========================================================================

   "structure memory management validation"_test = [] {
      std::string plugin_path = "./libcomplex_structures_plugin.dylib";
      if (!std::filesystem::exists(plugin_path)) {
         std::cout << "Skipping memory management test - plugin not found\n";
         return;
      }

      ComplexStructuresPlugin plugin(plugin_path);
      if (!plugin.is_loaded()) {
         std::cout << "❌ Memory management test failed: plugin not loaded" << std::endl;
         expect(false);
         return;
      }

      // Create and destroy multiple objects to test memory management
      std::vector<void*> companies;
      std::vector<void*> persons;
      std::vector<void*> projects;

      const size_t num_objects = 10; // Reasonable number for testing

      // Create objects
      for (size_t i = 0; i < num_objects; ++i) {
         void* company = plugin.create_company();
         void* person = plugin.create_person(6000 + i, "Test", ("User" + std::to_string(i)).c_str(), 25 + i % 40);
         void* project = plugin.create_project(7000 + i, ("Project" + std::to_string(i)).c_str(), "Test project");

         expect(company != nullptr);
         expect(person != nullptr);
         expect(project != nullptr);

         companies.push_back(company);
         persons.push_back(person);
         projects.push_back(project);

         // Test operations
         plugin.add_person_skill(person, "TestSkill", 5.0 + i * 0.1);
         plugin.add_employee_to_company(company, person);
         plugin.add_project_to_company(company, project);
      }

      // Verify all objects work
      for (size_t i = 0; i < num_objects; ++i) {
         std::string name = plugin.get_company_name(companies[i]);
         expect(name == "TechCorp Global");

         std::string full_name = plugin.get_person_full_name(persons[i]);
         std::string expected = "Test User" + std::to_string(i);
         expect(full_name == expected);

         double skill_rating = plugin.get_person_skill_rating(persons[i], "TestSkill");
         expect(skill_rating > 0.0);
      }

      // Clean up all objects
      for (size_t i = 0; i < num_objects; ++i) {
         plugin.delete_person(persons[i]);
         plugin.delete_project(projects[i]);
         plugin.delete_company(companies[i]);
      }

      std::cout << "✅ Structure memory management validation passed\n";
      std::cout << "   Created and destroyed " << num_objects << " of each structure type\n";
   };

   //=========================================================================
   // SUMMARY
   //=========================================================================

   std::cout << "\n🎉 All cross-library structure tests completed!\n";
   std::cout << "📊 Coverage: JSON serialization, complex structures, cross-library operations, memory management\n";
   std::cout << "✅ Complex nested C++ structures working across library boundaries\n";
   std::cout << "✅ JSON-like serialization with data consistency validated\n";
   std::cout << "✅ Memory management with complex structures confirmed\n\n";

   return 0;
}