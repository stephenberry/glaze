// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"

using namespace boost::ut;

#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

#include "glaze/asio/glaze_asio.hpp"

namespace glz
{
   inline void init_server(glz::repe::server<>& server)
   {
      server.on("sum", [](std::vector<int>& vec, int& sum){
         sum = std::reduce(vec.begin(), vec.end());
      });
   }
   
   template <class Server>
   struct asio_server
   {
      asio::io_context ctx{2}; // number of background threads

      asio::signal_set signals{ctx, SIGINT, SIGTERM};
      
      void run() {
         signals.async_wait([&](auto, auto){ ctx.stop(); });
         
         asio::co_spawn(ctx, listener(), asio::detached);
         
         ctx.run();
      }
      
      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         Server server{};
         init_server(server);
         
         std::string buffer{};
         
        try
        {
          while (true)
          {
             co_await receive_buffer(socket, buffer);
             server.call(buffer);
             co_await send_buffer(socket, server.response);
          }
        }
        catch (std::exception& e)
        {
          std::printf("echo Exception: %s\n", e.what());
        }
      }

      asio::awaitable<void> listener()
      {
         auto executor = co_await asio::this_coro::executor;
         asio::ip::tcp::acceptor acceptor(executor, {asio::ip::tcp::v4(), 1234});
         while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(executor, run_instance(std::move(socket)), asio::detached);
        }
      }
   };
}


void tester()
{
   std::cerr << "Server active...\n";
   
  try
   {
      glz::asio_server<glz::repe::server<>> server{};
      server.run();
  }
  catch (std::exception& e)
  {
     std::cerr << "Exception: " << e.what();
  }
   
   std::cerr << "Server closed...\n";
}

int main()
{
   tester();
   
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
