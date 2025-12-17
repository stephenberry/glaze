# Dockerfile for reproducing GCC 15 strict aliasing bug
#
# Build:   docker build -t glaze_issue_2115_repro .
# Run:     docker run --rm -it glaze_issue_2115_repro
#

FROM gentoo/stage3:latest

# Update portage and install required packages
RUN emerge-webrsync && \
    emerge --sync && \
    ACCEPT_KEYWORDS="~amd64" emerge -av --quiet \
        sys-devel/gcc \
        dev-build/cmake \
        dev-vcs/git \
        app-misc/jq

# Clone Glaze library (v6.2.0)
WORKDIR /work
RUN git clone --depth 1 --branch v6.2.0 https://github.com/stephenberry/glaze.git

# Clone UT library (dependency)
RUN git clone --depth 1 https://github.com/boost-ext/ut.git /work/ut

# Set up environment variables
ENV GLAZE_INCLUDE=/work/glaze/include
ENV UT_INCLUDE=/work/ut/include

# Create test directory
WORKDIR /work/test

# Copy necessary files
# Note: These files must be in the build context (folder where you run docker build)
COPY reduced_6805.cpp test.cpp
COPY json_test_shared_types.hpp .

# Default command: show instructions and try to reproduce
CMD ["/bin/bash", "-c", "echo '=== GCC 15 Strict Aliasing Bug Reproduction ===' && \
    g++ --version | head -1 && \
    echo '' && \
    echo 'Compiling test case...' && \
    g++ -std=c++23 -O3 -I$GLAZE_INCLUDE -I$UT_INCLUDE -I. test.cpp -o test_o3 && \
    echo 'Running test (expected to FAIL)...' && \
    ./test_o3 -tc='mod hash' || true && \
    echo '' && \
    echo 'Compiling with -O2 (expected to PASS)...' && \
    g++ -std=c++23 -O2 -I$GLAZE_INCLUDE -I$UT_INCLUDE -I. test.cpp -o test_o2 && \
    ./test_o2 -tc='mod hash'"]