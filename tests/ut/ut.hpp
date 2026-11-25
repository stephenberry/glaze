// Vendored into Glaze's tests from https://github.com/openalgz/ut v1.2.1 (MIT, see LICENSE in
// this directory). This copy is maintained here and is not installed.
//
// Refactored from:
// Copyright (c) 2024 Kris Jusiak (kris at jusiak dot net)
// Distributed under the Boost Software License, Version 1.0.
// (See http://www.boost.org/LICENSE_1_0.txt)
//
// UT: A simple C++23 unit testing library with compile-time and run-time support.
//
// Running specific tests:
//   Set the UT_RUN environment variable to run only specific tests by name.
//   Single test:    UT_RUN="my test" ./my_tests
//   Multiple tests: UT_RUN="[test1,test2,test3]" ./my_tests
//   If UT_RUN is not set, all tests run (default behavior).

#pragma once

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string_view>

namespace ut
{
   namespace detail
   {
      constexpr bool fatal = true;

      template <class>
      constexpr auto is_mutable_lambda_v = false;
      template <class R, class B, class... Ts>
      constexpr auto is_mutable_lambda_v<R (B::*)(Ts...)> = true;
      template <class R, class B, class... Ts>
      constexpr auto is_mutable_lambda_v<R (B::*)(Ts...) const> = false;
      template <class Fn>
      constexpr auto has_capture_lambda_v = sizeof(Fn) > 1ul;

      template <class T, class...>
      struct identity
      {
         using type = T;
      };
   }

   template <size_t Size>
   struct fixed_string
   {
      constexpr fixed_string(const char (&str)[Size])
      {
         for (size_t i = 0; i < Size; ++i) {
            storage[i] = str[i];
         }
      }
      [[nodiscard]] constexpr auto operator[](const auto i) const { return storage[i]; }
      [[nodiscard]] constexpr auto data() const { return storage; }
      [[nodiscard]] static constexpr auto size() { return Size - 1; }
      [[nodiscard]] constexpr operator std::string_view() const { return {storage, Size - 1}; }
      constexpr friend auto operator<<(auto& os, const fixed_string& fs) -> decltype(auto)
      {
         return os << std::string_view{fs.storage, fs.size()};
      }
      char storage[Size]{};
   };

   namespace events
   {
      enum class mode { run_time, compile_time };
      template <mode Mode>
      struct test_begin
      {
         std::string_view file_name{};
         uint_least32_t line{};
         std::string_view name{};
      };
      template <mode Mode>
      struct test_end
      {
         std::string_view file_name{};
         uint_least32_t line{};
         std::string_view name{};
         enum { FAILED, PASSED, COMPILE_TIME } result{};
      };
      struct assertion
      {
         bool passed{};
         std::string_view file_name{};
         uint_least32_t line{};
      };
      struct fatal
      {};
      template <class Msg>
      struct log
      {
         const Msg& msg;
         bool result{};
      };
      struct summary
      {
         enum { FAILED, PASSED, COMPILE_TIME };
         size_t asserts[2]{}; /* FAILED, PASSED */
         size_t tests[3]{}; /* FAILED, PASSED, COMPILE_TIME */
      };
   } // namespace events

