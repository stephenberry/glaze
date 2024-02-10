#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <glaze/glaze.hpp>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct TestMsg
{
   uint64_t id;
   std::string val;
};

std::vector<uint8_t> serialize(const TestMsg& msg)
{
   std::vector<uint8_t> buf;
   glz::write_json(msg, buf);
   buf.push_back('\0');
   return buf;
}

std::optional<TestMsg> deserialize(std::vector<uint8_t>&& stream)
{
   TestMsg msg;
   auto err = glz::read_json(msg, stream);

   if (err) {
      return std::nullopt;
   }

   return msg;
}

int main()
{
   const uint32_t threadCount{8};

   std::array<std::thread, threadCount> threads{};
   for (uint_fast32_t i = 0; i < threadCount; i++) {
      threads[i] = std::thread([] {
         TestMsg msg{20, "five hundred"};
         for (uint64_t j = 0; j < 100'000; j++) {
            auto res = serialize(msg);
            auto msg2 = deserialize(std::move(res));
            if (msg2->id != msg.id || msg2->val != msg.val) {
               std::terminate();
            }
         }
      });
   }
   for (uint_fast32_t i = 0; i < threadCount; i++) {
      threads[i].join();
   }
}
