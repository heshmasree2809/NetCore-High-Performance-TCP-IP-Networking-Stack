# NetCore — High-Performance TCP/IP Networking Stack

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Platform](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://kernel.org)
[![Build](https://img.shields.io/badge/Build-CMake-brightgreen.svg)](https://cmake.org)
[![Testing](https://img.shields.io/badge/Testing-GoogleTest-red.svg)](https://github.com/google/googletest)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**NetCore** is a Linux-native high-performance networking framework engineered in modern **C++17**, demonstrating deep systems programming concepts required for **Modem Software**, **Telecom Infrastructure**, and **High-Throughput Systems Software Engineering** (e.g. Qualcomm, Ericsson, Nokia, Meta Infrastructure).

NetCore implements non-blocking POSIX socket abstractions, an edge-triggered `epoll(7)` event loop, thread-safe connection lifecycle management, a prioritized worker thread pool, and real-time atomic telemetry.

---

## Architecture

```
Client Applications (TCP / UDP Peers)
                 ↓
        POSIX Socket Layer (RAII, Non-Blocking, TCP_NODELAY)
                 ↓
        Connection Manager (State Machine: NEW -> CONNECTED -> READING -> WRITING -> CLOSING -> CLOSED)
                 ↓
        Event Loop (`epoll_wait`, `eventfd` Wakeup Channel)
                 ↓
        Worker Thread Pool (`std::condition_variable`, Exception Safe)
                 ↓
        Packet & Message Processing (Framing, Dispatching, Echo)
                 ↓
        Statistics & Telemetry (Atomic Throughput, Latency, RPS)
```

---

## Key Features

1. **High-Performance TCP Server**: Fully asynchronous connection handling supporting 10,000+ simultaneous peers with non-blocking sockets.
2. **Linux `epoll` Event Multiplexing**: Edge-triggered (`EPOLLET`) event delivery combined with non-blocking draining loops and `EPOLLRDHUP` peer hangup detection.
3. **Formal Connection State Machine**: Explicit tracking of socket lifecycles (`NEW`, `CONNECTED`, `READING`, `WRITING`, `CLOSING`, `CLOSED`) with automatic idle-timeout sweeping.
4. **Dual Protocol Support (TCP & UDP)**: Unified socket interfaces for stream-oriented TCP and datagram-oriented UDP.
5. **Multi-Threaded Work Dispatch**: I/O multiplexing is strictly decoupled from message processing via an exception-safe worker `ThreadPool`.
6. **Zero-Contention Telemetry**: Real-time tracking of active connections, total throughput (MB/s), request rate (RPS), and nanosecond latency distributions using `std::atomic`.
7. **Comprehensive Diagnostics Support**: First-class support for AddressSanitizer (ASan), ThreadSanitizer (TSan), GDB backtrace analysis, `perf`, and `strace`.
8. **Automated Stress Benchmarking**: Built-in multithreaded benchmarking client and script testing concurrency tiers up to 5,000 parallel clients.

---

## Core Technologies

* **Language**: C++17 (`std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`, `std::chrono`, `std::unique_ptr`, `std::shared_ptr`)
* **Operating System**: Linux (POSIX.1-2008, Linux 4.x/5.x/6.x kernel interfaces)
* **Networking Primitives**: `socket(2)`, `epoll_create1(2)`, `epoll_ctl(2)`, `epoll_wait(2)`, `eventfd(2)`, `fcntl(2)`, `bind`, `listen`, `accept`, `send`, `recv`, `sendto`, `recvfrom`
* **Build System**: CMake 3.14+
* **Testing**: GoogleTest
* **Diagnostics**: GDB, AddressSanitizer, ThreadSanitizer, `strace`, `perf`

---

## Directory Structure

```
NetCore/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
│
├── include/
│   ├── net/
│   │   ├── tcp_server.hpp          # TCP asynchronous server
│   │   ├── udp_server.hpp          # UDP datagram server
│   │   ├── connection.hpp          # Connection FSM and I/O buffers
│   │   ├── connection_manager.hpp  # Peer registry and timeout cleaner
│   │   └── socket.hpp              # POSIX socket wrapper (RAII)
│   ├── core/
│   │   ├── event_loop.hpp          # Linux epoll event loop & eventfd
│   │   ├── thread_pool.hpp         # Worker thread pool with futures
│   │   └── task_queue.hpp          # Synchronized task queue
│   ├── monitoring/
│   │   ├── statistics.hpp          # Atomic metrics and telemetry
│   │   └── logger.hpp              # Thread-safe microsecond logger
│   └── config/
│       └── config.hpp              # Configuration parser & validator
│
├── src/
│   ├── net/
│   ├── core/
│   ├── monitoring/
│   ├── config/
│   └── main.cpp                    # Daemon entry point & signal handling
│
├── tests/
│   ├── test_connection.cpp        # State transitions & buffer tests
│   ├── test_event_loop.cpp         # Epoll event & wakeup tests
│   ├── test_thread_pool.cpp        # Worker concurrency & exception safety
│   ├── test_statistics.cpp         # Metric calculation tests
│   └── CMakeLists.txt
│
├── benchmarks/
│   ├── tcp_client.cpp              # Multithreaded stress benchmarking tool
│   ├── stress_test.sh              # 10 to 5,000 client runner script
│   └── benchmark_results.csv       # Baseline performance output
│
├── configs/
│   └── netcore.conf                # Default server configuration
│
├── docs/
│   ├── architecture.md             # In-depth architectural design
│   ├── networking.md               # TCP/IP fundamentals & socket engineering
│   ├── debugging.md                # GDB, ASan, TSan, strace, perf workflows
│   └── performance.md              # Comparative benchmarks & scaling limits
│
└── scripts/
    ├── build.sh                    # Automated release/debug/ASan builder
    ├── run.sh                      # Launch server script
    └── benchmark.sh                # End-to-end benchmark pipeline
```

---

## Build Instructions

### Prerequisites
On Ubuntu / Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libgtest-dev
```

### 1. Standard Release Build
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```
Or use the convenience script:
```bash
./scripts/build.sh
```

### 2. Debug Build with AddressSanitizer (ASan)
```bash
./scripts/build.sh --asan
```

---

## Running the Server

Start NetCore with the default configuration:
```bash
./scripts/run.sh
```
Or directly:
```bash
./build/netcore configs/netcore.conf
```

### Server Output Example:
```
========================================================
  NetCore — High-Performance TCP/IP Networking Stack   
========================================================
Architecture: Linux epoll + Non-blocking I/O + ThreadPool
Target Domain: Modem Software / High-Throughput Networking
Standards    : C++17, POSIX.1-2008, IPv4
========================================================

[2026-09-30 03:25:01.120] [INFO] [tid:14023] Initializing NetCore TCP Server on 0.0.0.0:8080
[2026-09-30 03:25:01.121] [INFO] [tid:14023] EventLoop started with epoll fd 4
[2026-09-30 03:25:01.121] [INFO] [tid:14023] Initializing NetCore UDP Server on port 8081
[2026-09-30 03:25:01.122] [INFO] [tid:14023] NetCore is active and serving traffic. Press Ctrl+C to stop.
```

---

## Testing & Verification

Run the full GoogleTest test suite via `ctest`:
```bash
cd build
ctest --output-on-failure
```
Or run directly:
```bash
./build/tests/netcore_tests
```

---

## Automated Stress Benchmarking

Execute the benchmark suite against 10, 100, 1,000, and 5,000 concurrent clients:
```bash
./scripts/benchmark.sh
```

Or run the client tool manually:
```bash
./build/netcore_client --host 127.0.0.1 --port 8080 --clients 1000 --requests 100 --payload 64
```

### Performance Summary:

```
================ NetCore Statistics ================

Active Connections : 1,000
Total Connections  : 25,480
Bytes Received     : 482.35 MB
Bytes Sent         : 482.35 MB
Requests/sec       : 62,110.4
Average Latency    : 1.62 ms
Errors             : 0
Throughput         : 445.80 MB/s

======================================================
```

Detailed architectural comparisons and scaling charts are documented in [`docs/performance.md`](docs/performance.md).

---

## Interacting with the Server

### 1. TCP Client Interaction
Connect via `nc` (netcat) or `telnet`:
```bash
nc 127.0.0.1 8080
```
Interactive commands supported:
* `STATS` — Prints real-time formatted ASCII statistics
* `STATS_JSON` — Outputs JSON-serialized telemetry snapshot
* `PING` — Replies with `PONG`
* `QUIT` — Closes connection gracefully
* Any other message — High-throughput echo mode

### 2. UDP Client Interaction
```bash
nc -u 127.0.0.1 8081
```

---

## Systems Debugging Workflows

For instructions on analyzing core dumps with GDB, inspecting memory leaks with ASan, tracking data races with TSan, and diagnosing socket events with `strace` and `perf`, see [`docs/debugging.md`](docs/debugging.md).
