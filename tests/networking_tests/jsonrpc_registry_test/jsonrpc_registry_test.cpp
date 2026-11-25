// Glaze Library
// For the license information refer to glaze.ixx

import std;

import glaze;
import glaze.ext.jsonrpc;
import glaze.rpc.registry;

import ut;

using std::size_t;

using namespace ut;

struct my_functions_t
{
   int i{};
   std::function<std::string_view()> hello = []() -> std::string_view { return "Hello"; };
   std::function<std::string_view()> world = []() -> std::string_view { return "World"; };
   std::function<int()> get_number = [] { return 42; };
   std::function<void()> void_func = [] {};
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) {
      return (std::ranges::max)(vec);
   };
};

struct meta_functions_t
{
   std::function<std::string_view()> hello = []() -> std::string_view { return "Hello"; };
   std::function<std::string_view()> world = []() -> std::string_view { return "World"; };
   std::function<int()> get_number = [] { return 42; };

   struct glaze
   {
      using T = meta_functions_t;
      static constexpr auto value = glz::object(&T::hello, &T::world, &T::get_number);
   };
};

struct my_nested_functions_t
{
   my_functions_t my_functions{};
   meta_functions_t meta_functions{};
   std::function<std::string(const std::string&)> append_awesome = [](const std::string& in) {
      return in + " awesome!";
   };
   std::string my_string{};
};

// Counts how many elements of a batch actually ran, which is how the limit is shown to be enforced
// as the batch is walked rather than after it has been built.
struct counting_functions_t
{
   size_t calls{};
   std::string blob = std::string(4096, 'x');
   std::string fetch()
   {
      ++calls;
      return blob;
   }

   struct glaze
   {
      using T = counting_functions_t;
      static constexpr auto value = glz::object(&T::calls, &T::blob, &T::fetch);
   };
};

struct example_functions_t
{
   std::string name{};
   std::string get_name() { return name; }
   void set_name(const std::string& new_name) { name = new_name; }

   struct glaze
   {
      using T = example_functions_t;
      static constexpr auto value =
         glz::object(&T::name, &T::get_name, &T::set_name, "custom_name", glz::custom<&T::set_name, &T::get_name>);
   };
};

