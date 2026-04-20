// HTTP Server Benchmark: Glaze vs Boost.Beast
//
// Compares end-to-end HTTP performance (accept, parse, route, serialize, respond)
// between glz::http_server and a Boost.Beast async server.
// Both servers serve identical pre-serialized payloads.
// Uses raw TCP sockets for load generation to avoid client-side bias.

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "bencher/bar_chart.hpp"
#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "bencher/file.hpp"
#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

// ---------------------------------------------------------------------------
// Test payloads (shared by both servers)
// ---------------------------------------------------------------------------

struct Message
{
   std::string message{};
};

struct User
{
   int64_t id{};
   std::string name{};
   std::string email{};
   int64_t age{};
   bool active{};
   int64_t score{};
};

static std::string hello_json;
static std::string user_json;
static std::string users_json;
static std::string users_10k_json;

static void init_payloads()
{
   Message msg{"Hello, World!"};
   (void)glz::write_json(msg, hello_json);

   User user{1, "Alice Smith", "alice@example.com", 30, true, 9500};
   (void)glz::write_json(user, user_json);

   auto make_users = [](int64_t count) {
      std::vector<User> v;
      v.reserve(count);
      for (int64_t i = 0; i < count; ++i) {
         v.push_back({i, "User " + std::to_string(i), "user" + std::to_string(i) + "@test.com", 20 + (i % 50),
                      (i % 2 == 0), i * 10});
      }
      return v;
   };

   auto users_100 = make_users(100);
   (void)glz::write_json(users_100, users_json);

   auto users_10k = make_users(10000);
   (void)glz::write_json(users_10k, users_10k_json);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void wait_for_server(uint16_t port, int max_tries = 100)
{
   for (int i = 0; i < max_tries; ++i) {
      try {
         net::io_context io;
         tcp::socket sock(io);
         sock.connect({net::ip::make_address("127.0.0.1"), port});
         sock.close();
         return;
      }
      catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
   }
   std::abort();
}

static size_t send_request(tcp::socket& sock, std::string_view request_data)
{
   net::write(sock, net::buffer(request_data.data(), request_data.size()));

   boost::system::error_code ec;
   net::streambuf header_buf;
   size_t header_bytes = net::read_until(sock, header_buf, "\r\n\r\n", ec);
   if (ec) return 0;
   size_t total = header_bytes;

   std::string headers{net::buffers_begin(header_buf.data()), net::buffers_end(header_buf.data())};
   size_t content_length = 0;
   auto cl_pos = headers.find("Content-Length: ");
   if (cl_pos == std::string::npos) cl_pos = headers.find("content-length: ");
   if (cl_pos != std::string::npos) {
      content_length = std::stoull(headers.substr(cl_pos + 16));
   }

   size_t header_end = headers.find("\r\n\r\n") + 4;
   size_t body_already_read = headers.size() - header_end;
   size_t body_remaining = (content_length > body_already_read) ? content_length - body_already_read : 0;

   if (body_remaining > 0) {
      std::vector<char> body_buf(body_remaining);
      total += net::read(sock, net::buffer(body_buf.data(), body_remaining), ec);
   }
   else {
      total += body_already_read;
   }

   return total;
}

static tcp::socket connect_to(net::io_context& io, uint16_t port)
{
   tcp::socket sock(io);
   sock.connect({net::ip::make_address("127.0.0.1"), port});
   return sock;
}

// Send a single request (Connection: close) and return {status_code, body}.
struct http_result
{
   int status{};
   std::string body{};
};

static http_result fetch(uint16_t port, std::string_view request_data)
{
   net::io_context io;
   auto sock = connect_to(io, port);
   net::write(sock, net::buffer(request_data.data(), request_data.size()));

   boost::system::error_code ec;
   net::streambuf buf;
   net::read_until(sock, buf, "\r\n\r\n", ec);
   if (ec) return {};

   std::string headers{net::buffers_begin(buf.data()), net::buffers_end(buf.data())};

   // Parse status code from "HTTP/1.1 200 ..."
   int status = 0;
   if (auto sp = headers.find(' '); sp != std::string::npos) {
      status = std::stoi(headers.substr(sp + 1, 3));
   }

   size_t content_length = 0;
   auto cl_pos = headers.find("Content-Length: ");
   if (cl_pos == std::string::npos) cl_pos = headers.find("content-length: ");
   if (cl_pos != std::string::npos) {
      content_length = std::stoull(headers.substr(cl_pos + 16));
   }

   size_t header_end = headers.find("\r\n\r\n") + 4;
   std::string body = headers.substr(header_end);

   if (body.size() < content_length) {
      size_t remaining = content_length - body.size();
      std::vector<char> tmp(remaining);
      net::read(sock, net::buffer(tmp.data(), remaining), ec);
      body.append(tmp.data(), tmp.size());
   }
   else {
      body.resize(content_length);
   }

   return {status, std::move(body)};
}

// Validate that both servers return identical 200 responses for all endpoints.
static void validate_servers(uint16_t glaze_port, uint16_t beast_port)
{
   struct endpoint
   {
      std::string name;
      std::string request;
   };

   std::vector<endpoint> endpoints = {
      {"GET /plaintext", "GET /plaintext HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"},
      {"GET /json", "GET /json HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"},
      {"GET /user", "GET /user HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"},
      {"GET /users", "GET /users HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"},
      {"GET /users-10k", "GET /users-10k HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"},
      {"POST /echo",
       "POST /echo HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
       "Content-Length: 34\r\nConnection: close\r\n\r\n{\"message\":\"benchmark\",\"value\":42}"},
   };

   for (const auto& ep : endpoints) {
      auto glz = fetch(glaze_port, ep.request);
      auto bst = fetch(beast_port, ep.request);

      if (glz.status != 200) {
         std::fprintf(stderr, "VALIDATION FAILED: Glaze %s returned status %d\n", ep.name.c_str(), glz.status);
         std::abort();
      }
      if (bst.status != 200) {
         std::fprintf(stderr, "VALIDATION FAILED: Beast %s returned status %d\n", ep.name.c_str(), bst.status);
         std::abort();
      }
      if (glz.body != bst.body) {
         std::fprintf(stderr, "VALIDATION FAILED: %s body mismatch\n  Glaze: %s\n  Beast: %s\n", ep.name.c_str(),
                      glz.body.c_str(), bst.body.c_str());
         std::abort();
      }
   }
}

// ---------------------------------------------------------------------------
// Boost.Beast async HTTP server
// ---------------------------------------------------------------------------

class beast_session : public std::enable_shared_from_this<beast_session>
{
   beast::tcp_stream stream_;
   beast::flat_buffer buffer_;
   http::request<http::string_body> req_;
   http::response<http::string_body> res_;

  public:
   explicit beast_session(tcp::socket&& socket) : stream_(std::move(socket)) {}

   void run() { do_read(); }

  private:
   void do_read()
   {
      req_ = {};
      res_ = {};
      stream_.expires_after(std::chrono::seconds(30));
      http::async_read(stream_, buffer_, req_, beast::bind_front_handler(&beast_session::on_read, shared_from_this()));
   }

   void on_read(beast::error_code ec, std::size_t)
   {
      if (ec == http::error::end_of_stream) {
         stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
         return;
      }
      if (ec) return;

      build_response();
      http::async_write(stream_, res_, beast::bind_front_handler(&beast_session::on_write, shared_from_this()));
   }

   void on_write(beast::error_code ec, std::size_t)
   {
      if (ec) return;
      if (res_.need_eof()) {
         beast::error_code shutdown_ec;
         stream_.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
         return;
      }
      do_read();
   }

   void build_response()
   {
      auto target = req_.target();

      res_.version(req_.version());
      res_.keep_alive(req_.keep_alive());

      if (target == "/plaintext") {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "text/plain");
         res_.body() = "Hello, World!";
      }
      else if (target == "/json") {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "application/json");
         res_.body() = hello_json;
      }
      else if (target == "/user") {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "application/json");
         res_.body() = user_json;
      }
      else if (target == "/users") {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "application/json");
         res_.body() = users_json;
      }
      else if (target == "/users-10k") {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "application/json");
         res_.body() = users_10k_json;
      }
      else if (target == "/echo" && req_.method() == http::verb::post) {
         res_.result(http::status::ok);
         res_.set(http::field::content_type, "application/json");
         res_.body() = req_.body();
      }
      else {
         res_.result(http::status::not_found);
         res_.body() = "Not Found";
      }

      res_.prepare_payload();
   }
};

