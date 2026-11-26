#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "glaze/base64/base64.hpp"
#include "glaze/net/websocket_connection.hpp"
#include "ut/ut.hpp"

using namespace ut;

// RFC 6455 4.1 asks for a 16-byte nonce, so Sec-WebSocket-Key spans 2^128 values. A
// generator that derives the nonce from one 32-bit seed spans 2^32 instead, and two
// connections then share a nonce after about 2^16 of them. 200,000 draws is past that
// bound by a wide margin, so a repeat here is a collapsed key space rather than bad luck:
// 2^128 makes a repeat in 200,000 draws a 1-in-10^28 event.
suite websocket_entropy_tests = [] {
   "handshake_nonce_is_sixteen_bytes"_test = [] {
      const std::string key = glz::ws_util::generate_websocket_key();
      expect(key.size() == 24) << "a base64 encoded 16-byte nonce is 24 characters";
      expect(glz::read_base64(key).size() == 16);
   };

   "handshake_nonces_do_not_repeat"_test = [] {
      constexpr size_t draws = 200'000;
      std::unordered_set<std::string> seen;
      seen.reserve(draws * 2);
      for (size_t i = 0; i < draws; ++i) {
         expect(seen.insert(glz::ws_util::generate_websocket_key()).second)
            << "two connections were handed the same Sec-WebSocket-Key after " << i << " draws";
      }
   };

   "masking_keys_vary_per_frame"_test = [] {
      constexpr size_t draws = 10'000;
      std::unordered_set<uint32_t> seen;
      std::array<uint8_t, 4> last{};
      bool all_equal = true;
      for (size_t i = 0; i < draws; ++i) {
         std::array<uint8_t, 4> mask{};
         glz::ws_util::fill_random_bytes(mask.data(), mask.size());
         if (i != 0 && mask != last) {
            all_equal = false;
         }
         last = mask;
         seen.insert(uint32_t(mask[0]) << 24 | uint32_t(mask[1]) << 16 | uint32_t(mask[2]) << 8 | mask[3]);
      }
      expect(!all_equal) << "every frame was masked with the same key";
      // A 32-bit key over 10,000 draws expects ~0.01 birthday collisions, so anything
      // beyond a handful means the key is not spanning its range.
      expect(seen.size() > draws - 8) << "masking keys repeated far more often than a 32-bit key should";
   };
};

int main() {}
