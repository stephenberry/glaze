// Glaze Library
// For the license information refer to glaze.ixx

import std;

import glaze.core.context;
import glaze.core.meta_fwd;
import glaze.core.opts;
import glaze.rpc.registry;
import glaze.rpc.repe;
import glaze.rpc.repe.buffer;
import glaze.rpc.repe.header;
import glaze.rpc.repe.plugin_helper;

import ut;

using std::uint64_t;

using namespace ut;

namespace
{
   // Helper to create a valid REPE request buffer
   std::string make_request(std::string_view query, std::string_view body, uint64_t id = 1, bool notify = false,
                            glz::error_code ec = glz::error_code::none)
   {
      glz::repe::header hdr{};
      hdr.spec = glz::repe::repe_magic;
      hdr.version = 1;
      hdr.id = id;
      hdr.notify = notify ? 1 : 0;
      hdr.ec = ec;
      hdr.query_length = query.size();
      hdr.body_length = body.size();
      hdr.length = sizeof(glz::repe::header) + query.size() + body.size();
      hdr.body_format = glz::repe::body_format::JSON;

      std::string buffer;
      buffer.resize(hdr.length);
      std::memcpy(buffer.data(), &hdr, sizeof(glz::repe::header));
      std::memcpy(buffer.data() + sizeof(glz::repe::header), query.data(), query.size());
      std::memcpy(buffer.data() + sizeof(glz::repe::header) + query.size(), body.data(), body.size());

      return buffer;
   }
}

suite parse_request_tests = [] {
   "parse_valid_request"_test = [] {
      auto buffer = make_request("/test", R"({"value":42})");
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(bool(result)) << "Parsing should succeed";
      expect(result.ec == glz::error_code::none);
      expect(result.request.id() == 1);
      expect(result.request.query == "/test");
      expect(result.request.body == R"({"value":42})");
      expect(!result.request.is_notify());
   };

   "parse_notify_request"_test = [] {
      auto buffer = make_request("/notify", "{}", 42, true);
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(bool(result)) << "Parsing should succeed";
      expect(result.request.id() == 42);
      expect(result.request.is_notify());
   };

   "parse_empty_body"_test = [] {
      auto buffer = make_request("/path", "");
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(bool(result)) << "Parsing should succeed";
      expect(result.request.query == "/path");
      expect(result.request.body.empty());
   };

   "parse_empty_query"_test = [] {
      auto buffer = make_request("", "body");
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(bool(result)) << "Parsing should succeed";
      expect(result.request.query.empty());
      expect(result.request.body == "body");
   };

   "parse_too_small_buffer"_test = [] {
      std::string buffer(10, '\0'); // Too small for header
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(!result) << "Parsing should fail";
      expect(result.ec == glz::error_code::invalid_header);
   };

   "parse_invalid_magic"_test = [] {
      auto buffer = make_request("/test", "{}");
      // Corrupt the magic bytes
      buffer[8] = 0;
      buffer[9] = 0;
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(!result) << "Parsing should fail";
      expect(result.ec == glz::error_code::invalid_header);
   };

   "parse_invalid_version"_test = [] {
      auto buffer = make_request("/test", "{}");
      // Corrupt the version
      buffer[10] = 99;
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(!result) << "Parsing should fail";
      expect(result.ec == glz::error_code::version_mismatch);
   };

   "parse_truncated_body"_test = [] {
      auto buffer = make_request("/test", "long body here");
      buffer.resize(buffer.size() - 5); // Truncate body
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});

      expect(!result) << "Parsing should fail";
      expect(result.ec == glz::error_code::invalid_body);
   };
};

