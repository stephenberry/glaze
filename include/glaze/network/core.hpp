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
#include <signal.h>
#include <ctime>
#include <time.h>
#endif

#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif

#if defined(__APPLE__)
#include <sys/event.h> // for kqueue on macOS
#elif defined(__linux__)
#include <sys/epoll.h> // for epoll on Linux
#include <sys/eventfd.h>
#endif

namespace glz::net
{
#ifdef _WIN32
   using file_handle_t = unsigned int;
   constexpr unsigned int invalid_file_handle = 0;
   using ssize_t = int64_t;
#else
   using file_handle_t = int;
   constexpr int invalid_file_handle = -1;
#endif
   
#if defined(__APPLE__)
   using poll_event_t = struct kevent;
#elif defined(__linux__)
   using poll_event_t = struct epoll_event;
#elif defined(_WIN32)
#endif
   
#if defined(__APPLE__)
   constexpr auto poll_in = EVFILT_READ;
   constexpr auto poll_out = EVFILT_WRITE;
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
   
   inline bool poll_error([[maybe_unused]] uint32_t events) {
#if defined(__APPLE__)
      return true;
#elif defined(__linux__)
      return events & EPOLLERR;
#elif defined(_WIN32)
      return true;
#endif
   }
   
   inline bool event_closed([[maybe_unused]] uint32_t events) {
#if defined(__APPLE__)
      return true;
#elif defined(__linux__)
      return events & EPOLLRDHUP || events & EPOLLHUP;
#elif defined(_WIN32)
      return true;
#endif
   }
}
