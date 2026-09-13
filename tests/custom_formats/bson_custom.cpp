// Glaze Library
// For the license information refer to glaze.hpp

// Only this format's header - see custom_common.hpp for why.
#include "glaze/bson.hpp"

#include "custom_common.hpp"

static const auto registered = custom_formats::make_suite<glz::opts{.format = glz::BSON}>("bson");
