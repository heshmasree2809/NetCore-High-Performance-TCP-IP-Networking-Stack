# NetCore Architecture Specification

## 1. System Overview

**NetCore** is a high-performance, non-blocking asynchronous TCP/IP and UDP networking stack engineered in modern C++17. Tailored for carrier-grade embedded network environments, modern modem software, and high-throughput server backends, NetCore utilizes the Linux `epoll` kernel interface combined with the **Multi-threaded Reactor Pattern** to sustain hundreds of thousands of concurrent connections with sub-millisecond latencies and a minimal resident memory footprint (< 100 MB).

---

## 2. Component Interaction Diagram

```text
       +-------------------------------------------------------------+
       |                      Client Network                         |
       +-------------------------------------------------------------+
                       |                               |
              [TCP 3-Way Handshake]           [UDP Datagrams]
                       |                               |
                       v                               v
       +-------------------------------+   +-------------------------+
       |     TcpServer (Listener)      |   |        UdpServer        |
       |  - Non-blocking socket fd     |   |  - Non-blocking UDP fd  |
       |  - Edge-Triggered EPOLLET     |   |  - Zero connection state|
       +-------------------------------+   +-------------------------+
                       |                               |
                       +---------------+---------------+
                                       |
                                       v
                     +-----------------------------------+
                     |      Reactor Event Loop           |
                     |  - Linux epoll_wait() loop        |
                     |  - Non-blocking eventfd wakeups   |
                     |  - Dispatches active I/O events   |
                     +-----------------------------------+
                                       |
                    +------------------+------------------+
                    |                                     |
                    v                                     v
       [Network Connection Management]          [Task Dispatch Queue]
       +-----------------------------+          +-------------------+
       |     ConnectionManager       |          |    ThreadPool     |
       | - Thread-safe hash map      |          | - Fixed worker set|
       | - Heartbeat & idle cleanup  |          | - Task stealing   |
       | - Lifecycle state machine   |          | - Future/promise  |
       +-----------------------------+          +-------------------+
                    |                                     |
                    +------------------+------------------+
                                       |
                                       v
                     +-----------------------------------+
                     |    NetCore Protocol Layer (NCP)   |
                     |  - Magic: 0x4E435031 ('NCP1')     |
                     |  - Framing & Stream Deserializer  |
                     |  - Hardware-accelerated CRC32     |
                     +-----------------------------------+
                                       |
                                       v
                     +-----------------------------------+
                     |    Telemetry & Metrics Engine     |
                     |  - p50 / p95 / p99 Latency Stats  |
                     |  - Lock-free atomic counters      |
                     |  - Real-time ASCII / JSON exports |
                     +-----------------------------------+
```

---

## 3. Threading Model Explanation

NetCore adopts the **Master-Reactor / Worker-Thread-Pool** architecture:

1. **Reactor Event Loop Thread**:
   - Executes the core Linux `epoll_wait` dispatch loop.
   - Monitors the TCP listening socket for incoming client handshakes (`accept4` with `SOCK_NONBLOCK` and `SOCK_CLOEXEC`).
   - Monitors existing client connections for `EPOLLIN` (data available to read) and `EPOLLOUT` (socket ready for non-blocking write drain).
   - Never performs heavy business computation, cryptographic verification, or blocking disk I/O.

2. **Worker Thread Pool**:
   - Configurable pool of worker threads (default: hardware concurrency count).
   - Offloads message parsing, framing extraction, serialization, and business callbacks.
   - Protects the Reactor loop from compute bottlenecks, ensuring low-latency event polling even under high load.

3. **Inter-Thread Coordination**:
   - Epoll wakeup signaling is implemented via Linux `eventfd` descriptors (`EFD_NONBLOCK | EFD_CLOEXEC`), allowing external threads to wake the reactor loop with zero latency and zero socket allocation.

---

## 4. Memory Management Strategy

1. **Pre-allocated Buffer Arenas**:
   - Connection read buffers use pre-sized storage buffers (`8 KB` default) to prevent runtime memory fragmentation (`std::realloc` storms).
2. **Move Semantics & Zero-Copy Slicing**:
   - Buffers are exchanged across layers using C++17 move semantics (`std::vector<uint8_t>&&`), eliminating deep payload copies.
   - Socket writes utilize non-allocating circular scatter-gather structures where applicable.
3. **Deterministic RAII Lifecycles**:
   - All file descriptors, sockets, epoll instances, and thread handles are encapsulated in RAII wrappers (`Socket`, `EventLoop`).
   - Sockets automatically close on destruction, guaranteeing no descriptor leaks.

---

## 5. NetCore Protocol (NCP) Specification

The NetCore Protocol is a binary framing protocol designed for rapid validation, minimal parsing overhead, and high throughput.

### Wire Header Layout (16 Bytes Fixed)

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic (0x4E435031)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Version (1)          |          Message Type         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Sequence ID                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload CRC-32                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                        Payload Data ...                       +
|                          (Variable)                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Header Fields
* **Magic (4 Bytes)**: `0x4E435031` (ASCII `"NCP1"`). Uniquely identifies NetCore frames.
* **Version (2 Bytes)**: Protocol revision (current: `1`).
* **Message Type (2 Bytes)**:
  - `0x0001`: `HELO` (Session Initiation)
  - `0x0002`: `AUTH` (Authentication Token)
  - `0x0003`: `PING` (Keepalive Probe)
  - `0x0004`: `PONG` (Keepalive Response)
  - `0x0005`: `DATA` (Application Payload)
  - `0x0006`: `ECHO_REQ` (Echo Request)
  - `0x0007`: `ECHO_RESP` (Echo Response)
  - `0x0008`: `DISCONNECT` (Graceful Session Close)
  - `0xFFFF`: `ERROR` (Malformed Frame / Protocol Violation)
* **Sequence ID (4 Bytes)**: Monotonically increasing 32-bit integer for ordering and deduplication.
* **Payload Length (4 Bytes)**: Payload size in bytes (maximum: 10 MB).
* **Payload CRC-32 (4 Bytes)**: IEEE 802.3 32-bit cyclic redundancy check computed across payload bytes.
