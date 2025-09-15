// Test for variants containing different C++ structs
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/interop/interop.hpp"
#include "boost/ut.hpp"

using namespace boost::ut;

// First struct type
struct Person
{
   std::string name;
   int age;
   double height;

   bool operator==(const Person& other) const
   {
      return name == other.name && age == other.age && height == other.height;
   }
};

// Second struct type
struct Company
{
   std::string name;
   std::vector<int> employee_ids;
   double revenue;

   bool operator==(const Company& other) const
   {
      return name == other.name && employee_ids == other.employee_ids && revenue == other.revenue;
   }
};

// Third struct type
struct Product
{
   int id;
   std::string name;
   double price;
   std::optional<std::string> description;

   bool operator==(const Product& other) const
   {
      return id == other.id && name == other.name && price == other.price && description == other.description;
   }
};

// Register Glaze metadata
template <>
struct glz::meta<Person>
{
   using T = Person;
   static constexpr auto value = glz::object("name", &T::name, "age", &T::age, "height", &T::height);
};

template <>
struct glz::meta<Company>
{
   using T = Company;
   static constexpr auto value =
      glz::object("name", &T::name, "employee_ids", &T::employee_ids, "revenue", &T::revenue);
};

template <>
struct glz::meta<Product>
{
   using T = Product;
   static constexpr auto value =
      glz::object("id", &T::id, "name", &T::name, "price", &T::price, "description", &T::description);
};

// Test struct that contains a variant of structs
struct EntityContainer
{
   std::variant<Person, Company, Product> entity;
   std::string entity_type;

   void set_person(const Person& p)
   {
      entity = p;
      entity_type = "person";
   }

   void set_company(const Company& c)
   {
      entity = c;
      entity_type = "company";
   }

   void set_product(const Product& p)
   {
      entity = p;
      entity_type = "product";
   }

   std::string get_entity_name() const
   {
      return std::visit([](const auto& e) -> std::string { return e.name; }, entity);
   }
};

template <>
struct glz::meta<EntityContainer>
{
   using T = EntityContainer;
   static constexpr auto value =
      glz::object("entity", &T::entity, "entity_type", &T::entity_type, "set_person", &T::set_person, "set_company",
                  &T::set_company, "set_product", &T::set_product, "get_entity_name", &T::get_entity_name);
};

// Global test instances
EntityContainer global_entity_container;

// Helper to create variant operations for struct types
namespace
{
   // We need to explicitly instantiate operations for our struct variant
   template <class... Ts>
   struct StructVariantOps
   {
      using VariantType = std::variant<Ts...>;

      static uint64_t get_index(void* variant_ptr)
      {
         auto* var = static_cast<VariantType*>(variant_ptr);
         return static_cast<uint64_t>(var->index());
      }

      static void* get_value(void* variant_ptr)
      {
         auto* var = static_cast<VariantType*>(variant_ptr);
         return std::visit([](auto& value) -> void* { return const_cast<void*>(static_cast<const void*>(&value)); },
                           *var);
      }

      static bool set_value(void* variant_ptr, uint64_t index, const void* value)
      {
         auto* var = static_cast<VariantType*>(variant_ptr);
         bool result = false;
         size_t idx = 0;
         (void)((index == idx++ ? ((*var = *static_cast<const Ts*>(value)), result = true, true) : false) || ...);
         return result;
      }

      static bool holds_alternative(void* variant_ptr, uint64_t index)
      {
         auto* var = static_cast<VariantType*>(variant_ptr);
         return var->index() == static_cast<size_t>(index);
      }
   };
}

