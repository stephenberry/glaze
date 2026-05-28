// Glaze Library
// For the license information refer to glaze.hpp

// Verifies that glz::registry and glz::http_router work under -fno-exceptions and,
// just as importantly, that errors are never hidden: route-registration conflicts are
// reported by return value (not swallowed to stderr or aborted), and a handler can fail
// a request by returning glz::expected instead of throwing. This translation lets the
// same registered handler report failures with or without exceptions.
//
// This file is compiled twice (see CMakeLists.txt): once with exceptions and once with
// -fno-exceptions, so the behavior is verified to be identical in both configurations.
// See https://github.com/stephenberry/glaze/issues/2265.

#include <functional>
#include <span>
#include <string>

#include "glaze/net/http_router.hpp"
#include "glaze/rpc/registry.hpp"
#include "ut/ut.hpp"

using namespace ut;
namespace repe = glz::repe;

namespace
{
   // Run a pre-built REPE request through the span-based (zero-copy) registry API and
   // return the parsed response message.
   template <class Registry>
   repe::message run_repe(Registry& registry, repe::message& request)
   {
      std::string request_buffer;
      std::string response_buffer;
      repe::to_buffer(request, request_buffer);
      registry.call(std::span<const char>{request_buffer}, response_buffer);
      repe::message response{};
      repe::from_buffer(response_buffer, response);
      return response;
   }
}

// ----------------------------------------------------------------------------
// http_router: structural conflicts are surfaced, identical routes overwrite.
// ----------------------------------------------------------------------------

suite http_router_registration = [] {
   "try_route_success"_test = [] {
      glz::http_router router{};
      auto result = router.try_route(glz::http_method::GET, "/users/:id", [](const glz::request&, glz::response&) {});
      expect(result.has_value());
      expect(!router.has_route_error());
   };

   "try_route_reports_parameter_conflict"_test = [] {
      glz::http_router router{};
      std::ignore = router.try_route(glz::http_method::GET, "/users/:id", [](const glz::request&, glz::response&) {});
      // A different parameter name at the same position cannot be represented.
      auto result =
         router.try_route(glz::http_method::GET, "/users/:name/profile", [](const glz::request&, glz::response&) {});
      expect(!result.has_value());
      expect(result.error().find("different parameter names") != std::string::npos) << result.error();
      expect(router.has_route_error());
      expect(router.route_error() == result.error());
      router.clear_route_error();
      expect(!router.has_route_error());
   };

   "try_route_reports_wildcard_not_last"_test = [] {
      glz::http_router router{};
      auto result =
         router.try_route(glz::http_method::GET, "/files/*path/extra", [](const glz::request&, glz::response&) {});
      expect(!result.has_value());
      expect(result.error().find("Wildcard must be the last segment") != std::string::npos) << result.error();
   };

   "identical_route_overwrites_without_error"_test = [] {
      glz::http_router router{};
      auto first = router.try_route(glz::http_method::GET, "/x", [](const glz::request&, glz::response&) {});
      auto second = router.try_route(glz::http_method::GET, "/x", [](const glz::request&, glz::response&) {});
      expect(first.has_value());
      expect(second.has_value()); // overwriting an identical route is intentional, not a conflict
   };

#if __cpp_exceptions
   "route_throws_on_conflict_when_exceptions_enabled"_test = [] {
      glz::http_router router{};
      router.get("/users/:id", [](const glz::request&, glz::response&) {});
      expect(throws([&] {
         router.route(glz::http_method::GET, "/users/:name", [](const glz::request&, glz::response&) {});
      })) << "route() must surface a structural conflict loudly when exceptions are enabled";
   };
#endif
};

// ----------------------------------------------------------------------------
// REPE: a handler may fail a request by returning glz::expected.
// ----------------------------------------------------------------------------

struct repe_expected_t
{
   // string error -> error_code::parse_error, matching a thrown handler
   std::function<glz::expected<int, std::string>(int)> halve = [](int x) -> glz::expected<int, std::string> {
      if (x % 2 != 0) {
         return glz::unexpected(std::string("odd input: ") + std::to_string(x));
      }
      return x / 2;
   };
   // explicit error_code error channel
   std::function<glz::expected<int, glz::error_code>(int)> non_negative =
      [](int x) -> glz::expected<int, glz::error_code> {
      if (x < 0) {
         return glz::unexpected(glz::error_code::invalid_body);
      }
      return x;
   };
};

