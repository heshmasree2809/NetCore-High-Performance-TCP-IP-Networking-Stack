#!/usr/bin/env bash
set -e

HOST="127.0.0.1"
PORT=9090
CSV_FILE="benchmarks/benchmark_results.csv"

echo "==> NetCore Automated Benchmark Suite"
echo "Target: $HOST:$PORT"

# Ensure CSV header
echo "Clients,RequestsPerClient,TotalRequests,DurationSec,RPS,ThroughputMBs,AvgLatencyMs,P99LatencyMs,Errors" > "$CSV_FILE"

# Client matrix as required: 10, 100, 1000, 5000 clients
CLIENT_CONCURRENCY=(10 100 1000 5000)
REQUESTS_PER_CLIENT=(500 200 50 10)

CLIENT_BIN="build/netcore_client"
if [ ! -f "$CLIENT_BIN" ]; then
    echo "Error: $CLIENT_BIN not found. Please compile project first."
    exit 1
fi

for i in "${!CLIENT_CONCURRENCY[@]}"; do
    C="${CLIENT_CONCURRENCY[$i]}"
    R="${REQUESTS_PER_CLIENT[$i]}"
    echo "--------------------------------------------------------"
    echo "Running Tier: Concurrency=$C clients | Requests/client=$R"
    echo "--------------------------------------------------------"

    OUTPUT=$("$CLIENT_BIN" --host "$HOST" --port "$PORT" --clients "$C" --requests "$R" --payload 64 2>&1)
    echo "$OUTPUT"

    # Extract CSV line
    CSV_LINE=$(echo "$OUTPUT" | grep "^CSV:" | sed 's/^CSV://')
    if [ -n "$CSV_LINE" ]; then
        echo "$CSV_LINE" >> "$CSV_FILE"
    fi

    sleep 1
done

echo ""
echo "========================================================"
echo "Benchmark suite finished. Summary exported to $CSV_FILE:"
cat "$CSV_FILE"
echo "========================================================"
