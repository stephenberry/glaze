#include <glaze/json.hpp>

[[maybe_unused]] static bool unused_function() {
    return glz::json_t{}.dump().has_value();
}
