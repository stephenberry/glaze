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
         threads.emplace_back(std::async([&, i]{
            auto& client = clients[i];
            const auto ec = client.init();
            if (!ec) {
                std::cout << "Connected to server" << std::endl;
            } else {
                std::cerr << "Error: " << ec.message() << std::endl;
            }
            
            std::vector<int> data;
            for (int j = 1; j < 100; ++j) {
               data.emplace_back(j);
            }
            
            int sum{};
            if (auto ec = client.call({"/sum"}, data, sum); ec) {
               std::cerr << glz::write_json(ec) << '\n';
            }
            else {
               std::cout << "i: " << i << ", " <<  sum << '\n';
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

int main()
{
   asio_client_test();

   return 0;
}
