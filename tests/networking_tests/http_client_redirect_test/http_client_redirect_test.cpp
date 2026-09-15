// Tests covering glz::http_client's automatic redirect following: the hop rules of
// RFC 9110 15.4, reference resolution of the Location field (RFC 3986 5.2/5.3), the hop
// limit, and the credential stripping that applies when a hop crosses origins.
//
// A hand-rolled HTTP/1.1 mini-server is used rather than glz::http_server so each route
// can answer with an exact set of response bytes - including deliberately malformed
// Location fields that a well-behaved server would never emit.

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/net/http_client.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

namespace
{
   struct recorded_request
   {
      std::string method;
      std::string target;
      std::string body;
      std::vector<std::pair<std::string, std::string>> headers;

      [[nodiscard]] std::optional<std::string> header(std::string_view name) const
      {
         for (const auto& [field_name, value] : headers) {
            if (glz::striequal(field_name, name)) {
               return value;
            }
         }
         return std::nullopt;
      }
   };

   std::string ok_response(std::string_view body)
   {
      std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
      resp.append(std::to_string(body.size()));
      resp.append("\r\nConnection: keep-alive\r\n\r\n");
      resp.append(body);
      return resp;
   }

   // A redirect with an empty body, framed by Content-Length so the connection stays
   // reusable - the same shape a real server sends.
   std::string redirect_response(int status_code, std::optional<std::string_view> location)
   {
      std::string resp = "HTTP/1.1 ";
      resp.append(std::to_string(status_code));
      resp.append(" ");
      resp.append(glz::detail::http_status_reason_phrase(status_code));
      resp.append("\r\n");
      if (location) {
         resp.append("Location: ");
         resp.append(*location);
         resp.append("\r\n");
      }
      resp.append("Content-Length: 0\r\nConnection: keep-alive\r\n\r\n");
      return resp;
   }

   // An HTTP/1.1 keep-alive server whose routes answer with exact response bytes. Every
   // asio object is touched only from the one io thread: sockets are not shared objects,
   // so a stop() that closed them from the test thread while a read was in flight would
   // be a race. Stopping the context abandons whatever is in flight instead.
   class redirect_server
   {
     public:
      // Takes the request that arrived and returns the exact response bytes to write.
      using route_handler = std::function<std::string(const recorded_request&)>;

      ~redirect_server() { stop(); }

      bool start()
      {
         asio::error_code ec;
         asio::ip::tcp::endpoint ep(asio::ip::address_v4::loopback(), 0);
         acceptor_.open(ep.protocol(), ec);
         if (ec) return false;
         acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
         acceptor_.bind(ep, ec);
         if (ec) return false;
         acceptor_.listen(asio::socket_base::max_listen_connections, ec);
         if (ec) return false;
         port_ = acceptor_.local_endpoint().port();

         work_.emplace(io_ctx_.get_executor());
         do_accept();
         thread_ = std::thread([this]() { io_ctx_.run(); });

         // listen() has already returned, so the port is accepting by the time start()
         // does. No sleep needed.
         return true;
      }

      void stop()
      {
         if (!work_) return;
         asio::post(io_ctx_, [this]() {
            asio::error_code ec;
            acceptor_.close(ec);
         });
         work_.reset();
         io_ctx_.stop();
         if (thread_.joinable()) thread_.join();
      }

      // Routes are registered before the client is pointed at the server, but the io
      // thread reads them, so the map is guarded like the request log.
      void route(std::string path, route_handler handler)
      {
         std::lock_guard lock{mutex_};
         routes_.insert_or_assign(std::move(path), std::move(handler));
      }

      [[nodiscard]] uint16_t port() const { return port_; }
      [[nodiscard]] std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

      [[nodiscard]] std::vector<recorded_request> requests() const
      {
         std::lock_guard lock{mutex_};
         return requests_;
      }

     private:
      struct connection : std::enable_shared_from_this<connection>
      {
         asio::ip::tcp::socket socket;
         asio::streambuf buffer;
         redirect_server* server;

         connection(asio::ip::tcp::socket s, redirect_server* srv) : socket(std::move(s)), server(srv) {}

         void start() { read_request(); }

