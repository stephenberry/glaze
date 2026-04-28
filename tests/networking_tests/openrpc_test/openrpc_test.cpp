// Glaze Library
// For the license information refer to glaze.hpp

#include <set>
#include <span>

#include "glaze/glaze.hpp"
#include "glaze/rpc/registry.hpp"
#include "glaze/rpc/repe/buffer.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;

// ============================================================
// Test helpers for span-based API
// ============================================================
namespace test_helpers
{
   namespace detail
   {
      inline std::string& request_buffer()
      {
         thread_local std::string buf;
         return buf;
      }

      inline std::string& response_buffer()
      {
         thread_local std::string buf;
         return buf;
      }

      inline repe::message& request_message()
      {
         thread_local repe::message msg;
         msg.query.clear();
         msg.body.clear();
         return msg;
      }
   }

   template <class Registry>
   inline repe::message call(Registry& registry, repe::message& request)
   {
      auto& request_buffer = detail::request_buffer();
      auto& response_buffer = detail::response_buffer();

      repe::to_buffer(request, request_buffer);
      registry.call(std::span<const char>{request_buffer}, response_buffer);

      repe::message response{};
      repe::from_buffer(response_buffer, response);
      return response;
   }

   template <class Registry>
   inline repe::message call_json(Registry& registry, const repe::user_header& hdr)
   {
      auto& request = detail::request_message();
      repe::request_json(hdr, request);
      return call(registry, request);
   }

   template <class Registry, class Value>
   inline repe::message call_json(Registry& registry, const repe::user_header& hdr, Value&& value)
   {
      auto& request = detail::request_message();
      repe::request_json(hdr, request, std::forward<Value>(value));
      return call(registry, request);
   }
}

// ============================================================
// Test types
// ============================================================

struct my_api_t
{
   int count{};
   std::string name{"default"};
   std::function<int()> get_count = [this] { return count; };
   std::function<void(int)> set_count = [this](int v) { count = v; };
   std::function<double(std::vector<double>&)> max_value = [](std::vector<double>& vec) {
      return (std::ranges::max)(vec);
   };
};

struct nested_t
{
   my_api_t api{};
   std::string label{};
};

// ============================================================
// Tests
// ============================================================