suite response_builder_tests = [] {
   "reset_and_set_body"_test = [] {
      std::string buffer;
      glz::repe::response_builder resp{buffer};
      resp.reset(123);

      // Use raw body to avoid reflection issues with local types
      resp.set_body_raw(R"({"value":42})", glz::repe::body_format::JSON);

      auto view = resp.view();
      expect(view.size() > sizeof(glz::repe::header)) << "Response should have body";
   };

   "set_error"_test = [] {
      std::string buffer;
      glz::repe::response_builder resp{buffer};
      resp.reset(456);
      resp.set_error(glz::error_code::invalid_query, "Bad params");

      auto view = resp.view();
      expect(!view.empty()) << "Response should not be empty";

      // Parse the response to verify
      auto result = glz::repe::parse_request({view.data(), view.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.error() == glz::error_code::invalid_query);
      expect(result.request.body == "Bad params");
   };

   "fail_helper"_test = [] {
      std::string buffer;
      glz::repe::response_builder resp{buffer};
      resp.reset(789);

      bool returned = resp.fail(glz::error_code::method_not_found, "Not found");
      expect(returned) << "fail() should return true";
      expect(!resp.empty()) << "Response should not be empty";
   };

   "set_body_raw"_test = [] {
      std::string buffer;
      glz::repe::response_builder resp{buffer};
      resp.reset(100);
      resp.set_body_raw(R"({"custom":true})", glz::repe::body_format::JSON);

      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.body == R"({"custom":true})");
   };

   "clear_and_reuse"_test = [] {
      std::string buffer;
      glz::repe::response_builder resp{buffer};

      resp.reset(1);
      resp.set_body_raw("first");
      expect(!resp.empty());

      resp.clear();
      expect(resp.empty());

      resp.reset(2);
      resp.set_body_raw("second");
      auto result = glz::repe::parse_request({buffer.data(), buffer.size()});
      expect(result.request.body == "second");
   };

   "reset_from_request_view"_test = [] {
      auto request_buf = make_request("/test", "{}", 999);
      auto parsed = glz::repe::parse_request({request_buf.data(), request_buf.size()});
      expect(bool(parsed));

      std::string response_buf;
      glz::repe::response_builder resp{response_buf};
      resp.reset(parsed.request);
      resp.set_body_raw("response");

      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.id() == 999) << "ID should be copied from request";
   };
};

// Test object for registry auto-registration
struct test_api
{
   int value = 42;

   int get_value() const { return value; }
   void set_value(int v) { value = v; }
};

suite registry_span_call_tests = [] {
   "span_call_with_auto_registered_type"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      // Create request for /value (reading the value field)
      auto request = make_request("/value", "");
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(!response_buf.empty()) << "Should have response";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.error() == glz::error_code::none) << "No error expected";
   };

   "span_call_method_not_found"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      auto request = make_request("/nonexistent", "");
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(!response_buf.empty()) << "Should have error response";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result));
      expect(result.request.error() == glz::error_code::method_not_found);
   };

   "span_call_notification_no_response"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      auto request = make_request("/value", "", 1, true); // notify=true
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(response_buf.empty()) << "Notification should not produce response";
   };

   "span_call_unknown_notification_silent"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      auto request = make_request("/unknown", "{}", 1, true); // notify=true
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(response_buf.empty()) << "Unknown notification should be silently ignored";
   };

   "span_call_invalid_request"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      std::string bad_request(10, '\0'); // Too small (less than header size)
      std::string response_buf;
      registry.call(std::span<const char>{bad_request.data(), bad_request.size()}, response_buf);

      expect(!response_buf.empty()) << "Should have error response";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      // Invalid request with too-small buffer returns invalid_header
      expect(result.request.error() == glz::error_code::invalid_header);
   };

   "span_call_request_id_preserved"_test = [] {
      glz::registry<> registry;
      test_api api{};
      registry.on(api);

      auto request = make_request("/value", "", 12345);
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.id() == 12345) << "Response ID should match request ID";
   };
};

// Requests arriving off the wire end where the buffer ends: there is no '\0' after the body for a
// reader to stop on. make_request returns a std::string, whose data()[size()] is a terminator, so
// these tests copy the message into an exactly sized vector to reproduce the wire shape.
namespace
{
   std::vector<char> exact_buffer(std::string_view message) { return {message.begin(), message.end()}; }
}

struct vec2
{
   int x{};
   int y{};
};

struct unterminated_api
{
   int value{};
   vec2 position{};
   std::variant<double, std::string> choice{};
};

// A type deduced by shape rather than by a tag: resolving it walks to the end of the buffer.
struct amount
{
   double value{};
};

template <>
struct glz::meta<amount>
{
   static constexpr auto value = &amount::value;
};

struct inferred_api
{
   std::variant<std::string, amount> measure{};
};

// A std::function member registers as a call with typed parameters, which is the one endpoint kind
// that reads a body without first checking whether there is one.
struct param_fn_api
{
   std::function<int(int)> doubled = [](int v) { return v * 2; };
};