         void read_request()
         {
            auto self = shared_from_this();
            asio::async_read_until(socket, buffer, "\r\n\r\n", [this, self](asio::error_code ec, size_t header_bytes) {
               if (ec) return;
               auto request = std::make_shared<recorded_request>(parse_head(header_bytes));
               buffer.consume(header_bytes);
               read_body(request);
            });
         }

         recorded_request parse_head(size_t header_bytes) const
         {
            recorded_request request;
            std::string_view head{static_cast<const char*>(buffer.data().data()), header_bytes};

            const auto request_line_end = head.find("\r\n");
            std::string_view request_line = head.substr(0, request_line_end);
            const auto method_end = request_line.find(' ');
            request.method = std::string(request_line.substr(0, method_end));
            const auto target_end = request_line.find(' ', method_end + 1);
            request.target = std::string(request_line.substr(method_end + 1, target_end - method_end - 1));

            std::string_view fields = head.substr(request_line_end + 2, header_bytes - request_line_end - 4);
            while (!fields.empty()) {
               const auto line_end = fields.find("\r\n");
               std::string_view line = fields.substr(0, line_end);
               fields.remove_prefix(line_end == std::string_view::npos ? fields.size() : line_end + 2);
               const auto colon = line.find(':');
               if (colon == std::string_view::npos) continue;
               std::string_view value = line.substr(colon + 1);
               while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
               request.headers.emplace_back(std::string(line.substr(0, colon)), std::string(value));
            }

            return request;
         }

         void read_body(std::shared_ptr<recorded_request> request)
         {
            size_t content_length = 0;
            if (const auto value = request->header("Content-Length")) {
               content_length = static_cast<size_t>(std::stoul(*value));
            }

            if (buffer.size() >= content_length) {
               finish(std::move(request), content_length);
               return;
            }

            auto self = shared_from_this();
            asio::async_read(socket, buffer, asio::transfer_exactly(content_length - buffer.size()),
                             [this, self, request, content_length](asio::error_code ec, size_t) mutable {
                                if (ec) return;
                                finish(std::move(request), content_length);
                             });
         }

         void finish(std::shared_ptr<recorded_request> request, size_t content_length)
         {
            request->body.assign(static_cast<const char*>(buffer.data().data()), content_length);
            buffer.consume(content_length);

            std::string_view path = request->target;
            if (const auto question = path.find('?'); question != std::string_view::npos) {
               path = path.substr(0, question);
            }

            route_handler handler;
            {
               std::lock_guard lock{server->mutex_};
               server->requests_.push_back(*request);
               if (const auto it = server->routes_.find(std::string(path)); it != server->routes_.end()) {
                  handler = it->second;
               }
            }

            auto response = std::make_shared<std::string>(
               handler ? handler(*request)
                       : std::string{"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n"});

            auto self = shared_from_this();
            asio::async_write(socket, asio::buffer(*response), [this, self, response](asio::error_code ec, size_t) {
               if (ec) return;
               read_request();
            });
         }
      };

      void do_accept()
      {
         acceptor_.async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
            if (ec) return;
            std::make_shared<connection>(std::move(socket), this)->start();
            do_accept();
         });
      }

      asio::io_context io_ctx_{1};
      std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_;
      asio::ip::tcp::acceptor acceptor_{io_ctx_};
      std::thread thread_;
      std::unordered_map<std::string, route_handler> routes_;
      std::vector<recorded_request> requests_;
      mutable std::mutex mutex_;
      uint16_t port_ = 0;
   };
}

