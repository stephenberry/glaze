#include <cassert>
#include <glaze/json.hpp>
#include <version> // Including this header triggers the undefined behavior, even though it's not otherwise used

int main(int argc, const char* argv[])
{
   (void)argc;
   (void)argv;
   auto has_value = glz::json_t{}.dump().has_value(); // cause SIGSEGV or  SIGABRT
   assert(has_value);
   return 0;
}
