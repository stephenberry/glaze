// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "ut/ut.hpp"

using namespace ut;

#include <iostream>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/thread/async_string.hpp"

// This test code is self-contained and spawns both the server and the client

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
      std::cout << "Server active...\n";

      try {
         my_data data{};
         server.on(data);
         server.run();
      }
      catch (const std::exception& e) {
         std::cerr << "Exception: " << e.what();
      }

      std::cout << "Server closed...\n";
   });
   
   try {
      glz::asio_client<> client{"localhost", std::to_string(port)};

      const auto ec = client.init();
      if (ec) {
         throw std::runtime_error(ec.message());
      }
      
      if (auto e_call = client.set({"/age"}, 29)) {
         std::cerr << glz::write_json(e_call).value_or("error") << '\n';
      }
      
      int age{};
      if (auto e_call = client.get({"/age"}, age)) {
         std::cerr << glz::write_json(e_call).value_or("error") << '\n';
      }
      
      expect(age == 29);

      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
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
            const auto ec = client.init();
            if (ec) {
               std::cerr << "Error: " << ec.message() << std::endl;
            }

            std::vector<int> data;
            for (int j = 1; j < 100; ++j) {
               data.emplace_back(j);
            }

            int sum{};
            if (auto e_call = client.call({"/sum"}, data, sum); e_call) {
               std::cerr << glz::write_json(e_call).value_or("error") << '\n';
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
      std::cerr << e.what() << '\n';
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
         (void)client.call({"/first/sum"}, 25, ret);
      }));

      threads.emplace_back(std::async([&] {
         int ret{};
         (void)client.call({"/second/sum"}, 5, ret);
      }));

      for (auto& t : threads) {
         t.get();
      }

      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
   }

   server_thread.get();
}

int main()
{
   async_clients_test();
   asio_client_test();
   async_calls();

   return 0;
}
