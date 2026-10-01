#pragma once

#include "net/socket.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>

namespace netcore {

enum class ConnectionState {
    CONNECTING = 0,
    NEW = 0,
    CONNECTED = 1,
    READING = 2,
    WRITING = 3,
    CLOSING = 4,
    CLOSED = 5
};

const char* connectionStateToString(ConnectionState state);

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using MessageHandler = std::function<void(ConnectionPtr, const std::vector<uint8_t>&)>;
using CloseHandler = std::function<void(ConnectionPtr)>;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(uint64_t id, int fd, Endpoint localEndpoint, Endpoint peerEndpoint);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    uint64_t getId() const { return id_; }
    int getFd() const { return fd_; }
    const Endpoint& getPeerEndpoint() const { return peerEndpoint_; }
    const Endpoint& getLocalEndpoint() const { return localEndpoint_; }

    ConnectionState getState() const { return state_.load(std::memory_order_acquire); }
    void setState(ConnectionState newState);

    // Buffer management for partial reads and non-blocking writes
    void appendReadData(const uint8_t* data, size_t len);
    std::vector<uint8_t> extractReadBuffer();
    
    // Outbound queueing
    void queueWriteData(const uint8_t* data, size_t len);
    void queueWriteData(const std::string& message);
    
    // Flush pending write data to the non-blocking socket
    // Returns true if all queued data was written, false if partial (EAGAIN/EWOULDBLOCK)
    bool flushWrites();
    
    bool hasPendingWrites() const;
    size_t getPendingWriteBytes() const;

    // State queries
    bool isClosed() const {
        return state_.load(std::memory_order_acquire) == ConnectionState::CLOSED;
    }
    bool isClosing() const {
        return state_.load(std::memory_order_acquire) == ConnectionState::CLOSING;
    }

    // Statistics & telemetry
    uint64_t getBytesReceived() const { return bytesReceived_.load(std::memory_order_relaxed); }
    uint64_t getBytesSent() const { return bytesSent_.load(std::memory_order_relaxed); }
    uint64_t getMessagesReceived() const { return messagesReceived_.load(std::memory_order_relaxed); }
    uint64_t getMessagesSent() const { return messagesSent_.load(std::memory_order_relaxed); }

    std::chrono::steady_clock::time_point getEstablishedTime() const { return establishedTime_; }
    std::chrono::steady_clock::time_point getLastActivityTime() const {
        std::lock_guard<std::mutex> lock(activityMutex_);
        return lastActivityTime_;
    }
    void updateLastActivity() {
        std::lock_guard<std::mutex> lock(activityMutex_);
        lastActivityTime_ = std::chrono::steady_clock::now();
    }

    void markClosing();
    void close();

private:
    uint64_t id_;
    int fd_;
    Endpoint localEndpoint_;
    Endpoint peerEndpoint_;
    std::atomic<ConnectionState> state_{ConnectionState::NEW};

    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> messagesReceived_{0};
    std::atomic<uint64_t> messagesSent_{0};

    std::chrono::steady_clock::time_point establishedTime_;
    std::chrono::steady_clock::time_point lastActivityTime_;
    mutable std::mutex activityMutex_;

    // Buffers protected by mutex for thread safety
    mutable std::mutex bufferMutex_;
    std::vector<uint8_t> readBuffer_;
    std::vector<uint8_t> writeBuffer_;
};

} // namespace netcore
