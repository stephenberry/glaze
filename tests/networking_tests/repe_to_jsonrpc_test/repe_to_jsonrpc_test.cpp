// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/rpc/repe/repe_to_jsonrpc.hpp"

#include "glaze/rpc/repe/repe.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;
namespace rpc = glz::rpc;

suite repe_to_jsonrpc_request_tests = [] {
   "simple_method_call"_test = [] {
      repe::message msg{};
      msg.query = "/add";
      msg.body = R"([1,2,3])";
      msg.header.id = 42;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.query_format = repe::query_format::JSON_POINTER;
      msg.header.notify = false;

      auto jsonrpc_str = repe::to_jsonrpc_request(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42})") << jsonrpc_str;
   };

   "method_without_leading_slash"_test = [] {
      repe::message msg{};
      msg.query = "subtract";
      msg.body = R"({"a":5,"b":3})";
      msg.header.id = 100;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.notify = false;

      auto jsonrpc_str = repe::to_jsonrpc_request(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","method":"subtract","params":{"a":5,"b":3},"id":100})") << jsonrpc_str;
   };

   "notification_request"_test = [] {
      repe::message msg{};
      msg.query = "/notify";
      msg.body = R"({"message":"hello"})";
      msg.header.id = 0;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.notify = true;

      auto jsonrpc_str = repe::to_jsonrpc_request(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","method":"notify","params":{"message":"hello"},"id":null})")
         << jsonrpc_str;
   };

   "empty_params"_test = [] {
      repe::message msg{};
      msg.query = "/get_status";
      msg.body = "";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.notify = false;

      auto jsonrpc_str = repe::to_jsonrpc_request(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","method":"get_status","params":{},"id":1})") << jsonrpc_str;
   };

   "non_json_body_error"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      msg.body = "binary data";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::RAW_BINARY;
      msg.header.notify = false;

      auto jsonrpc_str = repe::to_jsonrpc_request(msg);
      expect(jsonrpc_str.find("Invalid request") != std::string::npos) << jsonrpc_str;
      expect(jsonrpc_str.find("REPE body must be JSON format") != std::string::npos) << jsonrpc_str;
   };
};

suite repe_to_jsonrpc_response_tests = [] {
   "success_response"_test = [] {
      repe::message msg{};
      msg.body = R"({"result":"success"})";
      msg.header.id = 42;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.ec = glz::error_code::none;

      auto jsonrpc_str = repe::to_jsonrpc_response(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","result":{"result":"success"},"id":42})") << jsonrpc_str;
   };

   "null_result"_test = [] {
      repe::message msg{};
      msg.body = "";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.ec = glz::error_code::none;

      auto jsonrpc_str = repe::to_jsonrpc_response(msg);
      expect(jsonrpc_str == R"({"jsonrpc":"2.0","result":null,"id":1})") << jsonrpc_str;
   };

   "error_response_with_data"_test = [] {
      repe::message msg{};
      msg.body = "Method not found";
      msg.header.id = 42;
      msg.header.body_format = repe::body_format::UTF8;
      msg.header.ec = glz::error_code::method_not_found;

      auto jsonrpc_str = repe::to_jsonrpc_response(msg);
      expect(jsonrpc_str.find(R"("error":)") != std::string::npos) << jsonrpc_str;
      expect(jsonrpc_str.find(R"("code":-32601)") != std::string::npos) << jsonrpc_str;
      expect(jsonrpc_str.find(R"("message":"Method not found")") != std::string::npos) << jsonrpc_str;
      expect(jsonrpc_str.find(R"("data":"Method not found")") != std::string::npos) << jsonrpc_str;
   };

   "parse_error_response"_test = [] {
      repe::message msg{};
      msg.body = "Invalid JSON";
      msg.header.id = 10;
      msg.header.body_format = repe::body_format::UTF8;
      msg.header.ec = glz::error_code::parse_error;

      auto jsonrpc_str = repe::to_jsonrpc_response(msg);
      expect(jsonrpc_str.find(R"("code":-32700)") != std::string::npos) << jsonrpc_str;
      expect(jsonrpc_str.find(R"("message":"Parse error")") != std::string::npos) << jsonrpc_str;
   };
};

suite jsonrpc_to_repe_request_tests = [] {
   "simple_request"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"add","params":[1,2,3],"id":42})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value()) << "Failed to convert";
      expect(msg->query == "/add") << msg->query;
      expect(msg->body == "[1,2,3]") << msg->body;
      expect(msg->header.id == 42);
      expect(msg->header.notify == false);
      expect(msg->header.body_format == repe::body_format::JSON);
      expect(msg->header.query_format == repe::query_format::JSON_POINTER);
   };

   "request_with_object_params"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"subtract","params":{"a":5,"b":3},"id":100})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->query == "/subtract");
      expect(msg->body.find(R"("a":5)") != std::string::npos);
      expect(msg->body.find(R"("b":3)") != std::string::npos);
      expect(msg->header.id == 100);
   };

   "notification_request"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"notify","params":{"message":"hello"},"id":null})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->query == "/notify");
      expect(msg->header.notify == true);
   };

   "request_with_string_id"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":"test-123"})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id != 0); // Should be hashed
      expect(msg->header.notify == false);
   };

   "request_with_numeric_string_id"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":"999"})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id == 999);
      expect(msg->header.notify == false);
   };
};

