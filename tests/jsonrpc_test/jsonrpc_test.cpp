
#include "glaze/ext/jsonrpc.hpp"

#include <boost/ut.hpp>
#include <string>

namespace rpc = glz::rpc;
namespace ut = boost::ut;
using ut::operator""_test;
using ut::operator/;

ut::suite valid_vector_test_cases_server = [] {
   using vec_t = std::vector<int>;

   rpc::server<rpc::method<"add", vec_t, int>> server;

   server.on<"add">([](vec_t const& vec) -> glz::expected<int, rpc::error> {
      int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
      return sum;
   });

   const std::array valid_requests = {
      std::pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": 1})",
                R"({"jsonrpc": "2.0","result": 6,"id": 1})"),
      std::pair(
         // No id is valid
         R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3]})", ""),
      std::pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": null})", ""),
      std::pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": 2.0})",
                R"({"jsonrpc": "2.0","result": 6, "id": 2})"),
      std::pair(R"({"jsonrpc": "2.0","method": "add","params": [1, 2, 3],"id": "some_client_22"})",
                R"({"jsonrpc": "2.0","result": 6, "id": "some_client_22"})")};
   std::string raw_json;
   std::string resulting_request;

   for (auto const& pair : valid_requests) {
      std::tie(raw_json, resulting_request) = pair;
      auto stripped{std::make_shared<std::string>(resulting_request)};
      stripped->erase(std::remove_if(stripped->begin(), stripped->end(), ::isspace), stripped->end());

      ut::test("response:" + *stripped) = [&server, &raw_json, stripped]() {
         std::string response = server.call(raw_json);
         if (stripped->empty()) { // if no id is supplied expected response should be none
            ut::expect(response.empty());
            return;
         }
         ut::expect(response == *stripped);
      };
   }
};

ut::suite vector_test_cases = [] {
   using vec_t = std::vector<int>;

   rpc::server<rpc::method<"summer", vec_t, int>> server;
   rpc::client<rpc::method<"summer", vec_t, int>> client;

   server.on<"summer">([](vec_t const& vec) -> glz::expected<int, rpc::error> {
      int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
      return sum;
   });

   ut::test("sum_result = 6") = [&server, &client] {
      bool called{};
      auto request_str{client.request<"summer">(1, std::vector{1, 2, 3},
                                                [&called](glz::expected<int, rpc::error> value, rpc::id_t id) -> void {
                                                   called = true;
                                                   ut::expect(value.has_value());
                                                   ut::expect(value.value() == 6);
                                                   ut::expect(std::holds_alternative<std::int64_t>(id));
                                                   ut::expect(std::get<std::int64_t>(id) == std::int64_t{1});
                                                })};
      ut::expect(request_str.first == R"({"jsonrpc":"2.0","method":"summer","params":[1,2,3],"id":1})");

      [[maybe_unused]] auto& requests = client.get_request_map<"summer">();
      ut::expect(requests.size() == 1);
      ut::expect(requests.contains(1)); // the id is 1

      server.on<"summer">([](vec_t const& vec) -> glz::expected<int, rpc::error> {
         ut::expect(vec == std::vector{1, 2, 3});
         int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
         return sum;
      });
      std::string response = server.call(request_str.first);
      ut::expect(response == R"({"jsonrpc":"2.0","result":6,"id":1})");

      client.call(response);
      ut::expect(called);
   };
};

struct foo_params
{
   int foo_a{};
   std::string foo_b{};
};
template <>
struct glz::meta<foo_params>
{
   using T = foo_params;
   static constexpr auto value{glz::object("foo_a", &T::foo_a, "foo_b", &T::foo_b)};
};
struct foo_result
{
   bool foo_c{};
   std::string foo_d{};
   bool operator==(const foo_result& rhs) const noexcept { return foo_c == rhs.foo_c && foo_d == rhs.foo_d; }
};
template <>
struct glz::meta<foo_result>
{
   using T = foo_result;
   static constexpr auto value{glz::object("foo_c", &T::foo_c, "foo_d", &T::foo_d)};
};

