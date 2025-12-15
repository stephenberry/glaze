# Dockerfile for reproducing GCC 15 strict aliasing bug
#
# Build:   docker build -t glaze_gcc15_bug .
# Run:     docker run --rm -it glaze_gcc15_bug
#
# This creates an environment with GCC 15.2.1 to reproduce the bug.

FROM gentoo/stage3:latest

# Update portage and install required packages
RUN emerge-webrsync && \
    emerge --sync && \
    ACCEPT_KEYWORDS="~amd64" emerge -av --quiet \
        sys-devel/gcc \
        dev-build/cmake \
        dev-vcs/git \
        app-misc/jq

# Clone Glaze library (v6.2.0 - known to trigger the bug)
WORKDIR /work
RUN git clone --depth 1 --branch v6.2.0 https://github.com/stephenberry/glaze.git

# Build Glaze to fetch the ut testing framework dependency
WORKDIR /work/glaze
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_CXX_COMPILER=g++ -DCMAKE_CXX_STANDARD=23 && \
    make -j$(nproc) 2>/dev/null || true

# Create test directory
WORKDIR /work/test

# Copy the reproduction test file (will be mounted or copied in)
# The reduced_6805.cpp file triggers the bug

# Set up environment variables for easy compilation
ENV GLAZE_INCLUDE=/work/glaze/include
ENV UT_INCLUDE=/work/glaze/build/_deps/ut-src/include

# Default command: show instructions
CMD ["/bin/bash", "-c", "echo '=== GCC 15 Strict Aliasing Bug Reproduction ===' && \
    echo '' && \
    g++ --version | head -1 && \
    echo '' && \
    echo 'To reproduce the bug:' && \
    echo '  1. Copy reduced_6805.cpp to /work/test/' && \
    echo '  2. g++ -std=c++23 -O3 -I$GLAZE_INCLUDE -I$UT_INCLUDE test.cpp -o test' && \
    echo '  3. ./test -tc=\"mod hash\"' && \
    echo '' && \
    echo 'Expected: FAILED (bug present at -O3)' && \
    echo '' && \
    echo 'To verify fix:' && \
    echo '  g++ -std=c++23 -O2 ... (should PASS)' && \
    echo '  g++ -std=c++23 -O3 -fno-strict-aliasing ... (should PASS)' && \
    exec /bin/bash"]
