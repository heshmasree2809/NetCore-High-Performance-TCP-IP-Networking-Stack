#!/usr/bin/env bash
set -e

# Build if needed
if [ ! -f "build/netcore" ] || [ ! -f "build/netcore_client" ]; then
    ./scripts/build.sh
fi

# Launch server in background
echo "==> Starting NetCore server in background..."
./build/netcore configs/netcore.conf &
SERVER_PID=$!

# Ensure cleanup on exit
cleanup() {
    echo "==> Stopping NetCore server (PID: $SERVER_PID)..."
    kill -SIGINT "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

# Allow server to bind and listen
sleep 1

# Run benchmarks
./benchmarks/stress_test.sh
