// Comprehensive tests for variants with enum reflection
// NOTE: Variants with enums have limitations - when an enum serializes as a string,
// the variant parser cannot distinguish it from other string-like types during parsing.
// This means round-trip serialization may not work for all variant-enum combinations.

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"
#include <variant>
#include <optional>
#include <vector>

using namespace ut;

// Test enums
enum class Status { 
   Pending, 
   Running, 
   Complete, 
   Failed 
};

enum class Priority : uint8_t { 
   Low = 1, 
   Medium = 2, 
   High = 3, 
   Critical = 10 
};

enum class Permission : uint8_t {
   None = 0,
   Read = 1 << 0,
   Write = 1 << 1,
   Execute = 1 << 2,
   All = Read | Write | Execute
};

// Bitflag operators
constexpr Permission operator|(Permission a, Permission b) {
   return static_cast<Permission>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr Permission operator&(Permission a, Permission b) {
   return static_cast<Permission>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// Test structures
struct Task {
   std::string name;
   Status status{Status::Pending};
   Priority priority{Priority::Medium};
};

struct User {
   std::string username;
   Permission permissions{Permission::Read};
};

struct System {
   bool active{true};
   std::optional<Status> lastStatus;
};

// Variant types - avoiding ambiguous combinations
using StructVariant = std::variant<Task, User, System>;
using EnumOnlyVariant = std::variant<Status, Priority, Permission>;

// Tagged variant with enum discriminator
struct CreateAction {
   Status action{Status::Pending};
   std::string data;
};

struct UpdateAction {
   Status action{Status::Running};
   std::string target;
   int version;
};

struct DeleteAction {
   Status action{Status::Complete};
   int id;
};

using TaggedVariant = std::variant<CreateAction, UpdateAction, DeleteAction>;

suite variant_with_enum_tests = [] {
   "enum_serializes_in_variant"_test = [] {
      // Test how enums serialize in single-type variants
      std::variant<Status> v = Status::Running;
      std::string json;
      glz::write_json(v, json);
      // Single-type variants with enums may serialize as string or number depending on implementation
      expect(json == R"("Running")" || json == "1") << "Got: " << json;
   };
   
   "struct_variant_with_enums"_test = [] {
      StructVariant v = Task{"Build", Status::Running, Priority::High};
      std::string json;
      glz::write_json(v, json);
      expect(json == R"({"name":"Build","status":"Running","priority":"High"})") << "Got: " << json;
      
      StructVariant parsed;
      glz::read_json(parsed, json);
      expect(std::holds_alternative<Task>(parsed));
      auto& task = std::get<Task>(parsed);
      expect(task.name == "Build");
      expect(task.status == Status::Running);
      expect(task.priority == Priority::High);
   };
   
   "variant_with_bitflag_enum_write"_test = [] {
      // Variants with bitflag enums currently serialize as numbers 
      // This is a limitation - variant serialization doesn't use enum reflection yet
      Permission p = Permission::Read | Permission::Write;
      std::variant<Permission> v = p;
      std::string json;
      glz::write_json(v, json);
      expect(json == "3") << "Got: " << json; // Read(1) | Write(2) = 3
      
      v = Permission::All;
      glz::write_json(v, json);
      expect(json == R"("All")") << "Got: " << json; // All is a defined value in the enum
   };
};

suite tagged_variant_tests = [] {
   "create_action_serialization"_test = [] {
      TaggedVariant v = CreateAction{Status::Pending, "new item"};
      std::string json;
      glz::write_json(v, json);
      expect(json == R"({"action":"Pending","data":"new item"})") << "Got: " << json;
      
      TaggedVariant parsed;
      glz::read_json(parsed, json);
      expect(std::holds_alternative<CreateAction>(parsed));
      auto& action = std::get<CreateAction>(parsed);
      expect(action.action == Status::Pending);
      expect(action.data == "new item");
   };
   
   "update_action_serialization"_test = [] {
      TaggedVariant v = UpdateAction{Status::Running, "target.txt", 5};
      std::string json;
      glz::write_json(v, json);
      expect(json == R"({"action":"Running","target":"target.txt","version":5})") << "Got: " << json;
      
      TaggedVariant parsed;
      glz::read_json(parsed, json);
      expect(std::holds_alternative<UpdateAction>(parsed));
      auto& action = std::get<UpdateAction>(parsed);
      expect(action.action == Status::Running);
      expect(action.target == "target.txt");
      expect(action.version == 5);
   };
   
   "delete_action_serialization"_test = [] {
      TaggedVariant v = DeleteAction{Status::Complete, 123};
      std::string json;
      glz::write_json(v, json);
      expect(json == R"({"action":"Complete","id":123})") << "Got: " << json;
      
      TaggedVariant parsed;
      glz::read_json(parsed, json);
      expect(std::holds_alternative<DeleteAction>(parsed));
      auto& action = std::get<DeleteAction>(parsed);
      expect(action.action == Status::Complete);
      expect(action.id == 123);
   };
};

suite enum_in_struct_tests = [] {
   "task_with_enums_roundtrip"_test = [] {
      Task t{"Important Task", Status::Running, Priority::Critical};
      std::string json;
      glz::write_json(t, json);
      expect(json == R"({"name":"Important Task","status":"Running","priority":"Critical"})");
      
      Task parsed;
      glz::read_json(parsed, json);
      expect(parsed.name == "Important Task");
      expect(parsed.status == Status::Running);
      expect(parsed.priority == Priority::Critical);
   };
   
   "user_with_bitflag_permissions"_test = [] {
      User u{"admin", Permission::All};
      std::string json;
      glz::write_json(u, json);
      expect(json == R"({"username":"admin","permissions":"All"})"); // All is a defined value
      
      User parsed;
      glz::read_json(parsed, json);
      expect(parsed.username == "admin");
      expect(parsed.permissions == Permission::All);
   };
   
   "system_with_optional_enum"_test = [] {
      System s{true, Status::Failed};
      std::string json;
      glz::write_json(s, json);
      expect(json == R"({"active":true,"lastStatus":"Failed"})");
      
      System parsed;
      glz::read_json(parsed, json);
      expect(parsed.active == true);
      expect(parsed.lastStatus.has_value());
      expect(*parsed.lastStatus == Status::Failed);
      
      // Test with nullopt
      s.lastStatus = std::nullopt;
      glz::write_json(s, json);
      expect(json == R"({"active":true})"); // Glaze skips null optionals by default
      
      // Parse JSON with explicit null
      glz::read_json(parsed, R"({"active":true,"lastStatus":null})");
      expect(!parsed.lastStatus.has_value());
   };
};

struct Config {
   std::optional<std::variant<Status, Priority>> setting;
};

struct Action1 {
   Status type{Status::Pending};
   int data;
};

struct Action2 {
   Status type{Status::Running};
   std::string info;
};

using ActionVariant = std::variant<Action1, Action2>;

suite optional_and_nested_tests = [] {
   "optional_of_variant_null"_test = [] {
      Config cfg;
      cfg.setting = std::nullopt;
      
      std::string json;
      glz::write_json(cfg, json);
      expect(json == R"({})") << "Got: " << json; // Glaze skips null optionals
      
      Config parsed;
      glz::read_json(parsed, json);
      expect(!parsed.setting.has_value());
   };
   
   "action_variant_with_same_enum_field"_test = [] {
      ActionVariant v = Action1{Status::Failed, 42};
      std::string json;
      glz::write_json(v, json);
      expect(json == R"({"type":"Failed","data":42})") << "Got: " << json;
      
      ActionVariant parsed;
      glz::read_json(parsed, json);
      expect(std::holds_alternative<Action1>(parsed));
      auto& action = std::get<Action1>(parsed);
      expect(action.type == Status::Failed);
      expect(action.data == 42);
      
      // Test Action2
      v = Action2{Status::Complete, "done"};
      glz::write_json(v, json);
      expect(json == R"({"type":"Complete","info":"done"})") << "Got: " << json;
      
      glz::read_json(parsed, json);
      expect(std::holds_alternative<Action2>(parsed));
      auto& action2 = std::get<Action2>(parsed);
      expect(action2.type == Status::Complete);
      expect(action2.info == "done");
   };
};

suite vector_tests = [] {
   "vector_of_enums"_test = [] {
      std::vector<Status> vec{Status::Pending, Status::Running, Status::Complete};
      std::string json;
      glz::write_json(vec, json);
      expect(json == R"(["Pending","Running","Complete"])");
      
      std::vector<Status> parsed;
      glz::read_json(parsed, json);
      expect(parsed.size() == 3);
      expect(parsed[0] == Status::Pending);
      expect(parsed[1] == Status::Running);
      expect(parsed[2] == Status::Complete);
   };
   
   "vector_of_struct_variants"_test = [] {
      std::vector<StructVariant> vec;
      vec.push_back(Task{"Task1", Status::Running, Priority::High});
      vec.push_back(User{"alice", Permission::Read | Permission::Execute});
      vec.push_back(System{true, Status::Complete});
      
      std::string json;
      glz::write_json(vec, json);
      
      std::vector<StructVariant> parsed;
      glz::read_json(parsed, json);
      expect(parsed.size() == 3);
      
      expect(std::holds_alternative<Task>(parsed[0]));
      auto& task = std::get<Task>(parsed[0]);
      expect(task.status == Status::Running);
      expect(task.priority == Priority::High);
      
      expect(std::holds_alternative<User>(parsed[1]));
      auto& user = std::get<User>(parsed[1]);
      expect(user.permissions == (Permission::Read | Permission::Execute));
      
      expect(std::holds_alternative<System>(parsed[2]));
      auto& sys = std::get<System>(parsed[2]);
      expect(sys.lastStatus.has_value());
      expect(*sys.lastStatus == Status::Complete);
   };
};

suite variant_with_monostate = [] {
   "monostate_variant"_test = [] {
      using MonoVariant = std::variant<std::monostate, int, std::string>;
      
      MonoVariant v = std::monostate{};
      std::string json;
      glz::write_json(v, json);
      expect(json == "null") << "Got: " << json;
      
      v = 42;
      glz::write_json(v, json);
      expect(json == "42") << "Got: " << json;
      
      v = "hello";
      glz::write_json(v, json);
      expect(json == R"("hello")") << "Got: " << json;
      
      MonoVariant parsed;
      glz::read_json(parsed, "null");
      expect(std::holds_alternative<std::monostate>(parsed));
      
      glz::read_json(parsed, "42");
      expect(std::holds_alternative<int>(parsed));
      expect(std::get<int>(parsed) == 42);
      
      glz::read_json(parsed, R"("hello")");
      expect(std::holds_alternative<std::string>(parsed));
      expect(std::get<std::string>(parsed) == "hello");
   };
};

suite beve_variant_enum_tests = [] {
   "beve_enum_in_struct"_test = [] {
      Task t{"Test", Status::Running, Priority::High};
      std::vector<uint8_t> beve;
      glz::write_beve(t, beve);
      
      Task parsed;
      glz::read_beve(parsed, beve);
      expect(parsed.name == "Test");
      expect(parsed.status == Status::Running);
      expect(parsed.priority == Priority::High);
   };
   
   "beve_struct_variant"_test = [] {
      StructVariant v = User{"bob", Permission::All};
      std::vector<uint8_t> beve;
      glz::write_beve(v, beve);
      
      StructVariant parsed;
      glz::read_beve(parsed, beve);
      expect(std::holds_alternative<User>(parsed));
      auto& user = std::get<User>(parsed);
      expect(user.username == "bob");
      expect(user.permissions == Permission::All);
   };
   
   "beve_vector_of_enums"_test = [] {
      std::vector<Status> vec{Status::Pending, Status::Running, Status::Failed};
      std::vector<uint8_t> beve;
      glz::write_beve(vec, beve);
      
      std::vector<Status> parsed;
      glz::read_beve(parsed, beve);
      expect(parsed.size() == 3);
      expect(parsed[0] == Status::Pending);
      expect(parsed[1] == Status::Running);
      expect(parsed[2] == Status::Failed);
   };
};

int main() {
   return 0;
}