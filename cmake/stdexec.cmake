function (fetch_stdexec)
  set(branch_or_tag "main")
  set(url "https://github.com/NVIDIA/stdexec.git")
  set(target_folder "${CMAKE_BINARY_DIR}/_deps/stdexec-src")

  if (NOT EXISTS ${target_folder})
    execute_process(
        COMMAND git clone --depth 1 --branch "${branch_or_tag}" --recursive "${url}" "${target_folder}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        RESULT_VARIABLE exec_process_result
        OUTPUT_VARIABLE exec_process_output
    )
    if(NOT exec_process_result EQUAL "0")
        message(FATAL_ERROR "Git clone failed: ${exec_process_output}")
    else()
        message(STATUS "Git clone succeeded: ${exec_process_output}")
    endif()
  endif()

  set(stdexec_SOURCE_DIR ${target_folder} CACHE INTERNAL "stdexec source folder" FORCE)
  set(stdexec_INCLUDE_DIR ${target_folder}/include CACHE INTERNAL "stdexec include folder" FORCE)

    #[[
    include(FetchContent)

    FetchContent_Declare(
        stdexec
        GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )

    set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(stdexec)
    #]]

endfunction()