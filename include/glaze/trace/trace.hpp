// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"

namespace glz::trace
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
   
   struct event
   {
      std::string_view name{}; // The name of the event, as displayed in Trace Viewer
      std::string_view cat{}; // The event categories. A comma separated list of categories for the event.
      char ph{}; // The event type.
      uint64_t ts{}; // The tracing clock timestamp of the event. Provided at microsecond granularity.
      std::optional<uint64_t> tts{}; // The thread clock timestamp of the event. Provided at microsecond granularity.
      uint64_t pid{}; // The process ID for the process that output this event.
      uint64_t tid{}; // The thread ID for the thread that output this event.
      // ...args
      std::optional<std::string_view> cname{}; // A fixed color name to associate with the event.
   };
   
   struct profile
   {
      
   };
}
