// Glaze Library
// For the license information refer to glaze.hpp

// Dummy target since interface targets dont show up in some ides

#include "glaze/glaze.hpp"

struct obj_t {
    double x{};
    float y{};
};

template <>
struct glz::meta<obj_t>
{
    using T = obj_t;
    static constexpr auto value = object("x", &T::x);
};

int main() {
    std::string buffer{};
    obj_t obj{};
    glz::write_json(obj, buffer);
    std::ignore = glz::read_json(obj, buffer);
    return 0;
}