suite jsonrpc_to_repe_response_tests = [] {
   "success_response"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","result":{"value":42},"id":10})";

      auto msg = repe::from_jsonrpc_response(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id == 10);
      expect(msg->header.ec == glz::error_code::none);
      expect(msg->body.find(R"("value":42)") != std::string::npos);
      expect(msg->header.body_format == repe::body_format::JSON);
   };

   "error_response"_test = [] {
      std::string jsonrpc =
         R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Details here"},"id":42})";

      auto msg = repe::from_jsonrpc_response(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id == 42);
      expect(msg->header.ec == glz::error_code::method_not_found);
      expect(msg->body == "Details here") << msg->body;
      expect(msg->header.body_format == repe::body_format::UTF8);
   };

   "error_response_without_data"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"},"id":1})";

      auto msg = repe::from_jsonrpc_response(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.ec == glz::error_code::parse_error);
      expect(msg->body == "Parse error") << msg->body;
   };
};

suite error_code_mapping_tests = [] {
   "all_repe_to_jsonrpc_error_codes"_test = [] {
      // Test all major REPE error codes map correctly
      struct error_mapping
      {
         glz::error_code repe_code;
         int jsonrpc_code;
         std::string_view message_contains;
      };

      const std::vector<error_mapping> mappings = {{glz::error_code::none, 0, ""},
                                                   {glz::error_code::parse_error, -32700, "Parse error"},
                                                   {glz::error_code::syntax_error, -32700, "Parse error"},
                                                   {glz::error_code::invalid_header, -32600, "Invalid request"},
                                                   {glz::error_code::version_mismatch, -32600, "Invalid request"},
                                                   {glz::error_code::method_not_found, -32601, "Method not found"}};

      for (const auto& mapping : mappings) {
         if (mapping.repe_code == glz::error_code::none) continue;

         repe::message msg{};
         msg.body = "Error details";
         msg.header.id = 1;
         msg.header.body_format = repe::body_format::UTF8;
         msg.header.ec = mapping.repe_code;

         auto jsonrpc = repe::to_jsonrpc_response(msg);
         expect(jsonrpc.find(std::to_string(mapping.jsonrpc_code)) != std::string::npos) << jsonrpc;
         if (!mapping.message_contains.empty()) {
            expect(jsonrpc.find(mapping.message_contains) != std::string::npos) << jsonrpc;
         }
      }
   };

   "jsonrpc_to_repe_error_codes"_test = [] {
      // Test JSON-RPC error codes map to REPE
      struct error_mapping
      {
         int jsonrpc_code;
         glz::error_code expected_repe;
      };

      const std::vector<error_mapping> mappings = {{-32700, glz::error_code::parse_error},
                                                   {-32600, glz::error_code::invalid_header},
                                                   {-32601, glz::error_code::method_not_found},
                                                   {-32602, glz::error_code::parse_error},
                                                   {-32603, glz::error_code::parse_error}};

      for (const auto& mapping : mappings) {
         std::string jsonrpc = R"({"jsonrpc":"2.0","error":{"code":)" + std::to_string(mapping.jsonrpc_code) +
                               R"(,"message":"Test error"},"id":1})";

         auto msg = repe::from_jsonrpc_response(jsonrpc);
         expect(msg.has_value());
         expect(msg->header.ec == mapping.expected_repe);
      }
   };
};

