#include "net/socket.hpp"
#include "monitoring/logger.hpp"
#include <cstring>
#include <cerrno>
#include <poll.h>

namespace netcore {

Socket::Socket(Type type) : type_(type) {
}

Socket::Socket(int fd, Type type) : fd_(fd), type_(type) {
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_), type_(other.type_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        type_ = other.type_;
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::create(Type type) {
    close();
    type_ = type;
    int domain = AF_INET;
    int sockType = (type == Type::TCP) ? SOCK_STREAM : SOCK_DGRAM;

    fd_ = ::socket(domain, sockType, 0);
    if (fd_ < 0) {
        LOG_ERROR("socket() creation failed: " << strerror(errno));
        return false;
    }

    return true;
}

bool Socket::bind(const std::string& ip, uint16_t port) {
    if (!isValid()) {
        if (!create(type_)) return false;
    }

    sockaddr_in addr{};
    if (!parseAddress(ip, port, addr)) {
        LOG_ERROR("Invalid IP address: " << ip);
        return false;
    }

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("bind() failed on " << ip << ":" << port << ": " << strerror(errno));
        return false;
    }

    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) return false;
    if (::listen(fd_, backlog) < 0) {
        LOG_ERROR("listen() failed: " << strerror(errno));
        return false;
    }
    return true;
}

int Socket::accept(Endpoint& clientEndpoint) {
    if (!isValid()) return -1;

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = ::accept(fd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientFd >= 0) {
        clientEndpoint = toEndpoint(clientAddr);
    }
    return clientFd;
}

bool Socket::connect(const std::string& ip, uint16_t port, int timeoutMs) {
    if (!isValid()) {
        if (!create(type_)) return false;
    }

    sockaddr_in addr{};
    if (!parseAddress(ip, port, addr)) {
        return false;
    }

    // Set non-blocking to handle connection timeout properly
    setNonBlocking(true);

    int ret = ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret == 0) {
        return true;
    }

    if (errno != EINPROGRESS) {
        LOG_ERROR("connect() failed to " << ip << ":" << port << ": " << strerror(errno));
        return false;
    }

    // Wait for connect completion using poll
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLOUT;

    int pollRet = ::poll(&pfd, 1, timeoutMs);
    if (pollRet <= 0) {
        // Timeout or error
        return false;
    }

    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
        return false;
    }

    return true;
}

ssize_t Socket::send(const void* data, size_t len, int flags) {
    if (!isValid()) return -1;
    return ::send(fd_, data, len, flags | MSG_NOSIGNAL);
}

ssize_t Socket::recv(void* buf, size_t len, int flags) {
    if (!isValid()) return -1;
    return ::recv(fd_, buf, len, flags);
}

ssize_t Socket::sendto(const void* data, size_t len, const Endpoint& dest, int flags) {
    if (!isValid()) {
        if (!create(Type::UDP)) return -1;
    }

    sockaddr_in addr{};
    if (!parseAddress(dest.ip, dest.port, addr)) {
        return -1;
    }

    return ::sendto(fd_, data, len, flags, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

ssize_t Socket::recvfrom(void* buf, size_t len, Endpoint& src, int flags) {
    if (!isValid()) return -1;

    sockaddr_in addr{};
    socklen_t addrLen = sizeof(addr);

    ssize_t bytes = ::recvfrom(fd_, buf, len, flags, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (bytes >= 0) {
        src = toEndpoint(addr);
    }
    return bytes;
}

bool Socket::shutdown(int how) {
    if (!isValid()) return false;
    return ::shutdown(fd_, how) == 0;
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Socket::setNonBlocking(bool nonBlocking) {
    if (!isValid()) return false;
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) return false;
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(fd_, F_SETFL, flags) == 0;
}

bool Socket::setReuseAddr(bool reuse) {
    if (!isValid()) return false;
    int opt = reuse ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

bool Socket::setReusePort(bool reuse) {
    if (!isValid()) return false;
    int opt = reuse ? 1 : 0;
#ifdef SO_REUSEPORT
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == 0;
#else
    return false;
#endif
}

bool Socket::setTcpNoDelay(bool noDelay) {
    if (!isValid() || type_ != Type::TCP) return false;
    int opt = noDelay ? 1 : 0;
    return ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
}

bool Socket::setKeepAlive(bool enable, int idleSec, int intervalSec, int count) {
    if (!isValid() || type_ != Type::TCP) return false;
    int opt = enable ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        return false;
    }
    if (enable) {
        ::setsockopt(fd_, IPPROTO_TCP, TCP_KEEPIDLE, &idleSec, sizeof(idleSec));
        ::setsockopt(fd_, IPPROTO_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec));
        ::setsockopt(fd_, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    }
    return true;
}

bool Socket::setSendBufferSize(int size) {
    if (!isValid()) return false;
    return ::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0;
}

bool Socket::setRecvBufferSize(int size) {
    if (!isValid()) return false;
    return ::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == 0;
}

Endpoint Socket::getLocalEndpoint() const {
    if (!isValid()) return {"", 0};
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return toEndpoint(addr);
    }
    return {"", 0};
}

Endpoint Socket::getPeerEndpoint() const {
    if (!isValid()) return {"", 0};
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getpeername(fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return toEndpoint(addr);
    }
    return {"", 0};
}

bool Socket::parseAddress(const std::string& ip, uint16_t port, sockaddr_in& addr) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (ip.empty() || ip == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
        return true;
    }
    return ::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == 1;
}

Endpoint Socket::toEndpoint(const sockaddr_in& addr) {
    Endpoint ep;
    char ipStr[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
    ep.ip = ipStr;
    ep.port = ntohs(addr.sin_port);
    return ep;
}

} // namespace netcore
