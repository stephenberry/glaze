// Glaze Library
// C++26 P2996 Reflection Test for Non-Aggregate Types
// Tests reflection on types that are NOT aggregate initializable

#include "glaze/glaze.hpp"

#include "ut/ut.hpp"

using namespace ut;

#if GLZ_REFLECTION26

// ============================================================================
// Test 1: Class with user-defined constructor
// ============================================================================
class ConstructedClass
{
  public:
   std::string name;
   int value;
   double data;

   // User-defined constructor makes this non-aggregate
   ConstructedClass() : name("default"), value(0), data(0.0) {}
   ConstructedClass(std::string n, int v, double d) : name(std::move(n)), value(v), data(d) {}
};

// ============================================================================
// Test 2: Class with private members (requires glz::meta friend)
// ============================================================================
class PrivateMembersClass
{
   std::string secret_name;
   int secret_value;

  public:
   PrivateMembersClass() : secret_name("hidden"), secret_value(42) {}
   PrivateMembersClass(std::string n, int v) : secret_name(std::move(n)), secret_value(v) {}

   // Getters for verification
   const std::string& get_name() const { return secret_name; }
   int get_value() const { return secret_value; }

   // Allow Glaze to access private members
   friend struct glz::meta<PrivateMembersClass>;
};

template <>
struct glz::meta<PrivateMembersClass>
{
   using T = PrivateMembersClass;
   static constexpr auto value = object(&T::secret_name, &T::secret_value);
};

// ============================================================================
// Test 3: Class with virtual functions
// ============================================================================
class VirtualClass
{
  public:
   std::string name;
   int id;

   VirtualClass() : name("virtual"), id(0) {}
   VirtualClass(std::string n, int i) : name(std::move(n)), id(i) {}

   virtual ~VirtualClass() = default;
   virtual std::string describe() const { return name + ":" + std::to_string(id); }
};

// ============================================================================
// Test 4: Inheritance - derived class
// ============================================================================
class BaseClass
{
  public:
   std::string base_name;
   int base_id;

   BaseClass() : base_name("base"), base_id(0) {}
   BaseClass(std::string n, int i) : base_name(std::move(n)), base_id(i) {}
};

class DerivedClass : public BaseClass
{
  public:
   std::string derived_data;
   double derived_value;

   DerivedClass() : BaseClass(), derived_data("derived"), derived_value(0.0) {}
   DerivedClass(std::string bn, int bi, std::string dd, double dv)
      : BaseClass(std::move(bn), bi), derived_data(std::move(dd)), derived_value(dv)
   {}
};

// No explicit glz::meta needed!
// P2996 automatically includes base class members via std::meta::bases_of

// ============================================================================
// Test 5: Class with deleted copy constructor
// ============================================================================
class NoCopyClass
{
  public:
   std::string name;
   int value;

   NoCopyClass() : name("no_copy"), value(0) {}
   NoCopyClass(std::string n, int v) : name(std::move(n)), value(v) {}

   // Deleted copy operations
   NoCopyClass(const NoCopyClass&) = delete;
   NoCopyClass& operator=(const NoCopyClass&) = delete;

   // Allow move
   NoCopyClass(NoCopyClass&&) = default;
   NoCopyClass& operator=(NoCopyClass&&) = default;
};

// ============================================================================
// Test 6: Class with explicit constructor
// ============================================================================
class ExplicitConstructorClass
{
  public:
   std::string name;
   int count;

   explicit ExplicitConstructorClass(int c = 0) : name("explicit"), count(c) {}
   ExplicitConstructorClass(std::string n, int c) : name(std::move(n)), count(c) {}
};

// ============================================================================
// Test 7: Class with multiple constructors and default member initializers
// ============================================================================
class MultiConstructorClass
{
  public:
   std::string label = "default_label";
   int priority = 5;
   double weight = 1.0;
   bool active = true;

   MultiConstructorClass() = default;
   MultiConstructorClass(std::string l) : label(std::move(l)) {}
   MultiConstructorClass(std::string l, int p) : label(std::move(l)), priority(p) {}
   MultiConstructorClass(std::string l, int p, double w, bool a)
      : label(std::move(l)), priority(p), weight(w), active(a)
   {}
};

