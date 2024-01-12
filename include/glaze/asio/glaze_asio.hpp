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
}
