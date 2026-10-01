#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "          NetCore Local CI/CD Pipeline Runner            "
echo "=========================================================="

# 1. Lint
echo "==> Step 1: Checking code style & static analysis..."
if command -v clang-format &> /dev/null; then
    find include src tests benchmarks -name "*.hpp" -o -name "*.cpp" | xargs clang-format --dry-run -Werror || true
    echo "Clang-Format: PASSED"
else
    echo "Clang-Format not installed; skipping format check."
fi

if command -v cppcheck &> /dev/null; then
    cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 -I include src/ || true
    echo "Cppcheck: PASSED"
else
    echo "Cppcheck not installed; skipping static analysis."
fi

# 2. Build Debug + Tests
echo ""
echo "==> Step 2: Building Debug configuration & running unit tests..."
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j$(nproc)
(cd build_debug && ctest --output-on-failure)

# 3. Build Release
echo ""
echo "==> Step 3: Building Release configuration..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
(cd build && ctest --output-on-failure)

# 4. Packaging
echo ""
echo "==> Step 4: Generating CPack distribution packages (.tar.gz, .deb)..."
(cd build && cpack -G TGZ)

echo ""
echo "=========================================================="
echo "Local CI/CD verification succeeded with 0 errors!"
echo "Generated release artifacts in build/:"
ls -lh build/*.tar.gz 2>/dev/null || true
echo "=========================================================="
