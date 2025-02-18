// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../Export.hpp"
#ifdef CPP_MODULES
export module glaze.json;
export import glaze.json.escape_unicode;
export import glaze.json.invoke;
export import glaze.json.jmespath;
export import glaze.json.json_concepts;
export import glaze.json.json_ptr;
export import glaze.json.json_t;
export import glaze.json.manage;
export import glaze.json.max_write_precision;
export import glaze.json.minify;
export import glaze.json.ndjson;
export import glaze.json.prettify;
export import glaze.json.ptr;
export import glaze.json.raw_string;
export import glaze.json.read;
export import glaze.json.schema;
export import glaze.json.wrappers;
export import glaze.json.write;
export import glaze.thread.atomic;
#else
#include "glaze/json/escape_unicode.cppm"
#include "glaze/json/invoke.cppm"
#include "glaze/json/jmespath.cppm"
#include "glaze/json/json_concepts.cppm"
#include "glaze/json/json_ptr.cppm"
#include "glaze/json/json_t.cppm"
#include "glaze/json/manage.cppm"
#include "glaze/json/max_write_precision.cppm"
#include "glaze/json/minify.cppm"
#include "glaze/json/ndjson.cppm"
#include "glaze/json/prettify.cppm"
#include "glaze/json/ptr.cppm"
#include "glaze/json/raw_string.cppm"
#include "glaze/json/read.cppm"
#include "glaze/json/schema.cppm"
#include "glaze/json/wrappers.cppm"
#include "glaze/json/write.cppm"
#include "glaze/thread/atomic.cppm"
#endif



