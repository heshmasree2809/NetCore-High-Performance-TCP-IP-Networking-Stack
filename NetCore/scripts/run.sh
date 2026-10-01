#!/usr/bin/env bash
set -e

CONFIG_FILE="configs/netcore.conf"
if [ "$#" -ge 1 ]; then
    CONFIG_FILE="$1"
fi

if [ ! -f "build/netcore" ]; then
    echo "NetCore binary not found. Building first..."
    ./scripts/build.sh
fi

echo "==> Launching NetCore with config: $CONFIG_FILE"
./build/netcore "$CONFIG_FILE"
