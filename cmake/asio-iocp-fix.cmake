# Patch for ASIO issue #312: IOCP out-of-resources error reporting.
# https://github.com/chriskohlhoff/asio/issues/312
#
# When PostQueuedCompletionStatus fails due to resource exhaustion, the
# completion key (overlapped_contains_result) is lost. The operation falls
# back to an internal queue but do_one() dispatches it with the wrong
# error code and bytes_transferred, causing undefined behavior / crashes.
#
# Fix: store the completion key on the operation object before posting,
# so it survives the fallback path.
#
# Based on MongoDB's patch:
# https://github.com/mongodb-forks/asio/commit/d03c2e7002131305645374e735a8ece4191f2fc5
#
# Tested against: ASIO 1.36.0 (asio-1-36-0 tag)
# The patch targets specific string patterns in ASIO's IOCP implementation.
# If ASIO is updated, verify the patterns still match or update this patch.

function(apply_asio_iocp_fix asio_include_dir)
    set(OP_HEADER "${asio_include_dir}/asio/detail/win_iocp_operation.hpp")
    set(CTX_IMPL "${asio_include_dir}/asio/detail/impl/win_iocp_io_context.ipp")

    if(NOT EXISTS "${OP_HEADER}" OR NOT EXISTS "${CTX_IMPL}")
        message(WARNING "ASIO IOCP fix: headers not found at ${asio_include_dir}, skipping patch")
        return()
    endif()

    # Check if already patched (look for our added member)
    file(READ "${OP_HEADER}" op_content)
    if(op_content MATCHES "completionKey_")
        message(STATUS "ASIO IOCP fix: already applied")
        return()
    endif()

    # Verify we're working with a compatible ASIO version by checking
    # that the patterns we need to replace actually exist.
    set(patch_compatible TRUE)

    if(NOT op_content MATCHES "long ready_;")
        message(WARNING "ASIO IOCP fix: 'long ready_' not found in win_iocp_operation.hpp — ASIO version may be incompatible")
        set(patch_compatible FALSE)
    endif()

    file(READ "${CTX_IMPL}" ctx_content)

    if(NOT ctx_content MATCHES "PostQueuedCompletionStatus\\(iocp_\\.handle, 0, 0, op\\)")
        message(WARNING "ASIO IOCP fix: post_deferred_completion pattern not found in win_iocp_io_context.ipp — ASIO version may be incompatible")
        set(patch_compatible FALSE)
    endif()

    if(NOT ctx_content MATCHES "overlapped_contains_result, op\\)")
        message(WARNING "ASIO IOCP fix: on_pending/on_completion pattern not found in win_iocp_io_context.ipp — ASIO version may be incompatible")
        set(patch_compatible FALSE)
    endif()

    if(NOT patch_compatible)
        message(WARNING "ASIO IOCP fix: skipping patch due to incompatible ASIO version")
        return()
    endif()

    message(STATUS "Applying ASIO IOCP fix (issue #312)")

    # --- Patch win_iocp_operation.hpp ---
    # Add completionKey_ member and accessor

    # Add member after "long ready_;"
    string(REPLACE
        "long ready_;"
        "long ready_;\n  ULONG_PTR completionKey_;"
        op_content "${op_content}")

    # Initialize in constructor: find "ready_(0)" and add completionKey_(0)
    string(REPLACE
        "ready_(0)"
        "ready_(0), completionKey_(0)"
        op_content "${op_content}")

    # Add accessor method before the member block
    string(REPLACE
        "win_iocp_operation* next_;"
        "ULONG_PTR& completionKey() { return completionKey_; }\n\n  win_iocp_operation* next_;"
        op_content "${op_content}")

    # Verify the operation header patch took effect
    if(NOT op_content MATCHES "completionKey_")
        message(WARNING "ASIO IOCP fix: failed to patch win_iocp_operation.hpp")
        return()
    endif()

    file(WRITE "${OP_HEADER}" "${op_content}")

    # --- Patch win_iocp_io_context.ipp ---

    # Fix post_deferred_completion: pass op->completionKey() instead of 0 for the key
    string(REPLACE
        "::PostQueuedCompletionStatus(iocp_.handle, 0, 0, op)"
        "::PostQueuedCompletionStatus(iocp_.handle, 0, op->completionKey(), op)"
        ctx_content "${ctx_content}")

    # Fix on_pending/on_completion: store key on op before posting
    string(REPLACE
        "if (!::PostQueuedCompletionStatus(iocp_.handle,\n          0, overlapped_contains_result, op))"
        "op->completionKey() = overlapped_contains_result;\n      if (!::PostQueuedCompletionStatus(iocp_.handle,\n          0, op->completionKey(), op))"
        ctx_content "${ctx_content}")

    # Verify the io_context patch took effect
    if(NOT ctx_content MATCHES "op->completionKey\\(\\)")
        message(WARNING "ASIO IOCP fix: failed to patch win_iocp_io_context.ipp")
        return()
    endif()

    file(WRITE "${CTX_IMPL}" "${ctx_content}")

    message(STATUS "ASIO IOCP fix applied successfully")
endfunction()
