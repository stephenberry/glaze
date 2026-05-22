// Test: link OpenSSL DLLs but do NOT define GLZ_ENABLE_SSL.
// If this crashes, the mere loading of OpenSSL DLLs causes the issue.
// If this passes, the GLZ_ENABLE_SSL macro (changing template instantiations) is the cause.
//
// This file must NOT include any SSL headers or define GLZ_ENABLE_SSL.

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

#ifdef DELETE
#undef DELETE
#endif

using namespace ut;

#ifdef _WIN32
#include <windows.h>
inline void check_heap_link(const char* label)
{
   BOOL ok = HeapValidate(GetProcessHeap(), 0, NULL);
   std::fprintf(stderr, "[HEAP] %s: %s\n", label, ok ? "OK" : "CORRUPTED");
}
#else
inline void check_heap_link(const char*) {}
#endif

suite link_only_test = [] {
   "server_get_stop_no_ssl_macro"_test = [] {
      for (int round = 0; round < 20; ++round) {
         std::fprintf(stderr, "[LINK_ONLY] round %d\n", round);

         glz::http_server<> server;
         server.get("/ping", [](const glz::request&, glz::response& res) {
            res.status(200).body("pong");
         });

         uint16_t port = 0;
         for (uint16_t p = 19400; p < 19500; ++p) {
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

         // Make one GET request
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

         std::this_thread::sleep_for(std::chrono::milliseconds(50));
         check_heap_link("before stop");
         server.stop();
         check_heap_link("after stop");

         if (server_thread.joinable()) {
            server_thread.join();
         }
      }
      std::fprintf(stderr, "[LINK_ONLY] all 20 rounds passed\n");
   };
};

int main()
{
   std::cout << "Link-Only Test (OpenSSL linked, GLZ_ENABLE_SSL NOT defined)" << std::endl;
   return 0;
}
