# Deep Dive: TCP/IP Networking Concepts

This document explores the fundamental networking concepts implemented in **NetCore**, specifically tailored to the rigorous requirements of Linux systems and modem/telecom software engineering.

---

## 1. TCP Three-Way Handshake & Connection Teardown

### The Handshake Sequence
```
Client (Initiator)                         Server (NetCore)
       |                                          |
       |  1. SYN (seq = x)                        |
       |----------------------------------------->| Listen Queue (SYN_RCVD)
       |                                          |
       |  2. SYN-ACK (seq = y, ack = x + 1)       |
       |<-----------------------------------------|
       |                                          |
       |  3. ACK (ack = y + 1)                    |
       |----------------------------------------->| Accept Queue (ESTABLISHED)
       |                                          |
 [ESTABLISHED]                              [ESTABLISHED]
```

1. **SYN**: Client sends initial sequence number $x$ to synchronize.
2. **SYN-ACK**: NetCore's kernel network stack replies with its sequence number $y$ and acknowledges $x+1$. The connection enters the **SYN backlog**.
3. **ACK**: Client acknowledges sequence $y+1$. The socket moves from the SYN backlog to the **Accept queue** (`listen(backlog)`). `epoll` signals `EPOLLIN` on the listening socket, triggering `accept()` in NetCore.

### Connection Teardown (Four-Way Handshake)
When either endpoint finishes transmitting:
1. Active closer invokes `shutdown(SHUT_WR)` or `close()`, transmitting a `FIN` packet.
2. Passive receiver receives `FIN`, transitions to `CLOSE_WAIT`, and kernel triggers `EPOLLRDHUP` / `recv() == 0`.
3. Passive receiver finishes sending any remaining data and emits its own `FIN`.
4. Active closer sends final `ACK` and stays in `TIME_WAIT` (typically $2 \times \text{MSL} \approx 60\text{s}$) to absorb duplicate delayed packets and guarantee reliable ACK delivery.

---

## 2. Blocking vs. Non-blocking Sockets

| Mode | Behavior on `recv()` | Behavior on `send()` | Scalability Limit |
| :--- | :--- | :--- | :--- |
| **Blocking** | Blocks calling OS thread until data arrives | Blocks thread until kernel send buffer frees space | ~1,000 threads before context switching thrashing |
| **Non-blocking (`O_NONBLOCK`)** | Returns immediately with `EAGAIN` or `EWOULDBLOCK` | Copies as many bytes as possible; returns `EAGAIN` if full | **100,000+** connections with single event loop |

In **NetCore**, every accepted socket is immediately set to non-blocking via:
```cpp
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

---

## 3. I/O Multiplexing: `select` vs. `poll` vs. `epoll`

```
Primitive      Complexity    Internal Mechanism          Limit
select(2)      O(N)          Linear scan of bitmasks     FD_SETSIZE (default 1024)
poll(2)        O(N)          Array of pollfd structures   Memory bound, O(N) linear scan
epoll(7)       O(1) active   Red-Black Tree + Ready List  Scales to 1,000,000+ FDs
```

### Why epoll excels for high-concurrency:
1. **Kernel-level state persistence**: With `epoll_ctl()`, sockets are registered once in a kernel Red-Black tree. Unlike `select()` or `poll()`, the file descriptor set is **never re-copied** from user space to kernel space on every iteration.
2. **Ready List**: When a network packet arrives and the NIC interrupts the kernel, the kernel socket callback inserts the file descriptor into the epoll **ready list** (a doubly linked list).
3. **O(1) Return**: `epoll_wait()` only returns active file descriptors that experienced events, eliminating $O(N)$ scanning.

---

## 4. Level-Triggered (LT) vs. Edge-Triggered (ET)

- **Level-Triggered**: As long as there is unread data in the socket receive buffer, `epoll_wait()` will repeatedly signal `EPOLLIN`. This simplifies code but incurs extra syscall overhead.
- **Edge-Triggered (`EPOLLET`)**: `epoll_wait()` signals `EPOLLIN` **only when new data transitions onto the buffer**. NetCore uses Edge-Triggered mode and mandates a draining loop:

```cpp
while (true) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n > 0) {
        // Append to connection read buffer
    } else if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Buffer completely drained; safe to resume epoll_wait
            break;
        }
        // Handle error
    } else {
        // Peer disconnected (EOF)
        break;
    }
}
```

---

## 5. TCP Flow Control & Windowing

TCP is a reliable byte-stream protocol. It enforces flow control using a **Sliding Window**:
1. **Receive Window (`rwnd`)**: The receiver advertises the remaining space in its kernel receive buffer (`SO_RCVBUF`) in every TCP header ACK.
2. **Send Window (`cwnd`)**: The sender caps in-flight unacknowledged bytes to:
   $$\text{In-Flight} \le \min(\text{cwnd}, \text{rwnd})$$
3. **Zero Window**: If the user application reads too slowly, the receive buffer fills, and the receiver advertises a window of 0. The sender pauses transmission and sends periodic **Zero-Window Probes**.

---

## 6. Message Framing & Fragmentation

TCP has **no concept of message boundaries**. Packets may be split or merged by the network layer (MTU fragmentation, Nagle coalescing, delayed ACKs).

NetCore handles framing through:
1. **Dynamic Connection Buffering**: Incomplete bytes are retained in `readBuffer_` until a complete frame or delimiter arrives.
2. **Zero-Copy Output Queue**: Writes that cannot complete immediately are preserved in `writeBuffer_` without blocking worker threads.