suite reference_resolution = [] {
   "remove_dot_segments"_test = [] {
      expect(glz::detail::remove_dot_segments("/a/b/c") == "/a/b/c");
      expect(glz::detail::remove_dot_segments("/a/b/../c") == "/a/c");
      expect(glz::detail::remove_dot_segments("/a/./b") == "/a/b");
      // RFC 3986 5.2.4 steps 2B and 2C replace the "/." or "/.." prefix with "/", so a
      // reference that ends in one names the directory rather than the segment above it.
      expect(glz::detail::remove_dot_segments("/a/b/.") == "/a/b/");
      expect(glz::detail::remove_dot_segments("/a/b/..") == "/a/");
      // RFC 3986 5.4.2 resolves an over-long climb by discarding the extra steps.
      expect(glz::detail::remove_dot_segments("/../../a") == "/a");
      expect(glz::detail::remove_dot_segments("/") == "/");
   };

   // The reference-resolution examples of RFC 3986 5.4.1 and 5.4.2, which pin the whole
   // algorithm rather than the handful of cases that happened to come to mind. The ones
   // left out are the four the client deliberately does not treat as targets to follow:
   // "g:h" (a non-HTTP scheme), "" and "#s" (same-document references), and "http:g"
   // (resolved the way browsers do, as a relative reference).
   "RFC 3986 5.4 reference resolution"_test = [] {
      const url_parts base{"http", "a", 80, "/b/c/d;p?q"};
      const std::pair<std::string_view, std::string_view> vectors[]{
         // 5.4.1, normal examples
         {"g", "/b/c/g"},
         {"./g", "/b/c/g"},
         {"g/", "/b/c/g/"},
         {"/g", "/g"},
         {"?y", "/b/c/d;p?y"},
         {"g?y", "/b/c/g?y"},
         {"g#s", "/b/c/g"},
         {"g?y#s", "/b/c/g?y"},
         {";x", "/b/c/;x"},
         {"g;x", "/b/c/g;x"},
         {"g;x?y#s", "/b/c/g;x?y"},
         {".", "/b/c/"},
         {"./", "/b/c/"},
         {"..", "/b/"},
         {"../", "/b/"},
         {"../g", "/b/g"},
         {"../..", "/"},
         {"../../", "/"},
         {"../../g", "/g"},
         // 5.4.2, abnormal examples
         {"../../../g", "/g"},
         {"../../../../g", "/g"},
         {"/./g", "/g"},
         {"/../g", "/g"},
         {"g.", "/b/c/g."},
         {".g", "/b/c/.g"},
         {"g..", "/b/c/g.."},
         {"..g", "/b/c/..g"},
         {"./../g", "/b/g"},
         {"./g/.", "/b/c/g/"},
         {"g/./h", "/b/c/g/h"},
         {"g/../h", "/b/c/h"},
         {"g;x=1/./y", "/b/c/g;x=1/y"},
         {"g;x=1/../y", "/b/c/y"},
         {"g?y/./x", "/b/c/g?y/./x"},
         {"g?y/../x", "/b/c/g?y/../x"},
         {"g#s/./x", "/b/c/g"},
         {"g#s/../x", "/b/c/g"},
      };

      for (const auto& [location, target] : vectors) {
         const auto resolved = glz::resolve_redirect_url(base, location);
         expect(resolved.has_value()) << location;
         if (resolved) {
            expect(resolved->host == "a") << location;
            expect(resolved->path == target) << location << " -> " << resolved->path;
         }
      }
   };

   // Every reference in the table above stays on the origin it was resolved against; the
   // scheme-relative form is the one case that does not, and "//g" has an empty path,
   // which is sent as "/".
   "scheme relative reference from the RFC table"_test = [] {
      const url_parts base{"http", "a", 80, "/b/c/d;p?q"};
      const auto resolved = glz::resolve_redirect_url(base, "//g");
      expect(resolved.has_value());
      if (resolved) {
         expect(resolved->host == "g");
         expect(resolved->path == "/");
      }
   };

   "absolute location"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a/b"};
      const auto resolved = glz::resolve_redirect_url(base, "https://other.example:8443/x/y?q=1");
      expect(resolved.has_value());
      expect(resolved->protocol == "https");
      expect(resolved->host == "other.example");
      expect(resolved->port == uint16_t(8443));
      expect(resolved->path == "/x/y?q=1");
   };

   "scheme case is normalized"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a"};
      const auto resolved = glz::resolve_redirect_url(base, "HTTPS://Example.com/b");
      expect(resolved.has_value());
      if (resolved) {
         expect(resolved->protocol == "https");
         expect(resolved->port == uint16_t(443));
      }
   };

   "scheme relative location"_test = [] {
      const url_parts base{"https", "example.com", 443, "/a/b"};
      const auto resolved = glz::resolve_redirect_url(base, "//cdn.example/asset");
      expect(resolved.has_value());
      expect(resolved->protocol == "https");
      expect(resolved->host == "cdn.example");
      expect(resolved->port == uint16_t(443));
      expect(resolved->path == "/asset");
   };

   "root relative location"_test = [] {
      const url_parts base{"http", "example.com", 8080, "/a/b?x=1"};
      const auto resolved = glz::resolve_redirect_url(base, "/c/d");
      expect(resolved.has_value());
      expect(resolved->host == "example.com");
      expect(resolved->port == uint16_t(8080));
      expect(resolved->path == "/c/d");
   };

   "relative location"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a/b/c?x=1"};
      const auto resolved = glz::resolve_redirect_url(base, "../d/e?y=2");
      expect(resolved.has_value());
      expect(resolved->path == "/a/d/e?y=2");
   };

   "query only location keeps the base path"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a/b?x=1"};
      const auto resolved = glz::resolve_redirect_url(base, "?page=2");
      expect(resolved.has_value());
      expect(resolved->path == "/a/b?page=2");
   };

   "fragment is dropped"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a"};
      const auto resolved = glz::resolve_redirect_url(base, "/b#section");
      expect(resolved.has_value());
      expect(resolved->path == "/b");
   };

   "a query that contains a url is not an absolute reference"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a"};
      const auto resolved = glz::resolve_redirect_url(base, "/login?next=http://elsewhere/x");
      expect(resolved.has_value());
      expect(resolved->host == "example.com");
      expect(resolved->path == "/login?next=http://elsewhere/x");
   };

   // A space is not legal in a URI, but servers emit them and every other user agent
   // encodes rather than abandon the hop.
   "a space in the path is percent-encoded"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a"};
      const auto resolved = glz::resolve_redirect_url(base, "/files/my report.pdf");
      expect(resolved.has_value());
      if (resolved) {
         expect(resolved->path == "/files/my%20report.pdf");
      }

      const auto in_query = glz::resolve_redirect_url(base, "/search?q=two words");
      expect(in_query.has_value());
      if (in_query) {
         expect(in_query->path == "/search?q=two%20words");
      }
   };

   "unusable locations are rejected"_test = [] {
      const url_parts base{"http", "example.com", 80, "/a"};
      expect(!glz::resolve_redirect_url(base, ""));
      expect(!glz::resolve_redirect_url(base, "#only-a-fragment"));
      // A non-HTTP scheme is not a target an HTTP request can continue onto.
      expect(!glz::resolve_redirect_url(base, "ws://example.com/socket"));
      expect(!glz::resolve_redirect_url(base, "file:///etc/passwd"));
      // A CTL would split the message itself.
      expect(!glz::resolve_redirect_url(base, "/a\r\nX-Injected: 1"));
      expect(!glz::resolve_redirect_url(base, "/a\tb"));
      // A space in a host resolves to nothing a name lookup could use.
      expect(!glz::resolve_redirect_url(base, "http://ex ample.com/a"));
      // parse_url has no userinfo component, so credentials in a Location resolve to nothing.
      expect(!glz::resolve_redirect_url(base, "http://user:pass@example.com/a"));
   };
};

