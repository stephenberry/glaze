// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../Export.hpp"
#ifdef CPP_MODULES
export module glaze.toml;
export import glaze.toml.write;
#else
#include "glaze/toml/write.cppm"
#endif



