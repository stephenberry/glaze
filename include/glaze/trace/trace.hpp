// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"

#include <chrono>

namespace glz
{
   // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview
   
   constexpr char B = 'B'; // duration event begin
   constexpr char E = 'E'; // duration event end
   constexpr char X = 'X'; // complete event
   constexpr char i = 'i'; // instant event
   constexpr char C = 'C'; // counter event
   constexpr char b = 'b'; // async event begin
   constexpr char n = 'n'; // async event nestable instant
   constexpr char e = 'e'; // async event end
   // ...
   
   enum struct time_unit : uint32_t
   {
      s, // seconds
      ms, // milliseconds
      us, // microseconds
      ns // nanoseconds
   };
   
   template <>
   struct meta<time_unit>
   {
      static constexpr sv name = "glz::time_unit";
      using enum time_unit;
      static constexpr auto value =
      enumerate_no_reflect("s", s, //
                           "ms", ms, //
                           "us", us, //
                           "ns", ns);
   };
   
   struct event
   {
      std::string_view name{}; // The name of the event, as displayed in Trace Viewer
      std::optional<std::string_view> cat{}; // The event categories. A comma separated list of categories for the event.
      char ph{}; // The event type.
      uint64_t ts{}; // The tracing clock timestamp of the event. Provided at microsecond granularity.
      std::optional<uint64_t> tts{}; // The thread clock timestamp of the event. Provided at microsecond granularity.
      uint64_t pid{}; // The process ID for the process that output this event.
      uint64_t tid{}; // The thread ID for the thread that output this event.
      std::optional<uint64_t> id{}; // For async events. Events with the same category and id are treated as from the same event tree.
      glz::raw_json args = "{}"; // metadata
      std::optional<std::string_view> cname{}; // A fixed color name to associate with the event.
   };
   
   struct trace
   {
      std::deque<event> traceEvents{};
      time_unit displayTimeUnit = time_unit::ms;
      
      std::optional<std::chrono::time_point<std::chrono::steady_clock>> t0{}; // the time of the first event
      
      bool disabled = false;
      std::mutex mtx{};
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void begin(const std::string_view name, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         duration(name, 'B', std::forward<Args>(args)...);
      }
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void end(const std::string_view name, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         duration(name, 'E', std::forward<Args>(args)...);
      }
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void duration(const std::string_view name, const char phase, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         
         auto tnow = std::chrono::steady_clock::now();
         if (!t0) {
            t0 = tnow;
         }
         glz::event* event{};
         {
            std::unique_lock lock{mtx};
            event = &traceEvents.emplace_back();
         }
         event->name = name;
         event->ph = phase;
         event->ts = std::chrono::duration_cast<std::chrono::microseconds>(tnow - t0.value()).count();
         event->tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
         if constexpr (sizeof...(Args) > 0) {
            glz::write_json(std::forward<Args>(args)..., event->args.str);
         }
      }
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void async_begin(const std::string_view name, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         async(name, 'b', std::forward<Args>(args)...);
      }
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void async_end(const std::string_view name, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         async(name, 'e', std::forward<Args>(args)...);
      }
      
      template <class... Args>
         requires (sizeof...(Args) == 0 || sizeof...(Args) == 1)
      void async(const std::string_view name, const char phase, Args&&... args) noexcept {
         if (disabled) {
            return;
         }
         
         auto tnow = std::chrono::steady_clock::now();
         if (!t0) {
            t0 = tnow;
         }
         glz::event* event{};
         {
            std::unique_lock lock{mtx};
            event = &traceEvents.emplace_back();
         }
         event->name = name;
         event->ph = phase;
         event->ts = std::chrono::duration_cast<std::chrono::microseconds>(tnow - t0.value()).count();
         event->tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
         event->id = std::hash<std::string_view>{}(name);
         if constexpr (sizeof...(Args) > 0) {
            glz::write_json(std::forward<Args>(args)..., event->args.str);
         }
      }
   };
   
   template <>
   struct meta<trace>
   {
      using T = trace;
      static constexpr auto value = object(&T::traceEvents, &T::displayTimeUnit);
   };
}