suite unterminated_buffer_tests = [] {
   "scalar_body_ending_at_buffer_end"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/value", "7"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(api.value == 7) << "Scalar body should be applied";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.error() == glz::error_code::none) << "No error expected";
   };

   "object_body_ending_at_buffer_end"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/position", R"({"x":3,"y":4})"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(api.position.x == 3) << "Object body should be applied";
      expect(api.position.y == 4) << "Object body should be applied";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.error() == glz::error_code::none) << "No error expected";
   };

   "truncated_body_is_an_error"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/position", R"({"x":3,"y":4)"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.error() != glz::error_code::none) << "Truncated body should not report success";
      // Every registry read runs without a terminator, so a body that ends with containers still
      // open reports end_reached internally. That code is documented as a non-error, so putting it
      // in a response header would tell the client the request parsed and merely stopped early.
      expect(result.request.error() == glz::error_code::unexpected_end) << "Expected unexpected_end";
   };

   "whitespace_only_body_is_an_error"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      api.value = 99;
      registry.on(api);

      const auto request = exact_buffer(make_request("/value", "   "));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(api.value == 99) << "A body holding no value must not be applied";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.error() == glz::error_code::no_read_input) << "Expected no_read_input";
   };

   // A variant alternative that resolves at the end of the buffer rewinds its iterator, so the
   // read completes having reported zero bytes consumed. read_params must report that as success.
   "variant_body_ending_at_buffer_end_gets_a_response"_test = [] {
      glz::registry<> registry;
      inferred_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/measure", "42.5"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(!response_buf.empty()) << "A non-notify request must always get a response";
      expect(api.measure.index() == 1) << "Deduced alternative should be applied";
      expect(std::get<amount>(api.measure).value == 42.5);
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.error() == glz::error_code::none) << "No error expected";
   };

   // A parameterized function endpoint has nothing to call its function with when the body is
   // empty, so it reads without the has_body() guard the value endpoints use. read_params returned
   // false for an empty body without writing anything, and the endpoint returns immediately on
   // false, so the request went unanswered and its client waited on a reply that was never coming.
   "empty_body_to_a_param_function_is_answered"_test = [] {
      glz::registry<> registry;
      param_fn_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/doubled", ""));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(!response_buf.empty()) << "A non-notify request must always get a response";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.error() == glz::error_code::no_read_input) << "Expected no_read_input";
   };

   "a param function still answers a good body"_test = [] {
      glz::registry<> registry;
      param_fn_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/doubled", "21"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result));
      expect(result.request.error() == glz::error_code::none);
      expect(result.request.body == "42") << "the function should have run";
   };

   // The other half of the same rule: a notification is a request the sender has said it will not
   // read a reply to, so a read that fails on one is reported by not answering it.
   "a failed read of a notification is not answered"_test = [] {
      for (std::string_view body : {"", "{", "not json"}) {
         glz::registry<> registry;
         param_fn_api api{};
         registry.on(api);

         const auto request = exact_buffer(make_request("/doubled", body, 1, true));
         std::string response_buf;
         registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

         expect(response_buf.empty()) << "a notification must not be answered: " << body;
      }
   };

   // The registry echoes a request that already carries an error back to its sender. A notification
   // has no sender waiting on that echo, so it is dropped like every other unanswerable request.
   "a notification carrying an error is not echoed"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/value", "", 1, true, glz::error_code::parse_error));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(response_buf.empty()) << "a notification must not be answered";
   };

   "a request carrying an error is still echoed"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/value", "", 1, false, glz::error_code::parse_error));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(bool(result)) << "Response should be parseable";
      expect(result.request.error() == glz::error_code::parse_error) << "the error should be echoed back";
   };

   // A registry that accepts comments must reject a body that holds nothing but one, for the same
   // reason it rejects a body that holds nothing but whitespace.
   "comment_only_body_is_an_error"_test = [] {
      glz::registry<glz::opts{.comments = true}> registry;
      unterminated_api api{};
      api.value = 99;
      registry.on(api);

      const auto request = exact_buffer(make_request("/value", "// nothing here\n"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(api.value == 99) << "A body holding no value must not be applied";
      auto result = glz::repe::parse_request({response_buf.data(), response_buf.size()});
      expect(result.request.error() == glz::error_code::no_read_input) << "Expected no_read_input";
   };

   // plugin_call hands the registry a raw pointer and length across an FFI boundary, so its bytes
   // carry no terminator either.
   "plugin_call_body_ending_at_buffer_end"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/position", R"({"x":5,"y":6})"));
      const auto response = glz::repe::plugin_call(registry, request.data(), request.size());

      expect(api.position.x == 5) << "Object body should be applied";
      expect(api.position.y == 6) << "Object body should be applied";
      expect(response.size > 0) << "A non-notify request must always get a response";
      auto result = glz::repe::parse_request({response.data, response.size});
      expect(result.request.error() == glz::error_code::none) << "No error expected";
   };

   "tagless_variant_body_ending_at_buffer_end"_test = [] {
      glz::registry<> registry;
      unterminated_api api{};
      registry.on(api);

      const auto request = exact_buffer(make_request("/choice", "42.5"));
      std::string response_buf;
      registry.call(std::span<const char>{request.data(), request.size()}, response_buf);

      expect(!response_buf.empty()) << "A non-notify request must always get a response";
      expect(api.choice.index() == 0);
      expect(std::get<double>(api.choice) == 42.5);
   };
};

int main() { return 0; }
