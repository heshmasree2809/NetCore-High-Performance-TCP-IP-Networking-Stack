# NetCore C++ API Reference

## 1. `netcore::Socket` & `netcore::Endpoint`

### Class: `netcore::Endpoint`
Represents an IPv4 network address and port.

```cpp
#include "net/socket.hpp"

// Constructors
Endpoint();
Endpoint(std::string ip, uint16_t port);

// Methods
std::string toString() const; // Returns "IP:Port"
static std::optional<Endpoint> fromString(const std::string& str);
```

### Class: `netcore::Socket`
RAII wrapper for Linux network socket file descriptors.

```cpp
#include "net/socket.hpp"

Socket(Socket::Type type = Socket::Type::TCP);
~Socket(); // Automatically closes open file descriptor

bool create(Socket::Type type);
bool bind(const std::string& ip, uint16_t port);
bool listen(int backlog = 4096);
std::optional<std::pair<int, Endpoint>> accept();
bool connect(const std::string& ip, uint16_t port, int timeoutMs = 5000);

ssize_t send(const void* data, size_t length);
ssize_t recv(void* buffer, size_t length);
ssize_t sendto(const void* data, size_t length, const Endpoint& dest);
ssize_t recvfrom(void* buffer, size_t length, Endpoint& src);

bool setNonBlocking(bool nonBlocking);
bool setReuseAddr(bool reuse);
bool setReusePort(bool reuse);
bool setTcpNoDelay(bool noDelay);
bool setKeepAlive(bool enable, int idleSec, int intervalSec, int count);
void close();
```

---

## 2. `netcore::EventLoop`

Core I/O multiplexer powered by Linux `epoll`.

```cpp
#include "core/event_loop.hpp"

EventLoop(size_t maxEvents = 1024);
~EventLoop();

using EventCallback = std::function<void(int fd, uint32_t events)>;

bool registerSocket(int fd, uint32_t events, EventCallback callback);
bool modifySocket(int fd, uint32_t events);
bool unregisterSocket(int fd);

void run();     // Enters blocking epoll_wait loop
void stop();    // Safely halts the event loop
void wakeup();  // Awakens sleeping epoll_wait via eventfd
bool isRunning() const;
```

---

## 3. `netcore::ThreadPool`

Worker concurrency pool for task dispatch and future-based execution.

```cpp
#include "core/thread_pool.hpp"

ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
~ThreadPool();

void start();
void shutdown();
bool execute(std::function<void()> task);

template<typename F, typename... Args>
auto submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

size_t getThreadCount() const;
size_t getTasksCompleted() const;
```

---

## 4. `netcore::Connection` & `netcore::ConnectionManager`

### Class: `netcore::Connection`
Represents an active client session.

```cpp
#include "net/connection.hpp"

ConnectionId getId() const;
int getFd() const;
ConnectionState getState() const;
void setState(ConnectionState newState);

const Endpoint& getPeerEndpoint() const;
const Endpoint& getLocalEndpoint() const;

void appendReadData(const uint8_t* data, size_t len);
std::vector<uint8_t> extractReadBuffer();

void queueWriteData(const uint8_t* data, size_t len);
bool flushWrites();

void close();
bool isClosed() const;
```

### Class: `netcore::ConnectionManager`
Thread-safe repository for all active connections.

```cpp
#include "net/connection_manager.hpp"

bool addConnection(ConnectionPtr conn);
ConnectionPtr removeConnection(int fd);
ConnectionPtr getConnection(int fd);
ConnectionPtr getConnectionById(ConnectionId id);

std::vector<ConnectionPtr> cleanupTimedOutConnections(std::chrono::seconds timeout);
void closeAll();
size_t getConnectionCount() const;
```

---

## 5. `netcore::TcpServer` & `netcore::UdpServer`

High-level server instances.

```cpp
#include "net/tcp_server.hpp"
#include "net/udp_server.hpp"

// TCP Server
TcpServer(const ServerConfig& config);
bool init();
bool start();
void stop();

void setConnectHandler(ConnectCallback handler);
void setMessageHandler(MessageCallback handler);
void setDisconnectHandler(DisconnectCallback handler);
bool send(ConnectionId connId, const void* data, size_t len);

// UDP Server
UdpServer(const ServerConfig& config);
bool init();
bool start();
void stop();

void setDatagramHandler(DatagramCallback handler);
bool sendDatagram(const Endpoint& dest, const void* data, size_t len);
```

---

## 6. `netcore::Message` & `netcore::MessageDecoder`

NetCore binary protocol (NCP) framing and validation.

```cpp
#include "protocol/message.hpp"

Message(MessageType type, uint32_t sequenceId, const std::string& payload);
std::vector<uint8_t> serialize() const;
static std::optional<Message> deserialize(const uint8_t* data, size_t length);
bool verifyChecksum() const;

// Decoder for fragmented streams
MessageDecoder();
void feed(const uint8_t* data, size_t length);
std::vector<Message> extractMessages();
```
