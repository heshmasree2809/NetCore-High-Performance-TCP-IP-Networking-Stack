# NetCore Systems Debugging & Diagnostics Guide

As a high-performance network daemon operating under extreme concurrency, **NetCore** is engineered to be observable, instrumentable, and diagnosable using industry-standard Linux debugging tools.

---

## 1. Quick Tool Matrix

| Symptom | Diagnostic Tool | Invocation |
| :--- | :--- | :--- |
| **Crash / Segfault** | GDB, Core Dump | `gdb ./build/netcore core` |
| **Memory Corruptions & Leaks** | AddressSanitizer (ASan) | `cmake -DENABLE_ASAN=ON .. && make` |
| **Data Races & Thread Hazards** | ThreadSanitizer (TSan) | `cmake -DENABLE_TSAN=ON .. && make` |
| **Socket & Syscall Errors** | `strace` | `strace -f -e trace=network ./build/netcore` |
| **CPU Spikes & Cache Misses** | `perf` | `perf top -p $(pgrep netcore)` |
| **File Descriptor Leaks** | `lsof` / `/proc` | `ls -l /proc/$(pgrep netcore)/fd` |

---

## 2. Diagnosing Segmentation Faults with GDB

### Enabling Core Dumps
```bash
ulimit -c unlimited
# Set core dump pattern
echo "/tmp/core.%e.%p.%t" | sudo tee /proc/sys/kernel/core_pattern
```

### Inspecting Core Dump
```bash
gdb ./build/netcore /tmp/core.netcore.18342.1700000000
```
Common GDB investigation commands:
```gdb
(gdb) bt full              # Print backtrace with local variables
(gdb) thread apply all bt  # Backtrace for all worker threads
(gdb) info registers       # Check instruction pointer (RIP) and stack pointer (RSP)
(gdb) frame 3              # Jump to specific call frame
(gdb) print *conn          # Inspect connection state and memory contents
```

---

## 3. AddressSanitizer (ASan) & LeakSanitizer (LSan)

NetCore includes native CMake integration for AddressSanitizer.

### Compiling with ASan
```bash
./scripts/build.sh --asan
```

### Example ASan Diagnostic Output (Use-After-Free Detection)
```
=================================================================
==24102==ERROR: AddressSanitizer: heap-use-after-free on address 0x608000001f20
READ of size 4 at 0x608000001f20 thread T2 (worker_0)
    #0 0x55d7e12 in netcore::Connection::getState() include/net/connection.hpp:34
    #1 0x55d8201 in netcore::TcpServer::handleRead(std::shared_ptr<Connection>) src/net/tcp_server.cpp:210
    #2 0x55d911a in netcore::ThreadPool::workerLoop(unsigned long) src/core/thread_pool.cpp:64

0x608000001f20 was freed by thread T1 (loop_thread):
    #0 0x7f48102 in free (libasan.so.5)
    #1 0x55da402 in netcore::ConnectionManager::removeConnection(int) src/net/connection_manager.cpp:24
=================================================================
```
**Fix Implemented in NetCore**: NetCore uses `std::shared_ptr<Connection>` and `enable_shared_from_this`, ensuring connection objects remain valid across worker thread dispatches until all async references are released.

---

## 4. Diagnosing Concurrency & Data Races with ThreadSanitizer

Compile with TSan:
```bash
mkdir -p build_tsan && cd build_tsan
cmake -DENABLE_TSAN=ON ..
make -j$(nproc)
./netcore
```

TSan detects data races where two threads access the same memory location concurrently without synchronization. NetCore guards all connection write buffers with `std::mutex` and uses `std::atomic` for statistics to guarantee data race freedom.

---

## 5. Tracing Network Syscalls with `strace`

To observe non-blocking socket behavior and epoll events in real time:
```bash
# Trace network-specific syscalls with microsecond timestamps
strace -tt -T -e trace=epoll_create1,epoll_ctl,epoll_wait,accept4,recvfrom,sendto,recv,send -p $(pgrep netcore)
```
Output pattern to look for:
- `epoll_wait(...) = 1 [events={EPOLLIN, fd=8}] <0.000012>`
- `recv(8, "...", 8192, 0) = 64 <0.000008>`
- `recv(8, 0x..., 8192, 0) = -1 EAGAIN (Resource temporarily unavailable) <0.000005>`

Seeing `EAGAIN` confirms the edge-triggered socket drain loop is functioning correctly.

---

## 6. Profiling CPU Utilization & Hotspots with `perf`

```bash
# Record CPU call graphs at 99Hz sampling rate
perf record -F 99 -g -p $(pgrep netcore) -- sleep 30

# View interactive terminal flame report
perf report --stdio
```

Look for:
- **Lock Contention**: Excessive time in `__lll_lock_wait` indicates mutex contention. In NetCore, thread tasks are decoupled and statistics use atomics to minimize lock hold times.
- **Syscall Overhead**: Excessive time in `sys_enter_recv` or `sys_enter_send` indicates undersized buffer thresholds.
