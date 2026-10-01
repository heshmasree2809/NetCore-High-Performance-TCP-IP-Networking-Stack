#include "net/tcp_server.hpp"
#include "monitoring/logger.hpp"
#include "monitoring/statistics.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace netcore {

TcpServer::TcpServer(const ServerConfig& config)
    : config_(config),
      listenSocket_(Socket::Type::TCP),
      eventLoop_(static_cast<int>(config.maxConnections)),
      threadPool_(config.workerThreads) {
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::init() {
    LOG_INFO("Initializing NetCore TCP Server on " << config_.bindAddress << ":" << config_.serverPort);

    if (!listenSocket_.create(Socket::Type::TCP)) {
        LOG_ERROR("Failed to create listen socket");
        return false;
    }

    if (!listenSocket_.setReuseAddr(true)) {
        LOG_WARN("Failed to set SO_REUSEADDR");
    }

    if (!listenSocket_.setReusePort(true)) {
        LOG_WARN("Failed to set SO_REUSEPORT");
    }

    if (!listenSocket_.setNonBlocking(true)) {
        LOG_ERROR("Failed to set listen socket non-blocking");
        return false;
    }

    if (!listenSocket_.bind(config_.bindAddress, config_.serverPort)) {
        LOG_ERROR("Failed to bind listen socket to " << config_.bindAddress << ":" << config_.serverPort);
        return false;
    }

    if (!listenSocket_.listen(static_cast<int>(config_.backlog))) {
        LOG_ERROR("Failed to listen on socket with backlog " << config_.backlog);
        return false;
    }

    // Register listening socket with event loop
    bool registered = eventLoop_.registerSocket(
        listenSocket_.getFd(),
        EPOLLIN | EPOLLET,
        [this](int, uint32_t) {
            handleNewConnection();
        }
    );

    if (!registered) {
        LOG_ERROR("Failed to register listen socket with epoll");
        return false;
    }

    LOG_INFO("TCP Server initialized successfully. Max connections: " << config_.maxConnections 
             << ", Workers: " << config_.workerThreads);
    return true;
}

bool TcpServer::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return true; // Already running
    }

    // Start worker thread pool
    threadPool_.start();

    // Start maintenance thread (connection timeouts and stats logging)
    stopMaintenance_.store(false, std::memory_order_release);
    maintenanceThread_ = std::thread(&TcpServer::periodicMaintenance, this);

    // Run event loop in dedicated loop thread
    loopThread_ = std::thread([this]() {
        eventLoop_.run();
    });

    LOG_INFO("NetCore TCP Server running on port " << config_.serverPort);
    return true;
}

void TcpServer::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    LOG_INFO("Shutting down NetCore TCP Server...");

    // Stop maintenance thread
    stopMaintenance_.store(true, std::memory_order_release);
    if (maintenanceThread_.joinable()) {
        maintenanceThread_.join();
    }

    // Stop event loop
    eventLoop_.stop();
    if (loopThread_.joinable()) {
        loopThread_.join();
    }

    // Close listening socket
    listenSocket_.close();

    // Close all client connections
    connectionManager_.closeAll();

    // Shutdown worker threads
    threadPool_.shutdown();

    LOG_INFO("NetCore TCP Server shutdown completed.");
}

void TcpServer::handleNewConnection() {
    // Edge-triggered epoll requires looping until accept returns EAGAIN/EWOULDBLOCK
    while (true) {
        Endpoint clientEndpoint;
        int clientFd = listenSocket_.accept(clientEndpoint);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // All pending incoming connections accepted
            }
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("accept() error: " << strerror(errno));
            StatisticsManager::getInstance().onConnectionError();
            break;
        }

        if (connectionManager_.getConnectionCount() >= config_.maxConnections) {
            LOG_WARN("Max connections reached (" << config_.maxConnections << "), rejecting " << clientEndpoint.toString());
            ::close(clientFd);
            StatisticsManager::getInstance().onConnectionError();
            continue;
        }

        Socket clientSock(clientFd, Socket::Type::TCP);
        clientSock.setNonBlocking(true);

        if (config_.tcpNoDelay) {
            clientSock.setTcpNoDelay(true);
        }
        if (config_.keepAlive) {
            clientSock.setKeepAlive(true, config_.keepAliveIdle, config_.keepAliveInterval, config_.keepAliveCount);
        }

        uint64_t connId = nextConnectionId_.fetch_add(1, std::memory_order_relaxed);
        Endpoint localEp = clientSock.getLocalEndpoint();
        clientSock.release(); // Transfer ownership of descriptor to Connection

        auto conn = std::make_shared<Connection>(connId, clientFd, localEp, clientEndpoint);
        conn->setState(ConnectionState::CONNECTED);

        connectionManager_.addConnection(conn);
        StatisticsManager::getInstance().onConnectionOpened();

        LOG_INFO("Client connected: " << clientEndpoint.toString() << " (ID #" << connId << ", fd " << clientFd << ")");

        if (connectHandler_) {
            threadPool_.execute([this, conn]() {
                connectHandler_(conn);
            });
        }

        // Register client socket with EPOLLET, EPOLLIN, EPOLLRDHUP
        uint32_t events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        eventLoop_.registerSocket(clientFd, events, [this](int fd, uint32_t revents) {
            handleSocketEvent(fd, revents);
        });
    }
}

