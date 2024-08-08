#include <glaze/ext/jsonrpc.hpp>
#include <iostream>

struct foo_params
{
   int foo_a{};
   std::string foo_b{};
};

struct foo_result
{
   bool foo_c{};
   std::string foo_d{};
   auto operator<=>(const foo_result&) const = default;
};

struct bar_params
{
   int bar_a;
   std::string bar_b;
};

struct bar_result
{
   bool bar_c{};
   std::string bar_d{};
   auto operator<=>(const bar_result&) const = default;
};

namespace rpc = glz::rpc;

int main()
{
   rpc::server<rpc::method<"foo", foo_params, foo_result>, rpc::method<"bar", bar_params, bar_result>> server;
   rpc::client<rpc::method<"foo", foo_params, foo_result>, rpc::method<"bar", bar_params, bar_result>> client;

   // One long living callback per method for the server
   server.on<"foo">([](const foo_params& params) -> glz::expected<foo_result, glz::rpc::error> {
      // access to member variables for the request `foo`
      // params.foo_a
      // params.foo_b

      if (params.foo_a == 1337) {
         std::cout << "Server received valid data:" << params.foo_b << std::endl;
         return foo_result{.foo_c = true, .foo_d = "new world"};
      }
      else {
         std::cout << "Server received invalid data:" << params.foo_b << std::endl;
         // Or return an error:
         return glz::unexpected{glz::rpc::error{glz::rpc::error_e::invalid_params, {}, "foo_a should be equal to 0"}};
      }
   });
   server.on<"bar">([](const bar_params& params) { return bar_result{.bar_c = true, .bar_d = "new world"}; });

   std::string uuid{"42"};
   auto client_cb = [](glz::expected<foo_result, rpc::error> value, rpc::id_t id) -> void {
      if (value)
         std::cout << "Client received " << value.value().foo_c << ":" << value.value().foo_d << std::endl;
      else
         std::cerr << "Client received error " << value.error().message << std::endl;
   };

   // One callback per client request
   auto [request_str, inserted] =
      client.request<"foo">(uuid, foo_params{.foo_a = 1337, .foo_b = "hello world"}, client_cb);
   // request_str: R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})"
   // send request_str over your communication protocol to the server

   // you can assign timeout for the request in your event loop
   auto timeout = [uuid, &client]() {
      decltype(auto) map = client.get_request_map<"foo">();
      if (map.contains(uuid)) map.erase(uuid);
   };
#if 0
   // caling timeout would cause to not process response
   timeout();
#endif
   // Call the server callback for method `foo`
   // Returns response json string since the request_str can withold batch of requests.
   // If the request is a notification (no `id` in request) a response will not be generated.
   // For convenience, you can serialize the response yourself and get the responses as following:
   // auto response_vector = server.call<decltype(server)::raw_call_return_t>("...");
   // std::string response = glz::write_json(response_vector);
   std::string response = server.call(request_str);
   std::cout << "Server json response :" << response << std::endl;
   assert(response == R"({"jsonrpc":"2.0","result":{"foo_c":true,"foo_d":"new world"},"id":"42"})");

   // Call the client callback for method `foo` with the provided results
   // This will automatically remove the previously assigned callback
   auto err = client.call(response);
   std::cout << "Client call result :" << err.message << std::endl;
   // This would return an internal error since the `id` is still not in the request map
   err = client.call(response);
   std::cout << "Client retry call result :" << err.message << std::endl;

   std::tie(request_str, inserted) =
      client.request<"foo">(uuid, foo_params{.foo_a = -1, .foo_b = "invalid data"}, client_cb);

   response = server.call(request_str);
   std::cout << "Server json response :" << response << std::endl;
   assert(response == R"({"jsonrpc":"2.0","error":{"code":-32602,"message":"foo_a should be equal to 0"},"id":"42"})");
   err = client.call(response);
   std::cout << "Client call with error result :" << err.message << std::endl;
}
