// Glaze Library
// For the license information refer to glaze.hpp

// Only this format's header - see custom_common.hpp for why. It must precede custom_common.hpp,
// which includes no glaze header of its own, so keep clang-format from reordering them.
// clang-format off
#include "glaze/eetf.hpp"
#include "custom_common.hpp"
// clang-format on

// EETF has no std::optional or std::variant support for any field, custom or not.
static const auto registered = custom_formats::make_suite<glz::eetf::eetf_opts{}, false>("eetf");
