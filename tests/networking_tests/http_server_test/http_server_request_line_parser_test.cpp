#include <iomanip>
#include <string>
#include <string_view>
#include <ut/ut.hpp>

#include "glaze/net/http.hpp"
#include "glaze/net/http_server.hpp"

using namespace ut;
using glz::detail::parse_request_line;

bool parses_to(std::string_view line, glz::http_method method, std::string_view target, bool is_http11)
{
   auto result = parse_request_line(line);
   expect(result.has_value()) << "expected success for: " << line;
   if (not result.has_value()) {
      return false;
   }
   return result->method == method && result->target == target && result->is_http11 == is_http11;
}

void expect_rejected(std::string_view line, int code)
{
   auto result = parse_request_line(line);
   expect(not result.has_value()) << "expected rejection for: " << line;
   if (result.has_value()) {
      return;
   }
   expect(result.error() == code) << "wrong status code" << result.error() << ", expected" << code
                                  << "for:" << std::quoted(std::string{line});
}

suite http_server_request_line_validation_tests = [] {
   "parses a trivial request line for every implemented method"_test = [] {
      using enum glz::http_method;

      for (glz::http_method method : {GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS}) {
         std::string line;
         line += glz::to_string(method);
         line += " /products HTTP/1.1";
         expect(parses_to(line, method, "/products", true)) << "failed to parse valid request line: " << line;
      }
   };

   "parses HTTP/1.0 as not is_http11"_test = [] {
      expect(parses_to("GET /products HTTP/1.0", glz::http_method::GET, "/products", false));
   };

   "parses additional well-formed HTTP versions as not is_http11"_test = [] {
      expect(parses_to("GET /products HTTP/2.0", glz::http_method::GET, "/products", false));
      expect(parses_to("GET /products HTTP/0.9", glz::http_method::GET, "/products", false));
      expect(parses_to("GET /products HTTP/9.9", glz::http_method::GET, "/products", false));
      expect(parses_to("GET /products HTTP/0.0", glz::http_method::GET, "/products", false));
   };

   "parses higher HTTP/1.x minor versions as is_http11 per RFC 9110 Section 6.2"_test = [] {
      for (char minor = '2'; minor <= '9'; ++minor) {
         std::string line = "GET /products HTTP/1.";
         line.push_back(minor);
         expect(parses_to(line, glz::http_method::GET, "/products", true))
            << "failed to parse as is_http11: " << line;
      }
   };

   "parses asterisk-form for OPTIONS"_test = [] {
      expect(parses_to("OPTIONS * HTTP/1.1", glz::http_method::OPTIONS, "*", true));
   };

   "parses origin-form for OPTIONS"_test = [] {
      expect(parses_to("OPTIONS /products HTTP/1.1", glz::http_method::OPTIONS, "/products", true));
   };

   "parses absolute-form for OPTIONS"_test = [] {
      expect(parses_to("OPTIONS http://www.example.org:8001 HTTP/1.1", glz::http_method::OPTIONS,
                       "http://www.example.org:8001", true));
   };

   "rejects empty and terminated line inputs"_test = [] {
      expect_rejected("", 400);
      expect_rejected("\r\n", 400);
      expect_rejected("GET /products HTTP/1.1\r\n", 400);
      expect_rejected("GET /products HTTP/1.1\n", 400);
      expect_rejected("GET /products\r\nHTTP/1.1", 400);
   };

   "rejects incomplete request-line shapes"_test = [] {
      expect_rejected("GET/productsHTTP/1.1", 400);
      expect_rejected("GET/products HTTP/1.1", 400);
      expect_rejected("GET HTTP/1.1", 400);
      expect_rejected("GET /products", 400);
      expect_rejected("GET /products ", 400);
      expect_rejected("GET  HTTP/1.1", 400);
   };

   "rejects request-line spacing that is not exactly method SP target SP version"_test = [] {
      expect_rejected(" GET /products HTTP/1.1", 400);
      expect_rejected("GET  /products HTTP/1.1", 400);
      expect_rejected("GET /productsHTTP/1.1", 400);
      expect_rejected("GET /products  HTTP/1.1", 400);
      expect_rejected("GET /products HTTP/1.1 ", 400);
      expect_rejected("GET /products HI HTTP/1.1", 400);
   };

   "rejects non-SP whitespace around request-line components"_test = [] {
      expect_rejected("\tGET /products HTTP/1.1", 400);
      expect_rejected("GET\t/products HTTP/1.1", 400);
      expect_rejected("GET /products\tHTTP/1.1", 400);
      expect_rejected("GET\v/products HTTP/1.1", 400);
      expect_rejected("GET /products\vHTTP/1.1", 400);
      expect_rejected("GET\f/products HTTP/1.1", 400);
      expect_rejected("GET /products\fHTTP/1.1", 400);
      expect_rejected("GET\r/products HTTP/1.1", 400);
      expect_rejected("GET /products\rHTTP/1.1", 400);
      expect_rejected("GET /products HTTP/1.1\t", 400);
      expect_rejected("GET /products HTTP/1.1\r", 400);
   };

   "rejects malformed HTTP-version"_test = [] {
      expect_rejected("GET /products HTP/1.8", 400);
      expect_rejected("GET /products http/1.1", 400);
      expect_rejected("GET /products Http/1.1", 400);
      expect_rejected("GET /products HTTP/11.1", 400);
      expect_rejected("GET /products HTTP/1.11", 400);
      expect_rejected("GET /products HTTP/1.", 400);
      expect_rejected("GET /products HTTP/1", 400);
      expect_rejected("GET /products HTTP/.1", 400);
      expect_rejected("GET /products HTTP/1-1", 400);
      expect_rejected("GET /products HTTP/x.y", 400);
      expect_rejected("GET /products HTTP/a.1", 400);
      expect_rejected("GET /products HTTP/1.a", 400);
      expect_rejected("GET /products HTTP/1.1.0", 400);
      expect_rejected("GET /products HTTP//1.1", 400);
      expect_rejected("GET /products HTTP/1/1", 400);
   };

   "rejects control and non-ascii bytes in HTTP-version"_test = [] {
      for (char byte : {'\0', '\x01', '\x1f', '\x7f'}) {
         std::string line = "GET /products HTTP/1.";
         line.push_back(byte);
         expect_rejected(line, 400);
      }

      std::string line = "GET /products HTTP/1.";
      line.push_back(static_cast<char>(0x80));
      expect_rejected(line, 400);
   };

   "rejects valid but unsupported method tokens with 501"_test = [] {
      expect_rejected("POP /products HTTP/1.1", 501);
      expect_rejected("get /products HTTP/1.1", 501);
      expect_rejected("M-SEARCH /products HTTP/1.1", 501);
      expect_rejected("123 /products HTTP/1.1", 501);
      expect_rejected("!#$%&'*+-.^_`|~ /products HTTP/1.1", 501);
   };

   "rejects unsupported method tokens before request-target validation"_test = [] {
      expect_rejected("POP * HTTP/1.1", 501);
      expect_rejected("POP products HTTP/1.1", 501);
      expect_rejected("M-SEARCH * HTTP/1.1", 501);
      expect_rejected("PRI * HTTP/2.0", 501);
      expect_rejected("CONNECT example.com:443 HTTP/1.1", 501);
      expect_rejected("CONNECT example.com HTTP/1.1", 501);
      expect_rejected("CONNECT /products HTTP/1.1", 501);
      expect_rejected("CONNECT http://example.com:443 HTTP/1.1", 501);
   };

   "rejects delimiter characters inside method token"_test = [] {
      expect_rejected("GE/T /products HTTP/1.1", 400);
      expect_rejected("GE:T /products HTTP/1.1", 400);
      expect_rejected("GE;T /products HTTP/1.1", 400);
      expect_rejected("GE=T /products HTTP/1.1", 400);
      expect_rejected("GE?T /products HTTP/1.1", 400);
      expect_rejected("GE@T /products HTTP/1.1", 400);
      expect_rejected("GE[T /products HTTP/1.1", 400);
      expect_rejected("GE]T /products HTTP/1.1", 400);
      expect_rejected("GE{T /products HTTP/1.1", 400);
      expect_rejected("GE}T /products HTTP/1.1", 400);
      expect_rejected("GE\\T /products HTTP/1.1", 400);
      expect_rejected("GE\"T /products HTTP/1.1", 400);
      expect_rejected("GE<T /products HTTP/1.1", 400);
      expect_rejected("GE>T /products HTTP/1.1", 400);
      expect_rejected("GE(T /products HTTP/1.1", 400);
      expect_rejected("GE)T /products HTTP/1.1", 400);
      expect_rejected("GE,T /products HTTP/1.1", 400);
   };

   "rejects control and non-ascii bytes in method"_test = [] {
      for (char byte : {'\0', '\x01', '\x1f', '\x7f'}) {
         std::string line = "GE";
         line.push_back(byte);
         line += "T /products HTTP/1.1";
         expect_rejected(line, 400);
      }

      std::string line = "GE";
      line.push_back(static_cast<char>(0x80));
      line += "T /products HTTP/1.1";
      expect_rejected(line, 400);
   };

   "rejects request-targets that match no RFC 9112 form for implemented methods"_test = [] {
      expect_rejected("GET products HTTP/1.1", 400);
      expect_rejected("GET ?x=1 HTTP/1.1", 400);
      expect_rejected("GET example.com:443 HTTP/1.1", 400);
      expect_rejected("OPTIONS example.com:443 HTTP/1.1", 400);
      expect_rejected("GET ** HTTP/1.1", 400);
      expect_rejected("OPTIONS ** HTTP/1.1", 400);
      expect_rejected("OPTIONS *?x=1 HTTP/1.1", 400);
   };

   "rejects asterisk-form for implemented non-OPTIONS methods"_test = [] {
      expect_rejected("GET * HTTP/1.1", 400);
      expect_rejected("POST * HTTP/1.1", 400);
      expect_rejected("HEAD * HTTP/1.1", 400);
   };

   "parses valid origin-form request-target edge cases"_test = [] {
      expect(parses_to("GET / HTTP/1.1", glz::http_method::GET, "/", true));
      expect(parses_to("GET // HTTP/1.1", glz::http_method::GET, "//", true));
      expect(parses_to("GET /// HTTP/1.1", glz::http_method::GET, "///", true));
      expect(parses_to("GET /products/ HTTP/1.1", glz::http_method::GET, "/products/", true));
      expect(parses_to("GET /products//42 HTTP/1.1", glz::http_method::GET, "/products//42", true));
      expect(parses_to("GET /. HTTP/1.1", glz::http_method::GET, "/.", true));
      expect(parses_to("GET /.. HTTP/1.1", glz::http_method::GET, "/..", true));
      expect(parses_to("GET /a/../b HTTP/1.1", glz::http_method::GET, "/a/../b", true));
      expect(parses_to("GET /products;id=123 HTTP/1.1", glz::http_method::GET, "/products;id=123", true));
      expect(parses_to("GET /a:b@c HTTP/1.1", glz::http_method::GET, "/a:b@c", true));
      expect(parses_to("GET /a!$&'()*+,;=:@-._~ HTTP/1.1", glz::http_method::GET, "/a!$&'()*+,;=:@-._~", true));
      expect(parses_to("GET /a%2Fb HTTP/1.1", glz::http_method::GET, "/a%2Fb", true));
      expect(parses_to("GET /a%2fb HTTP/1.1", glz::http_method::GET, "/a%2fb", true));
      expect(parses_to("GET /a%00b HTTP/1.1", glz::http_method::GET, "/a%00b", true));
      expect(parses_to("GET /a%23b HTTP/1.1", glz::http_method::GET, "/a%23b", true));
      expect(parses_to("GET /%5B%5D HTTP/1.1", glz::http_method::GET, "/%5B%5D", true));
      expect(
         parses_to("GET /%D0%BF%D1%83%D1%82%D1%8C HTTP/1.1", glz::http_method::GET, "/%D0%BF%D1%83%D1%82%D1%8C", true));
      expect(parses_to("GET /search? HTTP/1.1", glz::http_method::GET, "/search?", true));
      expect(parses_to("GET /search?q HTTP/1.1", glz::http_method::GET, "/search?q", true));
      expect(parses_to("GET /search?q= HTTP/1.1", glz::http_method::GET, "/search?q=", true));
      expect(parses_to("GET /search?=value HTTP/1.1", glz::http_method::GET, "/search?=value", true));
      expect(parses_to("GET /search?q=a/b?c=d&x=1 HTTP/1.1", glz::http_method::GET, "/search?q=a/b?c=d&x=1", true));
      expect(parses_to("GET /search?x=!$&'()*+,;=:@-._~/? HTTP/1.1", glz::http_method::GET,
                       "/search?x=!$&'()*+,;=:@-._~/?", true));
      expect(parses_to("GET /search?x=100%25 HTTP/1.1", glz::http_method::GET, "/search?x=100%25", true));
      expect(parses_to("GET /search?x=%23&y=%3F&z=%5B%5D HTTP/1.1", glz::http_method::GET,
                       "/search?x=%23&y=%3F&z=%5B%5D", true));
      expect(parses_to("GET /* HTTP/1.1", glz::http_method::GET, "/*", true));
   };

   "rejects raw whitespace inside request-target"_test = [] {
      expect_rejected("GET /a b HTTP/1.1", 400);
      expect_rejected("GET /a\tb HTTP/1.1", 400);
      expect_rejected("GET /a\vb HTTP/1.1", 400);
      expect_rejected("GET /a\fb HTTP/1.1", 400);
   };

   "rejects control characters in request-target"_test = [] {
      for (char byte : {'\r', '\n', '\0', '\x01', '\x1f', '\x7f'}) {
         std::string line = "GET /a";
         line.push_back(byte);
         line += "b HTTP/1.1";
         expect_rejected(line, 400);
      }
   };

   "rejects raw non-ascii bytes in request-target"_test = [] {
      std::string single_byte_line = "GET /a";
      single_byte_line.push_back(static_cast<char>(0x80));
      single_byte_line += "b HTTP/1.1";
      expect_rejected(single_byte_line, 400);

      std::string utf8_line = "GET /caf";
      utf8_line.push_back(static_cast<char>(0xc3));
      utf8_line.push_back(static_cast<char>(0xa9));
      utf8_line += " HTTP/1.1";
      expect_rejected(utf8_line, 400);
   };

   "rejects raw characters not allowed in origin-form path"_test = [] {
      expect_rejected("GET /a\"b HTTP/1.1", 400);
      expect_rejected("GET /a<b HTTP/1.1", 400);
      expect_rejected("GET /a>b HTTP/1.1", 400);
      expect_rejected("GET /a[b HTTP/1.1", 400);
      expect_rejected("GET /a]b HTTP/1.1", 400);
      expect_rejected("GET /a\\b HTTP/1.1", 400);
      expect_rejected("GET /a^b HTTP/1.1", 400);
      expect_rejected("GET /a`b HTTP/1.1", 400);
      expect_rejected("GET /a{b HTTP/1.1", 400);
      expect_rejected("GET /a|b HTTP/1.1", 400);
      expect_rejected("GET /a}b HTTP/1.1", 400);
   };

   "rejects raw characters not allowed in origin-form query"_test = [] {
      expect_rejected("GET /a?x=\" HTTP/1.1", 400);
      expect_rejected("GET /a?x=< HTTP/1.1", 400);
      expect_rejected("GET /a?x=> HTTP/1.1", 400);
      expect_rejected("GET /a?x=[ HTTP/1.1", 400);
      expect_rejected("GET /a?x=] HTTP/1.1", 400);
      expect_rejected("GET /a?x=\\ HTTP/1.1", 400);
      expect_rejected("GET /a?x=^ HTTP/1.1", 400);
      expect_rejected("GET /a?x=` HTTP/1.1", 400);
      expect_rejected("GET /a?x={ HTTP/1.1", 400);
      expect_rejected("GET /a?x=| HTTP/1.1", 400);
      expect_rejected("GET /a?x=} HTTP/1.1", 400);
   };

   "rejects fragment marker in origin-form"_test = [] {
      expect_rejected("GET /products#section HTTP/1.1", 400);
      expect_rejected("GET /products?x=1#section HTTP/1.1", 400);
      expect_rejected("GET /#section HTTP/1.1", 400);
   };

   "rejects malformed percent-encoding in origin-form path"_test = [] {
      expect_rejected("GET /% HTTP/1.1", 400);
      expect_rejected("GET /%0 HTTP/1.1", 400);
      expect_rejected("GET /%GG HTTP/1.1", 400);
      expect_rejected("GET /%2G HTTP/1.1", 400);
      expect_rejected("GET /%u1234 HTTP/1.1", 400);
      expect_rejected("GET /products% HTTP/1.1", 400);
      expect_rejected("GET /products%0 HTTP/1.1", 400);
      expect_rejected("GET /products%GG HTTP/1.1", 400);
      expect_rejected("GET /products%2G HTTP/1.1", 400);
   };

   "rejects malformed percent-encoding in origin-form query"_test = [] {
      expect_rejected("GET /products?x=% HTTP/1.1", 400);
      expect_rejected("GET /products?x=%0 HTTP/1.1", 400);
      expect_rejected("GET /products?x=%GG HTTP/1.1", 400);
      expect_rejected("GET /products?x=%2G HTTP/1.1", 400);
      expect_rejected("GET /products?x=%u1234 HTTP/1.1", 400);
   };

   "parses valid absolute-form request-targets"_test = [] {
      expect(parses_to("GET http://example.com HTTP/1.1", glz::http_method::GET, "http://example.com", true));
      expect(parses_to("GET http://example.com/ HTTP/1.1", glz::http_method::GET, "http://example.com/", true));
      expect(parses_to("GET http://example.com?x=1 HTTP/1.1", glz::http_method::GET, "http://example.com?x=1", true));
      expect(parses_to("GET http://example.com/products?x=1 HTTP/1.1", glz::http_method::GET,
                       "http://example.com/products?x=1", true));
      expect(parses_to("GET https://example.com HTTP/1.1", glz::http_method::GET, "https://example.com", true));
      expect(parses_to("GET https://example.com:443/products HTTP/1.1", glz::http_method::GET,
                       "https://example.com:443/products", true));
      expect(parses_to("GET http://127.0.0.1:8080/products HTTP/1.1", glz::http_method::GET,
                       "http://127.0.0.1:8080/products", true));
      expect(parses_to("GET http://[2001:db8::1]:8080/products HTTP/1.1", glz::http_method::GET,
                       "http://[2001:db8::1]:8080/products", true));
      expect(parses_to("GET HTTP://EXAMPLE.COM/products HTTP/1.1", glz::http_method::GET, "HTTP://EXAMPLE.COM/products",
                       true));
   };

   "rejects invalid absolute-form request-targets"_test = [] {
      expect_rejected("GET http:///products HTTP/1.1", 400);
      expect_rejected("GET https:///products HTTP/1.1", 400);
      expect_rejected("GET http://user@example.com/products HTTP/1.1", 400);
      expect_rejected("GET http://example.com/products#section HTTP/1.1", 400);
      expect_rejected("GET http://example.com/products%GG HTTP/1.1", 400);
      expect_rejected("GET http://example.com/%u1234 HTTP/1.1", 400);
      expect_rejected("GET http://example.com:abc/products HTTP/1.1", 400);
      expect_rejected("GET http://[2001:db8::1/products HTTP/1.1", 400);
      expect_rejected("GET http://example.com/a[b HTTP/1.1", 400);
   };
};

int main() {}
