#pragma once

#include <string>
#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace netcore {

struct Endpoint {
    std::string ip;
    uint16_t port{0};

    std::string toString() const {
        return ip + ":" + std::to_string(port);
    }
};

class Socket {
public:
    enum class Type {
        TCP,
        UDP
    };

    explicit Socket(Type type = Type::TCP);
    explicit Socket(int fd, Type type = Type::TCP);
    ~Socket();

    // Move semantics (transfer ownership of file descriptor)
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Disallow copy semantics (RAII single ownership)
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Core Socket API
    bool create(Type type = Type::TCP);
    bool bind(const std::string& ip, uint16_t port);
    bool listen(int backlog = 4096);
    int accept(Endpoint& clientEndpoint);
    bool connect(const std::string& ip, uint16_t port, int timeoutMs = 5000);
    
    ssize_t send(const void* data, size_t len, int flags = 0);
    ssize_t recv(void* buf, size_t len, int flags = 0);
    
    ssize_t sendto(const void* data, size_t len, const Endpoint& dest, int flags = 0);
    ssize_t recvfrom(void* buf, size_t len, Endpoint& src, int flags = 0);

    bool shutdown(int how = SHUT_RDWR);
    void close();

    // Socket Configuration
    bool setNonBlocking(bool nonBlocking = true);
    bool setReuseAddr(bool reuse = true);
    bool setReusePort(bool reuse = true);
    bool setTcpNoDelay(bool noDelay = true);
    bool setKeepAlive(bool enable = true, int idleSec = 60, int intervalSec = 5, int count = 3);
    bool setSendBufferSize(int size);
    bool setRecvBufferSize(int size);

    // Queries
    int getFd() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }
    Type getType() const { return type_; }
    int release() { int fd = fd_; fd_ = -1; return fd; }
    
    Endpoint getLocalEndpoint() const;
    Endpoint getPeerEndpoint() const;

    // Static IPv4 utilities
    static bool parseAddress(const std::string& ip, uint16_t port, sockaddr_in& addr);
    static Endpoint toEndpoint(const sockaddr_in& addr);

private:
    int fd_{-1};
    Type type_{Type::TCP};
};

} // namespace netcore
