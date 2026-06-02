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
      // try_* is the explicit return-value path: the caller owns the error, and the
      // route_error() side-channel (for the chaining variants) stays clean.
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

   // try_stream()/try_websocket() give streaming and WebSocket routes the same
   // return-value conflict channel that try_route() gives normal routes, and like
   // try_route() they report purely by return value without touching route_error().
   "try_stream_reports_conflict_by_return_value"_test = [] {
      glz::http_router router{};
      auto ok = router.try_stream(glz::http_method::GET, "/events/:id", glz::streaming_handler{});
      expect(ok.has_value());
      expect(!router.has_route_error());
      // A different parameter name at the same position cannot be represented.
      auto conflict = router.try_stream(glz::http_method::GET, "/events/:name/tail", glz::streaming_handler{});
      expect(!conflict.has_value());
      expect(conflict.error().find("different parameter names") != std::string::npos) << conflict.error();
      expect(!router.has_route_error()); // returned by value, not written to the side-channel
   };

   "try_websocket_reports_conflict_by_return_value"_test = [] {
      glz::http_router router{};
      auto ok = router.try_websocket("/ws/:id", glz::websocket_handler{});
      expect(ok.has_value());
      expect(!router.has_route_error());
      auto conflict = router.try_websocket("/ws/:name/tail", glz::websocket_handler{});
      expect(!conflict.has_value());
      expect(conflict.error().find("different parameter names") != std::string::npos) << conflict.error();
      expect(!router.has_route_error());
   };

#if __cpp_exceptions
   "route_throws_on_conflict_when_exceptions_enabled"_test = [] {
      glz::http_router router{};
      router.get("/users/:id", [](const glz::request&, glz::response&) {});
      expect(throws([&] {
         router.route(glz::http_method::GET, "/users/:name", [](const glz::request&, glz::response&) {});
      })) << "route() must surface a structural conflict loudly when exceptions are enabled";
   };
#else
   "route_records_conflict_on_side_channel_without_exceptions"_test = [] {
      glz::http_router router{};
      router.get("/users/:id", [](const glz::request&, glz::response&) {});
      // Under -fno-exceptions the chaining route() cannot throw or return, so it records
      // the conflict on the side-channel. This is the path the try_* variants bypass.
      router.route(glz::http_method::GET, "/users/:name", [](const glz::request&, glz::response&) {});
      expect(router.has_route_error());
      expect(router.route_error().find("different parameter names") != std::string::npos) << router.route_error();
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
   // glz::error_ctx error channel: both the code and the custom message propagate.
   std::function<glz::expected<int, glz::error_ctx>(int)> bounded = [](int x) -> glz::expected<int, glz::error_ctx> {
      if (x > 100) {
         return glz::unexpected(
            glz::error_ctx{.ec = glz::error_code::constraint_violated, .custom_error_message = "out of range"});
      }
      return x;
   };
};

// A registered data member of type glz::expected is read as a stored value, not a
// handler result. The expected-as-error-channel interception lives in
// write_handler_result (applied only at function call sites), so reading the member
// serializes its JSON shape rather than translating an error state to a protocol error.
struct repe_expected_member_t
{
   glz::expected<int, std::string> cached = glz::unexpected(std::string("stale"));
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

   "repe_expected_error_ctx_propagates_code_and_message"_test = [] {
      glz::registry<glz::opts{}, glz::REPE> server{};
      repe_expected_t obj{};
      server.on(obj);

      repe::message request{};
      repe::request_json({"/bounded"}, request, 250);
      auto response = run_repe(server, request);
      // error_ctx carries both fields: the code lands in the header, the message in the body.
      expect(response.header.ec == glz::error_code::constraint_violated) << response.body;
      expect(response.body == "out of range") << response.body;
   };

   "repe_expected_data_member_is_serialized_not_intercepted"_test = [] {
      glz::registry<glz::opts{}, glz::REPE> server{};
      repe_expected_member_t obj{};
      server.on(obj);

      repe::message request{};
      repe::request_json({"/cached"}, request); // read the member (no body)
      auto response = run_repe(server, request);
      // Reading a stored glz::expected is a success that serializes its JSON shape; it is
      // NOT reinterpreted as a handler error.
      expect(response.header.ec == glz::error_code::none) << response.body;
      expect(response.body == R"({"unexpected":"stale"})") << response.body;
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
   // Full JSON-RPC fidelity: custom code, message, and data. The glz::rpc::result<T>
   // alias and the invalid_params() helper replace the longhand
   // glz::unexpected(glz::rpc::error{glz::rpc::error_e::invalid_params, std::optional<std::string>{...}}).
   std::function<glz::rpc::result<int>(int)> checked = [](int x) -> glz::rpc::result<int> {
      if (x < 0) {
         return glz::rpc::invalid_params("x must be >= 0");
      }
      return x;
   };
   // glz::error_ctx -> JSON-RPC internal error; the context detail is carried in data.
   std::function<glz::expected<int, glz::error_ctx>(int)> bounded = [](int x) -> glz::expected<int, glz::error_ctx> {
      if (x > 100) {
         return glz::unexpected(
            glz::error_ctx{.ec = glz::error_code::constraint_violated, .custom_error_message = "out of range"});
      }
      return x;
   };
};

// As with REPE, a registered glz::expected data member is read as a stored value
// (serialized JSON shape), not reinterpreted as a JSON-RPC error.
struct jsonrpc_expected_member_t
{
   glz::expected<int, std::string> cached = glz::unexpected(std::string("stale"));
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
      // Reserved code -> standard message, handler detail carried in data (mirrors the thrown path).
      expect(response.find(R"("message":"Internal error")") != std::string::npos) << response;
      expect(response.find(R"("data":"odd input")") != std::string::npos) << response;
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

