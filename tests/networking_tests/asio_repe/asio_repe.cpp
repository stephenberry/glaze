// Glaze Library
// For the license information refer to glaze.hpp

#include "ut/ut.hpp"

using namespace ut;

#include <iostream>
#include <latch>
#include <thread>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/thread/async_string.hpp"

// This test code is self-contained and spawns both the server and the client

struct notify_api
{
   std::function<void()> hello = [] { std::cout << "HELLO\n"; };
};

void notify_test()
{
   static constexpr int16_t port = 8431;
   std::latch ready{1};
   glz::asio_server server{.port = port, .concurrency = 4};
   server.reuse_address = true;
   server.on_listen = [&] { ready.count_down(); };

   std::future<void> server_thread = std::async([&] {
      try {
         notify_api api{};
         server.on(api);
         server.run();
      }
      catch (const std::exception& e) {
         std::cerr << "Exception: " << e.what();
      }
   });

   ready.wait();

   try {
      glz::asio_client client{"localhost", std::to_string(port)};

      if (auto ec = client.init(); bool(ec)) {
         throw std::runtime_error(glz::write_json(ec).value_or("error"));
      }

      glz::repe::message msg{};
      client.call({.query = "/hello", .notify = true}, msg);
      if (bool(msg.error())) {
         throw std::runtime_error(glz::repe::decode_error(msg));
      }

      client.call({"/hello"}, msg);
      if (bool(msg.error())) {
         throw std::runtime_error(glz::repe::decode_error(msg));
      }
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server.stop();
   server_thread.get();
}

struct my_data
{
   glz::async_string name{};
   std::atomic<int> age{};
};

void async_clients_test()
{
   static constexpr int16_t port = 8431;
   std::latch ready{1};

   glz::asio_server server{.port = port, .concurrency = 4};
   server.reuse_address = true;
   server.on_listen = [&] { ready.count_down(); };

   std::future<void> server_thread = std::async([&] {
      try {
         my_data data{};
         server.on(data);
         server.run();
      }
      catch (const std::exception& e) {
         std::cerr << "Exception: " << e.what();
      }
   });

   ready.wait();

   try {
      glz::asio_client client{"localhost", std::to_string(port)};

      if (auto ec = client.init(); bool(ec)) {
         throw std::runtime_error(glz::write_json(ec).value_or("error"));
      }

      glz::repe::message msg{};
      client.call({"/age"}, msg, 29);
      if (bool(msg.error())) {
         std::cerr << glz::repe::decode_error(msg) << '\n';
      }

      int age{};
      client.call({"/age"}, msg);
      auto err = glz::repe::decode_message(age, msg);
      if (err) {
         std::cerr << err.value() << '\n';
      }

      expect(age == 29);
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server.stop();
   server_thread.get();
}

struct api
{
   std::function<int(std::vector<int>& vec)> sum = [](std::vector<int>& vec) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return std::reduce(vec.begin(), vec.end());
   };
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) {
      return (std::ranges::max)(vec);
   };
};

