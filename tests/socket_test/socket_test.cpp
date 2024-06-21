// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/socket/socket.hpp"

#include <future>
#include <latch>
#include <thread>

#include "ut/ut.hpp"

using namespace ut;

constexpr bool user_input = false;

std::future<void> server_thread{};

constexpr auto n_clients = 10;
constexpr auto service_0_port{8080};
constexpr auto service_0_ip{"127.0.0.1"};

// std::latch is broken on MSVC:
//std::latch working_clients{n_clients};
static std::atomic_int working_clients{n_clients};

glz::windows_socket_startup_t<> wsa; // wsa_startup (ignored on macOS and Linux)

glz::server server{service_0_port};

suite make_server = [] {
   server_thread = std::async([&] {
      std::cout << std::format("Server started on port: {}\n", server.port);
      
      const auto ec = server.accept([](glz::socket&& client, auto& active) {
         std::cout << "New client connected!\n";

         if (auto ec = client.write_value("Welcome!")) {
            std::cerr << ec.message() << '\n';
            return;
         }
         
         while (active) {
            std::string received{};
            if (auto ec = client.read_value(received)) {
               std::cerr << ec.message() << '\n';
               return;
            }
            
            if (received == "disconnect") {
               std::cout << std::format("Client Disconnecting\n");
               break;
            }
            else if (received.size()) {
               std::cout << std::format("Server: {}\n", received);
            }
         }
      });

      if (ec) {
         std::cerr << ec.message() << '\n';
      }
   });
   
   std::this_thread::sleep_for(std::chrono::milliseconds(100));
};

suite socket_test = [] {
   
   std::vector<glz::socket> sockets(n_clients);
   std::vector<std::future<void>> threads(n_clients);
   for (size_t id{}; id < n_clients; ++id)
   {
      threads.emplace_back(std::async([id, &sockets]{
         glz::socket& socket = sockets[id];

         if (socket.connect(service_0_ip, service_0_port)) {
            std::cerr << std::format("Failed to connect to server.\nDetails: {}\n", glz::get_socket_error().message());
         }
         else {
            std::string received{};
            if (auto ec = socket.read_value(received)) {
               std::cerr << ec.message() << '\n';
               return;
            }
            std::cout << std::format("Received from server: {}\n", received);

            size_t tick{};
            while (tick < 3) {
               if (auto ec = socket.write_value(std::format("Client {}, {}", id, tick))) {
                  std::cerr << ec.message() << '\n';
                  return;
               }
               std::this_thread::sleep_for(std::chrono::seconds(2));
               ++tick;
            }
            //working_clients.count_down();
            --working_clients;
           
         }
      }));
   }

   //working_clients.arrive_and_wait();
   while (working_clients) std::this_thread::sleep_for(std::chrono::milliseconds(1));

   if constexpr (user_input) {
      std::cout << "\nFinished! Press any key to exit.";
      std::cin.get();
   }
};

int main() {
   std::signal(SIGINT, [](int){
      server.active = false;
      std::exit(0);
   });
   
   server.active = false;
   
   return 0;
}
