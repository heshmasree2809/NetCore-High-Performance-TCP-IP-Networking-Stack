#!/usr/bin/env bash
set -e

BUILD_DIR="build"
BUILD_TYPE="Release"
ENABLE_ASAN=OFF

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --asan)
            ENABLE_ASAN=ON
            shift
            ;;
        *)
            shift
            ;;
    esac
done

echo "==> Configuring NetCore ($BUILD_TYPE, ASAN=$ENABLE_ASAN)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DENABLE_ASAN="$ENABLE_ASAN"

echo "==> Compiling with $(nproc) cores..."
make -j"$(nproc)"

echo "==> Build successful! Binaries located in $BUILD_DIR/"
ls -lh netcore netcore_client tests/netcore_tests 2>/dev/null || true
