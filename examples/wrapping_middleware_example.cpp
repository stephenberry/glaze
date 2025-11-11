// Example: Wrapping Middleware
// This demonstrates wrapping middleware that can execute code before AND after
// handlers complete, enabling timing, logging, and response transformation.

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"

// Thread-safe metrics structure
struct ServerMetrics
{
   std::atomic<uint64_t> total_requests{0};
   std::atomic<uint64_t> total_responses{0};
   std::atomic<double> response_time_sum{0.0};
   std::atomic<uint64_t> status_2xx{0};
   std::atomic<uint64_t> status_4xx{0};
   std::atomic<uint64_t> status_5xx{0};

   void print_stats() const
   {
      auto total_req = total_requests.load();
      auto total_res = total_responses.load();
      auto time_sum = response_time_sum.load();

      std::cout << "\n=== Server Metrics ===\n";
      std::cout << "Total Requests:  " << total_req << "\n";
      std::cout << "Total Responses: " << total_res << "\n";
      std::cout << "Average Response Time: ";
      if (total_res > 0) {
         std::cout << (time_sum / total_res * 1000.0) << " ms\n";
      }
      else {
         std::cout << "N/A\n";
      }
      std::cout << "Status 2xx: " << status_2xx.load() << "\n";
      std::cout << "Status 4xx: " << status_4xx.load() << "\n";
      std::cout << "Status 5xx: " << status_5xx.load() << "\n";
      std::cout << "===================\n\n";
   }
};

int main()
{
   ServerMetrics metrics;

   glz::http_server<> server;

   // Wrapping Middleware #1: Logging middleware
   // Logs before and after each request
   server.wrap([](const glz::request& req, glz::response&, const auto& next) {
      std::cout << "→ Request: " << glz::to_string(req.method) << " " << req.target << "\n";
      next(); // Call the next middleware or handler
      std::cout << "← Response sent\n";
   });

   // Wrapping Middleware #2: Timing and metrics middleware
   // Measures request timing naturally by wrapping
   server.wrap([&metrics](const glz::request&, glz::response& res, const auto& next) {
      auto start = std::chrono::steady_clock::now();

      // Count incoming request
      metrics.total_requests.fetch_add(1, std::memory_order_relaxed);

      // Execute the rest of the middleware chain and handler
      next();

      // Now we have the response - measure timing and collect metrics
      auto end = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration<double>(end - start).count();

      metrics.response_time_sum.fetch_add(duration, std::memory_order_relaxed);
      metrics.total_responses.fetch_add(1, std::memory_order_relaxed);

      // Track status codes
      if (res.status_code >= 200 && res.status_code < 300) {
         metrics.status_2xx.fetch_add(1, std::memory_order_relaxed);
      }
      else if (res.status_code >= 400 && res.status_code < 500) {
         metrics.status_4xx.fetch_add(1, std::memory_order_relaxed);
      }
      else if (res.status_code >= 500) {
         metrics.status_5xx.fetch_add(1, std::memory_order_relaxed);
      }

      std::cout << "  ⏱  " << (duration * 1000.0) << " ms - Status " << res.status_code << "\n";
   });

   // Wrapping Middleware #3: Error handling middleware
   // Catches exceptions and converts them to 500 responses
   server.wrap([](const glz::request&, glz::response& res, const auto& next) {
      try {
         next();
      }
      catch (const std::exception& e) {
         std::cerr << "Error: " << e.what() << "\n";
         res.status(500).body("Internal Server Error");
      }
   });

   // Wrapping Middleware #4: Response transformation
   // Adds a custom header to all responses
   server.wrap([](const glz::request&, glz::response& res, const auto& next) {
      next();
      // After handler completes, we can modify the response
      res.header("X-Powered-By", "Glaze");
      res.header("X-Response-Time", std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
   });

   // Register some example routes
   server.get("/", [](const glz::request&, glz::response& res) { res.body("Hello, World!"); });

   server.get("/api/users", [](const glz::request&, glz::response& res) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      res.json({{"users", std::vector<std::string>{"alice", "bob", "charlie"}}});
   });

   server.get("/api/users/:id", [](const glz::request& req, glz::response& res) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      auto id = req.params.at("id");
      res.json({{"id", id}, {"name", "User " + id}});
   });

   server.get("/slow", [](const glz::request&, glz::response& res) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      res.body("This was slow");
   });

   server.get("/error", [](const glz::request&, glz::response&) {
      throw std::runtime_error("Simulated error");
      // The error handling middleware will catch this
   });

   server.get("/metrics", [&metrics](const glz::request&, glz::response& res) {
      res.json({{"total_requests", metrics.total_requests.load()},
                {"total_responses", metrics.total_responses.load()},
                {"avg_response_time_ms", metrics.total_responses.load() > 0 ? (metrics.response_time_sum.load() /
                                                                               metrics.total_responses.load() * 1000.0)
                                                                            : 0.0},
                {"status_2xx", metrics.status_2xx.load()},
                {"status_4xx", metrics.status_4xx.load()},
                {"status_5xx", metrics.status_5xx.load()}});
   });

   std::cout << "Wrapping Middleware Example\n";
   std::cout << "============================\n\n";
   std::cout << "This example demonstrates wrapping middleware that can execute\n";
   std::cout << "code both before and after handlers.\n\n";
   std::cout << "Middleware wraps the next() handler, allowing code execution:\n";
   std::cout << "  1. BEFORE the handler (request processing)\n";
   std::cout << "  2. AFTER the handler (response processing)\n\n";
   std::cout << "This enables:\n";
   std::cout << "  ✓ Natural timing measurement\n";
   std::cout << "  ✓ Response transformation\n";
   std::cout << "  ✓ Error handling around handlers\n";
   std::cout << "  ✓ Logging with full context\n";
   std::cout << "  ✓ Any cross-cutting concerns\n\n";

   std::cout << "Server starting on http://localhost:8080\n";
   std::cout << "Try these endpoints:\n";
   std::cout << "  GET /              - Home page\n";
   std::cout << "  GET /api/users     - List users (10ms processing)\n";
   std::cout << "  GET /api/users/123 - Get user (5ms processing)\n";
   std::cout << "  GET /slow          - Slow endpoint (100ms processing)\n";
   std::cout << "  GET /error         - Error endpoint (triggers error handler)\n";
   std::cout << "  GET /metrics       - View current metrics\n\n";
   std::cout << "Press Ctrl+C to stop the server\n\n";

   server.bind(8080).with_signals().start(4);

   server.wait_for_signal();

   // Print final metrics
   metrics.print_stats();

   return 0;
}