suite jsonrpc_basic_tests = [] {
   "basic_function_calls"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      obj.i = 55;

      // Test reading a variable
      auto response = server.call(R"({"jsonrpc":"2.0","method":"i","id":1})");
      expect(response.find(R"("result":55)") != std::string::npos) << response;
      expect(response.find(R"("id":1)") != std::string::npos) << response;

      // Test writing a variable
      response = server.call(R"({"jsonrpc":"2.0","method":"i","params":42,"id":2})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;
      expect(obj.i == 42);

      // Test calling a function with no params
      response = server.call(R"({"jsonrpc":"2.0","method":"hello","id":3})");
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;

      response = server.call(R"({"jsonrpc":"2.0","method":"get_number","id":4})");
      expect(response.find(R"("result":42)") != std::string::npos) << response;

      // Test void function
      response = server.call(R"({"jsonrpc":"2.0","method":"void_func","id":5})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;
   };

   "nested_function_calls"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_nested_functions_t obj{};
      server.on(obj);

      // Test nested function call
      auto response = server.call(R"({"jsonrpc":"2.0","method":"my_functions/hello","id":1})");
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;

      response = server.call(R"({"jsonrpc":"2.0","method":"meta_functions/get_number","id":2})");
      expect(response.find(R"("result":42)") != std::string::npos) << response;

      // Test function with string param
      response = server.call(R"({"jsonrpc":"2.0","method":"append_awesome","params":"you are","id":3})");
      expect(response.find(R"("result":"you are awesome!")") != std::string::npos) << response;

      // Test writing and reading a string
      response = server.call(R"({"jsonrpc":"2.0","method":"my_string","params":"Howdy!","id":4})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;

      response = server.call(R"({"jsonrpc":"2.0","method":"my_string","id":5})");
      expect(response.find(R"("result":"Howdy!")") != std::string::npos) << response;

      // Test function with array param
      response = server.call(R"({"jsonrpc":"2.0","method":"my_functions/max","params":[1.1,3.3,2.25],"id":6})");
      expect(response.find(R"("result":3.3)") != std::string::npos) << response;
   };

   "member_functions"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      example_functions_t obj{};
      server.on(obj);

      // Set name using direct field
      auto response = server.call(R"({"jsonrpc":"2.0","method":"name","params":"Susan","id":1})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;

      // Get name using member function
      response = server.call(R"({"jsonrpc":"2.0","method":"get_name","id":2})");
      expect(response.find(R"("result":"Susan")") != std::string::npos) << response;

      // Set name using member function
      response = server.call(R"({"jsonrpc":"2.0","method":"set_name","params":"Bob","id":3})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;
      expect(obj.name == "Bob");

      // Set name using custom endpoint
      response = server.call(R"({"jsonrpc":"2.0","method":"custom_name","params":"Alice","id":4})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;
      expect(obj.name == "Alice");
   };
};

suite jsonrpc_notification_tests = [] {
   "notifications_no_response"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 10;
      server.on(obj);

      // Notification (id is null) - should return empty response
      auto response = server.call(R"({"jsonrpc":"2.0","method":"i","params":99,"id":null})");
      expect(response.empty()) << "Notification should return empty response, got: " << response;
      expect(obj.i == 99) << "Value should have been updated";

      // Test notification with function call
      response = server.call(R"({"jsonrpc":"2.0","method":"void_func","id":null})");
      expect(response.empty()) << "Notification should return empty response";
   };
};

suite jsonrpc_batch_tests = [] {
   "batch_requests"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 10;
      server.on(obj);

      // Batch request with multiple calls
      auto response = server.call(R"([
         {"jsonrpc":"2.0","method":"hello","id":1},
         {"jsonrpc":"2.0","method":"get_number","id":2},
         {"jsonrpc":"2.0","method":"i","id":3}
      ])");

      // Should be a JSON array
      expect(response[0] == '[') << response;
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;
      expect(response.find(R"("result":42)") != std::string::npos) << response;
      expect(response.find(R"("result":10)") != std::string::npos) << response;
   };

   "batch_with_notifications"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 10;
      server.on(obj);

      // Batch with a notification (should not be in response)
      auto response = server.call(R"([
         {"jsonrpc":"2.0","method":"hello","id":1},
         {"jsonrpc":"2.0","method":"i","params":99,"id":null},
         {"jsonrpc":"2.0","method":"i","id":2}
      ])");

      // Should have 2 responses (notification excluded)
      expect(response[0] == '[') << response;
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;
      expect(response.find(R"("result":99)") != std::string::npos) << response; // Reading updated value
      expect(obj.i == 99) << "Value should have been updated by notification";
   };

   // A batch answers every element it holds, so a request of a fixed size can ask for an answer of
   // any size. Only the server can bound that.
   "batch_response_limit"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      std::string request = "[";
      for (size_t i = 0; i < 512; ++i) {
         if (i) {
            request += ',';
         }
         request += R"({"jsonrpc":"2.0","method":"","id":)";
         request += std::to_string(i);
         request += '}';
      }
      request += ']';

      // Every element reads the root endpoint, which answers with the whole object, so the batch is
      // well over a kilobyte of response.
      server.max_batch_response_size = 1024;
      auto response = server.call(request);
      expect(response.find(R"(-32000)") != std::string::npos) << response;
      expect(response.find(R"("result")") == std::string::npos) << response;
      expect(response[0] == '{') << "the batch is abandoned, not truncated to a partial array";
      expect(response.size() < 4096u) << response.size();

      // The same batch is answered in full once the limit allows it.
      server.max_batch_response_size = glz::rpc::default_max_batch_response_size;
      response = server.call(request);
      expect(response[0] == '[') << response.substr(0, 64);
      expect(response.find(R"(-32000)") == std::string::npos) << response.substr(0, 200);
      expect(response.size() > 1024u) << response.size();
   };

   // The limit is checked as the batch is walked, so the elements past it are never run. A check
   // that ran only after the loop would answer with the same error while still having built the
   // whole oversized response first, which is the case this pins.
   "batch_response_limit_stops_early"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      counting_functions_t obj{};
      server.on(obj);

      std::string request = "[";
      for (size_t i = 0; i < 512; ++i) {
         if (i) {
            request += ',';
         }
         request += R"({"jsonrpc":"2.0","method":"fetch","id":)";
         request += std::to_string(i);
         request += '}';
      }
      request += ']';

      // Each element answers with the 4 KB blob, so the limit is reached within the first few.
      server.max_batch_response_size = 8 * 1024;
      auto response = server.call(request);
      expect(response.find(R"(-32000)") != std::string::npos) << response;
      expect(obj.calls > 0u) << "the batch should have started";
      expect(obj.calls < 8u) << "elements past the limit must never run, ran " << obj.calls;

      obj.calls = 0;
      server.max_batch_response_size = glz::rpc::default_max_batch_response_size;
      response = server.call(request);
      expect(response[0] == '[') << response.substr(0, 64);
      expect(obj.calls == 512u) << obj.calls;
   };

   "empty_batch_error"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"([])");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32600)") != std::string::npos) << response; // Invalid Request
   };
};

