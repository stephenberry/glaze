// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif

#if defined(__APPLE__)
#include <sys/event.h> // for kqueue on macOS
#elif defined(__linux__)
#include <sys/epoll.h> // for epoll on Linux
#endif

namespace glz::net
{
#ifdef _WIN32
   using file_handle_t = unsigned int;
   using ssize_t = int64_t;
#else
   using file_handle_t = int;
#endif
   
#if defined(__APPLE__)
   using poll_event_t = struct kevent;
#elif defined(__linux__)
   using poll_event_t = struct epoll_event;
#elif defined(_WIN32)
#endif
   
#if defined(__APPLE__)
   constexpr auto poll_in = 0b00000001;
   constexpr auto poll_out = 0b00000010;
#elif defined(__linux__)
   constexpr auto poll_in = EPOLLIN;
   constexpr auto poll_out = EPOLLOUT;
#elif defined(_WIN32)
#endif

   // TODO: implement
   int event_write(auto, auto) { return 0; }
   
   inline auto close_socket(file_handle_t fd) {
#ifdef _WIN32
      ::closesocket(fd);
#else
      ::close(fd);
#endif
   }
   
   inline auto event_close(file_handle_t fd) {
#ifdef _WIN32
      WSACloseEvent(fd);
#else
      ::close(fd);
#endif
   }
   
   inline file_handle_t create_event_poll() {
#if defined(__APPLE__)
         return ::kqueue();
#elif defined(__linux__)
         return ::epoll_create1(EPOLL_CLOEXEC);
#elif defined(_WIN32)
         return WSACreateEvent();
#endif
   }
   
   inline file_handle_t create_shutdown_handle() {
#if defined(__APPLE__)
      return 0;
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return 0;
#endif
   }
   
   inline file_handle_t create_timer_handle() {
#if defined(__APPLE__)
      return 0;
#elif defined(__linux__)
      return ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
#elif defined(_WIN32)
      return 0;
#endif
   }
   
   inline file_handle_t create_schedule_handle() {
#if defined(__APPLE__)
      return 0;
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return 0;
#endif
   }
}
