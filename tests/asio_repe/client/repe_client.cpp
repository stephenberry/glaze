// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"

using namespace boost::ut;

#include "glaze/asio/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

struct looper_client
{
   asio::io_context ctx;
   asio::ip::tcp::socket socket{ctx};
   asio::ip::tcp::resolver resolver{ctx};

   std::string buffer{};

   void run()
   {
      asio::co_spawn(ctx, startup(), asio::detached);
      ctx.run();
   }

   asio::awaitable<void> startup()
   {
      using namespace glz;
      
      try {
         auto endpoints = co_await resolver.async_resolve("localhost", "1234", asio::use_awaitable);
         co_await asio::async_connect(socket, endpoints, asio::use_awaitable);
         std::cerr << "Connected to server.\n";

         std::vector<int> data{};

         int i = 0;

         while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            data.emplace_back(i);

            repe::request_json({"sum", uint64_t(i)}, data, buffer);
            co_await call_rpc(socket, buffer);

            std::cerr << buffer << '\n';
            
            repe::request_json({"max"}, data, buffer);
            co_await call_rpc(socket, buffer);

            std::cerr << buffer << '\n';

            ++i;
         }
      }
      catch (std::exception& e) {
         std::cerr << "Exception: " << e.what() << '\n';
      }
   }
};

void looper_test()
{
   looper_client client{};
   client.run();
}

int main()
{
   looper_test();

   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