// ============================================================================
// Test 8: Class with const members (read-only after construction)
// ============================================================================
class ConstMemberClass
{
  public:
   const std::string id;
   int mutable_value;

   ConstMemberClass() : id("const_id"), mutable_value(0) {}
   ConstMemberClass(std::string i, int v) : id(std::move(i)), mutable_value(v) {}
};

// Need meta to handle const member (only mutable_value can be written)
template <>
struct glz::meta<ConstMemberClass>
{
   using T = ConstMemberClass;
   // Note: const members can be written during object construction but not modified after
   static constexpr auto value = object(&T::id, &T::mutable_value);
};

// ============================================================================
// Test 9: Nested non-aggregate classes
// ============================================================================
class InnerClass
{
  public:
   std::string inner_name;
   int inner_value;

   InnerClass() : inner_name("inner"), inner_value(0) {}
   InnerClass(std::string n, int v) : inner_name(std::move(n)), inner_value(v) {}
};

class OuterClass
{
  public:
   std::string outer_name;
   InnerClass nested;
   std::vector<int> values;

   OuterClass() : outer_name("outer"), nested(), values{} {}
   OuterClass(std::string on, InnerClass ic, std::vector<int> v)
      : outer_name(std::move(on)), nested(std::move(ic)), values(std::move(v))
   {}
};

// ============================================================================
// Test 10: Template class with non-aggregate properties
// ============================================================================
template <typename T>
class TemplateClass
{
  public:
   std::string name;
   T value;
   std::vector<T> items;

   TemplateClass() : name("template"), value{}, items{} {}
   TemplateClass(std::string n, T v) : name(std::move(n)), value(std::move(v)), items{} {}
};

// ============================================================================
// Tests
// ============================================================================

