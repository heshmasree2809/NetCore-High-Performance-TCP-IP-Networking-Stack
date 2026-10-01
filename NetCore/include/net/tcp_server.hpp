#pragma once

#include "net/socket.hpp"
#include "net/connection.hpp"
#include "net/connection_manager.hpp"
#include "core/event_loop.hpp"
#include "core/thread_pool.hpp"
#include "config/config.hpp"
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace netcore {

using TcpMessageHandler = std::function<void(ConnectionPtr conn, const std::vector<uint8_t>& data)>;
using TcpConnectHandler = std::function<void(ConnectionPtr conn)>;
using TcpDisconnectHandler = std::function<void(ConnectionPtr conn)>;

class TcpServer {
public:
    explicit TcpServer(const ServerConfig& config);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // Initialize sockets, epoll, thread pool
    bool init();

    // Start server listening and processing events
    bool start();

    // Gracefully stop accepting, finish pending requests, shut down threads and sockets
    void stop();

    // Send data asynchronously to a connection by ID
    bool send(uint64_t connId, const std::string& message);
    bool send(uint64_t connId, const uint8_t* data, size_t len);

    // Close a connection gracefully
    void closeConnection(uint64_t connId);

    // Handler callbacks
    void setMessageHandler(TcpMessageHandler handler) { messageHandler_ = std::move(handler); }
    void setConnectHandler(TcpConnectHandler handler) { connectHandler_ = std::move(handler); }
    void setDisconnectHandler(TcpDisconnectHandler handler) { disconnectHandler_ = std::move(handler); }

    // State & components
    bool isRunning() const { return running_.load(std::memory_order_acquire); }
    ConnectionManager& getConnectionManager() { return connectionManager_; }
    ThreadPool& getThreadPool() { return threadPool_; }
    const ServerConfig& getConfig() const { return config_; }

private:
    void handleNewConnection();
    void handleSocketEvent(int fd, uint32_t events);
    void handleRead(ConnectionPtr conn);
    void handleWrite(ConnectionPtr conn);
    void handleError(ConnectionPtr conn);
    void handleClose(ConnectionPtr conn);

    void periodicMaintenance();

    ServerConfig config_;
    Socket listenSocket_;
    EventLoop eventLoop_;
    ThreadPool threadPool_;
    ConnectionManager connectionManager_;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> nextConnectionId_{1};

    TcpMessageHandler messageHandler_;
    TcpConnectHandler connectHandler_;
    TcpDisconnectHandler disconnectHandler_;

    std::thread loopThread_;
    std::thread maintenanceThread_;
    std::atomic<bool> stopMaintenance_{false};
};

} // namespace netcore
