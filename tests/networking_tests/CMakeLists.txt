FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-32-0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

# HTTP Examples
add_subdirectory(http_examples)

# Networking Tests
add_subdirectory(asio_repe)
add_subdirectory(https_test)
add_subdirectory(repe_test)
add_subdirectory(rest_test)
add_subdirectory(websocket_test)