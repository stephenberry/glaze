#!/bin/bash
#
# Script to reproduce the GCC 15 strict aliasing bug
#

set -e

IMAGE_NAME="glaze_issue_2115_repro"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure required files are present
if [ ! -f "$SCRIPT_DIR/json_test_shared_types.hpp" ]; then
    echo "Error: json_test_shared_types.hpp not found in $SCRIPT_DIR"
    exit 1
fi

if [ "$1" = "build" ] || ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "Building Docker image..."
    docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
    echo ""
fi

echo "=== Running Reproduction in Docker ==="
# We use the built-in CMD from Dockerfile which runs the reproduction steps
docker run --rm -it "$IMAGE_NAME"