void TcpServer::handleSocketEvent(int fd, uint32_t events) {
    auto conn = connectionManager_.getConnection(fd);
    if (!conn) {
        return;
    }

    if (events & (EPOLLERR | EPOLLHUP)) {
        handleError(conn);
        return;
    }

    if (events & EPOLLRDHUP) {
        handleClose(conn);
        return;
    }

    if (events & EPOLLIN) {
        handleRead(conn);
    }

    if (events & EPOLLOUT) {
        handleWrite(conn);
    }
}

void TcpServer::handleRead(ConnectionPtr conn) {
    if (!conn || conn->isClosed()) return;

    conn->setState(ConnectionState::READING);
    uint8_t buffer[8192];
    bool peerClosed = false;

    // Edge-triggered drain loop
    while (true) {
        ssize_t bytesRead = ::recv(conn->getFd(), buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            conn->appendReadData(buffer, static_cast<size_t>(bytesRead));
            StatisticsManager::getInstance().onBytesReceived(static_cast<size_t>(bytesRead));
            StatisticsManager::getInstance().onMessageReceived();
        } else if (bytesRead == 0) {
            peerClosed = true;
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Done reading available data for this event
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            LOG_WARN("recv error on fd " << conn->getFd() << ": " << strerror(errno));
            StatisticsManager::getInstance().onConnectionError();
            peerClosed = true;
            break;
        }
    }

    if (peerClosed) {
        handleClose(conn);
        return;
    }

    // Process buffered data via worker thread pool
    auto receivedData = conn->extractReadBuffer();
    if (!receivedData.empty()) {
        conn->setState(ConnectionState::CONNECTED);

        if (messageHandler_) {
            auto startTime = std::chrono::steady_clock::now();
            threadPool_.execute([this, conn, data = std::move(receivedData), startTime]() {
                messageHandler_(conn, data);
                auto endTime = std::chrono::steady_clock::now();
                auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                StatisticsManager::getInstance().recordLatencyUs(static_cast<uint64_t>(durationUs));
            });
        }
    }
}

void TcpServer::handleWrite(ConnectionPtr conn) {
    if (!conn || conn->isClosed()) return;

    conn->setState(ConnectionState::WRITING);
    bool flushed = conn->flushWrites();

    if (flushed) {
        conn->setState(ConnectionState::CONNECTED);
        // Mod epoll to disable EPOLLOUT interest since queue is empty
        eventLoop_.modifySocket(conn->getFd(), EPOLLIN | EPOLLRDHUP | EPOLLET);
    } else {
        // Still has pending writes; remain interested in EPOLLOUT
        eventLoop_.modifySocket(conn->getFd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
    }
}

void TcpServer::handleError(ConnectionPtr conn) {
    if (!conn) return;
    LOG_WARN("Socket error on " << conn->getPeerEndpoint().toString());
    StatisticsManager::getInstance().onConnectionError();
    handleClose(conn);
}

void TcpServer::handleClose(ConnectionPtr conn) {
    if (!conn || conn->isClosed()) return;

    int fd = conn->getFd();
    Endpoint peer = conn->getPeerEndpoint();

    LOG_INFO("Client disconnected: " << peer.toString() << " (ID #" << conn->getId() << ")");

    eventLoop_.removeSocket(fd);
    connectionManager_.removeConnection(fd);
    StatisticsManager::getInstance().onConnectionClosed();

    if (disconnectHandler_) {
        threadPool_.execute([this, conn]() {
            disconnectHandler_(conn);
        });
    }

    conn->close();
}

bool TcpServer::send(uint64_t connId, const std::string& message) {
    return send(connId, reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

bool TcpServer::send(uint64_t connId, const uint8_t* data, size_t len) {
    auto conn = connectionManager_.getConnectionById(connId);
    if (!conn || conn->isClosed()) {
        return false;
    }

    conn->queueWriteData(data, len);

    // Try flushing immediately from current thread
    bool flushed = conn->flushWrites();
    if (!flushed) {
        // Register EPOLLOUT on event loop to write when socket buffer frees up
        eventLoop_.queueInLoop([this, conn]() {
            eventLoop_.modifySocket(conn->getFd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
        });
    }
    return true;
}

void TcpServer::closeConnection(uint64_t connId) {
    auto conn = connectionManager_.getConnectionById(connId);
    if (conn) {
        handleClose(conn);
    }
}

void TcpServer::periodicMaintenance() {
    while (!stopMaintenance_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (stopMaintenance_.load(std::memory_order_acquire)) break;

        // Cleanup idle / timed out connections
        auto timedOut = connectionManager_.cleanupTimedOutConnections(
            std::chrono::seconds(config_.connectionTimeoutSec)
        );

        for (auto& conn : timedOut) {
            eventLoop_.removeSocket(conn->getFd());
            StatisticsManager::getInstance().onConnectionClosed();
            if (disconnectHandler_) {
                disconnectHandler_(conn);
            }
        }
    }
}

} // namespace netcore