void asio_client_test()
{
   static constexpr int16_t port = 8431;
   std::latch ready{1};

   glz::asio_server server{.port = port, .concurrency = 4};
   server.reuse_address = true;
   server.on_listen = [&] { ready.count_down(); };

   std::future<void> server_thread = std::async([&] {
      std::cout << "Server active...\n";

      try {
         api methods{};
         server.on(methods);
         server.run();
      }
      catch (const std::exception& e) {
         std::cerr << "Exception: " << e.what();
      }

      std::cout << "Server closed...\n";
   });

   ready.wait();

   try {
      constexpr auto N = 100;
      std::vector<glz::asio_client<>> clients;
      clients.reserve(N);

      std::vector<std::future<void>> threads;
      threads.reserve(N);

      for (size_t i = 0; i < N; ++i) {
         clients.emplace_back(glz::asio_client{"localhost", std::to_string(port)});
      }

      for (size_t i = 0; i < N; ++i) {
         threads.emplace_back(std::async([&, i] {
            auto& client = clients[i];
            if (auto ec = client.init(); bool(ec)) {
               std::cerr << "Error: " << glz::write_json(ec).value_or("error") << std::endl;
            }

            std::vector<int> data;
            for (int j = 1; j < 100; ++j) {
               data.emplace_back(j);
            }

            int sum{};
            glz::repe::message msg{};
            client.call({"/sum"}, msg, data);
            auto err = glz::repe::decode_message(sum, msg);
            if (err) {
               std::cerr << err.value() << '\n';
            }
            else {
               std::cout << "i: " << i << ", " << sum << '\n';
            }
         }));
      }

      for (auto& t : threads) {
         t.get();
      }
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server.stop();
   server_thread.get();
}

struct first_type
{
   std::function<int(int)> sum = [](int n) {
      for (auto i = 0; i < n; ++i) {
         std::cout << "n: " << n << '\n';
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return n;
   };
};

struct second_type
{
   std::function<int(int)> sum = [](int n) {
      for (auto i = 0; i < n; ++i) {
         std::cout << "n: " << n << '\n';
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return n;
   };
};

struct api2
{
   first_type first{};
   second_type second{};
};

void async_calls()
{
   static constexpr int16_t port = 8765;
   std::latch ready{1};

   glz::asio_server server{.port = port, .concurrency = 2};
   server.reuse_address = true;
   server.on_listen = [&] { ready.count_down(); };

   std::future<void> server_thread = std::async([&] {
      api2 methods{};
      server.on(methods);
      server.run();
   });

   ready.wait();

   try {
      glz::asio_client client{"localhost", std::to_string(port)};
      (void)client.init();

      std::vector<std::future<void>> threads;

      threads.emplace_back(std::async([&] {
         int ret{};
         glz::repe::message msg{};
         client.call({"/first/sum"}, msg, 25);
         glz::repe::decode_message(ret, msg);
      }));

      threads.emplace_back(std::async([&] {
         int ret{};
         glz::repe::message msg{};
         client.call({"/second/sum"}, msg, 5);
         glz::repe::decode_message(ret, msg);
      }));

      for (auto& t : threads) {
         t.get();
      }
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server.stop();
   server_thread.get();
}

struct raw_json_api
{
   std::function<void()> do_nothing = []() {};
};

void raw_json_tests()
{
   static constexpr int16_t port = 8765;
   std::latch ready{1};

   glz::asio_server server{.port = port, .concurrency = 2};
   server.reuse_address = true;
   server.on_listen = [&] { ready.count_down(); };

   std::future<void> server_thread = std::async([&] {
      api2 methods{};
      raw_json_api api{};
      server.on(api);
      server.run();
   });

   ready.wait();

   glz::raw_json results{};

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   glz::repe::message msg{};
   client.call({"/do_nothing"}, msg);
   if (bool(msg.error())) {
      std::cerr << glz::repe::decode_error(msg) << '\n';
   }

   server.stop();
}

struct async_api
{
   std::function<int(int)> times_two = [](int x) { return 2 * x; };
};

void async_server_test()
{
   static constexpr int16_t port = 8765;

   glz::asio_server server{.port = port, .concurrency = 1};

   async_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   int result{};
   glz::repe::message msg{};
   client.call({"/times_two"}, msg, 100);
   expect(not glz::repe::decode_message(result, msg));

   expect(result == 200);
}

struct error_api
{
   std::function<int()> func = []() {
      throw std::runtime_error("func error");
      return 0;
   };
};

void server_error_test()
{
   static constexpr int16_t port = 8765;

   glz::asio_server server{.port = port, .concurrency = 1};
   server.error_handler = [](const std::string& error) { expect(error == "func error"); };

   error_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   int result{};
   glz::repe::message msg{};
   client.call({"/func"}, msg, 100);
   expect(bool(glz::repe::decode_message(result, msg)));
}

struct some_object_t
{
   std::string name{};
   int age{};
   float speed{};
   std::function<int(int)> square = [](int x) { return x * x; };
};

suite send_receive_api_tests = [] {
   "send"_test = [] {
      static constexpr int16_t port = 8765;

      glz::asio_server server{.port = port, .concurrency = 1};

      some_object_t obj{};
      server.on(obj);

      server.run_async();

      glz::asio_client client{"localhost", std::to_string(port)};
      (void)client.init();

      client.set("/age", 33);
      expect(bool(obj.age == 33));

      {
         int age{};
         client.get("/age", age);
         expect(age == 33);
      }

      client.set("/name", "Ryan");
      expect(bool(obj.name == "Ryan"));

      {
         std::string name{};
         client.get("/name", name);
         expect(name == "Ryan");
      }

      client.set("/name", "Paul");
      expect(bool(obj.name == "Paul"));

      expect("Paul" == client.get<std::string>("/name"));

      {
         int x = 3;
         client.inout("/square", x, x);
         expect(x == 9);
      }
   };
};

struct keep_alive_api
{
   std::function<int()> broken = []() {
      throw std::runtime_error("broken");
      return 0;
   };
   std::function<int()> unknown_broken = []() {
      throw 5;
      return 0;
   };
   std::function<int()> works = []() { return 42; };
};

void server_keep_alive_test()
{
   static constexpr int16_t port = 8766;

   glz::asio_server server{.port = port, .concurrency = 1};
   server.error_handler = [](const std::string& error) {
      if (error != "broken" && error != "unknown error") {
         expect(false) << "Unexpected error: " << error;
      }
   };

   keep_alive_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   int result{};
   glz::repe::message msg{};

   // First call throws
   client.call({"/broken"}, msg);
   auto err = glz::repe::decode_message(result, msg);
   expect(bool(err));
   if (err) {
      expect(err.value() == "REPE error: parse_error | registry error for `/broken`: broken") << err.value();
   }

   // Second call throws unknown exception
   msg = {}; // reset message
   client.call({"/unknown_broken"}, msg);
   err = glz::repe::decode_message(result, msg);
   expect(bool(err));
   if (err) {
      expect(err.value() == "REPE error: parse_error | Unknown error") << err.value();
   }

   // Third call should succeed if server is still alive
   msg = {}; // reset message
   client.call({"/works"}, msg);
   expect(not glz::repe::decode_message(result, msg));
   expect(result == 42);
}

void client_exception_test()
{
   static constexpr int16_t port = 8767;

   glz::asio_server server{.port = port, .concurrency = 1};

   keep_alive_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   try {
      int i{};
      client.get("/broken", i);
      expect(false) << "Should have thrown";
   }
   catch (const std::exception& e) {
      expect(std::string_view(e.what()) == "parse_error: registry error for `/broken`: broken") << e.what();
   }

   try {
      int i{};
      client.get("/unknown_broken", i);
      expect(false) << "Should have thrown";
   }
   catch (const std::exception& e) {
      expect(std::string_view(e.what()) == "parse_error: Unknown error") << e.what();
   }
}

struct custom_call_api
{
   int value = 100;
};

void custom_call_handler_test()
{
   static constexpr int16_t port = 8768;

   glz::asio_server server{.port = port, .concurrency = 1};

   custom_call_api api{};
   server.on(api);

   // Set custom call handler that intercepts all calls using zero-copy API
   std::atomic<int> call_count{0};
   server.call = [&](std::span<const char> request, std::string& response_buffer) {
      ++call_count;

      // Zero-copy parse
      auto result = glz::repe::parse_request(request);
      if (!result) {
         glz::repe::encode_error_buffer(glz::error_code::parse_error, response_buffer, "Failed to parse request");
         return;
      }

      const auto& req = result.request;
      glz::repe::response_builder resp{response_buffer};

      // Custom routing: if path starts with /custom, handle directly
      if (req.query.starts_with("/custom")) {
         resp.reset(req);
         resp.set_body_raw(R"({"custom":true})", glz::repe::body_format::JSON);
      }
      else {
         // Forward to registry for other paths (zero-copy)
         server.registry.call(request, response_buffer);
      }
   };

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   glz::repe::message msg{};

   // Test custom path handling
   client.call({"/custom/endpoint"}, msg);
   expect(not bool(msg.error()));
   expect(msg.body == R"({"custom":true})") << msg.body;

   // Test forwarding to registry
   msg = {};
   client.call({"/value"}, msg);
   expect(not bool(msg.error()));
   expect(msg.body == "100") << msg.body;

   // Verify custom handler was called for both
   expect(call_count == 2);
}

void custom_call_middleware_test()
{
   static constexpr int16_t port = 8769;

   glz::asio_server server{.port = port, .concurrency = 1};

   custom_call_api api{};
   server.on(api);

   // Set middleware-style handler that logs and delegates using zero-copy API
   std::vector<std::string> logged_queries;
   std::mutex log_mutex;

   server.call = [&](std::span<const char> request, std::string& response_buffer) {
      // Zero-copy parse
      auto result = glz::repe::parse_request(request);
      if (!result) {
         glz::repe::encode_error_buffer(glz::error_code::parse_error, response_buffer, "Failed to parse request");
         return;
      }

      const auto& req = result.request;

      // Pre-processing: log the query
      {
         std::lock_guard lock{log_mutex};
         logged_queries.push_back(std::string{req.query});
      }

      // Delegate to registry (zero-copy)
      server.registry.call(request, response_buffer);

      // Post-processing could go here (response is in response_buffer)
   };

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   glz::repe::message msg{};
   client.call({"/value"}, msg);
   expect(not bool(msg.error()));

   msg = {};
   client.call({"/value"}, msg, 42);
   expect(not bool(msg.error()));

   // Verify logging happened
   expect(logged_queries.size() == 2);
   expect(logged_queries[0] == "/value");
   expect(logged_queries[1] == "/value");
}

void custom_call_error_handling_test()
{
   static constexpr int16_t port = 8770;

   glz::asio_server server{.port = port, .concurrency = 1};

   // Custom handler that returns an error for certain paths using zero-copy API
   server.call = [](std::span<const char> request, std::string& response_buffer) {
      // Zero-copy parse - query and body are views into the request buffer
      auto result = glz::repe::parse_request(request);
      if (!result) {
         glz::repe::encode_error_buffer(glz::error_code::parse_error, response_buffer, "Failed to parse request");
         return;
      }

      const auto& req = result.request;
      glz::repe::response_builder resp{response_buffer};

      if (req.query == "/forbidden") {
         resp.reset(req);
         resp.set_error(glz::error_code::invalid_query, "Access denied");
      }
      else {
         resp.reset(req);
         resp.set_body_raw(R"("ok")", glz::repe::body_format::JSON);
      }
   };

   server.run_async();

   glz::asio_client client{"localhost", std::to_string(port)};
   (void)client.init();

   glz::repe::message msg{};

   // Test allowed path
   client.call({"/allowed"}, msg);
   expect(not bool(msg.error()));
   expect(msg.body == R"("ok")");

   // Test forbidden path
   msg = {};
   client.call({"/forbidden"}, msg);
   expect(bool(msg.error()));
   expect(msg.header.ec == glz::error_code::invalid_query);
}

// Connection state tests
suite connection_state_tests = [] {
   "initial_connected_state"_test = [] {
      // Client should not be connected before init()
      glz::asio_client client{"localhost", "9999"};
      expect(not client.connected()) << "Client should not be connected before init()";
   };

   "connected_after_init"_test = [] {
      static constexpr int16_t port = 8771;

      glz::asio_server server{.port = port, .concurrency = 1};

      some_object_t obj{};
      server.on(obj);
      server.run_async();

      glz::asio_client client{"localhost", std::to_string(port)};
      expect(not client.connected()) << "Client should not be connected before init()";

      auto ec = client.init();
      expect(not bool(ec)) << "init() should succeed";
      expect(client.connected()) << "Client should be connected after successful init()";

      server.stop();
   };

   "connected_false_after_server_shutdown"_test = [] {
      static constexpr int16_t port = 8772;

      auto server = std::make_unique<glz::asio_server<>>();
      server->port = port;
      server->concurrency = 1;

      some_object_t obj{};
      server->on(obj);
      server->run_async();

      glz::asio_client client{"localhost", std::to_string(port)};
      (void)client.init();
      expect(client.connected()) << "Client should be connected initially";

      // Verify connection works
      int age{};
      client.get("/age", age);

      // Shutdown server - destructor joins threads so shutdown is complete after reset()
      server->stop();
      server.reset();

      // Try an operation - should fail and update connected state
      glz::repe::message msg{};
      client.call({"/age"}, msg);

      // After a failed call due to connection loss, connected() should be false
      expect(not client.connected()) << "Client should not be connected after server shutdown and failed call";
   };

   "connected_false_when_init_fails"_test = [] {
      // Try to connect to a port where no server is running
      glz::asio_client client{"localhost", "59999"};
      expect(not client.connected()) << "Client should not be connected before init()";

      auto ec = client.init();
      expect(bool(ec)) << "init() should fail when no server is running";
      expect(not client.connected()) << "Client should not be connected after failed init()";
   };

   "call_returns_error_when_not_connected"_test = [] {
      glz::asio_client client{"localhost", "59999"};

      // Don't call init(), try to call directly
      glz::repe::message msg{};
      client.call({"/test"}, msg);

      expect(bool(msg.error())) << "call() should return error when not connected";
   };

   "set_throws_when_not_connected"_test = [] {
      glz::asio_client client{"localhost", "59999"};

      bool threw = false;
      try {
         client.set("/test", 42);
      }
      catch (const std::runtime_error& e) {
         threw = true;
         expect(std::string(e.what()).find("NOT CONNECTED") != std::string::npos)
            << "Exception should mention NOT CONNECTED";
      }
      expect(threw) << "set() should throw when not connected";
   };

   "get_throws_when_not_connected"_test = [] {
      glz::asio_client client{"localhost", "59999"};

      bool threw = false;
      try {
         int value{};
         client.get("/test", value);
      }
      catch (const std::runtime_error& e) {
         threw = true;
         expect(std::string(e.what()).find("NOT CONNECTED") != std::string::npos)
            << "Exception should mention NOT CONNECTED";
      }
      expect(threw) << "get() should throw when not connected";
   };

   "inout_throws_when_not_connected"_test = [] {
      glz::asio_client client{"localhost", "59999"};

      bool threw = false;
      try {
         int x = 5;
         client.inout("/test", x, x);
      }
      catch (const std::runtime_error& e) {
         threw = true;
         expect(std::string(e.what()).find("NOT CONNECTED") != std::string::npos)
            << "Exception should mention NOT CONNECTED";
      }
      expect(threw) << "inout() should throw when not connected";
   };

   "reconnect_after_server_restart"_test = [] {
      static constexpr int16_t port = 8773;

      // Start server
      auto server = std::make_unique<glz::asio_server<>>();
      server->port = port;
      server->concurrency = 1;
      server->reuse_address = true;

      some_object_t obj{.age = 25};
      server->on(obj);
      server->run_async();

      glz::asio_client client{"localhost", std::to_string(port)};
      (void)client.init();
      expect(client.connected());

      // Verify connection works
      int age{};
      client.get("/age", age);
      expect(age == 25);

      // Shutdown server
      server->stop();
      server.reset();

      // Try operation - should fail
      glz::repe::message msg{};
      client.call({"/age"}, msg);
      expect(not client.connected()) << "Should be disconnected after server shutdown";

      // Restart server on same port
      server = std::make_unique<glz::asio_server<>>();
      server->port = port;
      server->concurrency = 1;
      server->reuse_address = true;

      some_object_t obj2{.age = 99};
      server->on(obj2);
      server->run_async();

      // Re-init client
      auto ec = client.init();
      expect(not bool(ec)) << "Re-init should succeed";
      expect(client.connected()) << "Should be connected after re-init";

      // Verify new connection works
      client.get("/age", age);
      expect(age == 99) << "Should get value from new server";

      server->stop();
   };
};

int main()
{
   notify_test();
   async_clients_test();
   asio_client_test();
   async_calls();
   raw_json_tests();
   async_server_test();
   server_error_test();
   server_keep_alive_test();
   client_exception_test();
   custom_call_handler_test();
   custom_call_middleware_test();
   custom_call_error_handling_test();

   return 0;
}
