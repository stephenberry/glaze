# Glaze headers that are include-fragments rather than standalone units. They specialize
# templates whose primary declaration lives only in a parent header that #includes them
# (e.g. the *_registry_impl.hpp fragments are pulled in by rpc/registry.hpp and fail to
# compile on their own). They must never be #included directly, so header CI checks drop
# them; they are still exercised transitively through their parent.
#
# Every other public header must compile when included alone.

include_guard(GLOBAL)

set(glaze_non_standalone_fragments
  "glaze/net/rest_registry_impl.hpp"
  "glaze/rpc/jsonrpc_registry_impl.hpp"
  "glaze/rpc/repe/repe_registry_impl.hpp"
)
