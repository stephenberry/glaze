project(asio_repe)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_subdirectory(server)
add_subdirectory(client)
