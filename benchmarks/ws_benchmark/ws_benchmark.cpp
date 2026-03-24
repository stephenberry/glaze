// WebSocket Benchmark: Glaze vs uWebSockets
//
// Compares WebSocket echo performance between glz::websocket_server
// and uWebSockets. Uses Boost.Beast WebSocket client for load generation
// to avoid client-side bias. Both servers echo identical payloads.

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "bencher/bar_chart.hpp"
#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "bencher/file.hpp"

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"

#include "App.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
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

static std::string small_payload; // 64 bytes
static std::string medium_payload; // 1KB
static std::string json_small; // {"message":"Hello, World!"}
static std::string json_users; // 100 users JSON array
static std::string large_payload; // 64KB

static void init_payloads()
{
   small_payload = std::string(64, 'x');
   medium_payload = std::string(1024, 'x');
   large_payload = std::string(65536, 'x');

   Message msg{"Hello, World!"};
   (void)glz::write_json(msg, json_small);

   auto make_users = [](int64_t count) {
      std::vector<User> v;
      v.reserve(count);
      for (int64_t i = 0; i < count; ++i) {
         v.push_back(
            {i, "User " + std::to_string(i), "user" + std::to_string(i) + "@test.com", 20 + (i % 50), (i % 2 == 0),
             i * 10});
      }
      return v;
   };

   auto users_100 = make_users(100);
   (void)glz::write_json(users_100, json_users);
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

// Connect and upgrade to WebSocket using Beast
static websocket::stream<tcp::socket> ws_connect(net::io_context& io, uint16_t port)
{
   tcp::resolver resolver(io);
   auto results = resolver.resolve("127.0.0.1", std::to_string(port));

   websocket::stream<tcp::socket> ws(io);
   net::connect(ws.next_layer(), results);
   ws.handshake("127.0.0.1:" + std::to_string(port), "/ws");
   ws.text(true);

   return ws;
}

// Send/receive N messages on a single WebSocket connection.
static uint64_t run_ws_echo_batch(uint16_t port, const std::string& payload, int batch_size)
{
   net::io_context io;
   auto ws = ws_connect(io, port);

   beast::flat_buffer buffer;
   for (int i = 0; i < batch_size; ++i) {
      ws.write(net::buffer(payload));
      buffer.consume(buffer.size());
      ws.read(buffer);
   }

   ws.close(websocket::close_code::normal);
   return uint64_t(batch_size);
}

// Open a new WebSocket connection, exchange one message, close.
static uint64_t run_ws_new_conn(uint16_t port, const std::string& payload)
{
   net::io_context io;
   auto ws = ws_connect(io, port);

   ws.write(net::buffer(payload));
   beast::flat_buffer buffer;
   ws.read(buffer);

   ws.close(websocket::close_code::normal);
   return uint64_t(1);
}

// Run concurrent WebSocket echo across multiple threads.
static uint64_t run_ws_concurrent(uint16_t port, const std::string& payload, int num_threads, int msgs_per_thread)
{
   std::atomic<uint64_t> total{0};
   std::vector<std::thread> threads;
   threads.reserve(num_threads);

   for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&] {
         net::io_context io;
         auto ws = ws_connect(io, port);

         beast::flat_buffer buffer;
         for (int i = 0; i < msgs_per_thread; ++i) {
            ws.write(net::buffer(payload));
            buffer.consume(buffer.size());
            ws.read(buffer);
            total.fetch_add(1, std::memory_order_relaxed);
         }

         ws.close(websocket::close_code::normal);
      });
   }

   for (auto& t : threads) {
      t.join();
   }
   return total.load();
}

// ---------------------------------------------------------------------------
// Glaze WebSocket server
// ---------------------------------------------------------------------------

struct glaze_ws_server
{
   glz::http_server<> server{};
   std::shared_ptr<glz::websocket_server> ws;
   std::thread thread;
   int server_threads_{};

   glaze_ws_server(uint16_t port, int server_threads = 1)
      : server_threads_(server_threads)
   {
      server.on_error([](std::error_code, std::source_location) {});

      ws = std::make_shared<glz::websocket_server>();
      ws->on_message([](auto conn, std::string_view msg, glz::ws_opcode op) {
         if (op == glz::ws_opcode::text) {
            conn->send_text(msg);
         }
         else {
            conn->send_binary(msg);
         }
      });

      server.websocket("/ws", ws);
      server.bind("127.0.0.1", port);
      server.ws_recv_buffer_size(512 * 1024); // 512KB shared receive buffer per thread
      thread = std::thread([this, server_threads]() { server.start(server_threads); });
      wait_for_server(port);
   }