class beast_listener : public std::enable_shared_from_this<beast_listener>
{
   net::io_context& ioc_;
   tcp::acceptor acceptor_;

  public:
   beast_listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(net::make_strand(ioc))
   {
      acceptor_.open(endpoint.protocol());
      acceptor_.set_option(net::socket_base::reuse_address(true));
      acceptor_.bind(endpoint);
      acceptor_.listen(net::socket_base::max_listen_connections);
   }

   void run() { do_accept(); }

  private:
   void do_accept()
   {
      acceptor_.async_accept(net::make_strand(ioc_),
                             [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
                                if (!ec) {
                                   std::make_shared<beast_session>(std::move(socket))->run();
                                }
                                self->do_accept();
                             });
   }
};

struct beast_server
{
   net::io_context ioc;
   std::vector<std::thread> threads;
   uint16_t port;

   beast_server(uint16_t p, unsigned num_threads) : ioc(static_cast<int>(num_threads)), port(p)
   {
      auto listener = std::make_shared<beast_listener>(ioc, tcp::endpoint{net::ip::make_address("127.0.0.1"), port});
      listener->run();
      threads.reserve(num_threads);
      for (unsigned i = 0; i < num_threads; ++i) {
         threads.emplace_back([this]() { ioc.run(); });
      }
      wait_for_server(port);
   }

