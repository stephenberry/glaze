# REPE to JSON-RPC Conversion Summary

Bidirectional conversion utilities between REPE messages with JSON bodies and JSON-RPC 2.0 messages.

## Files Added

- **`include/glaze/rpc/repe/repe_to_jsonrpc.hpp`** - Core conversion functions and error code mapping
- **`tests/networking_tests/repe_to_jsonrpc_test/`** - Test suite with 20 tests and 60 assertions
- **`docs/rpc/repe-jsonrpc-conversion.md`** - API documentation with examples
- **`examples/repe-jsonrpc-conversion.cpp`** - Runnable demonstration

## API Functions

**REPE → JSON-RPC:**
- `to_jsonrpc_request()` - Convert REPE request to JSON-RPC request
- `to_jsonrpc_response()` - Convert REPE response to JSON-RPC response

**JSON-RPC → REPE:**
- `from_jsonrpc_request()` - Convert JSON-RPC request to REPE message
- `from_jsonrpc_response()` - Convert JSON-RPC response to REPE message

## Features

- Automatic error code mapping between protocols
- Smart ID handling (numeric direct mapping, string parsing/hashing)
- Notification support (`notify=true` ↔ `id=null`)
- Lightweight string operations with O(1) error code mapping

## Use Cases

- Protocol bridging between REPE servers and JSON-RPC clients
- Integration with existing JSON-RPC systems
- Testing REPE implementations using JSON-RPC tools
- Multi-protocol support in applications
