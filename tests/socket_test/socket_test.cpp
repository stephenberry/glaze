// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <future>
#include <iostream>
#include <latch>
#include <thread>

#include "glaze/network/server.hpp"
#include "glaze/network/socket_io.hpp"
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

glz::server server{service_0_port};
std::future<void> server_thread{};

void handle_client(auto sched, glz::socket client, std::atomic<bool>& active)
{
   std::cout << "New client connected!\n";
   
   /*auto send_sender = glz::async_send_value(sched, client,  "Welcome!");
   stdexec::sync_wait(std::move(send_sender));

   // Receiving a value
   std::string received{};
   auto receive_sender = glz::async_receive_value(sched, client, received);
   stdexec::sync_wait(std::move(receive_sender));*/

   if (auto ec = glz::send_value(client, "Welcome!")) {
      std::cerr << ec.message() << '\n';
      return;
   }

   while (active) {
      std::string received{};
      if (auto ec = glz::receive_value(client, received)) {
         std::cerr << ec.message() << '\n';
         return;
      }

      std::cout << std::format("Server: {}\n", received);
   }
}

suite make_server = [] {
   std::abort(); // IMPORTANT: comment this out when testing, this was added to not make github actions spin forever
   std::cout << std::format("Server started on port: {}\n", server.port);
   
   server_thread = std::async([]{
      auto accept_sender = server.async_accept([](auto sched, glz::socket client_socket, std::atomic<bool>& active) {
         std::cout << "New client connected!" << std::endl;
         handle_client(sched, std::move(client_socket), active);
      });

      // Start the server
      std::cout << "Server starting on port 8080..." << std::endl;
      auto [result] = stdexec::sync_wait(std::move(accept_sender)).value();

      if (result) {
         std::error_code ec = result;
         if (ec) {
            std::cerr << "Server error: " << ec.message() << std::endl;
         }
      }
      else {
         std::cerr << "Server stopped unexpectedly" << std::endl;
      }
   });

   std::this_thread::sleep_for(std::chrono::seconds(1));
};

suite socket_test = [] {
   std::vector<glz::socket> sockets(n_clients);
   std::vector<std::future<void>> threads(n_clients);
   for (size_t id{}; id < n_clients; ++id) {
      threads.emplace_back(std::async([id, &sockets] {
         glz::socket& socket = sockets[id];

         if (socket.connect(service_0_ip, service_0_port)) {
            std::cerr << std::format("Failed to connect to server.\nDetails: {}\n", glz::get_socket_error().message());
         }
         else {
            std::string received{};
            if (auto ec = glz::receive_value(socket, received)) {
               std::cerr << ec.message() << '\n';
               return;
            }
            std::cout << std::format("Received from server: {}\n", received);

            size_t tick{};
            while (tick < 3) {
               if (auto ec = glz::send_value(socket, std::format("Client {}, {}", id, tick))) {
                  std::cerr << ec.message() << '\n';
                  return;
               }
               std::this_thread::sleep_for(std::chrono::seconds(2));
               ++tick;
            }
            // working_clients.count_down();
            --working_clients;
            
            while (working_clients) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (auto ec = glz::send_value(socket, "close")) {
               std::cerr << ec.message() << '\n';
               return;
            }
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