   ~beast_server()
   {
      ioc.stop();
      for (auto& t : threads) {
         if (t.joinable()) t.join();
      }
   }
};

// ---------------------------------------------------------------------------
// Glaze HTTP server
// ---------------------------------------------------------------------------

struct glaze_server
{
   glz::http_server<> server{};
   std::thread thread;
   uint16_t port;

   glaze_server(uint16_t p, unsigned num_threads) : port(p)
   {
      server.on_error([](std::error_code, std::source_location) {});

      server.get("/plaintext", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("text/plain").body("Hello, World!");
      });

      server.get("/json", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("application/json").body(hello_json);
      });

      server.get("/user", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("application/json").body(user_json);
      });

      server.get("/users", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("application/json").body(users_json);
      });

      server.get("/users-10k", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("application/json").body(users_10k_json);
      });

      server.post("/echo", [](const glz::request& req, glz::response& res) {
         res.status(200).content_type("application/json").body(req.body);
      });

      server.bind("127.0.0.1", port);
      thread = std::thread([this, num_threads]() { server.start(num_threads); });
      wait_for_server(port);
   }

   ~glaze_server()
   {
      server.stop();
      if (thread.joinable()) thread.join();
   }
};

// ---------------------------------------------------------------------------
// Request templates
// ---------------------------------------------------------------------------

static std::string make_get_req(std::string_view path)
{
   return "GET " + std::string(path) +
          " HTTP/1.1\r\n"
          "Host: 127.0.0.1\r\n"
          "Connection: keep-alive\r\n"
          "\r\n";
}

static std::string make_post_req(std::string_view path, const std::string& body)
{
   return "POST " + std::string(path) +
          " HTTP/1.1\r\n"
          "Host: 127.0.0.1\r\n"
          "Content-Type: application/json\r\n"
          "Content-Length: " +
          std::to_string(body.size()) +
          "\r\n"
          "Connection: keep-alive\r\n"
          "\r\n" +
          body;
}

// ---------------------------------------------------------------------------
// Chart helper
// ---------------------------------------------------------------------------

static std::string stage_bar_chart(const bencher::stage& stage)
{
   std::vector<std::string> names;
   std::vector<double> data;
   for (const auto& r : stage.results) {
      names.push_back(r.name);
      data.push_back(r.throughput_mb_per_sec);
   }
   chart_config cfg;
   cfg.title = stage.name;
   cfg.y_axis_label = stage.throughput_units_label;
   cfg.chart_width = 1000;
   cfg.chart_height = 700;
   cfg.margin_bottom = 280;
   cfg.font_size_bar_label = 16.0;
   cfg.font_size_value_label = 16.0;
   cfg.label_rotation = -45.0;
   return generate_bar_chart_svg(names, data, cfg);
}

// Run a batch of keep-alive requests on a fresh connection and return the count.
static uint64_t run_batch(uint16_t port, const std::string& req, int batch_size)
{
   net::io_context io;
   auto sock = connect_to(io, port);
   for (int i = 0; i < batch_size; ++i) {
      send_request(sock, req);
   }
   return uint64_t(batch_size);
}

// Open a fresh connection, send one request with Connection: close, read the response.
static uint64_t run_new_conn(uint16_t port, const std::string& req)
{
   net::io_context io;
   auto sock = connect_to(io, port);
   send_request(sock, req);
   return uint64_t(1);
}

