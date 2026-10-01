#include "net/connection.hpp"
#include "monitoring/logger.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace netcore {

const char* connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::CONNECTING: return "CONNECTING";
        case ConnectionState::CONNECTED:  return "CONNECTED";
        case ConnectionState::READING:    return "READING";
        case ConnectionState::WRITING:    return "WRITING";
        case ConnectionState::CLOSING:    return "CLOSING";
        case ConnectionState::CLOSED:     return "CLOSED";
        default:                          return "UNKNOWN";
    }
}

Connection::Connection(uint64_t id, int fd, Endpoint localEndpoint, Endpoint peerEndpoint)
    : id_(id),
      fd_(fd),
      localEndpoint_(std::move(localEndpoint)),
      peerEndpoint_(std::move(peerEndpoint)),
      state_(ConnectionState::NEW) {
    establishedTime_ = std::chrono::steady_clock::now();
    lastActivityTime_ = establishedTime_;
}

Connection::~Connection() {
    close();
}

void Connection::setState(ConnectionState newState) {
    ConnectionState oldState = state_.exchange(newState, std::memory_order_acq_rel);
    if (oldState != newState) {
        LOG_DEBUG("Conn #" << id_ << " state transition: " 
                  << connectionStateToString(oldState) << " -> " 
                  << connectionStateToString(newState));
    }
    updateLastActivity();
}

void Connection::appendReadData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        readBuffer_.insert(readBuffer_.end(), data, data + len);
    }
    bytesReceived_.fetch_add(len, std::memory_order_relaxed);
    messagesReceived_.fetch_add(1, std::memory_order_relaxed);
    updateLastActivity();
}

std::vector<uint8_t> Connection::extractReadBuffer() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    std::vector<uint8_t> data;
    data.swap(readBuffer_);
    return data;
}

void Connection::queueWriteData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        writeBuffer_.insert(writeBuffer_.end(), data, data + len);
    }
    updateLastActivity();
}

void Connection::queueWriteData(const std::string& message) {
    queueWriteData(reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

bool Connection::flushWrites() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (writeBuffer_.empty()) {
        return true;
    }

    if (fd_ < 0) {
        return false;
    }

    while (!writeBuffer_.empty()) {
        ssize_t n = ::send(fd_, writeBuffer_.data(), writeBuffer_.size(), MSG_NOSIGNAL);
        if (n > 0) {
            bytesSent_.fetch_add(static_cast<size_t>(n), std::memory_order_relaxed);
            messagesSent_.fetch_add(1, std::memory_order_relaxed);
            writeBuffer_.erase(writeBuffer_.begin(), writeBuffer_.begin() + n);
            updateLastActivity();
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket send buffer is full; keep remaining data in buffer and register for EPOLLOUT
                return false;
            }
            // Error occurred on socket
            LOG_WARN("Socket send error on fd " << fd_ << ": " << strerror(errno));
            return false;
        } else {
            // n == 0
            return false;
        }
    }

    return true;
}

bool Connection::hasPendingWrites() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return !writeBuffer_.empty();
}

size_t Connection::getPendingWriteBytes() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return writeBuffer_.size();
}

void Connection::markClosing() {
    setState(ConnectionState::CLOSING);
}

void Connection::close() {
    if (state_.load(std::memory_order_acquire) == ConnectionState::CLOSED) {
        return;
    }
    setState(ConnectionState::CLOSED);

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace netcore
