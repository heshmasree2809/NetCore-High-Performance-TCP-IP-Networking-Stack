#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "         NetCore Distribution Packaging Script            "
echo "=========================================================="

cd "$(dirname "$0")/.."

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

echo "==> Packaging .tar.gz distribution archive..."
(cd build && cpack -G TGZ)

if command -v dpkg &>/dev/null; then
    echo "==> Packaging Debian .deb distribution..."
    (cd build && cpack -G DEB)
fi

echo ""
echo "Packaging complete! Built artifacts:"
ls -lh build/*.tar.gz 2>/dev/null || true
ls -lh build/*.deb 2>/dev/null || true
echo "=========================================================="
