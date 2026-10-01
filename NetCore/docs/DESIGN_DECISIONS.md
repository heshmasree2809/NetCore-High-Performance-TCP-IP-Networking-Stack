# NetCore Architectural Design Decisions

## 1. Why Linux `epoll` Over `io_uring`, `select`, and `poll`?

### The Inadequacy of `select()` and `poll()`
* **Linear $O(N)$ Scanning**: `select()` and `poll()` require passing the entire set of monitored descriptors from user-space to kernel-space on every single invocation. As connections scale to 10,000+, scanning idle sockets consumes 95%+ of CPU cycles.
* **Descriptor Limits**: `select()` is hardcoded by glibc to a maximum of 1,024 descriptors (`FD_SETSIZE`), rendering it unsuitable for carrier-grade applications.
* **Epoll's $O(1)$ Event Return**: `epoll_wait()` registers descriptors once in the kernel red-black tree and only returns descriptors that have actively transitioned states, guaranteeing true $O(1)$ efficiency.

### Why Epoll Over `io_uring`?
* **Portability & Kernel Maturity**: While `io_uring` offers asynchronous completion-based I/O, it requires bleeding-edge Linux kernels (5.10+) and presents higher kernel attack surfaces and complex memory locking requirements. Linux `epoll` is rock-solid, supported across all Linux distributions (kernels 2.6 through 6.x), and universally deployed in enterprise and embedded carrier hardware.

---

## 2. Why Edge-Triggered (`EPOLLET`) Over Level-Triggered?

* **Elimination of Redundant Wakeups**: In level-triggered mode, if an event buffer has pending bytes, `epoll_wait` continues waking up the thread loop on every cycle until the buffer is fully drained.
* **System Call Reduction**: Edge-triggered mode alerts the application **only once** when state transitions occur (e.g. data arrives). The application then loops `read()` or `write()` until receiving `EAGAIN` or `EWOULDBLOCK`. This cuts kernel transitions by up to 60% under sustained traffic.

---

## 3. Thread Pool Architecture Decisions

* **Task Queue with Work Stealing vs Shared Queue**:
  - We implemented a synchronized, condition-variable backed bounded queue (`TaskQueue`). While lock-free work-stealing queues provide minor benefits at massive core counts (64+ cores), a condition-variable backed queue guarantees near-zero CPU usage when the server is idle.
* **Futures & Promises Support**:
  - NetCore's `ThreadPool::submit()` accepts arbitrary callable objects and returns `std::future<T>`, decoupling asynchronous business workflows from I/O dispatch.

---

## 4. Buffer Management Strategy & Trade-offs

* **Vector vs Pre-allocated Slabs**:
  - Instead of dynamically allocating tiny memory buffers on every packet arrival (which triggers memory fragmentation and heap lock contention), NetCore maintains contiguous buffer blocks with configured capacity (`buffer_size=8192`).
  - Read buffers are extracted and swapped using move semantics, keeping allocation rates near zero on the hot path.

---

## 5. Key Lessons Learned

1. **`SIGPIPE` Hazards**: Writing to a socket after the remote peer has sent a `RST` raises `SIGPIPE`, which terminates the process by default. Explicitly ignoring `SIGPIPE` (`std::signal(SIGPIPE, SIG_IGN)`) or passing `MSG_NOSIGNAL` is essential for production network stability.
2. **False Sharing Prevention**: Multi-threaded atomic metrics (such as request counters and throughput trackers) must be aligned to 64-byte boundaries (`alignas(64)`) to prevent cross-core cache line bouncing.
3. **Graceful Epoll Teardown**: When closing a file descriptor, Linux kernel automatically removes it from all epoll sets. However, unregistering it explicitly with `epoll_ctl(..., EPOLL_CTL_DEL, ...)` before `close(fd)` prevents race conditions with concurrent worker threads.
