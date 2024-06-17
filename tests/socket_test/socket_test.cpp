// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/socket/socket.hpp"

#include <future>
#include <latch>
#include <thread>

#include "ut/ut.hpp"

using namespace ut;

std::future<void> server_thread{};

suite make_server = [] {
   server_thread = std::async([] {
      glz::server server{8080};

      const auto ec = server.async_accept([&](glz::socket&& client) {
         std::cout << "New client connected!\n";

         std::string message = "Welcome!";
         client.write_value(message);
         
         // TODO: Change this to a std::condition_variable
         while (glz::active) {
            std::string received{};
            client.read_value(received);
            std::cout << std::format("Received from client: {}\n", received);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
      });

      if (ec) {
         std::cerr << ec.message() << '\n';
      }
      else {
         std::cout << std::format("Server started on port: {}\n", server.port);
      }

      // Keep thread alive
      while (glz::active) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
   });

   // Allow the socket to get started
   std::this_thread::sleep_for(std::chrono::milliseconds(100));
};

suite socket_test = [] {
   glz::socket socket{};

   if (socket.connect("127.0.0.1", 8080)) {
      std::cerr << "Failed to connect to server.\n";
   }
   else {
      std::cout << "Connected to server!\n";

      /*socket.read([](const std::string& received) {
         std::cout << "Received: " << received << std::endl;
      });*/
      
      std::string received{};
      socket.read_value(received);
      std::cout << "Received: " << received << std::endl;

      std::string message = "Hello World";
      socket.write_value(message);
   }

   std::cin.get();
   glz::active = false;
};

int main() {
   std::signal(SIGINT, [](int){
      glz::active = false;
      std::exit(0);
   });
   return 0;
}
