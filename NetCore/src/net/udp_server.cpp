#include "net/udp_server.hpp"
#include "monitoring/logger.hpp"
#include "monitoring/statistics.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace netcore {

UdpServer::UdpServer(const ServerConfig& config)
    : config_(config),
      socket_(Socket::Type::UDP),
      eventLoop_(64),
      threadPool_(config.workerThreads) {
}

UdpServer::~UdpServer() {
    stop();
}

bool UdpServer::init() {
    LOG_INFO("Initializing NetCore UDP Server on port " << config_.udpPort);

    if (!socket_.create(Socket::Type::UDP)) {
        LOG_ERROR("Failed to create UDP socket");
        return false;
    }

    if (!socket_.setReuseAddr(true)) {
        LOG_WARN("Failed to set SO_REUSEADDR on UDP socket");
    }

    if (!socket_.setNonBlocking(true)) {
        LOG_ERROR("Failed to set UDP socket non-blocking");
        return false;
    }

    if (!socket_.bind(config_.bindAddress, config_.udpPort)) {
        LOG_ERROR("Failed to bind UDP socket to port " << config_.udpPort);
        return false;
    }

    bool registered = eventLoop_.registerSocket(
        socket_.getFd(),
        EPOLLIN | EPOLLET,
        [this](int fd, uint32_t events) {
            handleSocketEvent(fd, events);
        }
    );

    if (!registered) {
        LOG_ERROR("Failed to register UDP socket with epoll");
        return false;
    }

    LOG_INFO("UDP Server initialized on port " << config_.udpPort);
    return true;
}

bool UdpServer::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return true;
    }

    threadPool_.start();

    loopThread_ = std::thread([this]() {
        eventLoop_.run();
    });

    LOG_INFO("NetCore UDP Server running on port " << config_.udpPort);
    return true;
}

void UdpServer::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    LOG_INFO("Shutting down UDP Server...");
    eventLoop_.stop();
    if (loopThread_.joinable()) {
        loopThread_.join();
    }

    socket_.close();
    threadPool_.shutdown();
    LOG_INFO("UDP Server stopped.");
}

void UdpServer::handleSocketEvent(int fd, uint32_t events) {
    (void)fd;
    if (!(events & EPOLLIN)) {
        return;
    }

    uint8_t buffer[65535]; // Maximum UDP datagram payload size

    // Edge-triggered non-blocking drain loop
    while (true) {
        Endpoint src;
        ssize_t bytes = socket_.recvfrom(buffer, sizeof(buffer), src);
        if (bytes > 0) {
            StatisticsManager::getInstance().onBytesReceived(static_cast<size_t>(bytes));
            StatisticsManager::getInstance().onMessageReceived();

            if (datagramHandler_) {
                std::vector<uint8_t> data(buffer, buffer + bytes);
                threadPool_.execute([this, src, dat = std::move(data)]() {
                    datagramHandler_(src, dat);
                });
            }
        } else if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more datagrams queued
            }
            if (errno == EINTR) {
                continue;
            }
            LOG_WARN("UDP recvfrom error: " << strerror(errno));
            break;
        } else {
            break;
        }
    }
}

ssize_t UdpServer::sendDatagram(const Endpoint& dest, const void* data, size_t len) {
    ssize_t sent = socket_.sendto(data, len, dest);
    if (sent > 0) {
        StatisticsManager::getInstance().onBytesSent(static_cast<size_t>(sent));
        StatisticsManager::getInstance().onMessageSent();
    }
    return sent;
}

ssize_t UdpServer::sendDatagram(const Endpoint& dest, const std::string& message) {
    return sendDatagram(dest, message.data(), message.size());
}

} // namespace netcore