struct bar_params
{
   int bar_a;
   std::string bar_b;
};
template <>
struct glz::meta<bar_params>
{
   using T = bar_params;
   static constexpr auto value{glz::object("bar_a", &T::bar_a, "bar_b", &T::bar_b)};
};
struct bar_result
{
   bool bar_c{};
   std::string bar_d{};
   bool operator==(const bar_result& rhs) const noexcept { return bar_c == rhs.bar_c && bar_d == rhs.bar_d; }
};
template <>
struct glz::meta<bar_result>
{
   using T = bar_result;
   static constexpr auto value{glz::object("bar_c", &T::bar_c, "bar_d", &T::bar_d)};
};

ut::suite struct_test_cases = [] {
   rpc::server<rpc::method<"foo", foo_params, foo_result>, rpc::method<"bar", bar_params, bar_result>> server;
   rpc::client<rpc::method<"foo", foo_params, foo_result>, rpc::method<"bar", bar_params, bar_result>> client;

   ut::test("valid foo request") = [&server, &client] {
      bool called{};
      auto request_str{
         client.request<"foo">("42", foo_params{.foo_a = 1337, .foo_b = "hello world"},
                               [&called](glz::expected<foo_result, rpc::error> value, rpc::id_t id) -> void {
                                  called = true;
                                  ut::expect(value.has_value());
                                  ut::expect(value.value() == foo_result{.foo_c = true, .foo_d = "new world"});
                                  ut::expect(std::holds_alternative<std::string_view>(id));
                                  ut::expect(std::get<std::string_view>(id) == "42");
                               })};
      ut::expect(request_str.first ==
                 R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})");

      server.on<"foo">([](foo_params const& params) -> glz::expected<foo_result, rpc::error> {
         ut::expect(params.foo_a == 1337);
         ut::expect(params.foo_b == "hello world");
         return foo_result{.foo_c = true, .foo_d = "new world"};
      });

      std::string response = server.call(request_str.first);
      ut::expect(response == R"({"jsonrpc":"2.0","result":{"foo_c":true,"foo_d":"new world"},"id":"42"})");

      client.call(response);
      ut::expect(called);

      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });
   };

   ut::test("valid bar request") = [&server, &client] {
      bool called{};
      auto request_str{client.request<"bar">(
         "bar-uuid", bar_params{.bar_a = 1337, .bar_b = "hello world"},
         [&called](glz::expected<bar_result, rpc::error> const& value, rpc::id_t const& id) -> void {
            called = true;
            ut::expect(value.has_value());
            ut::expect(value.value() == bar_result{.bar_c = true, .bar_d = "new world"});
            ut::expect(std::holds_alternative<std::string_view>(id));
            ut::expect(std::get<std::string_view>(id) == "bar-uuid");
         })};
      ut::expect(request_str.first ==
                 R"({"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id":"bar-uuid"})");

      server.on<"bar">([](bar_params const& params) -> glz::expected<bar_result, rpc::error> {
         ut::expect(params.bar_a == 1337);
         ut::expect(params.bar_b == "hello world");
         return bar_result{.bar_c = true, .bar_d = "new world"};
      });

      std::string response = server.call(request_str.first);
      ut::expect(response == R"({"jsonrpc":"2.0","result":{"bar_c":true,"bar_d":"new world"},"id":"bar-uuid"})");

      client.call(response);
      ut::expect(called);

      server.on<"bar">([](bar_params const&) -> glz::expected<bar_result, rpc::error> { return {}; });
   };

   ut::test("foo request error") = [&server, &client] {
      bool called{};
      auto request_str{
         client.request<"foo">("42", foo_params{.foo_a = 1337, .foo_b = "hello world"},
                               [&called](glz::expected<foo_result, rpc::error> value, rpc::id_t id) -> void {
                                  called = true;
                                  ut::expect(!value.has_value());
                                  ut::expect(value.error() == rpc::error{rpc::error_e::server_error_lower, "my error"});
                                  ut::expect(std::holds_alternative<std::string_view>(id));
                                  ut::expect(std::get<std::string_view>(id) == "42");
                               })};

      ut::expect(request_str.first ==
                 R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})");

      server.on<"foo">([](foo_params const& params) -> glz::expected<foo_result, rpc::error> {
         ut::expect(params.foo_a == 1337);
         ut::expect(params.foo_b == "hello world");
         return glz::unexpected(rpc::error{rpc::error_e::server_error_lower, "my error"});
      });

      std::string response = server.call(request_str.first);
      ut::expect(response ==
                 R"({"jsonrpc":"2.0","error":{"code":-32000,"message":"Server error","data":"my error"},"id":"42"})");

      client.call(response);
      ut::expect(called);

      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });
   };

   ut::test("server invalid version error") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      // invalid jsonrpc version
      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(
         R"({"jsonrpc":"42.0","method":"foo","params":{},"id":"uuid"})");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         glz::write_json(response_vec) ==
         R"([{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"Invalid version: 42.0 only supported version is 2.0"},"id":"uuid"}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::invalid_request);
   };

   ut::test("server method not found") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      // invalid method name
      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(
         R"({"jsonrpc":"2.0","method":"invalid_method_name","params":{},"id":"uuid"})");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         glz::write_json(response_vec) ==
         R"([{"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method: 'invalid_method_name' not found"},"id":"uuid"}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::method_not_found);
   };

   ut::test("server invalid json") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      // key "id" illformed missing `"`
      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(
         R"({"jsonrpc":"2.0","method":"invalid_method_name","params":{},"id:"uuid"}")");
      ut::expect(response_vec.size() == 1);
      [[maybe_unused]] auto dbg{glz::write_json(response_vec)};
      auto s = glz::write_json(response_vec);
      ut::expect(
         s ==
         R"([{"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error","data":"1:66: syntax_error\n   {\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":{},\"id:\"uuid\"}\"\n                                                                    ^\n"},"id":null}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::parse_error);
   };

   ut::test("server invalid json batch") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      // batch cut at params key
      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(
         R"([{"jsonrpc":"2.0","method":"invalid_method_name","params":{},"id":"uuid"},{"jsonrpc":"2.0","method":"invalid_method_name","params":]")");
      ut::expect(response_vec.size() == 1);
      auto s = glz::write_json(response_vec);
      ut::expect(
         s ==
         R"([{"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error","data":"1:132: syntax_error\n   [{\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":{},\"id\":\"uuid\"},{\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":]\"\n                                                                                                                                      ^\n"},"id":null}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::parse_error);
   };

   ut::test("server invalid json batch empty array") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"([])");

      ut::expect(response_vec.size() == 1);
      auto s = glz::write_json(response_vec);
      ut::expect(s == R"([{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request"},"id":null}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::invalid_request);
   };

   ut::test("server invalid json illformed batch one item") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"([1])");

      ut::expect(response_vec.size() == 1);
      ut::expect(
         glz::write_json(response_vec) ==
         R"([{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   1\n   ^\n"},"id":null}])");
      ut::expect(response_vec.at(0).error.has_value());
      ut::expect(response_vec.at(0).error->code == rpc::error_e::invalid_request);
   };

   ut::test("server invalid json illformed batch three items") = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"([1,2,3])");

      ut::expect(response_vec.size() == 3);
      ut::expect(
         glz::write_json(response_vec) ==
         R"([{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   1\n   ^\n"},"id":null},{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   2\n   ^\n"},"id":null},{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   3\n   ^\n"},"id":null}])");
      for (auto& response : response_vec) {
         ut::expect(response.error.has_value());
         ut::expect(response.error->code == glz::rpc::error_e::invalid_request);
      }
   };

   "server batch with both invalid and valid"_test = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });
      server.on<"bar">([](bar_params const&) -> glz::expected<bar_result, rpc::error> { return {}; });

      std::string response = server.call(R"(
      [
          {"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"},
          {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id":"bar-uuid"},
          {"jsonrpc": "2.0", "method": "invalid_method_name", "params": [42,23], "id": "2"},
          {"foo": "boo"},
          {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"}},
          {"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"4222222"},
          {"jsonrpc":"2.0","invalid_method_key":"foo","params":{},"id":"4222222"}
      ]
      )");
      // Note one of the requests is a valid notification(no id) a response won't be generated for it
      [[maybe_unused]] auto const unused_prettier{
         R"([
{"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"42"},
{"jsonrpc":"2.0","result":{"bar_c":false,"bar_d":""},"id":"bar-uuid"},
{"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method: 'invalid_method_name' not found"},"id":"2"},
{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:9: unknown_key\n   {\"foo\": \"boo\"}\n           ^\n"},"id":null},
{"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"4222222"},
{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:21: unknown_key\n   {\"jsonrpc\":\"2.0\",\"invalid_method_key\":\"foo\",\"params\":{},\"id\":\"4222222\"}\n                       ^\n"},"id":"4222222"}
])"};
      ut::expect(
         response ==
         R"([{"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"42"},{"jsonrpc":"2.0","result":{"bar_c":false,"bar_d":""},"id":"bar-uuid"},{"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method: 'invalid_method_name' not found"},"id":"2"},{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:9: unknown_key\n   {\"foo\": \"boo\"}\n           ^\n"},"id":null},{"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"4222222"},{"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:21: unknown_key\n   {\"jsonrpc\":\"2.0\",\"invalid_method_key\":\"foo\",\"params\":{},\"id\":\"4222222\"}\n                       ^\n"},"id":"4222222"}])");
   };

   "server weird id values"_test = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"(
      [
          {"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id": ["value1", "value2", "value3"]},
          {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id": { "name": "jhon"}}
      ]
      )");
      ut::expect(response_vec.size() == 2);
      for (auto& response : response_vec) {
         ut::expect(response.error.has_value());
         ut::expect(response.error->code == glz::rpc::error_e::invalid_request);
      }
   };
   "server invalid jsonrpc value"_test = [&server] {
      server.on<"foo">([](foo_params const&) -> glz::expected<foo_result, rpc::error> { return {}; });

      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"(
      [
          {"jsonrpc":"1.9","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id": null},
          {"jsonrpc":"1.9","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id": 10}
      ]
      )");
      ut::expect(response_vec.size() == 2);
      for (auto& response : response_vec) {
         ut::expect(response.error.has_value());
         ut::expect(response.error->code == glz::rpc::error_e::invalid_request);
      }
   };
   "client request map"_test = [&client] {
      bool first_call{};
      std::ignore = client.request<"foo">("first_call", foo_params{}, [&first_call](auto, auto) { first_call = true; });
      bool second_call{};
      std::ignore =
         client.request<"foo">("second_call", foo_params{}, [&second_call](auto, auto) { second_call = true; });
      bool third_call{};
      std::ignore = client.request<"foo">("third_call", foo_params{}, [&third_call](auto, auto) { third_call = true; });
      decltype(auto) map = client.get_request_map<"foo">();
      map.at("first_call")({}, {});
      ut::expect(first_call);
      ut::expect(!second_call);
      ut::expect(!third_call);
      map.at("second_call")({}, {});
      map.at("third_call")({}, {});
      ut::expect(second_call);
      ut::expect(third_call);
      map.clear();
   };
   "client request timeout"_test = [&client] {
      std::string id{"some id"};
      std::ignore = client.request<"foo">(id, foo_params{}, [](auto, auto) {});

      auto timeout = [&id, &client]() {
         decltype(auto) map = client.get_request_map<"foo">();
         ut::expect(map.contains(id));
         map.erase(id);
      };
      timeout();
   };
   "client request id needs to be unique"_test = [&client] {
      std::string id{"some id"};
      bool first_called{};
      auto [unused, inserted] =
         client.request<"foo">(id, foo_params{}, [&first_called](auto, auto) { first_called = true; });
      ut::expect(inserted);
      auto [unused2, second_insert] = client.request<"foo">(id, foo_params{}, [](auto, auto) {});
      ut::expect(!second_insert);

      decltype(auto) map = client.get_request_map<"foo">();
      map.at(id)({}, {});
      ut::expect(first_called);

      map.clear();
   };
   "client notification"_test = [&client] {
      auto const notify_str{client.notify<"foo">(foo_params{})};
      ut::expect(notify_str == R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":0,"foo_b":""},"id":null})");
   };
   "client call erases id from queue"_test = [&client, &server] {
      std::uint8_t call_cnt{};
      auto [request, inserted] =
         client.request<"foo">("next gen id", foo_params{}, [&call_cnt](auto, auto) { call_cnt++; });
      auto response = server.call(request);
      client.call(response);
      client.call(response);
      ut::expect(call_cnt == 1);
   };

   // glaze currently does not support required members, so the user will get default constructed.
   ut::skip / "server individual request parameters error"_test = [&server] {
      auto response_vec = server.call<std::vector<rpc::response_t<glz::raw_json>>>(R"(
      [
          {"jsonrpc":"2.0","method":"bar","params":{"bar_b":"hello world"},"id": 25},
          {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337},"id": 10}
      ]
      )");
      ut::expect(response_vec.size() == 2);
      for (auto& response : response_vec) {
         ut::expect(response.error.has_value());
         if (response.error.has_value()) {
            ut::expect(response.error->code == glz::rpc::error_e::invalid_params);
         }
      }
   };
};

auto main(int, char**) -> int
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
