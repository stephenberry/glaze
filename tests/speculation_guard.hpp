// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "glaze/core/context.hpp"
#include "glaze/core/reflect.hpp"
#include "ut/ut.hpp"

// Variant resolution is speculative: an alternative is parsed to find out whether it fits, and a
// rejected one is rewound and the next tried. Nest that and the re-parses multiply, so the cost of
// an ambiguous nest that nothing matches doubles or more per level until the speculation budget
// binds, and after that stops growing with depth. That flatness is what these guards time.
//
// An absolute bound timed the build instead. A capped read is ~5-10 ms in Release but seconds under
// a sanitizer on a slow runner, so a bound loose enough for every CI job would not notice a
// constant factor regression anyway. The ratio between two capped depths holds on any build, and a
// reversion -- the budget not charged on some path, or a level that keeps retrying after it is
// spent -- grows with the extra depth, exponentially in the worst case.
//
// `build(levels)` returns a nest `levels` deep that nothing matches at the bottom, and
// `read(buffer, ctx)` reads it through glz::read, which seeds the budget. Every read must fail with
// `expected`. The `exhaustive` depth must resolve without spending the budget; `shallow` and `deep`
// must both be stopped by it, and `deep` must not cost more than a few times `shallow`.
namespace glz_test
{
   template <class Build, class Read>
   void expect_bounded_by_speculation_budget(Build&& build, Read&& read, const glz::error_code expected,
                                             const size_t exhaustive, const size_t shallow, const size_t deep)
   {
      struct outcome
      {
         double ms{};
         bool exhausted{};
      };
      const auto resolve = [&](const size_t levels) {
         const std::string buffer = build(levels);
         glz::context ctx{};
         const auto start = std::chrono::steady_clock::now();
         const auto ec = read(buffer, ctx);
         const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
         ut::expect(ec == expected) << "levels=" << levels << ": " << glz::format_error(ec);
         return outcome{ms, glz::speculation_exhausted(ctx)};
      };

      ut::expect(not resolve(exhaustive).exhausted) << "levels=" << exhaustive << " should resolve within the budget";
      const auto s = resolve(shallow);
      const auto d = resolve(deep);
      ut::expect(s.exhausted && d.exhausted) << "levels=" << shallow << " and " << deep << " should spend the budget";
      // The 50 ms floor keeps timer and allocator noise in fast builds from reading as growth.
      ut::expect(d.ms < 4.0 * s.ms + 50.0)
         << shallow << " levels took " << s.ms << " ms but " << deep << " levels took " << d.ms << " ms";
   }
}
