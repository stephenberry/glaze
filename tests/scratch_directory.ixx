// Glaze Library
// For the license information refer to glaze.ixx

module;

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

export module glaze.tests.scratch_directory;

import std;

// Several suites exercise the file APIs by writing scratch files through relative paths, and some of
// those paths deliberately reach upward ("../file.json") to prove that an include resolves relative
// to the document that names it. A relative path resolves against the process working directory, so
// where those files land is decided by whoever launched the binary, not by the test: under ctest the
// working directory is set per test, but running the binary directly writes them into -- and one
// level above -- whatever directory the shell happened to be in. Nothing fails when that happens.
// The tests pass and the stray files are noticed later, somewhere else, by someone else.
//
// Declaring one of these ahead of the first suite moves the process into a private directory under
// the system temporary directory, so every relative path in the file resolves inside it however the
// binary was started. The directory is nested one level deep so that "../" stays inside it too, and
// it carries the process id so that two binaries built from the same source (json_test and
// json_test_non_null) cannot collide under `ctest -j`.
//
// It is a namespace-scope object rather than something main() does because ut runs a suite from its
// constructor, during static initialization, long before main() is entered. Declaration order within
// a translation unit is what sequences this ahead of the suites, so it belongs above the first one.
//
// The tree is removed on normal exit. A test that crashes or aborts leaves it behind, which is the
// case where the files are worth looking at.
namespace glz_test
{
   inline int process_id()
   {
#if defined(_WIN32)
      return ::_getpid();
#else
      return static_cast<int>(::getpid());
#endif
   }

   export struct scratch_directory final
   {
      explicit scratch_directory(const std::string_view test_name)
      {
         std::error_code ec{};
         const auto base = std::filesystem::temp_directory_path(ec);
         if (ec) {
            return; // no temp directory to move into; leave the working directory alone
         }

         previous = std::filesystem::current_path(ec);
         if (ec) {
            previous.clear();
            return;
         }

         root = base / ("glaze_" + std::string{test_name} + "." + std::to_string(process_id()));

         // A recycled process id can leave the previous run's tree in place, and these tests assert
         // on files they expect to have created themselves.
         std::filesystem::remove_all(root, ec);

         const auto cwd = root / "cwd";
         std::filesystem::create_directories(cwd, ec);
         if (ec) {
            root.clear();
            previous.clear();
            return;
         }

         std::filesystem::current_path(cwd, ec);
         if (ec) {
            std::filesystem::remove_all(root, ec);
            root.clear();
            previous.clear();
         }
      }

      ~scratch_directory()
      {
         if (root.empty()) {
            return;
         }
         std::error_code ec{};
         // Step out before removing: Windows refuses to remove the working directory.
         std::filesystem::current_path(previous, ec);
         std::filesystem::remove_all(root, ec);
      }

      scratch_directory(const scratch_directory&) = delete;
      scratch_directory& operator=(const scratch_directory&) = delete;

      std::filesystem::path root{};
      std::filesystem::path previous{};
   };
}
