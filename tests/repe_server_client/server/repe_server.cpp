// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/network/repe_server.hpp"

struct api
{
   std::function<int(std::vector<int>& vec)> sum = [](std::vector<int>& vec) {
      return std::reduce(vec.begin(), vec.end());
   };
   std::function<double(std::vector<double>& vec)> maximum = [](std::vector<double>& vec) { return (std::ranges::max)(vec); };
};

#include <iostream>

int main()
{
   std::cout << "Server active...\n";

   try {
      glz::repe_server<> server{.port = 8080, .print_errors = true};
      api methods{};
      server.on(methods);
      server.run();
   }
   catch (const std::exception& e) {
      std::cerr << "Exception: " << e.what();
   }

   std::cout << "Server closed...\n";

   return 0;
}