suite jsonrpc_error_tests = [] {
   "parse_error"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({invalid json)");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32700)") != std::string::npos) << response; // Parse error
   };

   "method_not_found"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"non_existent","id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32601)") != std::string::npos) << response; // Method not found
      expect(response.find(R"("id":1)") != std::string::npos) << response; // ID preserved
   };

   "invalid_params"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      // Try to set an integer with invalid params
      auto response = server.call(R"({"jsonrpc":"2.0","method":"i","params":"not_an_int","id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32602)") != std::string::npos) << response; // Invalid params
   };

   "invalid_version"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"1.0","method":"hello","id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32600)") != std::string::npos) << response; // Invalid Request
   };

   // `jsonrpc` and `method` are both required members. An absent one used to read back as the
   // default the request type carries, which made `{"id":1}` a version 2.0 call on the empty method
   // -- and the empty method is the root endpoint, so it answered with the whole registered object.
   "missing_version"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"method":"hello","id":1})");
      expect(response.find(R"(-32600)") != std::string::npos) << response; // Invalid Request
      expect(response.find(R"("result")") == std::string::npos) << response;
      expect(response.find(R"("id":1)") != std::string::npos) << response;
   };

   "missing_method"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 55;
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","id":1})");
      expect(response.find(R"(-32600)") != std::string::npos) << response; // Invalid Request
      expect(response.find(R"("result")") == std::string::npos) << response;
      // The root endpoint answers with the whole object, so its members must not appear.
      expect(response.find(R"("i":)") == std::string::npos) << "must not dispatch to the root endpoint: " << response;
   };

   "bare_id_is_not_a_request"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 55;
      server.on(obj);

      auto response = server.call(R"({"id":1})");
      expect(response.find(R"(-32600)") != std::string::npos) << response; // Invalid Request
      expect(response.find(R"("result")") == std::string::npos) << response;
      // The root endpoint answers with the whole object, so its members must not appear.
      expect(response.find(R"("i":)") == std::string::npos) << "must not dispatch to the root endpoint: " << response;
   };

   // Every element of a batch is validated on its own. A bare id used to reach the root endpoint,
   // which answers with the whole registered object -- so a batch of them returned one whole-object
   // response per element, and the response grew with the product of the batch length and the size
   // of the registered object rather than with the length of the request.
   "batch_of_bare_ids_is_not_amplified"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      std::string request = "[";
      for (size_t i = 0; i < 256; ++i) {
         if (i) {
            request += ',';
         }
         request += R"({"id":)";
         request += std::to_string(i);
         request += '}';
      }
      request += ']';

      auto response = server.call(request);
      expect(response[0] == '[') << response;
      expect(response.find(R"("result")") == std::string::npos) << response;

      size_t errors = 0;
      for (size_t pos = response.find("-32600"); pos != std::string::npos; pos = response.find("-32600", pos + 1)) {
         ++errors;
      }
      expect(errors == 256u) << errors;

      // Each element answers with a fixed-size error rather than a copy of the registered object,
      // so the response stays a constant multiple of the request no matter what is registered.
      expect(response.size() < 16 * request.size()) << response.size() << " vs " << request.size();
   };
};

