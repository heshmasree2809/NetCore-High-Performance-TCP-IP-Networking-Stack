#!/usr/bin/env bash
set -e

echo "==> Building NetCore and Test Suite..."
cd "$(dirname "$0")/.."
cmake -B build
cmake --build build --target netcore_tests -j$(nproc)

echo ""
echo "==> Executing NetCore Test Suite via CTest..."
cd build
ctest --output-on-failure

echo ""
echo "==> Running Direct Diagnostic Test Runner..."
./tests/netcore_tests
