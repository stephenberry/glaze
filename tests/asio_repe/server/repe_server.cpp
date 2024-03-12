// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

struct api
{
   std::function<int(std::vector<int>& vec)> sum = [](std::vector<int>& vec) {
      return std::reduce(vec.begin(), vec.end());
   };
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) { return std::ranges::max(vec); };
};

#include <iostream>

void run_server()
{
   std::cout << "Server active...\n";

   try {
      glz::asio_server<glz::repe::registry<>> server{.port = 8080};
      api methods{};
      server.init_registry = [&](glz::repe::registry<>& registry) { registry.on(methods); };
      server.run();
   }
   catch (const std::exception& e) {
      std::cerr << "Exception: " << e.what();
   }

   std::cout << "Server closed...\n";
}

int main()
{
   run_server();

   return 0;
}
