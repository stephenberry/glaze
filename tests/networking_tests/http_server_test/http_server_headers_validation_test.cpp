#include <chrono>
#include <future>
#include <string>
#include <string_view>
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

using namespace ut;
using namespace std::chrono_literals;
using glz::detail::validate_host_header;

constexpr int test_port = 8899;
constexpr char test_host[] = "127.0.0.1";

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
      std::this_thread::sleep_for(40ms);
   }
   throw std::runtime_error("Server did not start in time");
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

std::string send_raw(const std::string& request)
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
   asio::write(socket, asio::buffer(request.data(), request.size()));
   return read_response(socket);
}

static void error_handler(std::error_code, std::source_location) {}

suite http_server_host_header_value_validation_suite = [] {
   "accepts valid Host header DNS-like reg-name values"_test = [] {
      expect(validate_host_header("a"));
      expect(validate_host_header("localhost"));
      expect(validate_host_header("example.com"));
      expect(validate_host_header("EXAMPLE.com"));
      expect(validate_host_header("a-b.example"));
      expect(validate_host_header("0"));
      expect(validate_host_header("123"));
      expect(validate_host_header("123.example"));
      expect(validate_host_header("123.abc"));
      expect(validate_host_header("example.com."));

      // Underscore is accepted as a pragmatic superset of RFC 1035 labels: it appears in
      // real-world Host values (internal service names, underscore-prefixed labels like
      // "_dmarc" / "_sip._tcp") and common servers tolerate it. Unlike hyphen, it is valid
      // at any position within a label. Tilde '~' is in the same RFC 3986 unreserved set but
      // is still rejected (see the reject cases below), since it has no such real-world use.
      expect(validate_host_header("my_service.internal"));
      expect(validate_host_header("db_primary"));
      expect(validate_host_header("db_primary:5432"));
      expect(validate_host_header("exa_mple.com"));
      expect(validate_host_header("example_com"));
      expect(validate_host_header("_dmarc.example.com"));
      expect(validate_host_header("_sip._tcp.example.com"));
      expect(validate_host_header("trailing_.example"));
      expect(validate_host_header("_"));

      const std::string max_label(63, 'a');
      expect(validate_host_header(max_label));
      expect(validate_host_header(max_label + ".example"));

      const std::string max_host =
         std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." + std::string(61, 'd');
      expect(max_host.size() == 253);
      expect(validate_host_header(max_host));
   };

   "rejects Host header reg-name values outside server DNS-like policy"_test = [] {
      expect(not validate_host_header(""));
      expect(not validate_host_header("."));
      expect(not validate_host_header(".example.com"));
      expect(not validate_host_header("example..com"));
      expect(not validate_host_header("example.com.."));
      expect(not validate_host_header("-example.com"));
      expect(not validate_host_header("example-.com"));
      expect(not validate_host_header("example.-com"));
      expect(not validate_host_header("example.com-"));
      expect(not validate_host_header("example~com"));
      expect(not validate_host_header("example!$&'()*+,;=com"));
      expect(not validate_host_header("example.com/"));
      expect(not validate_host_header("example.com?x"));
      expect(not validate_host_header("example.com#x"));
      expect(not validate_host_header("user@example.com"));
      expect(not validate_host_header("example%2ecom"));
      expect(not validate_host_header("%41.example"));
      expect(not validate_host_header("%F0%9F%92%A9.example"));
      expect(not validate_host_header("example%"));
      expect(not validate_host_header("example%2"));
      expect(not validate_host_header("example%zz"));
      expect(not validate_host_header("example com"));
      expect(not validate_host_header(" example.com"));
      expect(not validate_host_header("example.com "));
      expect(not validate_host_header("\texample.com"));
      expect(not validate_host_header("example.com\t"));
      expect(not validate_host_header("example.com\n"));
      expect(not validate_host_header(std::string_view{"example.com\0", 12}));

      expect(not validate_host_header(std::string(64, 'a')));

      const std::string too_long_host =
         std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." + std::string(62, 'd');
      expect(too_long_host.size() == 254);
      expect(not validate_host_header(too_long_host));

      std::string del_char_host = "exa";
      del_char_host.push_back('\x7f');
      del_char_host += "mple.com";
      expect(not validate_host_header(del_char_host));

      std::string non_ascii_host = "exa";
      non_ascii_host.push_back(static_cast<char>(0x80));
      non_ascii_host += "mple.com";
      expect(not validate_host_header(non_ascii_host));
   };

   "accepts valid Host header port values"_test = [] {
      expect(validate_host_header("example.com:"));
      expect(validate_host_header("example.com:00080"));
      expect(validate_host_header("example.com:80"));
      expect(validate_host_header("example.com:65535"));
      expect(validate_host_header("127.0.0.1:"));
      expect(validate_host_header("127.0.0.1:443"));
      expect(validate_host_header("[::1]:"));
      expect(validate_host_header("[::1]:65535"));
   };

   "rejects invalid Host header port values"_test = [] {
      expect(not validate_host_header(":80"));
      expect(not validate_host_header("example.com:0"));
      expect(not validate_host_header("example.com:00"));
      expect(not validate_host_header("example.com:00000"));
      expect(not validate_host_header("example.com:65536"));
      expect(not validate_host_header("example.com:99999"));
      expect(not validate_host_header("example.com:80:90"));
      expect(not validate_host_header("example.com:abc"));
      expect(not validate_host_header("example.com:+80"));
      expect(not validate_host_header("example.com:-1"));
      expect(not validate_host_header("example.com: 80"));
      expect(not validate_host_header("example.com:80 "));
      expect(not validate_host_header("127.0.0.1:0"));
      expect(not validate_host_header("127.0.0.1:00000"));
      expect(not validate_host_header("[::1]:0"));
      expect(not validate_host_header("[::1]:00000"));
      expect(not validate_host_header("[::1]:65536"));
      expect(not validate_host_header("[::1]:abc"));
      expect(not validate_host_header("[::1]:-1"));
      expect(not validate_host_header("[::1]:443:extra"));
   };

   "accepts valid Host header IPv4 literal values"_test = [] {
      expect(validate_host_header("0.0.0.0"));
      expect(validate_host_header("9.10.99.100"));
      expect(validate_host_header("127.0.0.1"));
      expect(validate_host_header("255.255.255.255"));
      expect(validate_host_header("127.0.0.1:"));
      expect(validate_host_header("127.0.0.1:80"));
   };

   "rejects invalid Host header IPv4-like values"_test = [] {
      expect(not validate_host_header("256.0.0.1"));
      expect(not validate_host_header("1.2.3"));
      expect(not validate_host_header("127.1"));
      expect(not validate_host_header("1.2.3.4.5"));
      expect(not validate_host_header("1..2.3"));
      expect(not validate_host_header(".1.2.3.4"));
      expect(not validate_host_header("1.2.3.4."));
      expect(not validate_host_header("01.2.3.4"));
      expect(not validate_host_header("1.2.003.4"));
      expect(not validate_host_header("1.2.3.04"));
      expect(not validate_host_header("1.2.3.4:65536"));
   };

   "rejects invalid Host header IPv4 literal port values"_test = [] {
      expect(not validate_host_header("1.2.3.4:abc"));
      expect(not validate_host_header("1.2.3.4:80:90"));
   };

   "accepts valid Host header bracketed IPv6 literal values"_test = [] {
      expect(validate_host_header("[::]"));
      expect(validate_host_header("[::1]"));
      expect(validate_host_header("[2001:db8::1]"));
      expect(validate_host_header("[2001:db8::ff00:42:8329]"));
      expect(validate_host_header("[::ffff:192.0.2.1]"));
      expect(validate_host_header("[0:0:0:0:0:0:192.0.2.1]"));
      expect(validate_host_header("[ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255]"));
      expect(validate_host_header("[::1]:443"));
      expect(validate_host_header("[2001:db8::1]:443"));
   };

   "rejects invalid Host header IPv6 literal values"_test = [] {
      expect(not validate_host_header("::1"));
      expect(not validate_host_header("2001:db8::1"));
      expect(not validate_host_header("["));
      expect(not validate_host_header("[]"));
      expect(not validate_host_header("[::1"));
      expect(not validate_host_header("[::1]]"));
      expect(not validate_host_header("[::1]host"));
      expect(not validate_host_header("[gggg::1]"));
      expect(not validate_host_header("[12345::1]"));
      expect(not validate_host_header("[::ffff:999.0.2.1]"));
      expect(not validate_host_header("[::ffff:192.0.2.01]"));
      expect(not validate_host_header("[1:2:3:4:5:6:7:8:9]"));
      expect(not validate_host_header("[::1] :80"));
   };

   "rejects Host header IPvFuture literal values outside server policy"_test = [] {
      expect(not validate_host_header("[v1.fe80]"));
      expect(not validate_host_header("[vF.a:b]"));
      expect(not validate_host_header("[v1.!$&'()*+,;=:_~-]"));
      expect(not validate_host_header("[v1.fe80]:443"));
      expect(not validate_host_header("[v.fe80]"));
      expect(not validate_host_header("[vX.fe80]"));
      expect(not validate_host_header("[v1.]"));
   };
};

