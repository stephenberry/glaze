## TODO: sort in the alphabetic order


# Top-level
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      modules/glaze/glaze.ixx
      modules/glaze/json.ixx
      modules/glaze/cbor.ixx
      modules/glaze/beve.ixx
      modules/glaze/csv.ixx
      modules/glaze/chrono.ixx
      modules/glaze/version.ixx
      modules/glaze/glaze_exceptions.ixx
      modules/glaze/msgpack.ixx
)

# core
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      modules/glaze/core/array_apply.ixx
      modules/glaze/core/as_array_wrapper.ixx
      modules/glaze/core/buffer_traits.ixx
      modules/glaze/core/cast.ixx
      modules/glaze/core/chrono.ixx
      modules/glaze/core/common.ixx
      modules/glaze/core/constraint.ixx
      modules/glaze/core/context.ixx
      modules/glaze/core/convert_struct.ixx
      modules/glaze/core/custom.ixx
      modules/glaze/core/custom_meta.ixx
      modules/glaze/core/error_category.ixx
      modules/glaze/core/istream_buffer.ixx
      modules/glaze/core/manage.ixx
      modules/glaze/core/meta.ixx
      modules/glaze/core/meta_fwd.ixx
      modules/glaze/core/optimization_level.ixx
      modules/glaze/core/opts.ixx
      modules/glaze/core/ostream_buffer.ixx
      modules/glaze/core/ptr.ixx
      modules/glaze/core/read.ixx
      modules/glaze/core/reflect.ixx
      modules/glaze/core/seek.ixx
      modules/glaze/core/std_error_code.ixx
      modules/glaze/core/streaming_state.ixx
      modules/glaze/core/to.ixx
      modules/glaze/core/wrappers.ixx
      modules/glaze/core/wrapper_traits.ixx
      modules/glaze/core/write.ixx
      modules/glaze/core/write_chars.ixx
)

# json
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/json/escape_unicode.ixx
    modules/glaze/json/float_format.ixx
    modules/glaze/json/generic.ixx
    modules/glaze/json/invoke.ixx
    modules/glaze/json/jmespath.ixx
    modules/glaze/json/json_concepts.ixx
    modules/glaze/json/json_format.ixx
    modules/glaze/json/json_ptr.ixx
    modules/glaze/json/json_stream.ixx
    modules/glaze/json/lazy.ixx
    modules/glaze/json/max_write_precision.ixx
    modules/glaze/json/minify.ixx
    modules/glaze/json/ndjson.ixx
    modules/glaze/json/patch.ixx
    modules/glaze/json/prettify.ixx
    modules/glaze/json/ptr.ixx
    modules/glaze/json/raw_string.ixx
    modules/glaze/json/read.ixx
    modules/glaze/json/schema.ixx
    modules/glaze/json/skip.ixx
    modules/glaze/json/study.ixx
    modules/glaze/json/wrappers.ixx
    modules/glaze/json/write.ixx

)

# utils
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/util/string_literal.ixx
    modules/glaze/util/expected.ixx
    modules/glaze/util/dump.ixx
    modules/glaze/util/convert.ixx
    modules/glaze/util/parse.ixx
    modules/glaze/util/atoi.ixx
    modules/glaze/util/for_each.ixx
    modules/glaze/util/utility.ixx
    modules/glaze/util/type_traits.ixx
    modules/glaze/util/bit.ixx
    modules/glaze/util/compare.ixx
    modules/glaze/util/glaze_fast_float.ixx
    modules/glaze/util/simple_float.ixx
    modules/glaze/util/fast_float.ixx
    modules/glaze/util/variant.ixx
    modules/glaze/util/itoa.ixx
    modules/glaze/util/bit_array.ixx
    modules/glaze/util/help.ixx
    modules/glaze/util/tuple.ixx
    modules/glaze/util/validate.ixx
    modules/glaze/util/primes_64.ixx
    modules/glaze/util/dtoa.ixx
    modules/glaze/util/itoa_40kb.ixx
    modules/glaze/util/dragonbox.ixx
    modules/glaze/util/buffer_pool.ixx
    modules/glaze/util/progress_bar.ixx
)

# concepts
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/concepts/container_concepts.ixx
)