   ~glaze_ws_server()
   {
      server.stop();
      if (thread.joinable()) thread.join();
   }
};

// ---------------------------------------------------------------------------
// uWebSockets server
// ---------------------------------------------------------------------------

struct PerSocketData
{};

struct uws_server
{
   std::thread thread;
   struct us_listen_socket_t* listen_socket = nullptr;
   uWS::Loop* loop = nullptr;

   uws_server(uint16_t port)
   {
      std::promise<bool> ready;
      auto future = ready.get_future();

      thread = std::thread([this, port, &ready]() {
         uWS::App app;
         loop = uWS::Loop::get();

         app.ws<PerSocketData>(
               "/ws",
               {.compression = uWS::DISABLED,
                .maxPayloadLength = 128 * 1024,
                .idleTimeout = 120,
                .message =
                   [](auto* ws, std::string_view message, uWS::OpCode opCode) { ws->send(message, opCode); }})
            .listen(port,
                    [this, &ready](auto* token) {
                       listen_socket = token;
                       ready.set_value(token != nullptr);
                    })
            .run();
      });

      if (!future.get()) {
         throw std::runtime_error("uWebSockets failed to listen");
      }
      wait_for_server(port);
   }

   ~uws_server()
   {
      if (loop && listen_socket) {
         loop->defer([ls = listen_socket]() { us_listen_socket_close(0, ls); });
      }
      if (thread.joinable()) thread.join();
   }
};

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