   template <class OStream>
   struct outputter
   {
      template <events::mode Mode>
      constexpr auto on(const events::test_begin<Mode>&)
      {}
      constexpr auto on(const events::test_begin<events::mode::run_time>& event) { current_test = event; }
      template <events::mode Mode>
      constexpr auto on(const events::test_end<Mode>&)
      {}
      constexpr auto on(const events::assertion& event)
      {
         if (not event.passed && not std::is_constant_evaluated()) {
            if (initial_new_line == '\n') {
               os << initial_new_line;
            }
            else {
               initial_new_line = '\n';
            }
            os << "FAILED \"" << current_test.name << "\" ";
            const auto n = event.file_name.size();
            const auto start = n <= 32 ? 0 : n - 32;
            if (start > 0) {
               os << "...";
            }
            os << event.file_name.substr(start, n) << ":" << event.line << '\n';
         }
      }
      constexpr auto on(const events::fatal&) {}
      template <class Msg>
      constexpr auto on(const events::log<Msg>& event)
      {
         if (!std::is_constant_evaluated() && !event.result) {
            os << ' ' << event.msg;
         }
      }
      constexpr auto on(const events::summary& event)
      {
         using namespace events;
         if (!std::is_constant_evaluated()) {
            if (event.asserts[summary::FAILED] || event.tests[summary::FAILED]) {
               os << "\nFAILED\n";
            }
            else {
               os << "\nPASSED\n";
            }
            os << "tests: " << (event.tests[summary::PASSED] + event.tests[summary::FAILED]) << " ("
               << event.tests[summary::PASSED] << " passed, " << event.tests[summary::FAILED] << " failed, "
               << event.tests[summary::COMPILE_TIME] << " compile-time)\n"
               << "asserts: " << (event.asserts[summary::PASSED] + event.asserts[summary::FAILED]) << " ("
               << event.asserts[summary::PASSED] << " passed, " << event.asserts[summary::FAILED] << " failed)\n";
         }
      }

      OStream& os;
      events::test_begin<events::mode::run_time> current_test{};
      char initial_new_line{};
   };

   template <class Outputter, uint32_t MaxDepth = 16>
   struct reporter
   {
      constexpr auto on(const events::test_begin<events::mode::run_time>& event)
      {
         asserts_failed[current++] = summary.asserts[events::summary::FAILED];
         outputter.on(event);
      }
      constexpr auto on(const events::test_end<events::mode::run_time>& event)
      {
         const auto result = summary.asserts[events::summary::FAILED] == asserts_failed[--current];
         ++summary.tests[result];
         events::test_end<events::mode::run_time> te{event};
         te.result = static_cast<decltype(te.result)>(result);
         outputter.on(te);
      }
      constexpr auto on(const events::test_begin<events::mode::compile_time>&)
      {
         ++summary.tests[events::summary::COMPILE_TIME];
      }
      constexpr auto on(const events::test_end<events::mode::compile_time>&) {}
      constexpr auto on(const events::assertion& event)
      {
         if (event.passed) {
            ++summary.asserts[events::summary::PASSED];
         }
         else {
            ++summary.asserts[events::summary::FAILED];
         }
         outputter.on(event);
      }
      constexpr auto on(const events::fatal& event)
      {
         ++summary.tests[events::summary::FAILED];
         outputter.on(event);
         outputter.on(summary);
         std::exit(1);
      }

      ~reporter()
      { // non constexpr
         outputter.on(summary);
         if (summary.asserts[events::summary::FAILED]) {
            std::exit(1);
         }
      }

      Outputter& outputter;
      events::summary summary{};
      size_t asserts_failed[MaxDepth]{};
      size_t current{};
   };

   template <class Reporter>
   struct runner
   {
      template <class Test>
      constexpr auto on(Test test, const std::string_view file_name, uint_least32_t line, const std::string_view name)
         -> bool
      {
         if (std::is_constant_evaluated()) {
            if constexpr (requires { requires detail::is_mutable_lambda_v<decltype(&Test::operator())>; }) {
               return false;
            }
            else {
               test();
               return true;
            }
         }
         else {
            static const std::string_view filter = []() -> std::string_view {
               if (const char* env = std::getenv("UT_RUN")) return env;
               return {};
            }();

            auto matches_filter = [](std::string_view test_name, std::string_view f) {
               if (f.empty()) return true;

               // Array format: [test1,test2,test3]
               if (f.starts_with('[') && f.ends_with(']')) {
                  auto content = f.substr(1, f.size() - 2);
                  size_t pos = 0;
                  while (pos < content.size()) {
                     auto comma = content.find(',', pos);
                     auto token =
                        (comma == std::string_view::npos) ? content.substr(pos) : content.substr(pos, comma - pos);
                     if (token == test_name) return true;
                     if (comma == std::string_view::npos) break;
                     pos = comma + 1;
                  }
                  return false;
               }

               // Single test name
               return test_name == f;
            };

            if (!matches_filter(name, filter)) {
               return false;
            }

#if defined(UT_COMPILE_TIME)
            if constexpr (!requires { requires detail::is_mutable_lambda_v<decltype(&Test::operator())>; } &&
                          !detail::has_capture_lambda_v<Test>) {
               reporter.on(events::test_begin<events::mode::compile_time>{file_name, line, name});
               static_assert((test(), "[FAILED]"));
               reporter.on(events::test_end<events::mode::compile_time>{file_name, line, name});
            }
#endif

            reporter.on(events::test_begin<events::mode::run_time>{file_name, line, name});
            test();
            reporter.on(events::test_end<events::mode::run_time>{file_name, line, name});
         }
         return true;
      }

