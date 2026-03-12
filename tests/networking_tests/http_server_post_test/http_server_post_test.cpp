#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <ut/ut.hpp>

#include "glaze/net/http_server.hpp"

#if defined(GLZ_USING_BOOST_ASIO)
namespace asio
{
   using namespace boost::asio;
   using error_code = boost::system::error_code;
}
#endif

namespace
{
   using namespace ut;

   constexpr int test_port = 8888;
   constexpr char test_host[] = "127.0.0.1";
   constexpr char test_route[] = "/post_test";

   constexpr const char* post_body = R"({
  "property1": "EG6ieru6ughohvei5aXooch0veiSee0Aesah5beFeewaixahDievohto",
  "property1": "Oe5hee0Aingae6AhNgu6wooh4eegh3reTai3lojeij1nahb5yohl6ait",
  "property1": "na7chuigies3osa0Zaa9xah9Eechu1aefiThuuthair3foeWuugephe1",
  "property1": "oodei9hai7OqueishaexohXeicahpahphiiGeech9Goofee8fohghua8",
  "property1": "AiNei1iegheiPhei5ohtee2aepaz6jo3aLoviuyia4laeh6eich6phoo",
  "property1": "yoh9loiqueiy9soox3hu7ar6uxuc2Jai4lola6Ooyavoo8zohfah4gee",
  "property1": "xah8ohQuaicie0foeghaiNeeluaco3Saphohwie4aihoo9aesh8ohbax",
  "property1": "jome7ao8nashaec2ci3etuquil9ooZohri8joo1ithooX5kohSei9cah",
  "property1": "quai2ohZaegh6peex4jaijeiCho0shahveu2eeriphiidauyei0deeph",
  "property1": "aeboomeLoo9eYohaeshue4Aesheimoal9EC9Ohquarepei8ut0aethue"
})";

   void wait_for_server_ready(int port, int max_tries = 50)
   {
      for (int tries = 0; tries < max_tries; ++tries) {
         try {
            asio::io_context io_ctx;
            asio::ip::tcp::socket sock(io_ctx);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(port));
            asio::error_code ec;
            sock.connect(endpoint, ec);
            if (ec != asio::error::connection_refused) {
               return;
            }
         }
         catch (...) {
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(40));
      }
      throw std::runtime_error("Server did not start in time");
   }

   // Build HTTP headers with many custom headers to simulate the original bug scenario
   std::string build_headers(size_t content_length)
   {
      std::ostringstream req;
      req << "POST " << test_route << " HTTP/1.1\r\n"
          << "Content-Type: application/octet-stream\r\n"
          << "User-Agent: glaze-test/1.0\r\n"
          << "Content-Length: " << content_length << "\r\n"
          << "Accept-Encoding: gzip, compress, deflate, br\r\n"
          << "Host: " << test_host << ":" << test_port << "\r\n"
          << "Connection: close\r\n"
          // Many additional headers to fill the initial buffer read
          << "X-Testheader-1: zie3ethahf4oomouHohPhi5HuhahvuL8jeilohqua0Ohdaivahqueido\r\n"
          << "X-Testheader-2: maihai7feeS5epachotahxei5ietaepieheeWahyuaLeequeish5dee1\r\n"
          << "X-Testheader-3: Chiecohghaer9xieJ4elaejee8iPheiMoo5umiuShah2ooyia2nee4fi\r\n"
          << "X-Testheader-4: ae5ahbaiM9naechoo5Aze6ietohrohnaenguob7ce7de6aveey6yoo0o\r\n"
          << "X-Testheader-5: baijah6xahFaichee8dah4quon8Eish4jai7dao6dahG3Wophekiek9u\r\n"
          << "X-Testheader-6: Jua1DeeB3esh7AerahS6ip2Tohngaizah4ihei9xeeb5QuieKeebie0r\r\n"
          << "X-Testheader-7: ovewahgh5ab6jahsahd9Aim6Ookooto2aex9AidohsheeGo1de2veeng\r\n"
          << "X-Testheader-8: toreeweiwohghahlae0queew6ahso3taiNgei5echaiqueewax1Eig1u\r\n"
          << "X-Testheader-9: efuid3xoo4Vei3ooghaH3aiY0eeraiyooPe8rie8oothav1eimoochei\r\n"
          << "\r\n";
      return req.str();
   }

   std::string read_response(asio::ip::tcp::socket& socket)
   {
      std::string resp;
      std::array<char, 4096> buf{};
      asio::error_code ec;
      for (;;) {
         std::size_t n = socket.read_some(asio::buffer(buf), ec);
         if (n == 0 || ec) break;
         resp.append(buf.data(), n);
      }
      return resp;
   }

   // Send headers and body in a single write (tests the case where body is already buffered)
   std::string post_single_write(const std::string& body)
   {
      wait_for_server_ready(test_port);
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(test_port));
      asio::error_code ec;
      socket.connect(endpoint, ec);
      if (ec) {
         return "";
      }

      std::string headers = build_headers(body.size());
      std::string request = headers + body;
      asio::write(socket, asio::buffer(request.data(), request.size()));

      return read_response(socket);
   }

   // Send headers first, then body separately to exercise the async_read path
   // This simulates the scenario where the body hasn't arrived when headers are parsed
   std::string post_chunked_write(const std::string& body)
   {
      wait_for_server_ready(test_port);
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(test_port));
      asio::error_code ec;
      socket.connect(endpoint, ec);
      if (ec) {
         return "";
      }

      std::string headers = build_headers(body.size());

      // Send headers first
      asio::write(socket, asio::buffer(headers.data(), headers.size()));

      // Small delay to ensure headers are processed before body arrives
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      // Send body separately - this exercises the async_read code path
      asio::write(socket, asio::buffer(body.data(), body.size()));

      return read_response(socket);
   }

} // namespace

