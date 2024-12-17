#include <version> // Including this header triggers the undefined behavior, even though it's not otherwise used
#include <glaze/json.hpp>
#include <cassert>


int main(int argc, char const *argv[]) {
   (void)argc;
   (void)argv;
    auto has_value =  glz::json_t{}.dump().has_value(); // cause SIGSEGV or  SIGABRT
    assert(has_value);
    return 0;
}