      Reporter& reporter;
   };
}

namespace ut
{
   inline struct
   {
      struct
      {
         friend constexpr decltype(auto) operator<<([[maybe_unused]] auto& os, [[maybe_unused]] const auto& t)
         {
            static_assert(requires { std::clog << t; });
            return (std::clog << t);
         }
      } stream;
      ut::outputter<decltype(stream)> outputter{stream};
      ut::reporter<decltype(outputter)> reporter{outputter};
      ut::runner<decltype(reporter)> runner{reporter};
   } cfg;

   constexpr struct
   {
      template <bool Fatal>
      struct eval final
      {
         template <class T>
            requires std::convertible_to<T, bool>
         constexpr eval(T&& test_passed, auto&& loc) : passed(static_cast<bool>(test_passed))
         {
            if (std::is_constant_evaluated()) {
               if (not passed) {
                  std::abort();
               }
            }
            else {
               cfg.reporter.on(events::assertion{passed, loc.file_name(), loc.line()});
               if (not passed) {
                  if constexpr (Fatal) {
                     cfg.reporter.on(events::fatal{});
                  }
               }
            }
         }
         bool passed{};
      };

      template <class T>
         requires std::convertible_to<T, bool>
      constexpr auto operator()(T&& test_passed,
                                const std::source_location& loc = std::source_location::current()) const
      {
         return log{eval<not detail::fatal>{test_passed, loc}.passed};
      }

#if __cpp_multidimensional_subscript >= 202211L && !defined(_MSC_VER)
      template <class T>
         requires std::convertible_to<T, bool>
      constexpr auto operator[](T&& test_passed,
                                const std::source_location& loc = std::source_location::current()) const
      {
         return log{eval<detail::fatal>{test_passed, loc}.passed};
      }
#else
      template <class T>
         requires std::convertible_to<T, bool>
      constexpr auto operator[](T&& test_passed) const
      {
         return log{eval<detail::fatal>{test_passed, std::source_location::current()}.passed};
      }
#endif

     private:
      struct log final
      {
         bool passed{};

         template <class Msg>
         constexpr const auto& operator<<(const Msg& msg) const
         {
            cfg.outputter.on(events::log<Msg>{msg, passed});
            return *this;
         }
      };
   } expect{};

   struct suite final
   {
      suite(auto&& tests) { tests(); }
   };

   namespace detail
   {
      template <fixed_string Name>
      struct test final
      {
         constexpr auto operator=(auto test) const
         {
            const auto& loc = std::source_location::current();
            return cfg.runner.on(test, loc.file_name(), loc.line(), Name);
         }
      };

      struct runtime_test final
      {
         std::string_view name{};

         constexpr auto operator=(auto test) const
         {
            const auto& loc = std::source_location::current();
            return cfg.runner.on(test, loc.file_name(), loc.line(), name);
         }
      };
   }

   constexpr auto test(const std::string_view name) { return detail::runtime_test{name}; }

   template <fixed_string Str>
   [[nodiscard]] constexpr auto operator""_test()
   {
      return detail::test<Str>{};
   }

#if __cpp_exceptions
   template <class Callable, class... Args>
   constexpr auto throws(Callable&& c, Args&&... args)
   {
      try {
         std::forward<Callable>(c)(std::forward<Args>(args)...);
      }
      catch (...) {
         return true;
      }
      return false;
   }
#endif
}

using ut::operator""_test;