# api/std
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/api/std/array.ixx
    modules/glaze/api/std/deque.ixx
    modules/glaze/api/std/functional.ixx
    modules/glaze/api/std/list.ixx
    modules/glaze/api/std/map.ixx
    modules/glaze/api/std/optional.ixx
    modules/glaze/api/std/set.ixx
    modules/glaze/api/std/shared_ptr.ixx
    modules/glaze/api/std/span.ixx
    modules/glaze/api/std/string.ixx
    modules/glaze/api/std/tuple.ixx
    modules/glaze/api/std/unique_ptr.ixx
    modules/glaze/api/std/unordered_map.ixx
    modules/glaze/api/std/unordered_set.ixx
    modules/glaze/api/std/variant.ixx
    modules/glaze/api/std/vector.ixx
)

# api
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/api/hash.ixx
    modules/glaze/api/tuplet.ixx
    modules/glaze/api/type_support.ixx
    modules/glaze/api/xxh64.ixx
    modules/glaze/api/impl.ixx
    modules/glaze/api/api.ixx
    modules/glaze/api/trait.ixx
)

# tuplet
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/tuplet/tuple.ixx
)

# thread
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/thread/atomic.ixx
    modules/glaze/thread/async.ixx
    modules/glaze/thread/async_string.ixx
    modules/glaze/thread/async_vector.ixx
    modules/glaze/thread/shared_async_map.ixx
    modules/glaze/thread/shared_async_vector.ixx
    modules/glaze/thread/threadpool.ixx
    modules/glaze/thread/value_proxy.ixx
    modules/glaze/thread/guard.ixx
)

# simd
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/simd/avx.ixx
    modules/glaze/simd/neon.ixx
    modules/glaze/simd/sse.ixx
)

# reflection
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/reflection/to_tuple.ixx
    modules/glaze/reflection/get_name.ixx
    modules/glaze/reflection/requires_key.ixx
)

# base64
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/base64/base64.ixx
)

# beve
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/beve/header.ixx
    modules/glaze/beve/beve_to_json.ixx
    modules/glaze/beve/key_traits.ixx
    modules/glaze/beve/lazy.ixx
    modules/glaze/beve/peek_header.ixx
    modules/glaze/beve/ptr.ixx
    modules/glaze/beve/read.ixx
    modules/glaze/beve/size.ixx
    modules/glaze/beve/skip.ixx
    modules/glaze/beve/wrappers.ixx
    modules/glaze/beve/write.ixx
)

# file
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/file/file_ops.ixx
    modules/glaze/file/hostname_include.ixx
    modules/glaze/file/raw_or_file.ixx
    modules/glaze/file/read_directory.ixx
    modules/glaze/file/write_directory.ixx
)

# cbor
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/cbor/cbor_to_json.ixx
    modules/glaze/cbor/header.ixx
    modules/glaze/cbor/read.ixx
    modules/glaze/cbor/skip.ixx
    modules/glaze/cbor/wrappers.ixx
    modules/glaze/cbor/write.ixx
)

# containers
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/containers/flat_map.ixx
    modules/glaze/containers/inplace_vector.ixx
)

# hardware
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/hardware/volatile_array.ixx
)

# record
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/record/recorder.ixx
)

# trace
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/trace/trace.ixx
)

# csv
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/csv/read.ixx
    modules/glaze/csv/skip.ixx
    modules/glaze/csv/write.ixx
)

# exceptions
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/exceptions/binary_exceptions.ixx
    modules/glaze/exceptions/cbor_exceptions.ixx
    modules/glaze/exceptions/core_exceptions.ixx
    modules/glaze/exceptions/csv_exceptions.ixx
    modules/glaze/exceptions/json_exceptions.ixx
    modules/glaze/exceptions/json_schema_exceptions.ixx
    modules/glaze/exceptions/msgpack_exceptions.ixx
)

# msgpack
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/msgpack/common.ixx
    modules/glaze/msgpack/read.ixx
    modules/glaze/msgpack/skip.ixx
    modules/glaze/msgpack/write.ixx
)

# ext (external)
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/ext/cli_menu.ixx
    # modules/glaze/ext/eigen.ixx
    modules/glaze/ext/jsonrpc.ixx
)

# compare
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/compare/compare.ixx
    modules/glaze/compare/approx.ixx
)

# stencil
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/stencil/stencil.ixx
    modules/glaze/stencil/stencilcount.ixx
)

# format
target_sources(glaze_glaze
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    modules/glaze/format/format_to.ixx
)