suite non_aggregate_reflection_tests = [] {
   "constructed class serialization"_test = [] {
      ConstructedClass obj("test", 42, 3.14);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"test","value":42,"data":3.14})") << json;

      ConstructedClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "test");
      expect(obj2.value == 42);
      expect(obj2.data == 3.14);
   };

   "constructed class member names"_test = [] {
      constexpr auto names = glz::member_names<ConstructedClass>;
      expect(names.size() == 3_ul);
      expect(names[0] == "name");
      expect(names[1] == "value");
      expect(names[2] == "data");
   };

   "private members class serialization"_test = [] {
      PrivateMembersClass obj("secret", 100);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"secret_name":"secret","secret_value":100})") << json;

      PrivateMembersClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.get_name() == "secret");
      expect(obj2.get_value() == 100);
   };

   "virtual class serialization"_test = [] {
      VirtualClass obj("polymorphic", 99);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"polymorphic","id":99})") << json;

      VirtualClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "polymorphic");
      expect(obj2.id == 99);
      expect(obj2.describe() == "polymorphic:99");
   };

   "derived class serialization"_test = [] {
      DerivedClass obj("base_name", 1, "derived_data", 2.5);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"base_name":"base_name","base_id":1,"derived_data":"derived_data","derived_value":2.5})")
         << json;

      DerivedClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.base_name == "base_name");
      expect(obj2.base_id == 1);
      expect(obj2.derived_data == "derived_data");
      expect(obj2.derived_value == 2.5);
   };

   "derived class automatic inheritance"_test = [] {
      // Verify that P2996 automatically includes base class members
      constexpr auto count = glz::detail::count_members<DerivedClass>;
      expect(count == 4_ul) << "Expected 4 members (2 from base + 2 from derived), got " << count;

      constexpr auto names = glz::member_names<DerivedClass>;
      expect(names.size() == 4_ul);
      // Base class members come first
      expect(names[0] == "base_name");
      expect(names[1] == "base_id");
      // Then derived class members
      expect(names[2] == "derived_data");
      expect(names[3] == "derived_value");
   };

   "no copy class serialization"_test = [] {
      NoCopyClass obj("unique", 777);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"unique","value":777})") << json;

      NoCopyClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "unique");
      expect(obj2.value == 777);
   };

   "explicit constructor class serialization"_test = [] {
      ExplicitConstructorClass obj("explicit_test", 50);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"explicit_test","count":50})") << json;

      ExplicitConstructorClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "explicit_test");
      expect(obj2.count == 50);
   };

   "multi constructor class serialization"_test = [] {
      MultiConstructorClass obj("custom", 10, 2.5, false);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"label":"custom","priority":10,"weight":2.5,"active":false})") << json;

      MultiConstructorClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.label == "custom");
      expect(obj2.priority == 10);
      expect(obj2.weight == 2.5);
      expect(obj2.active == false);
   };

   "multi constructor class default values"_test = [] {
      MultiConstructorClass obj;

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"label":"default_label","priority":5,"weight":1,"active":true})") << json;
   };

   "const member class write"_test = [] {
      ConstMemberClass obj("immutable_id", 42);

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"id":"immutable_id","mutable_value":42})") << json;
   };

   "nested non-aggregate serialization"_test = [] {
      OuterClass obj("outer_test", InnerClass("inner_test", 123), {1, 2, 3});

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"outer_name":"outer_test","nested":{"inner_name":"inner_test","inner_value":123},"values":[1,2,3]})")
         << json;

      OuterClass obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.outer_name == "outer_test");
      expect(obj2.nested.inner_name == "inner_test");
      expect(obj2.nested.inner_value == 123);
      expect(obj2.values == std::vector<int>{1, 2, 3});
   };

   "template class serialization"_test = [] {
      TemplateClass<int> obj("int_template", 42);
      obj.items = {1, 2, 3, 4, 5};

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"int_template","value":42,"items":[1,2,3,4,5]})") << json;

      TemplateClass<int> obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "int_template");
      expect(obj2.value == 42);
      expect(obj2.items == std::vector<int>{1, 2, 3, 4, 5});
   };

   "template class with string"_test = [] {
      TemplateClass<std::string> obj("string_template", "hello");
      obj.items = {"a", "b", "c"};

      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"name":"string_template","value":"hello","items":["a","b","c"]})") << json;

      TemplateClass<std::string> obj2;
      expect(not glz::read_json(obj2, json));
      expect(obj2.name == "string_template");
      expect(obj2.value == "hello");
      expect(obj2.items == std::vector<std::string>{"a", "b", "c"});
   };

   "beve format with constructed class"_test = [] {
      ConstructedClass obj("beve_test", 999, 1.23);

      std::string beve;
      expect(not glz::write_beve(obj, beve));

      ConstructedClass obj2;
      expect(not glz::read_beve(obj2, beve));
      expect(obj2.name == "beve_test");
      expect(obj2.value == 999);
      expect(obj2.data == 1.23);
   };

   "pretty json with non-aggregate"_test = [] {
      ConstructedClass obj("pretty", 1, 2.0);

      std::string json;
      expect(not glz::write<glz::opts{.prettify = true}>(obj, json));
      expect(json.find('\n') != std::string::npos) << "Expected newlines in prettified output";
      expect(json.find("\"name\"") != std::string::npos);
   };

   "count_members for non-aggregates"_test = [] {
      constexpr auto count1 = glz::detail::count_members<ConstructedClass>;
      expect(count1 == 3_ul);

      constexpr auto count2 = glz::detail::count_members<VirtualClass>;
      expect(count2 == 2_ul);

      constexpr auto count3 = glz::detail::count_members<MultiConstructorClass>;
      expect(count3 == 4_ul);

      constexpr auto count4 = glz::detail::count_members<NoCopyClass>;
      expect(count4 == 2_ul);
   };

   "type_name for non-aggregates"_test = [] {
      constexpr auto name1 = glz::type_name<ConstructedClass>;
      expect(std::string_view(name1).find("ConstructedClass") != std::string_view::npos) << name1;

      constexpr auto name2 = glz::type_name<VirtualClass>;
      expect(std::string_view(name2).find("VirtualClass") != std::string_view::npos) << name2;
   };
};

#else // !GLZ_REFLECTION26

suite non_aggregate_reflection_tests = [] {
   "P2996 not available"_test = [] {
      // This test file requires C++26 P2996 reflection
      // Non-aggregate types cannot be reflected without P2996
      expect(true) << "P2996 reflection not enabled - skipping non-aggregate tests";
   };
};

#endif // GLZ_REFLECTION26

int main() { return 0; }
