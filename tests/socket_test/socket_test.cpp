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

      const auto ec = server.async_accept([](glz::socket&& client) {
         std::cout << "New client connected!\n";

         client.read([](const std::string& data, int bytes_read) {
            std::string received(data.begin(), data.begin() + bytes_read);
            std::cout << "Received from client: " << received << std::endl;
         });

         std::string message = "Welcome!";
         client.write(message, [](const std::string& data, int /*bytes_sent*/) {
            std::cout << std::format("Sent to client: {}\n", data);
         });
      });

      if (ec) {
         std::cerr << ec.message() << '\n';
      }
      else {
         std::cout << std::format("Server started on port: {}\n", server.port);
      }

      // Keep thread alive
      std::this_thread::sleep_for(std::chrono::hours(1));
   });

   // Allow the socket to get started
   std::this_thread::sleep_for(std::chrono::milliseconds(10));
};

suite socket_test = [] {
   glz::socket socket{};

   if (socket.connect("127.0.0.1", 8080)) {
      std::cerr << "Failed to connect to server.\n";
   }
   else {
      std::cout << "Connected to server!\n";

      socket.read([](const std::string& data, int bytes_read) {
         std::string received(data.begin(), data.begin() + bytes_read);
         std::cout << "Received: " << received << std::endl;
      });

      std::string message = "Hello World";
      socket.write(message, [](const std::string& data, int bytes_sent) {
         std::cout << "Sent from client: " << std::string(data.begin(), data.begin() + bytes_sent) << std::endl;
      });
   }

   std::this_thread::sleep_for(std::chrono::seconds(10));
};

int main() { return 0; }
