// Minimal reproducer: MinGW + OpenSSL heap corruption
//
// On MSYS2 MINGW64 with GCC 15 + OpenSSL 3.x, linking OpenSSL causes
// intermittent heap corruption (0xc0000374) during std::thread::join()
// after an ASIO worker thread handles a TCP connection.
//
// The crash occurs even though no SSL code is executed — merely linking
// the OpenSSL libraries is sufficient.
//
// Build:
//   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
//   cmake --build .
//
// Run:
//   ./repro    (repeat if it passes — ~50% reproduction rate)

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

// A minimal TCP server using ASIO
class mini_server
{
  public:
   void start(uint16_t port)
   {
      io_ctx_ = std::make_unique<asio::io_context>();
      acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
         *io_ctx_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

      do_accept();

      worker_ = std::thread([this] { io_ctx_->run(); });
   }

   void stop()
   {
      if (acceptor_) {
         asio::error_code ec;
         acceptor_->close(ec);
      }
      if (io_ctx_) io_ctx_->stop();
      if (worker_.joinable()) worker_.join(); // <-- crash here
   }

   ~mini_server() { stop(); }

  private:
   void do_accept()
   {
      acceptor_->async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
         if (!ec) {
            // Handle the connection: read request, send response, close
            auto buf = std::make_shared<asio::streambuf>();
            asio::async_read_until(socket, *buf, "\r\n\r\n",
                                   [this, s = std::make_shared<asio::ip::tcp::socket>(std::move(socket)),
                                    buf](asio::error_code ec, std::size_t) {
                                      if (!ec) {
                                         auto response = std::make_shared<std::string>(
                                            "HTTP/1.1 200 OK\r\n"
                                            "Content-Length: 4\r\n"
                                            "Connection: close\r\n"
                                            "\r\n"
                                            "pong");
                                         asio::async_write(*s, asio::buffer(*response),
                                                           [s, response](asio::error_code, std::size_t) {
                                                              asio::error_code ec;
                                                              s->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                                                           });
                                      }
                                   });
         }
         // Accept next connection
         if (acceptor_->is_open()) {
            do_accept();
         }
      });
   }

   std::unique_ptr<asio::io_context> io_ctx_;
   std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
   std::thread worker_;
};

// Send a simple HTTP GET and read the response
bool send_get(uint16_t port)
{
   try {
      asio::io_context io;
      asio::ip::tcp::socket sock(io);
      sock.connect({asio::ip::make_address("127.0.0.1"), port});

      std::string req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
      asio::write(sock, asio::buffer(req));

      asio::streambuf buf;
      asio::error_code ec;
      asio::read(sock, buf, asio::transfer_all(), ec);

      std::string response{std::istreambuf_iterator<char>(&buf), std::istreambuf_iterator<char>()};
      return response.find("pong") != std::string::npos;
   }
   catch (...) {
      return false;
   }
}

int main()
{
   std::fprintf(stderr, "MinGW + OpenSSL heap corruption reproducer\n");
   std::fprintf(stderr, "Repeat 20 rounds of: start server, GET, stop server\n\n");

   constexpr uint16_t port = 19876;

   for (int i = 0; i < 20; ++i) {
      std::fprintf(stderr, "round %d... ", i);

      mini_server server;
      server.start(port);

      // Wait for server to be ready
      for (int attempt = 0; attempt < 50; ++attempt) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

      if (!send_get(port)) {
         std::fprintf(stderr, "FAILED (GET failed)\n");
         return 1;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      server.stop();

      std::fprintf(stderr, "OK\n");
   }

   std::fprintf(stderr, "\nAll 20 rounds passed.\n");
   return 0;
}
