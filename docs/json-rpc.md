## JSON-RPC 2.0

Compile time specification of JSON-RPC methods making it unnecessary to convert JSON to its according params/result/error type.

- [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification)

### Example

```C++
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

int main() {
    rpc::server<rpc::method<"foo", foo_params, foo_result>,
                rpc::method<"bar", bar_params, bar_result>>
       server;
    rpc::client<rpc::method<"foo", foo_params, foo_result>,
                rpc::method<"bar", bar_params, bar_result>>
       client;
   
    // One long living callback per method for the server
    server.on<"foo">([](foo_params const& params) {
        // access to member variables for the request `foo`
        // params.foo_a 
        // params.foo_b
        return foo_result{.foo_c = true, .foo_d = "new world"};
        // Or return an error:
        // return rpc::error{rpc::error_e::server_error_lower, "my error"};
    });
    server.on<"bar">([](bar_params const& params) {
        return bar_result{.bar_c = true, .bar_d = "new world"};
    });
    
    std::string uuid{"42"};
    // One callback per client request
    auto [request_str, inserted] = client.request<"foo">(
            uuid, 
            foo_params{.foo_a = 1337, .foo_b = "hello world"}, 
            [](glz::expected<foo_result, rpc::error> value, rpc::id_t id) -> void {
        // Access to value and/or id
    });
    // request_str: R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})"
    // send request_str over your communication protocol to the server
    
    // you can assign timeout for the request in your event loop
    auto timeout = [uuid, &client]() {
        decltype(auto) map = client.get_request_map<"foo">();
        if (map.contains(id));
            map.erase(id);
    };
    timeout();
    
    // Call the server callback for method `foo`
    // Returns response json string since the request_str can withold batch of requests.
    // If the request is a notification (no `id` in request) a response will not be generated.
    // For convenience, you can serialize the response yourself and get the responses as following:
    // auto response_vector = server.call<decltype(server)::raw_call_return_t>("...");
    // std::string response = glz::write_json(response_vector);
    std::string response = server.call(request_str);
   
    assert(response ==
         R"({"jsonrpc":"2.0","result":{"foo_c":true,"foo_d":"new world"},"id":"42"})");
   
    // Call the client callback for method `foo` with the provided results
    // This will automatically remove the previously assigned callback
    client.call(response);
    // This would return an internal error since the `id` is still not in the request map
    auto err = client.call(response);
}
```

> Thanks to **[jbbjarnason](https://github.com/jbbjarnason)**
