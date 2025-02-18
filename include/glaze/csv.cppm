// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../Export.hpp"
#ifdef CPP_MODULES
export module glaze.csv;
export import glaze.csv.read;
export import glaze.csv.write;
export import glaze.thread.atomic;
#else
#include "glaze/csv/read.cppm"
#include "glaze/csv/write.cppm"
#include "glaze/thread/atomic.cppm"
#endif



