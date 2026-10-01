# NetCore Operational Runbook

## 1. Build Instructions

### Prerequisites
* Linux OS (Ubuntu 20.04+, Debian 11+, RHEL 8+, or Arch Linux)
* Modern C++17 compiler (`g++ >= 9.0` or `clang++ >= 10.0`)
* CMake `>= 3.14`
* Make or Ninja

### Compilation Commands
```bash
# Clone and enter directory
cd ~/NetCore

# 1. Standard Release Build (Optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 2. Debug Build with Symbols
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j$(nproc)

# 3. Sanitized Build (Memory Leak & Data Race Auditing)
cmake -B build_asan -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build_asan -j$(nproc)
```

---

## 2. Deployment Guide

### Running as a Systemd Service

Create `/etc/systemd/system/netcore.service`:
```ini
[Unit]
Description=NetCore High-Performance Networking Stack
After=network.target

[Service]
Type=simple
User=netcore
Group=netcore
WorkingDirectory=/opt/netcore
ExecStart=/opt/netcore/bin/netcore --config /opt/netcore/etc/netcore.conf
Restart=always
RestartSec=3s
LimitNOFILE=1048576

[Install]
WantedBy=multi-user.target
```

Reload and enable:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now netcore
sudo systemctl status netcore
```

---

## 3. Real-time Monitoring & Telemetry

### CLI Status Query
NetCore exposes real-time telemetry over its listening TCP port:
```bash
# Query human-readable dashboard
echo "STATS" | nc -q 1 127.0.0.1 8080

# Query machine-readable JSON (ideal for Prometheus / Datadog exporter)
echo "STATS_JSON" | nc -q 1 127.0.0.1 8080
```

Sample JSON Output:
```json
{
  "activeConnections": 1042,
  "totalConnections": 284000,
  "bytesReceived": 18459200,
  "bytesSent": 18459200,
  "messagesReceived": 284000,
  "messagesSent": 284000,
  "connectionErrors": 0,
  "averageLatencyMs": 1.15,
  "p50LatencyMs": 0.98,
  "p95LatencyMs": 1.84,
  "p99LatencyMs": 2.31,
  "requestsPerSec": 88450.0,
  "throughputMBs": 10.79,
  "uptimeSec": 3600.0
}
```

---

## 4. Linux Kernel Tuning (`sysctl`)

For maximum connection scale (100,000+ sockets) and low latency, apply the following sysctl parameters in `/etc/sysctl.d/99-netcore.conf`:

```ini
# Maximum socket listen queue backlog
net.core.somaxconn = 65535

# Increase max pending connection requests in SYN queue
net.ipv4.tcp_max_syn_backlog = 65535

# Maximum file descriptors across entire system
fs.file-max = 2097152

# Reuse TIME_WAIT sockets for outgoing connections
net.ipv4.tcp_tw_reuse = 1

# Reduce FIN timeout to reclaim sockets faster
net.ipv4.tcp_fin_timeout = 15

# Buffer memory tuning for 10GbE / high-throughput links
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
```

Apply immediately:
```bash
sudo sysctl -p /etc/sysctl.d/99-netcore.conf
```

Raise per-process file descriptor limits in `/etc/security/limits.conf`:
```text
*    soft    nofile    1048576
*    hard    nofile    1048576
```

---

## 5. Troubleshooting Common Issues

1. **`Address already in use (bind error)`**:
   - Cause: Another process is utilizing port 8080 or previous socket is in `TIME_WAIT`.
   - Fix: Identify process with `sudo lsof -i :8080`. NetCore already enables `SO_REUSEADDR` to bypass standard `TIME_WAIT` delays.
2. **`Too many open files (EMFILE)`**:
   - Cause: Process descriptor ceiling exceeded.
   - Fix: Check current limit with `ulimit -n`. Increase via `ulimit -n 1048576` before starting the server.
3. **`Latency spikes / high p99 under stress`**:
   - Cause: CPU power scaling or thread context switching storms.
   - Fix: Lock CPU frequency governor to performance: `sudo cpupower frequency-set -g performance`.
