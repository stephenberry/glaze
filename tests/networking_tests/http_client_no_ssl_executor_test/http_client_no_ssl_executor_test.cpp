// Glaze Library
// For the license information refer to glaze.hpp

#ifdef GLZ_ENABLE_SSL
#undef GLZ_ENABLE_SSL
#endif

#include "glaze/net/http_client.hpp"

#include <chrono>
#include <future>
#include <thread>

#include "ut/ut.hpp"

using namespace ut;

suite http_client_no_ssl_executor_tests = [] {
   "external_executor_async_https_reports_not_supported"_test = [] {
      asio::io_context io;
      glz::http_client client{io.get_executor()};

      auto promise = std::make_shared<std::promise<std::expected<glz::response, std::error_code>>>();
      auto future = promise->get_future();

      client.get_async(
         "https://example.com", {},
         [promise](std::expected<glz::response, std::error_code> result) { promise->set_value(std::move(result)); });

      std::thread io_thread([&io]() { io.run(); });

      auto status = future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "HTTPS async callback should complete";
      if (status == std::future_status::ready) {
         auto result = future.get();
         expect(!result.has_value());
         if (!result.has_value()) {
            expect(result.error() == glz::ssl_error::ssl_not_supported)
               << "HTTPS without SSL should return ssl_not_supported";
         }
      }

      io.stop();
      io_thread.join();
   };

   "external_executor_stream_https_reports_not_supported"_test = [] {
      asio::io_context io;
      glz::http_client client{io.get_executor()};

      auto promise = std::make_shared<std::promise<std::error_code>>();
      auto future = promise->get_future();

      glz::stream_request_params params;
      params.url = "https://example.com";
      params.on_data = [](std::string_view) {};
      params.on_error = [promise](std::error_code ec) { promise->set_value(ec); };

      auto connection = client.stream_request(params);
      expect(connection == nullptr) << "Stream request should fail immediately without SSL support";

      std::thread io_thread([&io]() { io.run(); });

      auto status = future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "HTTPS stream error callback should complete";
      if (status == std::future_status::ready) {
         const auto ec = future.get();
         expect(ec == glz::ssl_error::ssl_not_supported) << "Stream HTTPS should report ssl_not_supported";
      }

      io.stop();
      io_thread.join();
   };
};

int main() {}
