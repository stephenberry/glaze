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

inline constexpr auto n_clients = 10;
inline constexpr auto service_0_port{8080};
inline constexpr auto service_0_ip{"127.0.0.1"};

static std::latch working_clients{n_clients};

suite make_server = [] {
   server_thread = std::async([] {
      glz::server server{service_0_port};

      std::cout << std::format("Server started on port: {}\n", server.port);
      
      const auto ec = server.accept([](glz::socket&& client) {
         std::cout << "New client connected!\n";

         client.write_value("Welcome!");
         
         // TODO: Change this to a std::condition_variable???
         while (glz::active) {

            std::string received{};

            client.read_value(received);
            
            if (received == "disconnect") {
               std::cout << std::format("Client Disconnecting\n");
               break;
            }
            
            std::cout << std::format("Server: {}\n", received);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

         if (socket.connect(service_0_ip, 90)) {
            std::cerr << std::format("Failed to connect to server.\nDetails: {}\n", glz::get_socket_error().message());
         }
         else {
            std::string received{};
            socket.read_value(received);
            std::cout << std::format("Received from server: {}\n", received);

            size_t tick{};
            while (glz::active && tick < 3) {
               socket.write_value(std::format("Client {}, {}", id, tick));
               std::this_thread::sleep_for(std::chrono::seconds(2));
               ++tick;
            }
            working_clients.count_down();
         }
      }));
   }

   working_clients.arrive_and_wait();

   std::cout << "\nFinished! Press any key to exit.";
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