suite jsonrpc_id_types_tests = [] {
   "string_id"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"hello","id":"my-request-id"})");
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;
      expect(response.find(R"("id":"my-request-id")") != std::string::npos) << response;
   };

   "numeric_id"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"hello","id":12345})");
      expect(response.find(R"("result":"Hello")") != std::string::npos) << response;
      expect(response.find(R"("id":12345)") != std::string::npos) << response;
   };
};

suite jsonrpc_root_endpoint_tests = [] {
   "root_endpoint"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 55;
      server.on(obj);

      // Empty method should access root
      auto response = server.call(R"({"jsonrpc":"2.0","method":"","id":1})");
      expect(response.find(R"("result")") != std::string::npos) << response;
      expect(response.find(R"("i":55)") != std::string::npos) << response;
   };
};

// Test types for merge functionality
struct first_object_t
{
   int value1 = 42;
   std::string name1 = "first";

   struct glaze
   {
      using T = first_object_t;
      static constexpr auto value = glz::object(&T::value1, &T::name1);
   };
};

struct second_object_t
{
   double value2 = 3.14;
   std::string name2 = "second";

   struct glaze
   {
      using T = second_object_t;
      static constexpr auto value = glz::object(&T::value2, &T::name2);
   };
};

suite jsonrpc_merge_tests = [] {
   "merge_basic"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      // Test reading from first object
      auto response = server.call(R"({"jsonrpc":"2.0","method":"value1","id":1})");
      expect(response.find(R"("result":42)") != std::string::npos) << response;

      response = server.call(R"({"jsonrpc":"2.0","method":"name1","id":2})");
      expect(response.find(R"("result":"first")") != std::string::npos) << response;

      // Test reading from second object
      response = server.call(R"({"jsonrpc":"2.0","method":"value2","id":3})");
      expect(response.find(R"("result":3.14)") != std::string::npos) << response;

      response = server.call(R"({"jsonrpc":"2.0","method":"name2","id":4})");
      expect(response.find(R"("result":"second")") != std::string::npos) << response;

      // Test merged root endpoint
      response = server.call(R"({"jsonrpc":"2.0","method":"","id":5})");
      expect(response.find(R"("value1":42)") != std::string::npos) << response;
      expect(response.find(R"("value2":3.14)") != std::string::npos) << response;
   };

   "merge_write_to_root_not_supported"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      // Writing to merged root should return error
      auto response = server.call(R"({"jsonrpc":"2.0","method":"","params":{"value1":999},"id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find("not supported") != std::string::npos) << response;
   };
};

struct throwing_functions_t
{
   std::function<int()> throw_func = []() -> int { throw std::runtime_error("Test exception"); };
   std::function<int()> throw_special = []() -> int {
      throw std::runtime_error("Error with \"quotes\" and\nnewlines");
   };
};

suite jsonrpc_exception_tests = [] {
   "exception_handling"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      throwing_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"throw_func","id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32603)") != std::string::npos) << response; // Internal error
      expect(response.find("Test exception") != std::string::npos) << response;
      expect(response.find(R"("id":1)") != std::string::npos) << response; // ID preserved
   };

   "exception_with_special_chars_produces_valid_json"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      throwing_functions_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"throw_special","id":1})");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32603)") != std::string::npos) << response; // Internal error

      // Verify the response is valid JSON
      auto err = glz::validate_json(response);
      expect(!err) << "Response must be valid JSON: " << glz::format_error(err, response);
   };
};

