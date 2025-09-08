# [Choice] bionic (18.04), focal (20.04), jammy (22.04), noble (24.04)
ARG VARIANT="noble"
FROM mcr.microsoft.com/devcontainers/cpp:${VARIANT}

# Restate the variant to use it later on in the llvm and cmake installations
ARG VARIANT

# Install wget, gpg, make and ninja
RUN apt-get update -qq && export DEBIAN_FRONTEND=noninteractive && \
    apt-get install -y --no-install-recommends \
        lsb-release software-properties-common \
        wget apt-utils file zip \
        openssh-client gnupg gpg-agent socat rsync \
        make ninja-build \
        ca-certificates pkg-config libssl-dev

# User-settable versions:
# This Dockerfile should support gcc-[7, 8, 9, 10, 11, 12, 13] and clang-[10, 11, 12, 13. 14, 15, 16, 17]
ARG GCC_VER="14"
# Add gcc-${GCC_VER}
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN apt-get update -qq && export DEBIAN_FRONTEND=noninteractive && \
    apt-get install -y --no-install-recommends \
        gcc-${GCC_VER} g++-${GCC_VER} gdb

# Set gcc-${GCC_VER} as default gcc
RUN update-alternatives --install /usr/bin/gcc gcc $(which gcc-${GCC_VER}) 100
RUN update-alternatives --install /usr/bin/g++ g++ $(which g++-${GCC_VER}) 100

ARG LLVM_VER="18"
# Add clang-${LLVM_VER}
ARG LLVM_URL="http://apt.llvm.org/${VARIANT}/"
ARG LLVM_PKG="llvm-toolchain-${VARIANT}-${LLVM_VER}"
# Use keyrings and signed-by (apt-key is removed in Ubuntu noble)
RUN mkdir -p /etc/apt/keyrings && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /etc/apt/keyrings/llvm-snapshot.gpg
RUN echo "deb [signed-by=/etc/apt/keyrings/llvm-snapshot.gpg] ${LLVM_URL} ${LLVM_PKG} main" \
    > /etc/apt/sources.list.d/llvm-${LLVM_VER}.list
RUN apt-get update -qq && export DEBIAN_FRONTEND=noninteractive && \
    apt-get install -y --no-install-recommends \
        clang-${LLVM_VER} lldb-${LLVM_VER} lld-${LLVM_VER} \
        clang-tidy-${LLVM_VER} clang-format-${LLVM_VER} \
        libc++-${LLVM_VER}-dev libc++abi-${LLVM_VER}-dev \
        clang-tools-${LLVM_VER} libclang-common-${LLVM_VER}-dev \
        libclang-${LLVM_VER}-dev libclang1-${LLVM_VER} \
        clangd-${LLVM_VER} libclang-rt-${LLVM_VER}-dev \
        libfuzzer-${LLVM_VER}-dev

# Set clang-${LLVM_VER} as default clang
RUN update-alternatives --install /usr/bin/clang clang $(which clang-${LLVM_VER}) 100
RUN update-alternatives --install /usr/bin/clang++ clang++ $(which clang++-${LLVM_VER}) 100

# Set the default clang-tidy, so CMake can find it
RUN update-alternatives --install /usr/bin/clang-tidy clang-tidy $(which clang-tidy-${LLVM_VER}) 1

# Set the default clang-format
RUN update-alternatives --install /usr/bin/clang-format clang-format $(which clang-format-${LLVM_VER}) 1

# Set the default clangd, so IDEs can find it
RUN update-alternatives --install /usr/bin/clangd clangd $(which clangd-${LLVM_VER}) 1

# Add current cmake/ccmake, from Kitware (keyrings + signed-by)
ARG CMAKE_URL="https://apt.kitware.com/ubuntu/"
ARG CMAKE_PKG=${VARIANT}
RUN mkdir -p /etc/apt/keyrings && \
    wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /etc/apt/keyrings/kitware-archive.gpg
RUN echo "deb [signed-by=/etc/apt/keyrings/kitware-archive.gpg] ${CMAKE_URL} ${CMAKE_PKG} main" \
    > /etc/apt/sources.list.d/kitware.list
RUN apt-get update -qq && export DEBIAN_FRONTEND=noninteractive && \
    apt-get install -y --no-install-recommends cmake cmake-curses-gui

# Cleanup cached apt data we don't need anymore
RUN apt-get autoremove -y && apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Allow the user to set compiler defaults
ARG USE_CLANG
# if --build-arg USE_CLANG=1, set CC to 'clang' or set to null otherwise.
ENV CC=${USE_CLANG:+"clang"}
ENV CXX=${USE_CLANG:+"clang++"}
# if CC is null, set it to 'gcc' (or leave as is otherwise).
ENV CC=${CC:-"gcc"}
ENV CXX=${CXX:-"g++"}