suite repe_expected_handlers = [] {
   "repe_expected_success"_test = [] {
      glz::registry<glz::opts{}, glz::REPE> server{};
      repe_expected_t obj{};
      server.on(obj);

      repe::message request{};
      repe::request_json({"/halve"}, request, 10);
      auto response = run_repe(server, request);
      expect(response.header.ec == glz::error_code::none) << response.body;
      expect(response.body == "5") << response.body;
   };

   "repe_expected_string_error_maps_to_parse_error_and_preserves_id"_test = [] {
      glz::registry<glz::opts{}, glz::REPE> server{};
      repe_expected_t obj{};
      server.on(obj);

      repe::message request{};
      repe::request_json({"/halve"}, request, 3);
      request.header.id = 4242;
      auto response = run_repe(server, request);
      // Returning glz::unexpected(string) yields the same code a thrown handler would.
      expect(response.header.ec == glz::error_code::parse_error) << response.body;
      expect(response.header.id == 4242) << "id must be preserved through a handler error";
      expect(response.body.find("odd input: 3") != std::string::npos) << response.body;
   };

   "repe_expected_error_code_passthrough"_test = [] {
      glz::registry<glz::opts{}, glz::REPE> server{};
      repe_expected_t obj{};
      server.on(obj);

      repe::message request{};
      repe::request_json({"/non_negative"}, request, -1);
      auto response = run_repe(server, request);
      expect(response.header.ec == glz::error_code::invalid_body) << response.body;
   };
};

// ----------------------------------------------------------------------------
// JSON-RPC: glz::expected -> JSON-RPC error translation (issue #2265).
// ----------------------------------------------------------------------------

struct jsonrpc_expected_t
{
   std::function<glz::expected<int, std::string>(int)> halve = [](int x) -> glz::expected<int, std::string> {
      if (x % 2 != 0) {
         return glz::unexpected(std::string("odd input"));
      }
      return x / 2;
   };
   // Full JSON-RPC fidelity: custom code, message, and data.
   std::function<glz::expected<int, glz::rpc::error>(int)> checked = [](int x) -> glz::expected<int, glz::rpc::error> {
      if (x < 0) {
         return glz::unexpected(
            glz::rpc::error{glz::rpc::error_e::invalid_params, std::optional<std::string>{"x must be >= 0"}});
      }
      return x;
   };
};

suite jsonrpc_expected_handlers = [] {
   "jsonrpc_expected_success"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      jsonrpc_expected_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"halve","params":4,"id":1})");
      expect(response.find(R"("result":2)") != std::string::npos) << response;
   };

   "jsonrpc_expected_string_error_maps_to_internal_error"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      jsonrpc_expected_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"halve","params":3,"id":2})");
      expect(response.find(R"("code":-32603)") != std::string::npos) << response; // internal error
      expect(response.find("odd input") != std::string::npos) << response;
      expect(response.find(R"("id":2)") != std::string::npos) << response;
   };

   "jsonrpc_expected_rpc_error_full_fidelity"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      jsonrpc_expected_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"checked","params":-5,"id":3})");
      expect(response.find(R"("code":-32602)") != std::string::npos) << response; // invalid_params
      expect(response.find("x must be >= 0") != std::string::npos) << response; // data passthrough
      expect(response.find(R"("id":3)") != std::string::npos) << response;
   };
};

// ----------------------------------------------------------------------------
// REST: try_on() reports registration outcome without throwing.
// ----------------------------------------------------------------------------

struct rest_obj_t
{
   int value{};
   std::string name{};
};

suite rest_registration = [] {
   "rest_try_on_success"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_obj_t obj{};
      auto result = reg.try_on(obj);
      expect(result.has_value()) << (result ? std::string{} : std::string(result.error()));
      expect(!reg.endpoints.has_route_error());
      expect(!reg.endpoints.routes().empty()) << "registration must create routes";
      // A registered member route must be reachable after registration.
      auto [handler, params] = reg.endpoints.match(glz::http_method::GET, "/value");
      expect(bool(handler)) << "registered member route should be reachable";
   };
};

int main() { return 0; }
