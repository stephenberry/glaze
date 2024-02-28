// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

#include <iostream>

void asio_client_test()
{
   try {
      glz::asio_client client{};

      std::vector<int> data{};
      for (int i = 1; i < 10; ++i) {
         data.emplace_back(i);
         int sum{};
         if (auto ec = client.call({"/sum"}, data, sum); ec) {
            std::cerr << glz::write_json(ec) << '\n';
         }
         else {
            std::cout << sum << '\n';
         }
      }
      
      auto sum = client.callable<int(const std::vector<int>&)>({"/sum"});
      
      std::cout << "callable result: " << sum(std::vector<int>{1, 2, 3}) << '\n';
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
