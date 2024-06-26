// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/coroutine.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite generator = [] {
   std::atomic<uint64_t> result{};
   auto task = [&](uint64_t count_to) -> glz::task<void>
    {
        // Create a generator function that will yield and incrementing
        // number each time its called.
        auto gen = []() -> glz::generator<uint64_t>
        {
            uint64_t i = 0;
            while (true)
            {
                co_yield i;
                ++i;
            }
        };

        // Generate the next number until its greater than count to.
        for (auto val : gen())
        {
            std::cout << val << ", ";
           result += val;

            if (val >= count_to)
            {
                break;
            }
        }
        co_return;
    };

   glz::sync_wait(task(100));
   
   expect(result == 5050) << result;
};

int main() { return 0; }