suite edge_case_tests = [] {
   "special_characters_in_body"_test = [] {
      // Test JSON string escaping in error messages
      repe::message msg{};
      msg.body = R"(Error with "quotes" and \backslashes\ and
newlines)";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::UTF8;
      msg.header.ec = glz::error_code::parse_error;

      auto jsonrpc = repe::to_jsonrpc_response(msg);
      // Should properly escape special characters
      expect(jsonrpc.find(R"(\")") != std::string::npos) << jsonrpc; // Escaped quotes
      expect(jsonrpc.find(R"(\\)") != std::string::npos) << jsonrpc; // Escaped backslashes
      expect(jsonrpc.find(R"(\n)") != std::string::npos) << jsonrpc; // Escaped newlines
   };

   "large_id_values"_test = [] {
      // Test with maximum uint64_t values
      repe::message msg{};
      msg.query = "/test";
      msg.body = "{}";
      msg.header.id = 18446744073709551615ULL; // max uint64_t
      msg.header.body_format = repe::body_format::JSON;
      msg.header.notify = false;

      auto jsonrpc = repe::to_jsonrpc_request(msg);
      expect(jsonrpc.find("18446744073709551615") != std::string::npos) << jsonrpc;
   };

   "empty_method_name"_test = [] {
      repe::message msg{};
      msg.query = "";
      msg.body = "{}";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::JSON;
      msg.header.notify = false;

      auto jsonrpc = repe::to_jsonrpc_request(msg);
      expect(jsonrpc.find(R"("method":"")") != std::string::npos) << jsonrpc;
   };

   "complex_nested_json_params"_test = [] {
      std::string complex_json =
         R"({"jsonrpc":"2.0","method":"process","params":{"nested":{"deeply":{"value":42,"array":[1,2,3],"object":{"key":"value"}}}},"id":1})";

      auto msg = repe::from_jsonrpc_request(complex_json);
      expect(msg.has_value());
      expect(msg->body.find("nested") != std::string::npos);
      expect(msg->body.find("deeply") != std::string::npos);
      expect(msg->body.find("array") != std::string::npos);
   };

   "beve_body_format_error"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      msg.body = "beve data";
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::BEVE;
      msg.header.notify = false;

      auto jsonrpc = repe::to_jsonrpc_request(msg);
      expect(jsonrpc.find("Invalid request") != std::string::npos) << jsonrpc;
      expect(jsonrpc.find("REPE body must be JSON format") != std::string::npos) << jsonrpc;
   };

   "invalid_jsonrpc_parse"_test = [] {
      std::string invalid_json = "not valid json";

      auto msg = repe::from_jsonrpc_request(invalid_json);
      expect(!msg.has_value());
      expect(msg.error() == "Failed to parse JSON-RPC request");
   };

   "invalid_jsonrpc_response_parse"_test = [] {
      std::string invalid_json = "{broken json";

      auto msg = repe::from_jsonrpc_response(invalid_json);
      expect(!msg.has_value());
      expect(msg.error() == "Failed to parse JSON-RPC response");
   };

   "very_long_string_id"_test = [] {
      std::string long_id(1000, 'a'); // 1000 character ID
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":")" + long_id + R"("})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id != 0); // Should be hashed
      expect(msg->header.notify == false);
   };

   "zero_id"_test = [] {
      // Zero is a valid ID
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":0})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->header.id == 0);
      expect(msg->header.notify == false);
   };

   "negative_id_in_jsonrpc"_test = [] {
      // Negative IDs should be handled (will be cast to uint64_t)
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":[],"id":-1})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      // -1 as int64_t becomes very large uint64_t
      expect(msg->header.notify == false);
   };

   "empty_error_body"_test = [] {
      repe::message msg{};
      msg.body = ""; // Empty error message
      msg.header.id = 1;
      msg.header.body_format = repe::body_format::UTF8;
      msg.header.ec = glz::error_code::parse_error;

      auto jsonrpc = repe::to_jsonrpc_response(msg);
      expect(jsonrpc.find(R"("error":)") != std::string::npos) << jsonrpc;
      expect(jsonrpc.find(R"("code":-32700)") != std::string::npos) << jsonrpc;
   };

   "json_array_params"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"sum","params":[1,2,3,4,5],"id":1})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->query == "/sum");
      expect(msg->body == "[1,2,3,4,5]") << msg->body;
   };

   "null_params"_test = [] {
      std::string jsonrpc = R"({"jsonrpc":"2.0","method":"test","params":null,"id":1})";

      auto msg = repe::from_jsonrpc_request(jsonrpc);
      expect(msg.has_value());
      expect(msg->body == "" || msg->body == "null");
   };
};

suite roundtrip_tests = [] {
   "request_roundtrip"_test = [] {
      // Create REPE request
      repe::message original{};
      original.query = "/calculate";
      original.body = R"({"x":10,"y":20})";
      original.header.id = 123;
      original.header.body_format = repe::body_format::JSON;
      original.header.notify = false;

      // Convert to JSON-RPC
      auto jsonrpc = repe::to_jsonrpc_request(original);
      expect(jsonrpc.find("calculate") != std::string::npos);

      // Convert back to REPE
      auto converted = repe::from_jsonrpc_request(jsonrpc);
      expect(converted.has_value());
      expect(converted->query == "/calculate");
      expect(converted->header.id == 123);
      expect(converted->header.notify == false);
   };

   "response_roundtrip_success"_test = [] {
      // Create REPE response
      repe::message original{};
      original.body = R"(30)";
      original.header.id = 123;
      original.header.body_format = repe::body_format::JSON;
      original.header.ec = glz::error_code::none;

      // Convert to JSON-RPC
      auto jsonrpc = repe::to_jsonrpc_response(original);

      // Convert back to REPE
      auto converted = repe::from_jsonrpc_response(jsonrpc);
      expect(converted.has_value());
      expect(converted->header.id == 123);
      expect(converted->header.ec == glz::error_code::none);
      expect(converted->body == "30");
   };

   "response_roundtrip_error"_test = [] {
      // Create REPE error response
      repe::message original{};
      original.body = "Something went wrong";
      original.header.id = 456;
      original.header.body_format = repe::body_format::UTF8;
      original.header.ec = glz::error_code::parse_error;

      // Convert to JSON-RPC
      auto jsonrpc = repe::to_jsonrpc_response(original);

      // Convert back to REPE
      auto converted = repe::from_jsonrpc_response(jsonrpc);
      expect(converted.has_value());
      expect(converted->header.id == 456);
      expect(converted->header.ec == glz::error_code::parse_error);
   };
};

int main() { return 0; }
