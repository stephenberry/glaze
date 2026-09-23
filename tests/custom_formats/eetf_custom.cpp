// Glaze Library
// For the license information refer to glaze.hpp

// Only this format's header - see custom_common.hpp for why.
#include "custom_common.hpp"
#include "glaze/eetf.hpp"

// EETF has no std::optional or std::variant support for any field, custom or not.
static const auto registered = custom_formats::make_suite<glz::eetf::eetf_opts{}, false>("eetf");