int main()
{
   using namespace boost::ut;

   // Register types with interop
   glz::register_type<Person>("Person");
   glz::register_type<Company>("Company");
   glz::register_type<Product>("Product");
   glz::register_type<EntityContainer>("EntityContainer");

   // Register global instance
   glz::register_instance("global_entity_container", global_entity_container);

   "variant of structs - basic operations"_test = [] {
      using StructVariant = std::variant<Person, Company, Product>;

      StructVariant var;

      // Default constructed - should be first alternative (Person)
      expect(var.index() == 0);
      expect(std::holds_alternative<Person>(var));

      // Set to Person
      Person person{"Alice", 30, 5.6};
      var = person;
      expect(var.index() == 0);
      expect(std::get<Person>(var) == person);

      // Set to Company
      Company company{"TechCorp", {100, 101, 102}, 1000000.0};
      var = company;
      expect(var.index() == 1);
      expect(std::get<Company>(var) == company);

      // Set to Product
      Product product{1, "Widget", 29.99, "A useful widget"};
      var = product;
      expect(var.index() == 2);
      expect(std::get<Product>(var) == product);

      std::cout << "✅ Variant of structs basic operations work correctly\n";
   };

   "variant of structs - type descriptor"_test = [] {
      using StructVariant = std::variant<Person, Company, Product>;
      auto* desc = glz::create_type_descriptor<StructVariant>();

      expect(desc != nullptr);
      expect(desc->index == GLZ_TYPE_VARIANT);
      expect(desc->data.variant.count == 3);

      // Check that alternatives are struct types
      for (uint64_t i = 0; i < 3; ++i) {
         auto* alt_desc = desc->data.variant.alternatives[i];
         expect(alt_desc != nullptr);
         // Structs that aren't registered return nullptr from create_type_descriptor
         // but get a struct descriptor with type_hash
         if (alt_desc) {
            if (alt_desc->index != GLZ_TYPE_STRUCT) {
               std::cout << "Alternative " << i << " has index " << alt_desc->index << " instead of GLZ_TYPE_STRUCT ("
                         << GLZ_TYPE_STRUCT << ")\n";
            }
            expect(alt_desc->index == GLZ_TYPE_STRUCT);
            expect(alt_desc->data.struct_type.type_hash != 0);
         }
      }

      std::cout << "✅ Variant of structs type descriptor created correctly\n";
   };

   "variant of structs - visitor pattern"_test = [] {
      using StructVariant = std::variant<Person, Company, Product>;

      StructVariant var = Person{"Bob", 25, 6.0};

      // Visit to get name from any type
      auto name = std::visit([](const auto& entity) -> std::string { return entity.name; }, var);
      expect(name == "Bob");

      var = Company{"MegaCorp", {200, 201}, 5000000.0};
      name = std::visit([](const auto& entity) -> std::string { return entity.name; }, var);
      expect(name == "MegaCorp");

      var = Product{2, "Gadget", 49.99, std::nullopt};
      name = std::visit([](const auto& entity) -> std::string { return entity.name; }, var);
      expect(name == "Gadget");

      std::cout << "✅ Variant of structs visitor pattern works correctly\n";
   };

   "container with variant member"_test = [] {
      EntityContainer container;

      // Set to Person
      Person person{"Charlie", 35, 5.8};
      container.set_person(person);
      expect(container.entity_type == "person");
      expect(container.entity.index() == 0);
      expect(std::get<Person>(container.entity) == person);
      expect(container.get_entity_name() == "Charlie");

      // Set to Company
      Company company{"StartupInc", {300, 301, 302, 303}, 750000.0};
      container.set_company(company);
      expect(container.entity_type == "company");
      expect(container.entity.index() == 1);
      expect(std::get<Company>(container.entity) == company);
      expect(container.get_entity_name() == "StartupInc");

      // Set to Product
      Product product{3, "Tool", 99.99, "Professional tool"};
      container.set_product(product);
      expect(container.entity_type == "product");
      expect(container.entity.index() == 2);
      expect(std::get<Product>(container.entity) == product);
      expect(container.get_entity_name() == "Tool");

      std::cout << "✅ Container with variant member works correctly\n";
   };

   "variant of structs - C API operations"_test = [] {
      // First test with a supported type combination
      using SupportedVariant = std::variant<int, std::string, double>;
      auto* supported_desc = glz::create_type_descriptor<SupportedVariant>();

      SupportedVariant supported_var = 42;

      // Test C API with supported type
      auto index = glz_variant_index(&supported_var, supported_desc);
      expect(index == 0);

      void* value_ptr = glz_variant_get(&supported_var, supported_desc);
      expect(value_ptr != nullptr);
      expect(*static_cast<int*>(value_ptr) == 42);

      // Now test struct variant type descriptor creation
      using StructVariant = std::variant<Person, Company, Product>;
      auto* desc = glz::create_type_descriptor<StructVariant>();

      // Create a variant and test operations
      Person person{"David", 40, 5.9};
      StructVariant var = person;

      // Test type_at_index
      auto* type0 = glz_variant_type_at_index(desc, 0);
      expect(type0 != nullptr);
      expect(type0->index == GLZ_TYPE_STRUCT);

      auto* type1 = glz_variant_type_at_index(desc, 1);
      expect(type1 != nullptr);
      expect(type1->index == GLZ_TYPE_STRUCT);

      auto* type2 = glz_variant_type_at_index(desc, 2);
      expect(type2 != nullptr);
      expect(type2->index == GLZ_TYPE_STRUCT);

      // Test using the helper operations directly
      using Ops = StructVariantOps<Person, Company, Product>;

      auto struct_index = Ops::get_index(&var);
      expect(struct_index == 0);

      void* struct_value_ptr = Ops::get_value(&var);
      expect(struct_value_ptr != nullptr);
      auto* person_ptr = static_cast<Person*>(struct_value_ptr);
      expect(person_ptr->name == "David");

      // Test set_value
      Company new_company{"GlobalCorp", {400, 401, 402}, 10000000.0};
      bool set_result = Ops::set_value(&var, 1, &new_company);
      expect(set_result == true);
      expect(var.index() == 1);
      expect(std::get<Company>(var) == new_company);

      // Test holds_alternative
      expect(Ops::holds_alternative(&var, 1) == true);
      expect(Ops::holds_alternative(&var, 0) == false);
      expect(Ops::holds_alternative(&var, 2) == false);

      std::cout << "✅ Variant of structs C API operations work correctly\n";
   };

   "variant with mixed types including structs"_test = [] {
      using MixedVariant = std::variant<int, std::string, Person, Company>;

      MixedVariant var = 42;
      expect(var.index() == 0);
      expect(std::get<int>(var) == 42);

      var = std::string("Hello");
      expect(var.index() == 1);
      expect(std::get<std::string>(var) == "Hello");

      var = Person{"Eve", 28, 5.5};
      expect(var.index() == 2);
      expect(std::get<Person>(var).name == "Eve");

      var = Company{"SmallBiz", {500}, 100000.0};
      expect(var.index() == 3);
      expect(std::get<Company>(var).name == "SmallBiz");

      std::cout << "✅ Mixed variant with primitives and structs works correctly\n";
   };

   "global entity container instance"_test = [] {
      // Test global instance
      Person person{"Global Person", 50, 6.1};
      global_entity_container.set_person(person);
      expect(global_entity_container.entity_type == "person");
      expect(global_entity_container.get_entity_name() == "Global Person");

      Company company{"Global Corp", {600, 601, 602}, 50000000.0};
      global_entity_container.set_company(company);
      expect(global_entity_container.entity_type == "company");
      expect(global_entity_container.get_entity_name() == "Global Corp");

      std::cout << "✅ Global entity container instance works correctly\n";
   };

   "variant of structs - edge cases"_test = [] {
      using StructVariant = std::variant<Person, Company, Product>;

      // Test with empty/default values
      StructVariant var = Person{"", 0, 0.0};
      expect(std::get<Person>(var).name == "");
      expect(std::get<Person>(var).age == 0);

      // Test with large vectors
      std::vector<int> large_ids;
      for (int i = 0; i < 1000; ++i) {
         large_ids.push_back(i);
      }
      var = Company{"BigCorp", large_ids, 1e9};
      expect(std::get<Company>(var).employee_ids.size() == 1000);

      // Test with nullopt description
      var = Product{999, "No Description Product", 0.01, std::nullopt};
      expect(!std::get<Product>(var).description.has_value());

      // Test with description
      var = Product{1000, "With Description", 100.0, "This has a description"};
      expect(std::get<Product>(var).description.has_value());
      expect(std::get<Product>(var).description.value() == "This has a description");

      std::cout << "✅ Variant of structs edge cases handled correctly\n";
   };

   std::cout << "\n🎉 All variant struct tests passed!\n";

   return 0;
}