static void error_handler(std::error_code ec, std::source_location loc)
{
   std::fprintf(stderr, "Server error at %s:%d: %s\n", loc.file_name(), loc.line(), ec.message().c_str());
}

suite http_server_post_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<false> server(io_ctx, error_handler);

   // Track received body for verification
   std::mutex body_mutex;
   std::string received_body;

   std::thread server_thr([&] {
      server.post(test_route, [&](const glz::request& req, glz::response& res) {
         {
            std::lock_guard<std::mutex> lock(body_mutex);
            received_body = req.body;
         }
         res.status(200);
         res.content_type("text/plain");
         res.body("OK:" + std::to_string(req.body.size()));
      });

      server.bind("0.0.0.0", test_port);
      server.start(0);
      io_ctx->run();
   });

   "POST with body sent in single write"_test = [&] {
      std::string body(post_body);
      std::future<std::string> f = std::async(std::launch::async, [&] { return post_single_write(body); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("200 OK") != std::string::npos) << "Expected 200 OK response";
      expect(response.find("OK:" + std::to_string(body.size())) != std::string::npos) << "Expected correct body size";

      {
         std::lock_guard<std::mutex> lock(body_mutex);
         expect(received_body == body) << "Body content mismatch";
      }
   };

   "POST with headers and body sent separately (exercises async_read)"_test = [&] {
      std::string body(post_body);
      std::future<std::string> f = std::async(std::launch::async, [&] { return post_chunked_write(body); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("200 OK") != std::string::npos) << "Expected 200 OK response";
      expect(response.find("OK:" + std::to_string(body.size())) != std::string::npos) << "Expected correct body size";

      {
         std::lock_guard<std::mutex> lock(body_mutex);
         expect(received_body == body) << "Body content mismatch";
      }
   };

   "POST with binary data containing null bytes"_test = [&] {
      // Create binary data with embedded null bytes
      std::string binary_body;
      binary_body.reserve(256);
      for (int i = 0; i < 256; ++i) {
         binary_body.push_back(static_cast<char>(i)); // includes \0 at i=0
      }
      // Add more data after the null bytes
      binary_body += "data after nulls";
      binary_body.push_back('\0');
      binary_body += "more data";

      std::future<std::string> f = std::async(std::launch::async, [&] { return post_chunked_write(binary_body); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("200 OK") != std::string::npos) << "Expected 200 OK response";
      expect(response.find("OK:" + std::to_string(binary_body.size())) != std::string::npos)
         << "Expected correct body size for binary data";

      {
         std::lock_guard<std::mutex> lock(body_mutex);
         expect(received_body == binary_body) << "Binary body content mismatch";
      }
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main()
{
   std::cout << "Testing implementations from http_server.md documentation\n\n";
   return 0;
}
