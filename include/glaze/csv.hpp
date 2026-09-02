// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/as_array_wrapper.hpp"
// CSV cannot serialize a glz::custom field - its columnar layout requires every struct field to be
// a container of row values, not a single value. This is included anyway so that attempting it
// fails inside the CSV writer, which names the real constraint, rather than on an undefined
// from/to specialization that reads like a missing include.
#include "glaze/core/custom.hpp"
#include "glaze/core/wrapper_traits.hpp"
#include "glaze/csv/read.hpp"
#include "glaze/csv/skip.hpp"
#include "glaze/csv/write.hpp"
#include "glaze/thread/atomic.hpp"