suite hop_rules = [] {
   // The rules apply_redirect carries that the public request methods cannot reach:
   // HEAD never becomes GET, and a status that is not a redirect is never a hop.
   "303 keeps HEAD as HEAD"_test = [] {
      glz::detail::redirect_request request{"HEAD", url_parts{"http", "example.com", 80, "/a"}, "", {}, 0};
      response resp;
      resp.status_code = 303;
      resp.response_headers.add("Location", "/b");

      const auto applied = glz::detail::apply_redirect(resp, request, 5);
      expect(applied.has_value());
      expect(request.method == "HEAD");
      expect(request.url.path == "/b");
      expect(request.hops == 1u);
   };

   "statuses that are not followed"_test = [] {
      // 300 names no single target, 305 was deprecated as unsafe to act on, and 304 is
      // not a redirect at all.
      expect(!glz::is_redirect_status(300));
      expect(!glz::is_redirect_status(304));
      expect(!glz::is_redirect_status(305));
      expect(glz::is_redirect_status(301));
      expect(glz::is_redirect_status(302));
      expect(glz::is_redirect_status(303));
      expect(glz::is_redirect_status(307));
      expect(glz::is_redirect_status(308));
   };
};

suite redirect_following = [] {
   "not followed by default"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [&](const recorded_request&) { return redirect_response(302, "/end"); });
      server.route("/end", [](const recorded_request&) { return ok_response("arrived"); });

      http_client client{};
      expect(client.max_redirects() == 0u);
      const auto resp = client.get(server.base_url() + "/start");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 302);
         expect(resp->response_headers.first_value("Location").value_or("") == "/end");
      }
      expect(server.requests().size() == 1u);
   };

   "followed when enabled"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [&](const recorded_request&) { return redirect_response(302, "/end"); });
      server.route("/end", [](const recorded_request&) { return ok_response("arrived"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 200);
         expect(resp->response_body == "arrived");
      }

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].method == "GET");
         expect(requests[1].target == "/end");
      }
   };

   "absolute location to another origin"_test = [] {
      redirect_server first;
      redirect_server second;
      expect(first.start());
      expect(second.start());
      const std::string target = second.base_url() + "/end";
      first.route("/start", [&](const recorded_request&) { return redirect_response(302, target); });
      second.route("/end", [](const recorded_request&) { return ok_response("second"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(first.base_url() + "/start");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 200);
         expect(resp->response_body == "second");
      }

      const auto requests = second.requests();
      expect(requests.size() == 1u);
      if (requests.size() == 1) {
         // The Host of the new origin, not the one the chain started on.
         expect(requests[0].header("Host").value_or("") == "127.0.0.1:" + std::to_string(second.port()));
      }
   };

   "relative location resolves against the current url"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/api/v1/thing", [&](const recorded_request&) { return redirect_response(302, "../v2/thing"); });
      server.route("/api/v2/thing", [](const recorded_request&) { return ok_response("v2"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/api/v1/thing");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 200);
         expect(resp->response_body == "v2");
      }
   };

   "chained redirects"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/one", [](const recorded_request&) { return redirect_response(302, "/two"); });
      server.route("/two", [](const recorded_request&) { return redirect_response(302, "/three"); });
      server.route("/three", [](const recorded_request&) { return ok_response("third"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/one");
      expect(resp.has_value());
      if (resp) {
         expect(resp->response_body == "third");
      }
      expect(server.requests().size() == 3u);
   };

   "hop limit"_test = [] {
      redirect_server server;
      expect(server.start());
      // A self-referential redirect: only the hop limit ends it.
      server.route("/loop", [](const recorded_request&) { return redirect_response(302, "/loop"); });

      http_client client{};
      client.max_redirects(3);
      const auto resp = client.get(server.base_url() + "/loop");
      expect(!resp.has_value());
      if (!resp) {
         expect(resp.error() == make_error_code(http_client_error::too_many_redirects));
      }
      // The first request plus the three hops that were allowed.
      expect(server.requests().size() == 4u);
   };

   "missing location"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, std::nullopt); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start");
      expect(!resp.has_value());
      if (!resp) {
         expect(resp.error() == make_error_code(http_client_error::invalid_redirect));
      }
   };

   "unusable location"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, "ws://127.0.0.1/socket"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start");
      expect(!resp.has_value());
      if (!resp) {
         expect(resp.error() == make_error_code(http_client_error::invalid_redirect));
      }
   };

   "303 continues as GET without the body"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/submit", [](const recorded_request&) { return redirect_response(303, "/result"); });
      server.route("/result", [](const recorded_request&) { return ok_response("result"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp =
         client.post(server.base_url() + "/submit", R"({"a":1})", http_headers{{"Content-Type", "application/json"}});
      expect(resp.has_value());
      if (resp) {
         expect(resp->response_body == "result");
      }

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[0].method == "POST");
         expect(requests[0].body == R"({"a":1})");
         expect(requests[1].method == "GET");
         expect(requests[1].body.empty());
         // The media type described a body that is no longer being sent.
         expect(!requests[1].header("Content-Type").has_value());
      }
   };

   "301 rewrites POST to GET"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/old", [](const recorded_request&) { return redirect_response(301, "/new"); });
      server.route("/new", [](const recorded_request&) { return ok_response("new"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.post(server.base_url() + "/old", "payload");
      expect(resp.has_value());

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].method == "GET");
         expect(requests[1].body.empty());
      }
   };

   "307 preserves the method and body"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/submit", [](const recorded_request&) { return redirect_response(307, "/elsewhere"); });
      server.route("/elsewhere", [](const recorded_request&) { return ok_response("ok"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp =
         client.post(server.base_url() + "/submit", "payload", http_headers{{"Content-Type", "text/plain"}});
      expect(resp.has_value());

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].method == "POST");
         expect(requests[1].body == "payload");
         expect(requests[1].header("Content-Type").value_or("") == "text/plain");
         expect(requests[1].header("Content-Length").value_or("") == "7");
      }
   };

   "308 preserves the method and body"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/old", [](const recorded_request&) { return redirect_response(308, "/new"); });
      server.route("/new", [](const recorded_request&) { return ok_response("ok"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.put(server.base_url() + "/old", "payload");
      expect(resp.has_value());

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].method == "PUT");
         expect(requests[1].body == "payload");
      }
   };

   "credentials survive a same-origin hop"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, "/end"); });
      server.route("/end", [](const recorded_request&) { return ok_response("ok"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start", {{"Authorization", "Bearer secret"}});
      expect(resp.has_value());

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].header("Authorization").value_or("") == "Bearer secret");
      }
   };

   "credentials are dropped when a hop crosses origins"_test = [] {
      redirect_server first;
      redirect_server second;
      expect(first.start());
      expect(second.start());
      const std::string target = second.base_url() + "/end";
      first.route("/start", [&](const recorded_request&) { return redirect_response(302, target); });
      second.route("/end", [](const recorded_request&) { return ok_response("ok"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(first.base_url() + "/start",
                                   {{"Authorization", "Bearer secret"}, {"Cookie", "session=1"}, {"X-Trace", "keep"}});
      expect(resp.has_value());

      const auto requests = second.requests();
      expect(requests.size() == 1u);
      if (requests.size() == 1) {
         expect(!requests[0].header("Authorization").has_value());
         expect(!requests[0].header("Cookie").has_value());
         // Only the credential fields go; the caller's own headers still travel.
         expect(requests[0].header("X-Trace").value_or("") == "keep");
      }
   };

   "a space in a Location reaches the server encoded"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, "/my file"); });
      server.route("/my%20file", [](const recorded_request&) { return ok_response("encoded"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start");
      expect(resp.has_value());
      if (resp) {
         expect(resp->response_body == "encoded");
      }

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[1].target == "/my%20file");
      }
   };

   "a 300 is returned rather than followed"_test = [] {
      redirect_server server;
      expect(server.start());
      // A Location is legal on a 300, but it is an offer among alternatives rather than a
      // target to continue onto.
      server.route("/choices", [](const recorded_request&) { return redirect_response(300, "/one"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/choices");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 300);
      }
      expect(server.requests().size() == 1u);
   };

   "one hop is allowed at the limit"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/one", [](const recorded_request&) { return redirect_response(302, "/two"); });
      server.route("/two", [](const recorded_request&) { return ok_response("second"); });

      http_client client{};
      client.max_redirects(1);
      const auto resp = client.get(server.base_url() + "/one");
      expect(resp.has_value());
      if (resp) {
         expect(resp->response_body == "second");
      }

      // The same limit refuses the second hop of a longer chain.
      server.route("/two", [](const recorded_request&) { return redirect_response(302, "/three"); });
      const auto refused = client.get(server.base_url() + "/one");
      expect(!refused.has_value());
      if (!refused) {
         expect(refused.error() == make_error_code(http_client_error::too_many_redirects));
      }
   };

   "a redirect body is drained before the next hop"_test = [] {
      redirect_server server;
      expect(server.start());
      // A 3xx carrying a body is the case where a hop taken on the same pooled connection
      // would read the leftover body as the head of the next response.
      server.route("/start", [](const recorded_request&) {
         static constexpr std::string_view body = "<html><body>Moved</body></html>";
         std::string resp = "HTTP/1.1 302 Found\r\nLocation: /end\r\nContent-Type: text/html\r\nContent-Length: ";
         resp.append(std::to_string(body.size()));
         resp.append("\r\nConnection: keep-alive\r\n\r\n");
         resp.append(body);
         return resp;
      });
      server.route("/end", [](const recorded_request&) { return ok_response("arrived"); });

      http_client client{};
      client.max_redirects(5);
      const auto resp = client.get(server.base_url() + "/start");
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 200);
         expect(resp->response_body == "arrived");
      }
   };

   "a caller-pinned Host does not cross origins"_test = [] {
      redirect_server first;
      redirect_server second;
      expect(first.start());
      expect(second.start());
      const std::string target = second.base_url() + "/end";
      first.route("/start", [&](const recorded_request&) { return redirect_response(302, target); });
      second.route("/end", [](const recorded_request&) { return ok_response("ok"); });

      http_client client{};
      client.max_redirects(5);
      // build_http_request_bytes prefers a caller's Host over the one in the URL, so
      // without the erase this authority would be sent to the second origin.
      const auto resp = client.get(first.base_url() + "/start", {{"Host", "pinned.example"}});
      expect(resp.has_value());

      const auto requests = second.requests();
      expect(requests.size() == 1u);
      if (requests.size() == 1) {
         expect(requests[0].header("Host").value_or("") == "127.0.0.1:" + std::to_string(second.port()));
      }
   };

   "streaming requests deliver the redirect itself"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, "/end"); });
      server.route("/end", [](const recorded_request&) { return ok_response("arrived"); });

      http_client client{};
      client.max_redirects(5);

      std::promise<int> promise;
      auto future = promise.get_future();
      auto connection = client.stream_request_v2(
         {.url = server.base_url() + "/start",
          .on_data = [](std::string_view) {},
          .on_error = [](std::error_code) {},
          .on_connect = [&promise](const response& resp) mutable { promise.set_value(resp.status_code); }});
      expect(connection != nullptr);

      expect(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
      expect(future.get() == 302);
      // The stream stops at the redirect; the target is never requested.
      expect(server.requests().size() == 1u);
   };

   "async requests follow redirects"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/one", [](const recorded_request&) { return redirect_response(302, "/two"); });
      server.route("/two", [](const recorded_request&) { return redirect_response(302, "/three"); });
      server.route("/three", [](const recorded_request&) { return ok_response("third"); });

      http_client client{};
      client.max_redirects(5);

      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();
      client.get_async(
         server.base_url() + "/one", {},
         [&promise](std::expected<response, std::error_code> result) mutable { promise.set_value(std::move(result)); });

      expect(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
      const auto resp = future.get();
      expect(resp.has_value());
      if (resp) {
         expect(resp->status_code == 200);
         expect(resp->response_body == "third");
      }
      expect(server.requests().size() == 3u);
   };

   "async hop limit"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/loop", [](const recorded_request&) { return redirect_response(302, "/loop"); });

      http_client client{};
      client.max_redirects(2);

      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();
      client.get_async(
         server.base_url() + "/loop", {},
         [&promise](std::expected<response, std::error_code> result) mutable { promise.set_value(std::move(result)); });

      expect(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
      const auto resp = future.get();
      expect(!resp.has_value());
      if (!resp) {
         expect(resp.error() == make_error_code(http_client_error::too_many_redirects));
      }
   };

   "async missing location"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/start", [](const recorded_request&) { return redirect_response(302, std::nullopt); });

      http_client client{};
      client.max_redirects(5);

      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();
      client.get_async(
         server.base_url() + "/start", {},
         [&promise](std::expected<response, std::error_code> result) mutable { promise.set_value(std::move(result)); });

      expect(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
      const auto resp = future.get();
      expect(!resp.has_value());
      if (!resp) {
         expect(resp.error() == make_error_code(http_client_error::invalid_redirect));
      }
   };

   "async post follows a 303 as a GET"_test = [] {
      redirect_server server;
      expect(server.start());
      server.route("/submit", [](const recorded_request&) { return redirect_response(303, "/result"); });
      server.route("/result", [](const recorded_request&) { return ok_response("result"); });

      http_client client{};
      client.max_redirects(5);

      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();
      client.post_async(
         server.base_url() + "/submit", "payload", {},
         [&promise](std::expected<response, std::error_code> result) mutable { promise.set_value(std::move(result)); });

      expect(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
      const auto resp = future.get();
      expect(resp.has_value());
      if (resp) {
         expect(resp->response_body == "result");
      }

      const auto requests = server.requests();
      expect(requests.size() == 2u);
      if (requests.size() == 2) {
         expect(requests[0].method == "POST");
         expect(requests[1].method == "GET");
      }
   };
};

int main() { return 0; }
