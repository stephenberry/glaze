// Glaze Library
// For the license information refer to glaze.hpp

#include <iostream>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

void asio_client_test()
{
   try {
      constexpr auto N = 100;
      std::vector<glz::asio_client<>> clients;
      clients.reserve(N);

      std::vector<std::future<void>> threads;
      threads.reserve(N);

      for (size_t i = 0; i < N; ++i) {
         clients.emplace_back(glz::asio_client<>{"localhost", "8080"});
      }

      for (size_t i = 0; i < N; ++i) {
         threads.emplace_back(std::async([&, i] {
            auto& client = clients[i];
            const auto ec = client.init();
            if (!ec) {
               std::cout << "Connected to server" << std::endl;
            }
            else {
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
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
   }
}

struct first_type
{
   std::function<int(int)> sum = [](int n) {
      for (auto i = 0; i < n; ++i) {
         std::cout << "n: " << n << '\n';
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      return n;
   };
};

struct second_type
{
   std::function<int(int)> sum = [](int n) {
      for (auto i = 0; i < n; ++i) {
         std::cout << "n: " << n << '\n';
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      return n;
   };
};

struct api
{
   first_type first{};
   second_type second{};
};

void async_calls()
{
   std::future<void> server_thread = std::async([] {
      glz::asio_server<> server{.port = 8080, .concurrency = 2};
      api methods{};
      server.on(methods);
      server.run();
   });

   std::this_thread::sleep_for(std::chrono::seconds(1));

   try {
      glz::asio_client<> client{"localhost", "8080"};
      (void)client.init();

      std::vector<std::future<void>> threads;

      threads.emplace_back(std::async([&] {
         int ret{};
         (void)client.call({"/first/sum"}, 99, ret);
      }));

      std::this_thread::sleep_for(std::chrono::seconds(1));

      threads.emplace_back(std::async([&] {
         int ret{};
         (void)client.call({"/second/sum"}, 10, ret);
      }));

      for (auto& t : threads) {
         t.get();
      }
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
   }

   server_thread.get();
}

int main()
{
   // asio_client_test();
   async_calls();

   std::this_thread::sleep_for(std::chrono::seconds(5));
   return 0;
}