// Run concurrent batches across multiple threads and return total request count.
static uint64_t run_concurrent(uint16_t port, const std::string& req, int num_threads, int requests_per_thread)
{
   std::atomic<uint64_t> total{0};
   std::vector<std::thread> threads;
   threads.reserve(num_threads);
   for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&] {
         net::io_context io;
         auto sock = connect_to(io, port);
         for (int i = 0; i < requests_per_thread; ++i) {
            send_request(sock, req);
            total.fetch_add(1, std::memory_order_relaxed);
         }
      });
   }
   for (auto& t : threads) {
      t.join();
   }
   return total.load();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
   init_payloads();

   static constexpr uint16_t glaze_port = 18765;
   static constexpr uint16_t beast_port = 18766;
   static constexpr int batch = 1000;

   const unsigned hw_threads = std::max(1u, std::thread::hardware_concurrency());
   const int client_threads = static_cast<int>(hw_threads);

   const auto plaintext_req = make_get_req("/plaintext");
   const auto json_req = make_get_req("/json");
   const auto user_req = make_get_req("/user");
   const auto users_req = make_get_req("/users");
   const std::string echo_body = R"({"message":"benchmark","value":42})";
   const auto echo_req = make_post_req("/echo", echo_body);

   std::string all_markdown;

   // ========================================================================
   // Single-threaded server tests (1 server thread, 1 client connection)
   // ========================================================================
   {
      glaze_server glz_srv(glaze_port, 1);
      beast_server bst_srv(beast_port, 1);

      validate_servers(glaze_port, beast_port);

      // Plaintext
      {
         bencher::stage stage;
         stage.name = "Plaintext (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, plaintext_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, plaintext_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_plaintext.svg");
      }

      // JSON (small)
      {
         bencher::stage stage;
         stage.name = "JSON Small (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, json_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, json_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_json_small.svg");
      }

      // JSON (single user)
      {
         bencher::stage stage;
         stage.name = "JSON Single User (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, user_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, user_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_json_user.svg");
      }

      // JSON (100 users)
      {
         bencher::stage stage;
         stage.name = "JSON 100 Users (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, users_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, users_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_json_users.svg");
      }

      // JSON (10,000 users — large payload)
      {
         const auto users_10k_req = make_get_req("/users-10k");

         bencher::stage stage;
         stage.name = "JSON 10K Users (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, users_10k_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, users_10k_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_json_users_10k.svg");
      }

      // POST echo
      {
         bencher::stage stage;
         stage.name = "POST Echo JSON (single connection, keep-alive)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_batch(glaze_port, echo_req, batch); });
         stage.run("Boost.Beast", [&] { return run_batch(beast_port, echo_req, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_post_echo.svg");
      }

      // Connection-per-request (new TCP connection each time)
      {
         const std::string close_plaintext =
            "GET /plaintext HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Connection: close\r\n"
            "\r\n";

         const std::string close_json =
            "GET /json HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Connection: close\r\n"
            "\r\n";

         bencher::stage stage;
         stage.name = "Connection-per-Request (Connection: close)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze /plaintext", [&] { return run_new_conn(glaze_port, close_plaintext); });
         stage.run("Beast /plaintext", [&] { return run_new_conn(beast_port, close_plaintext); });
         stage.run("Glaze /json", [&] { return run_new_conn(glaze_port, close_json); });
         stage.run("Beast /json", [&] { return run_new_conn(beast_port, close_json); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_connection_per_request.svg");
      }
   } // single-threaded servers shut down here

   // ========================================================================
   // Multi-threaded server tests (hardware_concurrency server & client threads)
   // ========================================================================
   {
      static constexpr uint16_t glaze_mt_port = 18767;
      static constexpr uint16_t beast_mt_port = 18768;

      glaze_server glz_mt(glaze_mt_port, hw_threads);
      beast_server bst_mt(beast_mt_port, hw_threads);

      // Concurrent plaintext
      {
         bencher::stage stage;
         stage.name = "Concurrent Plaintext (" + std::to_string(client_threads) + " clients x 1000 req, " +
                      std::to_string(hw_threads) + " server threads)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_concurrent(glaze_mt_port, plaintext_req, client_threads, 1000); });
         stage.run("Boost.Beast", [&] { return run_concurrent(beast_mt_port, plaintext_req, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_concurrent_plaintext.svg");
      }

      // Concurrent JSON 100 users
      {
         bencher::stage stage;
         stage.name = "Concurrent JSON 100 Users (" + std::to_string(client_threads) + " clients x 1000 req, " +
                      std::to_string(hw_threads) + " server threads)";
         stage.throughput_units_label = "req/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_concurrent(glaze_mt_port, users_req, client_threads, 1000); });
         stage.run("Boost.Beast", [&] { return run_concurrent(beast_mt_port, users_req, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "http_concurrent_users.svg");
      }
   } // multi-threaded servers shut down here

   bencher::save_file(all_markdown, "http_benchmark.md");

   return 0;
}
