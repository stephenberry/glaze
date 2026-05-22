// Minimal reproducer for MinGW + SSL heap corruption (issue #2411)
//
// Reproduces heap corruption on MinGW/GCC 15.2.0 + Windows when
// GLZ_ENABLE_SSL is defined. The crash occurs during thread.join()
// in http_server::stop() after handling a connection.

#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include <openssl/crypto.h>

#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

#ifdef DELETE
#undef DELETE
#endif

using namespace ut;

#ifdef _WIN32
#include <windows.h>
inline void check_heap(const char* label)
{
   BOOL ok = HeapValidate(GetProcessHeap(), 0, NULL);
   std::fprintf(stderr, "[HEAP] %s: %s\n", label, ok ? "OK" : "CORRUPTED");
}
#else
inline void check_heap(const char*) {}
#endif

suite mingw_ssl_repro = [] {
   // Minimal reproducer: create server, handle one request, stop.
   // This mirrors what http_client_test's basic_get_request does.
   "minimal_server_get_stop"_test = [] {
      for (int round = 0; round < 20; ++round) {
         std::fprintf(stderr, "[REPRO] round %d\n", round);

         // Create and start server
         glz::http_server<> server;
         server.get("/ping", [](const glz::request&, glz::response& res) {
            res.status(200).body("pong");
         });

         uint16_t port = 0;
         for (uint16_t p = 19300; p < 19400; ++p) {
            try {
               server.bind("127.0.0.1", p);
               port = p;
               break;
            }
            catch (...) {
               continue;
            }
         }
         expect(port > 0) << "Failed to bind";

         std::thread server_thread([&]() { server.start(1); });

         // Wait for server
         for (int i = 0; i < 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            try {
               asio::io_context io;
               asio::ip::tcp::socket sock(io);
               sock.connect({asio::ip::make_address("127.0.0.1"), port});
               sock.close();
               break;
            }
            catch (...) {
            }
         }

         // Make one GET request using raw TCP (same as simple_test_client)
         {
            asio::io_context io;
            asio::ip::tcp::socket sock(io);
            asio::ip::tcp::resolver resolver(io);
            auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
            asio::connect(sock, endpoints);

            std::string req = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
            asio::write(sock, asio::buffer(req));

            asio::streambuf response_buf;
            asio::error_code ec;
            asio::read(sock, response_buf, asio::transfer_all(), ec);

            std::string response{std::istreambuf_iterator<char>(&response_buf), std::istreambuf_iterator<char>()};
            expect(response.find("pong") != std::string::npos) << "Response should contain 'pong'";
         }

         // Small delay then stop — this is where the crash happens
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
         check_heap("before stop");
         server.stop();
         check_heap("after stop");

         if (server_thread.joinable()) {
            server_thread.join();
         }
         check_heap("after server_thread join");
      }
      std::fprintf(stderr, "[REPRO] all 20 rounds passed\n");
   };
};

int main()
{
   std::cout << "MinGW + SSL Minimal Reproducer" << std::endl;
#ifdef __VERSION__
   std::cout << "Compiler: " << __VERSION__ << std::endl;
#elif defined(_MSC_VER)
   std::cout << "Compiler: MSVC " << _MSC_VER << std::endl;
#endif
   std::cout << "OpenSSL: " << OpenSSL_version(OPENSSL_VERSION) << std::endl;
   std::cout << std::endl;
   return 0;
}