suite jsonrpc_error_json_validity_tests = [] {
   "parse_error_with_special_chars_produces_valid_json"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      server.on(obj);

      // Malformed JSON with special characters that will appear in the error message
      auto response = server.call("{\"test\n\"}");
      expect(response.find(R"("error")") != std::string::npos) << response;
      expect(response.find(R"(-32700)") != std::string::npos) << response; // Parse error

      // Verify the response is valid JSON
      auto err = glz::validate_json(response);
      expect(!err) << "Response must be valid JSON: " << glz::format_error(err, response);
   };

   "missing_id_field_treated_as_notification"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      my_functions_t obj{};
      obj.i = 42;
      server.on(obj);

      // Request without id field at all (not id:null, but completely missing)
      auto response = server.call(R"({"jsonrpc":"2.0","method":"i"})");
      expect(response.empty()) << "Missing id field should be treated as notification, got: " << response;

      // Verify the value can still be read with a proper request
      response = server.call(R"({"jsonrpc":"2.0","method":"i","id":1})");
      expect(response.find(R"("result":42)") != std::string::npos) << response;
   };
};

// Test for issue #2221: function pointers with arguments causing stack overflow
struct static_method_api_t
{
   struct Greet
   {
      constexpr static std::string_view name = "greet";
      static std::string method() { return "Hello, World!"; }
   };

   struct GetNumber
   {
      constexpr static std::string_view name = "getNumber";
      static void method(int) {}
   };

   struct Add
   {
      constexpr static std::string_view name = "add";
      static int method(int a) { return a + 10; }
   };

   struct glaze
   {
      static constexpr auto value =
         glz::object(Greet::name, &Greet::method, GetNumber::name, &GetNumber::method, Add::name, &Add::method);
   };
};

suite jsonrpc_function_pointer_tests = [] {
   "static_method_function_pointers"_test = [] {
      // Issue #2221: This was causing a stack overflow due to infinite recursion
      // when serializing function pointers that matched the nullable_like concept
      glz::registry<glz::opts{}, glz::JSONRPC> server{};

      static_method_api_t api{};
      server.on(api);

      // Test no-arg function returning string
      auto response = server.call(R"({"jsonrpc":"2.0","method":"greet","id":1})");
      expect(response.find(R"("result":"Hello, World!")") != std::string::npos) << response;

      // Test function with int param returning void
      response = server.call(R"({"jsonrpc":"2.0","method":"getNumber","params":42,"id":2})");
      expect(response.find(R"("result":null)") != std::string::npos) << response;

      // Test function with int param returning int
      response = server.call(R"({"jsonrpc":"2.0","method":"add","params":5,"id":3})");
      expect(response.find(R"("result":15)") != std::string::npos) << response;
   };
};

// A request read off a socket ends where the buffer ends. The registry parses the caller's bytes in
// place, so every parse it performs has to stop at that end rather than scan for a '\0' terminator.
// Exactly sized copies reproduce the wire shape; run under a sanitizer these fail loudly otherwise.
struct scalar_api
{
   int value{};
};

suite unterminated_request_tests = [] {
   auto call_exact = [](auto& server, std::string_view request) {
      const std::vector<char> exact{request.begin(), request.end()};
      return server.call(std::string_view{exact.data(), exact.size()});
   };

   "valid_request_ending_at_buffer_end"_test = [&] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      scalar_api api{};
      server.on(api);

      const auto response = call_exact(server, R"({"jsonrpc":"2.0","method":"/value","params":7,"id":1})");
      expect(api.value == 7) << "Params should be applied";
      expect(response.find(R"("result":null)") != std::string::npos) << response;
   };

   "truncated_batch_ending_at_buffer_end"_test = [&] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      scalar_api api{};
      server.on(api);

      const auto response = call_exact(server, "[7");
      expect(response.find(R"("code":-32700)") != std::string::npos) << response;
   };

   "truncated_object_ending_at_buffer_end"_test = [&] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      scalar_api api{};
      server.on(api);

      const auto response = call_exact(server, R"({"jsonrpc":"2.0","method":"/value","id":1,"params":7)");
      expect(response.find(R"("error")") != std::string::npos) << response;
   };

   "bare_scalar_ending_at_buffer_end"_test = [&] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      scalar_api api{};
      server.on(api);

      const auto response = call_exact(server, "7");
      expect(response.find(R"("code":-32600)") != std::string::npos) << response;
   };
};

int main() { return 0; }
