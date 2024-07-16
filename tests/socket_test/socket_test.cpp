// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/network/server.hpp"
#include "glaze/network/socket_io.hpp"

#include <iostream>
#include <future>
#include <latch>
#include <thread>

#include "ut/ut.hpp"

using namespace ut;

constexpr bool user_input = false;

constexpr auto n_clients = 10;
constexpr auto service_0_port{8080};
constexpr auto service_0_ip{"127.0.0.1"};

// std::latch is broken on MSVC:
// std::latch working_clients{n_clients};
static std::atomic_int working_clients{n_clients};

glz::windows_socket_startup_t<> wsa; // wsa_startup (ignored on macOS and Linux)

glz::server server{.port = service_0_port};

suite make_server = [] {
   std::cout << std::format("Server started on port: {}\n", server.port);

   const auto future = server.async_accept([](glz::socket&& client, auto& active) {
      std::cout << "New client connected!\n";

      if (auto ec = glz::send(client, "Welcome!", std::string{})) {
         std::cerr << ec.message() << '\n';
         return;
      }

      while (active) {
         std::string received{};
         if (auto ec = glz::receive(client, received, std::string{}, 5000)) {
            std::cerr << ec.message() << '\n';
            return;
         }
         std::cout << std::format("Server: {}\n", received);
         std::ignore = glz::send(client, std::format("Hello to {} from server.\n", received), std::string{});
      }
   });

   if (future.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
      std::cerr << future.get().message() << '\n';
   }
};

suite socket_test = [] {
   std::vector<glz::socket> sockets(n_clients);
   std::vector<std::future<void>> threads(n_clients);
   for (size_t id{}; id < n_clients; ++id) {
      threads.emplace_back(std::async([id, &sockets] {
         glz::socket& socket = sockets[id];

         if (connect(socket, service_0_ip, service_0_port)) {
            std::cerr << std::format("Failed to connect to server.\nDetails: {}\n", glz::get_socket_error().message());
         }
         else {
            std::string received{};
            if (auto ec = glz::receive(socket, received, std::string{}, 100)) {
               std::cerr << ec.message() << '\n';
               return;
            }
            std::cout << std::format("Received from server: {}\n", received);

            size_t tick{};
            std::string result;
            while (tick < 3) {
               if (auto ec = glz::send(socket, std::format("Client {}, {}", id, tick), std::string{})) {

                  std::cerr << ec.message() << '\n';
                  return;
               }
               if (auto ec = glz::receive(socket, result, std::string{}, 100)) {
                  continue;
               }
               else {
                  expect(result.size() > 0);
                  std::cout << result;
               }


               std::this_thread::sleep_for(std::chrono::seconds(2));
               ++tick;
            }
            // working_clients.count_down();
            --working_clients;
         }
      }));
   }

   // working_clients.arrive_and_wait();
   while (working_clients) std::this_thread::sleep_for(std::chrono::milliseconds(1));

   if constexpr (user_input) {
      std::cout << "\nFinished! Press any key to exit.";
      std::cin.get();
   }

   server.active = false;
};

int main()
{
   std::signal(SIGINT, [](int) {
      server.active = false;
      std::exit(0);
   });

   // GCC needs this sleep
   std::this_thread::sleep_for(std::chrono::milliseconds(100));

   return 0;
}