static void validate_servers(uint16_t glaze_port, uint16_t uws_port)
{
   const std::string test_msg = "Hello, WebSocket benchmark!";

   auto validate_echo = [&](uint16_t port, const char* name) {
      net::io_context io;
      auto ws = ws_connect(io, port);

      ws.write(net::buffer(test_msg));
      beast::flat_buffer buf;
      ws.read(buf);

      std::string response = beast::buffers_to_string(buf.data());
      if (response != test_msg) {
         std::fprintf(stderr, "VALIDATION FAILED: %s echo mismatch\n  Expected: %s\n  Got: %s\n", name,
                      test_msg.c_str(), response.c_str());
         std::abort();
      }

      ws.close(websocket::close_code::normal);
   };

   validate_echo(glaze_port, "Glaze");
   validate_echo(uws_port, "uWebSockets");
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

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
   init_payloads();

   static constexpr uint16_t glaze_port = 19765;
   static constexpr uint16_t uws_port = 19766;
   static constexpr int batch = 1000;

   const unsigned hw_threads = (std::max)(1u, std::thread::hardware_concurrency());
   const int client_threads = static_cast<int>(hw_threads);

   std::string all_markdown;

   std::printf("=== Single-threaded servers (1 server thread) ===\n\n");

   {
      glaze_ws_server glz_srv(glaze_port, 1);
      uws_server uws_srv(uws_port);

      validate_servers(glaze_port, uws_port);

      // Echo Small Text (64B)
      {
         bencher::stage stage;
         stage.name = "Echo Small Text 64B (single connection)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_echo_batch(glaze_port, small_payload, batch); });
         stage.run("uWebSockets", [&] { return run_ws_echo_batch(uws_port, small_payload, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_echo_small.svg");
      }

      // Echo Medium Text (1KB)
      {
         bencher::stage stage;
         stage.name = "Echo Medium Text 1KB (single connection)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_echo_batch(glaze_port, medium_payload, batch); });
         stage.run("uWebSockets", [&] { return run_ws_echo_batch(uws_port, medium_payload, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_echo_medium.svg");
      }

      // Echo JSON Small
      {
         bencher::stage stage;
         stage.name = "Echo JSON Small (single connection)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_echo_batch(glaze_port, json_small, batch); });
         stage.run("uWebSockets", [&] { return run_ws_echo_batch(uws_port, json_small, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_echo_json_small.svg");
      }

      // Echo JSON 100 Users
      {
         bencher::stage stage;
         stage.name = "Echo JSON 100 Users (single connection)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_echo_batch(glaze_port, json_users, batch); });
         stage.run("uWebSockets", [&] { return run_ws_echo_batch(uws_port, json_users, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_echo_json_users.svg");
      }

      // Echo Large Text (64KB)
      {
         bencher::stage stage;
         stage.name = "Echo Large Text 64KB (single connection)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_echo_batch(glaze_port, large_payload, batch); });
         stage.run("uWebSockets", [&] { return run_ws_echo_batch(uws_port, large_payload, batch); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_echo_large.svg");
      }

      // Connection Upgrade (new WebSocket per message exchange)
      {
         bencher::stage stage;
         stage.name = "Connection Upgrade (new WebSocket per message)";
         stage.throughput_units_label = "conn/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_new_conn(glaze_port, small_payload); });
         stage.run("uWebSockets", [&] { return run_ws_new_conn(uws_port, small_payload); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_connection_upgrade.svg");
      }

      // Concurrent Echo Small
      {
         bencher::stage stage;
         stage.name = "Concurrent Echo Small (" + std::to_string(client_threads) + " clients, 1 server thread)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_concurrent(glaze_port, small_payload, client_threads, 1000); });
         stage.run("uWebSockets", [&] { return run_ws_concurrent(uws_port, small_payload, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_concurrent_small.svg");
      }

      // Concurrent Echo JSON 100 Users
      {
         bencher::stage stage;
         stage.name =
            "Concurrent Echo JSON 100 Users (" + std::to_string(client_threads) + " clients, 1 server thread)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze", [&] { return run_ws_concurrent(glaze_port, json_users, client_threads, 1000); });
         stage.run("uWebSockets", [&] { return run_ws_concurrent(uws_port, json_users, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_concurrent_json.svg");
      }
   }

   // -----------------------------------------------------------------------
   // Multi-threaded server benchmarks
   // Glaze runs with server_threads == hw_threads; uWebSockets is single-threaded
   // by design (one event loop). This tests Glaze's multi-threaded scaling.
   // -----------------------------------------------------------------------

   const int server_threads = client_threads; // match server threads to hardware concurrency

   std::printf("\n=== Multi-threaded Glaze server (%d server threads) vs single-threaded uWebSockets ===\n\n",
               server_threads);

   {
      static constexpr uint16_t glaze_mt_port = 19767;
      static constexpr uint16_t uws_mt_port = 19768;

      glaze_ws_server glz_srv(glaze_mt_port, server_threads);
      uws_server uws_srv(uws_mt_port);

      validate_servers(glaze_mt_port, uws_mt_port);

      // Multi-threaded Concurrent Echo Small
      {
         bencher::stage stage;
         stage.name = "MT Concurrent Echo Small (" + std::to_string(client_threads) + " clients, Glaze " +
                      std::to_string(server_threads) + "T)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze (" + std::to_string(server_threads) + "T)",
                   [&] { return run_ws_concurrent(glaze_mt_port, small_payload, client_threads, 1000); });
         stage.run("uWebSockets (1T)",
                   [&] { return run_ws_concurrent(uws_mt_port, small_payload, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_mt_concurrent_small.svg");
      }

      // Multi-threaded Concurrent Echo JSON 100 Users
      {
         bencher::stage stage;
         stage.name = "MT Concurrent Echo JSON 100 Users (" + std::to_string(client_threads) + " clients, Glaze " +
                      std::to_string(server_threads) + "T)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze (" + std::to_string(server_threads) + "T)",
                   [&] { return run_ws_concurrent(glaze_mt_port, json_users, client_threads, 1000); });
         stage.run("uWebSockets (1T)",
                   [&] { return run_ws_concurrent(uws_mt_port, json_users, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_mt_concurrent_json.svg");
      }

      // Multi-threaded Concurrent Echo Large (64KB)
      {
         bencher::stage stage;
         stage.name = "MT Concurrent Echo Large 64KB (" + std::to_string(client_threads) + " clients, Glaze " +
                      std::to_string(server_threads) + "T)";
         stage.throughput_units_label = "msg/s";
         stage.throughput_units_divisor = 1;

         stage.run("Glaze (" + std::to_string(server_threads) + "T)",
                   [&] { return run_ws_concurrent(glaze_mt_port, large_payload, client_threads, 1000); });
         stage.run("uWebSockets (1T)",
                   [&] { return run_ws_concurrent(uws_mt_port, large_payload, client_threads, 1000); });

         bencher::print_results(stage);
         all_markdown += bencher::to_markdown(stage);
         bencher::save_file(stage_bar_chart(stage), "ws_mt_concurrent_large.svg");
      }
   }

   bencher::save_file(all_markdown, "ws_benchmark.md");

   return 0;
}
