// Glaze Library
// For the license information refer to glaze.hpp

#include <iostream>

#include "glaze/network/repe_client.hpp"

void asio_client_test()
{
   try {
      constexpr auto N = 100;
      std::vector<glz::repe_client<>> clients;
      clients.reserve(N);

      std::vector<std::future<void>> threads;
      threads.reserve(N);

      for (size_t i = 0; i < N; ++i) {
         clients.emplace_back(glz::repe_client<>{"127.0.0.1", 8080});
      }
      
      std::mutex mtx{};
      std::vector<int> results{};

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
            if (auto e_call = client.call({"/sum"}, data, sum)) {
               std::cerr << glz::write_json(e_call).value_or("error") << '\n';
            }
            else {
               std::unique_lock lock{mtx};
               std::cout << "i: " << i << ", " << sum << '\n';
               results.emplace_back(sum);
            }
         }));
      }

      for (auto& t : threads) {
         t.get();
      }
      
      for (auto v : results) {
         if (v != 4950) {
            std::abort();
         }
      }
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
   }
}

int main()
{
   asio_client_test();

   std::this_thread::sleep_for(std::chrono::seconds(5));
   return 0;
}
