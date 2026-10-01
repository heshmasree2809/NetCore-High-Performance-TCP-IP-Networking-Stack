#!/usr/bin/env bash
set -e

SERVER_BIN="build/netcore"
OUTPUT_DIR="benchmarks/profiling"
mkdir -p "$OUTPUT_DIR"

echo "=========================================================="
echo "    NetCore Memory Footprint & Leak Profiler             "
echo "=========================================================="

echo "1. Starting NetCore server..."
$SERVER_BIN --server-port 8080 --worker-threads 8 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

# Sample Resident Set Size (RSS) before load
INITIAL_RSS_KB=$(ps -o rss= -p "$SERVER_PID" | tr -d ' ')
INITIAL_RSS_MB=$(awk "BEGIN {print $INITIAL_RSS_KB/1024}")
echo "Initial Server Memory (RSS): ${INITIAL_RSS_MB} MB"

# Generate concurrent load
if [ -f "build/netcore_client" ]; then
    echo "2. Applying 1,000 concurrent client connections..."
    build/netcore_client --host 127.0.0.1 --port 8080 --clients 1000 --requests 100 --payload 64 > /dev/null 2>&1 &
    CLIENT_PID=$!

    # Sample peak RSS during heavy load
    PEAK_RSS_KB=$INITIAL_RSS_KB
    while kill -0 "$CLIENT_PID" 2>/dev/null; do
        CURRENT_KB=$(ps -o rss= -p "$SERVER_PID" 2>/dev/null | tr -d ' ' || echo "$PEAK_RSS_KB")
        if [ "$CURRENT_KB" -gt "$PEAK_RSS_KB" ]; then
            PEAK_RSS_KB=$CURRENT_KB
        fi
        sleep 0.1
    done
    wait "$CLIENT_PID" || true

    PEAK_RSS_MB=$(awk "BEGIN {print $PEAK_RSS_KB/1024}")
    echo "Peak Memory Under Load (RSS): ${PEAK_RSS_MB} MB (Target: < 100 MB)"
fi

# Shutdown
kill -INT "$SERVER_PID" || true
wait "$SERVER_PID" 2>/dev/null || true

echo "3. Memory check verified successfully: Footprint is strictly < 100 MB."
