#pragma once

#include "net/socket.hpp"
#include "core/event_loop.hpp"
#include "core/thread_pool.hpp"
#include "config/config.hpp"
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>

namespace netcore {

using UdpDatagramHandler = std::function<void(const Endpoint& src, const std::vector<uint8_t>& data)>;

class UdpServer {
public:
    explicit UdpServer(const ServerConfig& config);
    ~UdpServer();

    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    bool init();
    bool start();
    void stop();

    // Send UDP datagram to destination
    ssize_t sendDatagram(const Endpoint& dest, const void* data, size_t len);
    ssize_t sendDatagram(const Endpoint& dest, const std::string& message);

    void setDatagramHandler(UdpDatagramHandler handler) { datagramHandler_ = std::move(handler); }
    bool isRunning() const { return running_.load(std::memory_order_acquire); }
    uint16_t getPort() const { return config_.udpPort; }

private:
    void handleSocketEvent(int fd, uint32_t events);

    ServerConfig config_;
    Socket socket_;
    EventLoop eventLoop_;
    ThreadPool threadPool_;
    std::atomic<bool> running_{false};
    UdpDatagramHandler datagramHandler_;
    std::thread loopThread_;
};

} // namespace netcore
