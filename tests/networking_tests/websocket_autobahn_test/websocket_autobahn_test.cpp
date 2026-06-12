#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include "glaze/net/websocket_client.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;
using namespace std::chrono_literals;

constexpr std::string_view autobahn_url = "ws://127.0.0.1:9001";
constexpr std::string_view glaze_agent = "glaze-websocket-client";
constexpr size_t autobahn_max_message_size = 64ZU * 1024ZU * 1024ZU;
constexpr auto autobahn_count_timeout = 5s;
constexpr auto autobahn_startup_timeout = 30s;
constexpr auto autobahn_case_timeout = 1min;
constexpr auto autobahn_report_timeout = 10s;

template <typename Predicate>
bool wait_for_condition(Predicate pred, std::chrono::milliseconds timeout)
{
   auto start = std::chrono::steady_clock::now();
   while (!pred()) {
      if (std::chrono::steady_clock::now() - start > timeout) {
         return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   return true;
}

std::optional<uint32_t> parse_u32(std::string_view text)
{
   uint32_t value{};
   const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
   if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
   return value;
}

bool write_case_count(uint32_t case_count)
{
   const char* path = std::getenv("AUTOBAHN_CASE_COUNT_FILE");
   if (path == nullptr || *path == '\0') return true;

   std::ofstream output{path, std::ios::trunc};
   if (!output) {
      std::cerr << "[autobahn] Could not open case count file: " << path << "\n";
      return false;
   }

   output << case_count << '\n';
   if (!output) {
      std::cerr << "[autobahn] Could not write case count file: " << path << "\n";
      return false;
   }

   return true;
}

enum class single_message_status : uint8_t {
   ok,
   timeout,
   error,
   closed_without_message,
   non_text_message,
};

struct single_message_result
{
   single_message_status status{};
   std::string message{};
   std::error_code error{};
};

std::string_view status_name(single_message_status status)
{
   switch (status) {
   case single_message_status::ok:
      return "ok";
   case single_message_status::timeout:
      return "timeout";
   case single_message_status::error:
      return "error";
   case single_message_status::closed_without_message:
      return "closed_without_message";
   case single_message_status::non_text_message:
      return "non_text_message";
   }
   return "unknown";
}

single_message_result read_one_text_message(std::string_view url, std::chrono::milliseconds timeout)
{
   auto ctx = std::make_shared<asio::io_context>();
   websocket_client client{ctx};
   client.set_max_message_size(autobahn_max_message_size);

   std::atomic<bool> finished{false};
   std::mutex mu;
   single_message_result result{single_message_status::closed_without_message};

   client.on_message([&](std::string_view message, ws_opcode opcode) {
      {
         std::scoped_lock lock(mu);
         if (opcode == ws_opcode::text) {
            result.status = single_message_status::ok;
            result.message.assign(message);
         }
         else {
            result.status = single_message_status::non_text_message;
         }
      }
      finished = true;
      client.close();
      ctx->stop();
   });

   client.on_close([&](ws_close_code, std::string_view) {
      finished = true;
      ctx->stop();
   });

   client.on_error([&](std::error_code ec) {
      {
         std::scoped_lock lock(mu);
         result.status = single_message_status::error;
         result.error = ec;
      }
      finished = true;
      ctx->stop();
   });

   client.connect(url);
   std::jthread client_thread([&] { ctx->run(); });

   if (!wait_for_condition([&] { return finished.load(); }, timeout)) {
      {
         std::scoped_lock lock(mu);
         result.status = single_message_status::timeout;
      }
   }

   ctx->stop();
   client_thread.join();

   return result;
}

single_message_result read_case_count()
{
   const auto url = std::format("{}/getCaseCount", autobahn_url);
   const auto deadline = std::chrono::steady_clock::now() + autobahn_startup_timeout;
   single_message_result result{single_message_status::timeout};

   do {
      result = read_one_text_message(url, autobahn_count_timeout);
      if (result.status == single_message_status::ok) {
         return result;
      }
      std::this_thread::sleep_for(500ms);
   } while (std::chrono::steady_clock::now() < deadline);

   return result;
}

enum class case_status : uint8_t {
   ok,
   timeout,
   error_before_open,
   error_after_open,
};

struct case_result
{
   case_status status{};
   std::error_code error{};
   uint64_t echoed_messages{};
};

case_result run_case(uint32_t case_id, std::chrono::milliseconds timeout)
{
   auto ctx = std::make_shared<asio::io_context>();
   websocket_client client{ctx};
   client.set_max_message_size(autobahn_max_message_size);

   std::atomic<bool> opened{false};
   std::atomic<bool> finished{false};
   std::atomic<uint64_t> echoed_messages{0};
   std::mutex mu;
   case_result result{case_status::ok};

   client.on_open([&] { opened = true; });

   client.on_message([&](std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         std::string payload{message};
         client.send(payload);
         ++echoed_messages;
      }
      else if (opcode == ws_opcode::binary) {
         std::string payload{message};
         client.send_binary(payload);
         ++echoed_messages;
      }
   });

   client.on_close([&](ws_close_code, std::string_view) {
      finished = true;
      ctx->stop();
   });

   client.on_error([&](std::error_code ec) {
      {
         std::scoped_lock lock(mu);
         result.status = opened.load() ? case_status::error_after_open : case_status::error_before_open;
         result.error = ec;
      }
      finished = true;
      ctx->stop();
   });

   const auto url = std::format("{}/runCase?case={}&agent={}", autobahn_url, case_id, glaze_agent);
   client.connect(url);

   std::jthread client_thread([&] { ctx->run(); });

   if (!wait_for_condition([&] { return finished.load(); }, timeout)) {
      {
         std::scoped_lock lock(mu);
         result.status = case_status::timeout;
      }
   }

   ctx->stop();
   client_thread.join();

   result.echoed_messages = echoed_messages.load();
   return result;
}

bool update_reports(std::chrono::milliseconds timeout)
{
   auto ctx = std::make_shared<asio::io_context>();
   websocket_client client{ctx};

   std::atomic<bool> opened{false};
   std::atomic<bool> finished{false};
   std::atomic<bool> failed{false};

   client.on_open([&] { opened = true; });
   client.on_close([&](ws_close_code, std::string_view) {
      finished = true;
      ctx->stop();
   });
   client.on_error([&](std::error_code) {
      failed = !opened.load();
      finished = true;
      ctx->stop();
   });

   const auto url = std::format("{}/updateReports?agent={}", autobahn_url, glaze_agent);
   client.connect(url);

   std::jthread client_thread([&] { ctx->run(); });

   const bool completed = wait_for_condition([&] { return finished.load(); }, timeout);
   if (!completed) {
      failed = true;
      ctx->stop();
   }

   ctx->stop();

   return completed && !failed.load();
}

suite websocket_autobahn_tests = [] {
   "autobahn_fuzzingclient"_test = [] {
      const auto count_message = read_case_count();
      if (count_message.status != single_message_status::ok) {
         std::cerr << "[autobahn] Could not read /getCaseCount from " << autobahn_url
                   << " (status=" << status_name(count_message.status);
         if (count_message.error) {
            std::cerr << ", error=" << count_message.error.message();
         }
         std::cerr << ")\n";
         expect(false) << "Autobahn fuzzingserver is not available";
         return;
      }

      const auto case_count = parse_u32(count_message.message);
      expect(case_count.has_value()) << "Autobahn /getCaseCount returned a non-integer payload";
      if (!case_count) return;

      const bool case_count_written = write_case_count(*case_count);
      expect(case_count_written) << "Could not persist the Autobahn case count";
      if (!case_count_written) return;

      std::cout << "[autobahn] Running " << *case_count << " cases as " << glaze_agent << "\n";

      uint32_t timeouts = 0;
      uint32_t failed_opens = 0;

      for (uint32_t case_id = 1; case_id <= *case_count; ++case_id) {
         const auto result = run_case(case_id, autobahn_case_timeout);
         if (result.status == case_status::timeout) {
            ++timeouts;
            std::cerr << "[autobahn] Case " << case_id << " timed out after echoing " << result.echoed_messages
                      << " messages\n";
         }
         else if (result.status == case_status::error_before_open) {
            ++failed_opens;
            std::cerr << "[autobahn] Case " << case_id << " failed before open: " << result.error.message() << "\n";
         }

         std::cout << "[autobahn] Completed " << case_id << "/" << *case_count << " cases\n";
      }

      const bool reports_updated = update_reports(autobahn_report_timeout);

      expect(timeouts == 0) << "Autobahn run had " << timeouts << " timed-out cases";
      expect(failed_opens == 0) << "Autobahn run had " << failed_opens << " cases that failed before opening";
      expect(reports_updated) << "Autobahn /updateReports failed";
   };
};

int main() {}