   "jsonrpc_expected_error_ctx_maps_to_internal_error"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      jsonrpc_expected_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"bounded","params":250,"id":7})");
      expect(response.find(R"("code":-32603)") != std::string::npos) << response; // internal error
      // Reserved code -> standard message, context detail carried in data (mirrors the thrown path).
      expect(response.find(R"("message":"Internal error")") != std::string::npos) << response;
      expect(response.find(R"("data":"out of range")") != std::string::npos) << response;
      expect(response.find(R"("id":7)") != std::string::npos) << response;
   };

   "jsonrpc_expected_data_member_is_serialized_not_intercepted"_test = [] {
      glz::registry<glz::opts{}, glz::JSONRPC> server{};
      jsonrpc_expected_member_t obj{};
      server.on(obj);

      auto response = server.call(R"({"jsonrpc":"2.0","method":"cached","id":8})");
      // Read as a value: the result is the serialized expected, not a JSON-RPC error object.
      expect(response.find(R"("result":{"unexpected":"stale"})") != std::string::npos) << response;
      expect(response.find(R"("error")") == std::string::npos) << response;
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

// ----------------------------------------------------------------------------
// Compile-time guard: merge registration is available for REPE/JSON-RPC but not
// REST. REST's registry_impl has no register_merge_endpoint, so on(merge) and
// try_on(merge) are constrained out (requires Proto != REST) and produce an honest
// "no matching function" diagnostic instead of a hard error deep in the
// implementation. These checks lock the constraint in: they fail to compile if it
// is dropped, and they will flag it if REST merge is ever implemented (at which
// point the constraint, and these asserts, should be revisited). See issue #2265.
// ----------------------------------------------------------------------------

namespace merge_constraint_checks
{
   struct merge_a_t
   {
      int x{};
   };
   struct merge_b_t
   {
      int y{};
   };

   template <uint32_t Proto>
   using reg_t = glz::registry<glz::opts{}, Proto>;

   template <class Reg>
   concept can_on_merge = requires(Reg reg, glz::merge<merge_a_t&, merge_b_t&> m) { reg.on(m); };
   template <class Reg>
   concept can_try_on_merge = requires(Reg reg, glz::merge<merge_a_t&, merge_b_t&> m) { reg.try_on(m); };

   static_assert(!can_on_merge<reg_t<glz::REST>>, "REST must not advertise on(merge)");
   static_assert(!can_try_on_merge<reg_t<glz::REST>>, "REST must not advertise try_on(merge)");
   static_assert(can_on_merge<reg_t<glz::REPE>>, "REPE must support on(merge)");
   static_assert(can_try_on_merge<reg_t<glz::REPE>>, "REPE must support try_on(merge)");
   static_assert(can_on_merge<reg_t<glz::JSONRPC>>, "JSON-RPC must support on(merge)");
   static_assert(can_try_on_merge<reg_t<glz::JSONRPC>>, "JSON-RPC must support try_on(merge)");
}

// ----------------------------------------------------------------------------
// REST: a reflected handler may fail a request by returning glz::expected. The
// registry maps the error to an HTTP status + glz::http_error body rather than
// serializing the expected as a 200 success payload. See issue #2265.
// ----------------------------------------------------------------------------

struct rest_expected_t
{
   // POST /halve : string error -> 500 (server-side) by default.
   std::function<glz::expected<int, std::string>(int)> halve = [](int x) -> glz::expected<int, std::string> {
      if (x % 2 != 0) return glz::unexpected(std::string("odd input"));
      return x / 2;
   };

   // POST /non_negative : glz::error_code -> mapped HTTP status (invalid_body -> 400).
   std::function<glz::expected<int, glz::error_code>(int)> non_negative =
      [](int x) -> glz::expected<int, glz::error_code> {
      if (x < 0) return glz::unexpected(glz::error_code::invalid_body);
      return x;
   };

   // POST /find : glz::http_error -> explicit status + message (full control).
   std::function<glz::expected<int, glz::http_error>(int)> find = [](int id) -> glz::expected<int, glz::http_error> {
      if (id != 42) return glz::unexpected(glz::http_error{404, "not found"});
      return id;
   };

   // GET /ping : no-arg function endpoint that fails with an explicit status.
   std::function<glz::expected<std::string, glz::http_error>()> ping =
      []() -> glz::expected<std::string, glz::http_error> {
      return glz::unexpected(glz::http_error{503, "warming up"});
   };

   // POST /reset : expected<void, E> -> 204 on success, mapped status on failure.
   std::function<glz::expected<void, std::string>(int)> reset = [](int code) -> glz::expected<void, std::string> {
      if (code != 0) return glz::unexpected(std::string("bad code"));
      return {};
   };

   // POST /bounded : glz::error_ctx -> 500 with the formatted context as the message.
   std::function<glz::expected<int, glz::error_ctx>(int)> bounded = [](int x) -> glz::expected<int, glz::error_ctx> {
      if (x > 100)
         return glz::unexpected(
            glz::error_ctx{.ec = glz::error_code::constraint_violated, .custom_error_message = "out of range"});
      return x;
   };
};

// As with REPE and JSON-RPC, a registered glz::expected data member is read as a stored
// value (serialized JSON shape), not reinterpreted as an HTTP error. REST never had the
// #2265 bug because it keeps the stored-value path separate from handler-result writing,
// but nothing asserted that until now.
struct rest_expected_member_t
{
   glz::expected<int, std::string> cached = glz::unexpected(std::string("stale"));
};

namespace
{
   // Invoke a registered REST route directly (no live server) and return the response.
   glz::response run_rest(glz::registry<glz::opts{}, glz::REST>& reg, glz::http_method method, std::string_view path,
                          std::string body)
   {
      auto [handler, params] = reg.endpoints.match(method, path);
      expect(bool(handler)) << "route should be reachable: " << path;
      glz::request req{
         .method = method, .target = std::string(path), .params = std::move(params), .body = std::move(body)};
      glz::response res{};
      if (handler) {
         handler(req, res);
      }
      return res;
   }
}

suite rest_expected_handlers = [] {
   "rest_expected_success_serializes_value"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::POST, "/halve", "4");
      expect(res.status_code == 200) << res.status_code;
      expect(res.response_body == "2") << res.response_body; // unwrapped value, not {"unexpected":..}
   };

   "rest_expected_string_error_defaults_to_500"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::POST, "/halve", "3");
      expect(res.status_code == 500) << res.status_code;
      glz::http_error err{};
      expect(!glz::read_json(err, res.response_body)) << res.response_body;
      expect(err.status == 500);
      expect(err.message == "odd input") << res.response_body;
   };

   "rest_expected_error_code_maps_to_status"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::POST, "/non_negative", "-1");
      expect(res.status_code == 400) << res.status_code; // invalid_body -> 400
      glz::http_error err{};
      expect(!glz::read_json(err, res.response_body)) << res.response_body;
      expect(err.status == 400);
   };

   "rest_expected_http_error_full_control"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::POST, "/find", "7");
      expect(res.status_code == 404) << res.status_code;
      glz::http_error err{};
      expect(!glz::read_json(err, res.response_body)) << res.response_body;
      expect(err.status == 404);
      expect(err.message == "not found") << res.response_body;
      // The success branch (id == 42) returns the unwrapped int body with a 200 status.
      auto ok = run_rest(reg, glz::http_method::POST, "/find", "42");
      expect(ok.status_code == 200) << ok.status_code;
      expect(ok.response_body == "42") << ok.response_body;
   };

   "rest_expected_get_no_arg_failure"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::GET, "/ping", "");
      expect(res.status_code == 503) << res.status_code;
   };

   "rest_expected_void_success_is_204"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto ok = run_rest(reg, glz::http_method::POST, "/reset", "0");
      expect(ok.status_code == 204) << ok.status_code;
      auto bad = run_rest(reg, glz::http_method::POST, "/reset", "1");
      expect(bad.status_code == 500) << bad.status_code;
   };

   "rest_expected_error_ctx_maps_to_500_with_formatted_message"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_t obj{};
      reg.on(obj);
      auto res = run_rest(reg, glz::http_method::POST, "/bounded", "250");
      expect(res.status_code == 500) << res.status_code;
      glz::http_error err{};
      expect(!glz::read_json(err, res.response_body)) << res.response_body;
      expect(err.status == 500);
      expect(err.message.find("out of range") != std::string::npos) << res.response_body;
   };

   "rest_expected_data_member_is_serialized_not_intercepted"_test = [] {
      glz::registry<glz::opts{}, glz::REST> reg{};
      rest_expected_member_t obj{};
      reg.on(obj);
      // GET the member: reading a stored glz::expected is a success that serializes its
      // JSON shape with a 2xx status; it is NOT translated into an HTTP error status.
      auto res = run_rest(reg, glz::http_method::GET, "/cached", "");
      expect(res.status_code == 200) << res.status_code;
      expect(res.response_body == R"({"unexpected":"stale"})") << res.response_body;
   };
};

int main() { return 0; }
