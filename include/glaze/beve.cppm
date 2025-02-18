// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../Export.hpp"
#ifdef CPP_MODULES
export module glaze.beve;
export import glaze.beve.header;
export import glaze.beve.ptr;
export import glaze.beve.read;
export import glaze.beve.wrappers;
export import glaze.beve.write;
export import glaze.thread.atomic;
#else
#include "glaze/beve/header.cppm"
#include "glaze/beve/ptr.cppm"
#include "glaze/beve/read.cppm"
#include "glaze/beve/wrappers.cppm"
#include "glaze/beve/write.cppm"
#include "glaze/thread/atomic.cppm"
#endif



