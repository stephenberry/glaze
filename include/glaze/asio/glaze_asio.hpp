// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <asio.hpp>
#include <coroutine>

namespace glz
{
   inline asio::awaitable<void> send_buffer(asio::ip::tcp::socket& socket, const std::string_view str)
   {
      const uint64_t size = str.size();
      co_await asio::async_write(socket, asio::buffer(&size, sizeof(uint64_t)), asio::use_awaitable);
      co_await asio::async_write(socket, asio::buffer(str), asio::use_awaitable);
   }

   inline asio::awaitable<void> receive_buffer(asio::ip::tcp::socket& socket, std::string& str)
   {
      uint64_t size;
      co_await asio::async_read(socket, asio::buffer(&size, sizeof(size)), asio::use_awaitable);
      str.resize(size);
      co_await asio::async_read(socket, asio::buffer(str), asio::use_awaitable);
   }
   
   template <class Server>
   struct asio_server
   {
      asio::io_context ctx{1}; // number of background threads

      asio::signal_set signals{ctx, SIGINT, SIGTERM};
      
      std::function<void(Server&)> init{};
      
      void run() {
         if (init) {
            signals.async_wait([&](auto, auto){ ctx.stop(); });
            
            asio::co_spawn(ctx, listener(), asio::detached);
            
            ctx.run();
         }
         else {
            std::cerr << "init was never supplied\n";
         }
      }
      
      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         Server server{};
         std::string buffer{};
         
        try
        {
           init(server);
           
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
