// Example demonstrating REPE to JSON-RPC conversion
// Compile: g++ -std=c++20 -I../include repe-jsonrpc-conversion.cpp -o repe-jsonrpc-conversion

#include <iostream>

#include "glaze/rpc/repe/repe_to_jsonrpc.hpp"

namespace repe = glz::repe;

void print_separator(const std::string& title) { std::cout << "\n========== " << title << " ==========\n"; }

int main()
{
   print_separator("REPE Request to JSON-RPC Request");
   {
      // Create a REPE request
      repe::message repe_request{};
      repe_request.query = "/calculate";
      repe_request.body = R"({"x":10,"y":20,"operation":"add"})";
      repe_request.header.id = 12345;
      repe_request.header.body_format = repe::body_format::JSON;
      repe_request.header.notify = false;

      // Convert to JSON-RPC
      std::string jsonrpc_request = repe::to_jsonrpc_request(repe_request);

      std::cout << "REPE Request:\n";
      std::cout << "  Query: " << repe_request.query << "\n";
      std::cout << "  Body: " << repe_request.body << "\n";
      std::cout << "  ID: " << repe_request.header.id << "\n";
      std::cout << "\nJSON-RPC Request:\n";
      std::cout << "  " << jsonrpc_request << "\n";
   }

   print_separator("REPE Response to JSON-RPC Response (Success)");
   {
      // Create a REPE success response
      repe::message repe_response{};
      repe_response.body = R"({"result":30})";
      repe_response.header.id = 12345;
      repe_response.header.body_format = repe::body_format::JSON;
      repe_response.header.ec = glz::error_code::none;

      // Convert to JSON-RPC
      std::string jsonrpc_response = repe::to_jsonrpc_response(repe_response);

      std::cout << "REPE Response:\n";
      std::cout << "  Body: " << repe_response.body << "\n";
      std::cout << "  ID: " << repe_response.header.id << "\n";
      std::cout << "  Error Code: " << static_cast<int>(repe_response.header.ec) << "\n";
      std::cout << "\nJSON-RPC Response:\n";
      std::cout << "  " << jsonrpc_response << "\n";
   }

   print_separator("REPE Response to JSON-RPC Response (Error)");
   {
      // Create a REPE error response
      repe::message repe_error{};
      repe_error.body = "Invalid operation specified";
      repe_error.header.id = 12345;
      repe_error.header.body_format = repe::body_format::UTF8;
      repe_error.header.ec = glz::error_code::parse_error;

      // Convert to JSON-RPC
      std::string jsonrpc_error = repe::to_jsonrpc_response(repe_error);

      std::cout << "REPE Error Response:\n";
      std::cout << "  Body: " << repe_error.body << "\n";
      std::cout << "  ID: " << repe_error.header.id << "\n";
      std::cout << "  Error Code: " << static_cast<int>(repe_error.header.ec) << "\n";
      std::cout << "\nJSON-RPC Error Response:\n";
      std::cout << "  " << jsonrpc_error << "\n";
   }

   print_separator("JSON-RPC Request to REPE Request");
   {
      std::string jsonrpc_request = R"({"jsonrpc":"2.0","method":"multiply","params":{"a":5,"b":7},"id":99})";

      // Convert to REPE
      auto repe_request = repe::from_jsonrpc_request(jsonrpc_request);

      std::cout << "JSON-RPC Request:\n";
      std::cout << "  " << jsonrpc_request << "\n";

      if (repe_request.has_value()) {
         std::cout << "\nREPE Request:\n";
         std::cout << "  Query: " << repe_request->query << "\n";
         std::cout << "  Body: " << repe_request->body << "\n";
         std::cout << "  ID: " << repe_request->header.id << "\n";
         std::cout << "  Notify: " << (repe_request->header.notify ? "true" : "false") << "\n";
      }
      else {
         std::cout << "\nConversion failed: " << repe_request.error() << "\n";
      }
   }

   print_separator("JSON-RPC Response to REPE Response");
   {
      std::string jsonrpc_response = R"({"jsonrpc":"2.0","result":{"value":35},"id":99})";

      // Convert to REPE
      auto repe_response = repe::from_jsonrpc_response(jsonrpc_response);

      std::cout << "JSON-RPC Response:\n";
      std::cout << "  " << jsonrpc_response << "\n";

      if (repe_response.has_value()) {
         std::cout << "\nREPE Response:\n";
         std::cout << "  Body: " << repe_response->body << "\n";
         std::cout << "  ID: " << repe_response->header.id << "\n";
         std::cout << "  Error Code: " << static_cast<int>(repe_response->header.ec) << "\n";
      }
      else {
         std::cout << "\nConversion failed: " << repe_response.error() << "\n";
      }
   }

   print_separator("Notification Handling");
   {
      // REPE notification (notify=true)
      repe::message notification{};
      notification.query = "/log";
      notification.body = R"({"level":"info","message":"System started"})";
      notification.header.notify = true;
      notification.header.body_format = repe::body_format::JSON; // Must set format!

      std::string jsonrpc_notification = repe::to_jsonrpc_request(notification);

      std::cout << "REPE Notification (notify=true):\n";
      std::cout << "  Query: " << notification.query << "\n";
      std::cout << "  Body: " << notification.body << "\n";
      std::cout << "\nJSON-RPC Notification (id=null):\n";
      std::cout << "  " << jsonrpc_notification << "\n";
   }

   print_separator("Roundtrip Conversion");
   {
      // Create original REPE request
      repe::message original{};
      original.query = "/divide";
      original.body = R"({"dividend":100,"divisor":5})";
      original.header.id = 777;
      original.header.body_format = repe::body_format::JSON;
      original.header.notify = false;

      // Convert to JSON-RPC
      std::string jsonrpc = repe::to_jsonrpc_request(original);

      // Convert back to REPE
      auto roundtrip = repe::from_jsonrpc_request(jsonrpc);

      std::cout << "Original REPE Request:\n";
      std::cout << "  Query: " << original.query << "\n";
      std::cout << "  Body: " << original.body << "\n";
      std::cout << "  ID: " << original.header.id << "\n";

      std::cout << "\nAfter Roundtrip:\n";
      if (roundtrip.has_value()) {
         std::cout << "  Query: " << roundtrip->query << "\n";
         std::cout << "  Body: " << roundtrip->body << "\n";
         std::cout << "  ID: " << roundtrip->header.id << "\n";
         std::cout << "  Match: "
                   << (original.query == roundtrip->query && original.header.id == roundtrip->header.id ? "✓" : "✗")
                   << "\n";
      }
      else {
         std::cout << "  Conversion failed!\n";
      }
   }

   std::cout << "\n";
   return 0;
}