suite http_server_headers_validation_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<> server(io_ctx, error_handler);

   std::thread server_thr([&] {
      server.bind("0.0.0.0", test_port);
      server.start(0);
      io_ctx->run();
   });

   "request with missing Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for missing Host header";
   };

   "request with multiple Host headers is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: first\r\n"
         "Host: second\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for multiple Host headers";
   };

   "request with case-insensitive multiple Host headers is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: first\r\n"
         "hoST: second\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for multiple Host headers";
   };

   "request with empty Host header value is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host:\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for empty Host header value";
   };

   "request with Host header userinfo is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: user@example.com\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with userinfo";
   };

   "request with Host header path delimiter is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com/path\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with path delimiter";
   };

   "request with Host header query delimiter is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com?x=1\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with query delimiter";
   };

   "request with Host header malformed percent encoding is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example%zz.com\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with malformed percent encoding";
   };

   "request with percent-encoded Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example%2ecom\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for percent-encoded Host header";
   };

   "request with Host header outside DNS-like reg-name policy is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example~com\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header outside DNS-like reg-name policy";
   };

   "request with underscore Host header passes host validation"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: my_service.internal\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      // Host validation must accept the underscore; the request then reaches routing, and
      // since no route is registered the unmatched path returns 404 (not the 400 a rejected
      // Host would produce). Asserting the positive 404 also fails closed on an empty/timed-out
      // response, unlike a bare "not 400" check which an empty string would satisfy vacuously.
      expect(response.contains("HTTP/1.1 404"))
         << "Expected underscore Host to pass validation and reach routing (404)";
   };

   "request with trailing OWS in Host header passes host validation"_test = [&] {
      // RFC 9110 §5.5 / RFC 9112 §5: trailing optional whitespace (SP/HTAB) is not part of the
      // field value; a recipient MUST strip it before evaluating. The header parser must remove
      // it so a spec-legal "Host: example.com " does not reach the reg-name check as "example.com "
      // and get spuriously rejected. Both a trailing space and a trailing tab must be tolerated.
      for (const std::string& host_line : {"Host: example.com \r\n", "Host: example.com\t\r\n"}) {
         std::string payload = "GET /front HTTP/1.1\r\n" + host_line + "Connection: close\r\n\r\n";

         std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

         std::string response;
         if (f.wait_for(5s) == std::future_status::ready) {
            response = f.get();
         }

         // Trailing OWS is stripped, so the Host passes validation and reaches routing; with no
         // route registered the unmatched path returns 404 rather than the 400 a rejected Host
         // would produce. The positive 404 also fails closed on an empty/timed-out response.
         expect(response.contains("HTTP/1.1 404"))
            << "Expected trailing-OWS Host to pass validation and reach routing (404)";
      }
   };

   "request with Host header non-digit port is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com:http\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with non-digit port";
   };

   "request with Host header zero port is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com:0\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with zero port";
   };

   "request with bracketed IPv6 Host header zero port is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: [::1]:0\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for bracketed IPv6 Host header with zero port";
   };

   "request with Host header port above server policy range is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com:65536\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header port above server policy range";
   };

   "request with Host header repeated port delimiter is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: example.com:80:90\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for Host header with repeated port delimiter";
   };

   "request with unbracketed IPv6 Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: 2001:db8::1\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for unbracketed IPv6 Host header";
   };

   "request with invalid bracketed IPv6 Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: [gggg::1]\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for invalid bracketed IPv6 Host header";
   };

   "request with IPvFuture Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: [v1.fe80]\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for IPvFuture Host header";
   };

   "request with invalid IPvFuture Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: [vX.fe80]\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for invalid IPvFuture Host header";
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