suite open_rpc_spec_generation = [] {
   "open_rpc_spec_basic"_test = [] {
      glz::registry server{};
      server.open_rpc_info.title = "My API";
      server.open_rpc_info.version = "2.0.0";
      server.open_rpc_info.description = "Test API";

      my_api_t api{};
      server.on(api);

      auto spec = server.open_rpc_spec();

      expect(spec.openrpc == "1.3.2");
      expect(spec.info.title == "My API");
      expect(spec.info.version == "2.0.0");
      expect(spec.info.description == "Test API");
      expect(!spec.methods.empty());

      // Check that we have methods for: root, /count, /name, /get_count, /set_count, /max_value
      std::set<std::string> method_names;
      for (const auto& m : spec.methods) {
         method_names.insert(m.name);
      }

      expect(method_names.contains("")); // root endpoint
      expect(method_names.contains("/count"));
      expect(method_names.contains("/name"));
      expect(method_names.contains("/get_count"));
      expect(method_names.contains("/set_count"));
      expect(method_names.contains("/max_value"));
   };

   "open_rpc_spec_function_metadata"_test = [] {
      glz::registry server{};

      my_api_t api{};
      server.on(api);

      auto spec = server.open_rpc_spec();

      // Find /get_count - no-param function returning int
      auto it = std::find_if(spec.methods.begin(), spec.methods.end(),
                              [](const auto& m) { return m.name == "/get_count"; });
      expect(it != spec.methods.end());
      expect(it->params.empty()); // no params
      expect(it->result.has_value()); // has result
      expect(it->result->name == "result");
      expect(it->result->schema.str.find("integer") != std::string::npos); // int schema contains "integer"

      // Find /set_count - function with int param, void return
      it = std::find_if(spec.methods.begin(), spec.methods.end(),
                         [](const auto& m) { return m.name == "/set_count"; });
      expect(it != spec.methods.end());
      expect(it->params.size() == 1u);
      expect(it->params[0].name == "params");
      expect(it->params[0].required.has_value());
      expect(it->params[0].required.value() == true);
      expect(!it->result.has_value()); // void return

      // Find /max_value - function with vector<double> param, double return
      it = std::find_if(spec.methods.begin(), spec.methods.end(),
                         [](const auto& m) { return m.name == "/max_value"; });
      expect(it != spec.methods.end());
      expect(it->params.size() == 1u);
      expect(it->params[0].required.has_value());
      expect(it->params[0].required.value() == true);
      expect(it->result.has_value());
      expect(it->result->schema.str.find("number") != std::string::npos); // double schema contains "number"
   };

   "open_rpc_spec_variable_metadata"_test = [] {
      glz::registry server{};

      my_api_t api{};
      server.on(api);

      auto spec = server.open_rpc_spec();

      // Find /count - int variable (read/write)
      auto it = std::find_if(spec.methods.begin(), spec.methods.end(),
                              [](const auto& m) { return m.name == "/count"; });
      expect(it != spec.methods.end());
      // Variables have params (for write) and result (for read)
      expect(it->params.size() == 1u);
      expect(!it->params[0].required.has_value()); // params not required (read doesn't need them)
      expect(it->result.has_value());
      expect(it->result->schema.str.find("integer") != std::string::npos);

      // Find /name - string variable
      it = std::find_if(spec.methods.begin(), spec.methods.end(),
                         [](const auto& m) { return m.name == "/name"; });
      expect(it != spec.methods.end());
      expect(it->params.size() == 1u);
      expect(it->result.has_value());
      expect(it->result->schema.str.find("string") != std::string::npos);
   };

   "open_rpc_spec_serialization"_test = [] {
      glz::registry server{};
      server.open_rpc_info.title = "Test API";
      server.open_rpc_info.version = "1.0.0";

      my_api_t api{};
      server.on(api);

      auto spec = server.open_rpc_spec();

      // Serialize to JSON
      auto json = glz::write_json(spec);
      expect(json.has_value());

      // Verify it's valid JSON containing expected fields
      auto& str = *json;
      expect(str.find("\"openrpc\":\"1.3.2\"") != std::string::npos);
      expect(str.find("\"title\":\"Test API\"") != std::string::npos);
      expect(str.find("\"methods\"") != std::string::npos);
      expect(str.find("/get_count") != std::string::npos);
      expect(str.find("/max_value") != std::string::npos);
   };

   "open_rpc_endpoint_repe"_test = [] {
      using namespace test_helpers;

      glz::registry server{};
      server.open_rpc_info.title = "REPE API";

      my_api_t api{};
      server.on(api);
      server.register_open_rpc();

      // Call the /open_rpc endpoint
      auto response = call_json(server, repe::user_header{.query = "/open_rpc"});
      expect(response.header.ec == glz::error_code::none);

      // Parse the response body as OpenRPC
      auto spec = glz::read_json<glz::open_rpc>(response.body);
      expect(spec.has_value()) << "Failed to parse OpenRPC response: " << response.body;

      if (spec) {
         expect(spec->openrpc == "1.3.2");
         expect(spec->info.title == "REPE API");
         expect(!spec->methods.empty());
      }
   };

   "open_rpc_endpoint_jsonrpc"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      server.open_rpc_info.title = "JSONRPC API";

      my_api_t api{};
      server.on(api);
      server.register_open_rpc();

      // Call via JSON-RPC
      auto response =
         server.call(R"({"jsonrpc":"2.0","method":"/open_rpc","id":1})");

      expect(!response.empty());
      expect(response.find("\"result\"") != std::string::npos);
      expect(response.find("1.3.2") != std::string::npos);
      expect(response.find("JSONRPC API") != std::string::npos);
   };

   "open_rpc_nested_objects"_test = [] {
      glz::registry server{};

      nested_t nested{};
      server.on(nested);

      auto spec = server.open_rpc_spec();

      std::set<std::string> method_names;
      for (const auto& m : spec.methods) {
         method_names.insert(m.name);
      }

      // Root and nested object endpoints should be present
      expect(method_names.contains(""));
      expect(method_names.contains("/api"));
      // Leaf endpoints within the nested object
      expect(method_names.contains("/api/count"));
      expect(method_names.contains("/api/get_count"));
      // Top-level leaf
      expect(method_names.contains("/label"));
   };

   "open_rpc_clear"_test = [] {
      glz::registry server{};

      my_api_t api{};
      server.on(api);

      auto spec1 = server.open_rpc_spec();
      expect(!spec1.methods.empty());

      server.clear();

      auto spec2 = server.open_rpc_spec();
      expect(spec2.methods.empty());
   };
};

int main() {}
