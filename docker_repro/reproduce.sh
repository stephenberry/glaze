#!/bin/bash
#
# Script to reproduce the GCC 15 strict aliasing bug
#
# Usage:
#   ./reproduce.sh           # Run reproduction in existing container
#   ./reproduce.sh build     # Build the Docker image first
#

set -e

IMAGE_NAME="glaze_gcc15_bug"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$1" = "build" ]; then
    echo "Building Docker image..."
    docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
    echo ""
fi

# Check if image exists
if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "Docker image '$IMAGE_NAME' not found. Building..."
    docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
    echo ""
fi

echo "=== Running GCC 15 Strict Aliasing Bug Reproduction ==="
echo ""

docker run --rm -it \
    -v "$SCRIPT_DIR:/repro:ro" \
    "$IMAGE_NAME" \
    /bin/bash -c '
        cd /work/test
        cp /repro/reduced_6805.cpp test.cpp

        echo "=== GCC Version ==="
        g++ --version | head -1
        echo ""

        echo "=== Compiling with -O3 (bug should trigger) ==="
        g++ -std=c++23 -O3 -I$GLAZE_INCLUDE -I$UT_INCLUDE test.cpp -o test_o3

        echo "=== Running with -O3 ==="
        ./test_o3 -tc="mod hash" || true
        echo ""

        echo "=== Compiling with -O2 (no bug) ==="
        g++ -std=c++23 -O2 -I$GLAZE_INCLUDE -I$UT_INCLUDE test.cpp -o test_o2

        echo "=== Running with -O2 ==="
        ./test_o2 -tc="mod hash"
        echo ""

        echo "=== Compiling with -O3 -fno-strict-aliasing (no bug) ==="
        g++ -std=c++23 -O3 -fno-strict-aliasing -I$GLAZE_INCLUDE -I$UT_INCLUDE test.cpp -o test_o3_nsa

        echo "=== Running with -O3 -fno-strict-aliasing ==="
        ./test_o3_nsa -tc="mod hash"
        echo ""

        echo "=== Summary ==="
        echo "  -O3:                        FAILS (bug present)"
        echo "  -O2:                        PASSES"
        echo "  -O3 -fno-strict-aliasing:   PASSES"
    '
