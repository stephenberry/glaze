// Glaze Library
// For the license information refer to glaze.hpp

#include "ut/ut.hpp"

using namespace ut;

#include <iostream>

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
   glz::asio_server<> server{.port = port, .concurrency = 4};

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

   try {
      glz::asio_client<> client{"localhost", std::to_string(port)};

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

      server.stop();
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

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

   glz::asio_server<> server{.port = port, .concurrency = 4};

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

   try {
      glz::asio_client<> client{"localhost", std::to_string(port)};

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

      server.stop();
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server_thread.get();
}

struct api
{
   std::function<int(std::vector<int>& vec)> sum = [](std::vector<int>& vec) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return std::reduce(vec.begin(), vec.end());
   };
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) { return std::ranges::max(vec); };
};

void asio_client_test()
{
   static constexpr int16_t port = 8431;

   glz::asio_server<> server{.port = port, .concurrency = 4};

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

   std::this_thread::sleep_for(std::chrono::seconds(1));

   try {
      constexpr auto N = 100;
      std::vector<glz::asio_client<>> clients;
      clients.reserve(N);

      std::vector<std::future<void>> threads;
      threads.reserve(N);

      for (size_t i = 0; i < N; ++i) {
         clients.emplace_back(glz::asio_client<>{"localhost", std::to_string(port)});
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

      server.stop();
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

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

   glz::asio_server<> server{.port = port, .concurrency = 2};

   std::future<void> server_thread = std::async([&] {
      api2 methods{};
      server.on(methods);
      server.run();
   });

   std::this_thread::sleep_for(std::chrono::milliseconds(100));

   try {
      glz::asio_client<> client{"localhost", std::to_string(port)};
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

      server.stop();
   }
   catch (const std::exception& e) {
      expect(false) << e.what();
   }

   server_thread.get();
}

struct raw_json_api
{
   std::function<void()> do_nothing = []() {};
};

void raw_json_tests()
{
   static constexpr int16_t port = 8765;

   glz::asio_server<> server{.port = port, .concurrency = 2};

   std::future<void> server_thread = std::async([&] {
      api2 methods{};
      raw_json_api api{};
      server.on(api);
      server.run();
   });

   std::this_thread::sleep_for(std::chrono::milliseconds(100));

   glz::raw_json results{};

   glz::asio_client<> client{"localhost", std::to_string(port)};
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

   glz::asio_server<> server{.port = port, .concurrency = 1};

   async_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client<> client{"localhost", std::to_string(port)};
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

   glz::asio_server<> server{.port = port, .concurrency = 1};
   server.error_handler = [](const std::string& error) {
      expect(error == "func error");
   };

   error_api api{};
   server.on(api);

   server.run_async();

   glz::asio_client<> client{"localhost", std::to_string(port)};
   (void)client.init();

   int result{};
   glz::repe::message msg{};
   client.call({"/func"}, msg, 100);
   expect(bool(glz::repe::decode_message(result, msg)));
}

int main()
{
   notify_test();
   async_clients_test();
   asio_client_test();
   async_calls();
   raw_json_tests();
   async_server_test();
   server_error_test();

   return 0;
}
