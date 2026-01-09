#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <ut/ut.hpp>

#include "asio.hpp"
#include "asio/io_context.hpp"
#include "glaze/net/http_server.hpp"

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
            std::error_code ec;
            sock.connect(endpoint, ec);
            bool connection_refused = (ec == asio::error::connection_refused);
            if (!connection_refused) {
               return;
            }
         }
         catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
         }
      }
      throw std::runtime_error("Server did not start in time");
   }

   // Minimal HTTP/1.1 POST using ASIO, all headers as requested
   std::string asio_post()
   {
      wait_for_server_ready(test_port);
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(test_port));
      std::error_code ec;
      socket.connect(endpoint, ec);
      bool connection_refused = (ec == asio::error::connection_refused);
      if (connection_refused) {
         return "";
      }

      std::ostringstream req;
      req << "POST " << test_route
          << " HTTP/1.1\r\n"
          // << "Accept: application/json\r\n"
          << "Content-Type: application/json\r\n"
          << "User-Agent: axios/1.11.0\r\n"
          << "Content-Length: " << strlen(post_body) << "\r\n"
          << "Accept-Encoding: gzip, compress, deflate, br\r\n"
          << "Host: " << test_host << ":" << test_port << "\r\n"
          << "Connection: keep-alive\r\n"
          << "X-Testheader-1: zie3ethahf4oomouHohPhi5HuhahvuL8jeilohqua0Ohdaivahqueido\r\n"
          << "X-Testheader-2: maihai7feeS5epachotahxei5ietaepieheeWahyuaLeequeish5dee1\r\n"
          << "X-Testheader-3: Chiecohghaer9xieJ4elaejee8iPheiMoo5umiuShah2ooyia2nee4fi\r\n"
          << "X-Testheader-4: ae5ahbaiM9naechoo5Aze6ietohrohnaenguob7ce7de6aveey6yoo0o\r\n"
          << "X-Testheader-5: baijah6xahFaichee8dah4quon8Eish4jai7dao6dahG3Wophekiek9u\r\n"
          << "X-Testheader-6: Jua1DeeB3esh7AerahS6ip2Tohngaizah4ihei9xeeb5QuieKeebie0r\r\n"
          << "X-Testheader-7: ovewahgh5ab6jahsahd9Aim6Ookooto2aex9AidohsheeGo1de2veeng\r\n"
          << "X-Testheader-8: toreeweiwohghahlae0queew6ahso3taiNgei5echaiqueewax1Eig1u\r\n"
          << "X-Testheader-9: efuid3xoo4Vei3ooghaH3aiY0eeraiyooPe8rie8oothav1eimoochei\r\n"
          << "\r\n"
          << post_body;

      auto req_str = req.str();
      asio::write(socket, asio::buffer(req_str.data(), req_str.size()));

      std::string resp;
      std::array<char, 4096> buf{};
      // asio::error_code ec;
      for (;;) {
         std::size_t n = socket.read_some(asio::buffer(buf), ec);
         if (n == 0 || ec) break;
         resp.append(buf.data(), n);
         // server will not close socket due Connection: keep-alive
         if (resp.find("Okay") != std::string::npos) break;
      }
      return resp;
   }

} // namespace

static void error_handler(std::error_code ec, std::source_location loc)
{
   std::fprintf(stderr, "Server error at %s:%d: %s\n", loc.file_name(), loc.line(), ec.message().c_str());
};

suite http_server_post_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<false> server(io_ctx, error_handler);

   std::thread server_thr([&] {
      // Start server
      server.post(test_route, [](const glz::request& req, glz::response& res) {
         std::cerr << "--------- server.post ----------" << req.body << std::endl;
         res.status(200);
         res.content_type("application/json");

         res.json("Okay");
      });
      server.bind("0.0.0.0", test_port);
      server.start(0);
      io_ctx->run();
   });

   // Call client, check result
   std::future<std::string> f = std::async(std::launch::async, asio_post);
   std::chrono::system_clock::time_point future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(3);
   std::string response = "";
   if (std::future_status::ready == f.wait_until(future_timeout)) {
      response = f.get();
   }
   std::cout << "-> resp: " << response << std::endl;
   expect(response.find("200 OK") != std::string::npos);
   // expect(response.find("Content-Type: application/json") != std::string::npos);
   expect(response.find("Okay") != std::string::npos);

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main()
{
   std::cout << "Testing implementations from http_server.md documentation\n\n";
   return 0;
}
