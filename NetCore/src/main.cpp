#include "config/config.hpp"
#include "monitoring/logger.hpp"
#include "monitoring/statistics.hpp"
#include "net/tcp_server.hpp"
#include "net/udp_server.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>

namespace {
    std::atomic<bool> g_running{true};

    void signalHandler(int signum) {
        if (signum == SIGINT || signum == SIGTERM) {
            std::cout << "\n[NetCore] Caught shutdown signal (" 
                      << (signum == SIGINT ? "SIGINT" : "SIGTERM") 
                      << "). Initiating graceful shutdown...\n" << std::endl;
            g_running.store(false, std::memory_order_release);
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to prevent crash on writing to closed socket

    std::cout << "========================================================\n"
              << "  NetCore — High-Performance TCP/IP Networking Stack   \n"
              << "========================================================\n"
              << "Architecture: Linux epoll + Non-blocking I/O + ThreadPool\n"
              << "Target Domain: Modem Software / High-Throughput Networking\n"
              << "Standards    : C++17, POSIX.1-2008, IPv4\n"
              << "========================================================\n\n";

    // 1. Load configuration
    std::string configFile = "configs/netcore.conf";
    for (int i = 1; i < argc; ++i) {
        if ((std::string(argv[i]) == "--config" || std::string(argv[i]) == "-c") && i + 1 < argc) {
            configFile = argv[i + 1];
        }
    }

    netcore::ConfigParser configParser;
    if (!configParser.loadFromFile(configFile)) {
        LOG_WARN("Could not load config from " << configFile << ", using defaults.");
    } else {
        LOG_INFO("Loaded configuration from " << configFile);
    }

    // Apply command-line overrides
    configParser.parseCommandLine(argc, argv);

    const auto& config = configParser.getConfig();
    netcore::Logger::getInstance().setLogLevel(config.logLevel);

    LOG_INFO("Config: TCP Port=" << config.serverPort 
             << ", UDP Port=" << config.udpPort
             << ", Workers=" << config.workerThreads 
             << ", MaxConns=" << config.maxConnections);

    // 2. Initialize TCP Server
    netcore::TcpServer tcpServer(config);

    tcpServer.setConnectHandler([](netcore::ConnectionPtr conn) {
        LOG_INFO("[TCP Handshake Complete] New peer: " << conn->getPeerEndpoint().toString()
                 << " assigned Connection ID #" << conn->getId());
    });

    tcpServer.setMessageHandler([&tcpServer](netcore::ConnectionPtr conn, const std::vector<uint8_t>& data) {
        std::string req(reinterpret_cast<const char*>(data.data()), data.size());
        
        // Strip trailing newlines for command checks
        std::string trimmed = req;
        while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
            trimmed.pop_back();
        }

        if (trimmed == "STATS") {
            std::string summary = netcore::StatisticsManager::getInstance().formatSummary();
            tcpServer.send(conn->getId(), summary);
        } else if (trimmed == "STATS_JSON") {
            std::string json = netcore::StatisticsManager::getInstance().formatJson() + "\n";
            tcpServer.send(conn->getId(), json);
        } else if (trimmed == "PING") {
            tcpServer.send(conn->getId(), "PONG\n");
        } else if (trimmed == "QUIT") {
            tcpServer.send(conn->getId(), "BYE\n");
            tcpServer.closeConnection(conn->getId());
        } else {
            // High-throughput echo mode (default for benchmark testing)
            tcpServer.send(conn->getId(), data.data(), data.size());
        }
    });

    tcpServer.setDisconnectHandler([](netcore::ConnectionPtr conn) {
        LOG_INFO("[TCP Closed] Peer disconnected: " << conn->getPeerEndpoint().toString()
                 << " (Transferred: " << conn->getBytesReceived() << " B recv, " 
                 << conn->getBytesSent() << " B sent)");
    });

    if (!tcpServer.init() || !tcpServer.start()) {
        LOG_ERROR("Failed to start NetCore TCP Server!");
        return 1;
    }

    // 3. Initialize UDP Server
    netcore::UdpServer udpServer(config);
    udpServer.setDatagramHandler([&udpServer](const netcore::Endpoint& src, const std::vector<uint8_t>& data) {
        // Echo datagram back to sender
        udpServer.sendDatagram(src, data.data(), data.size());
    });

    if (!udpServer.init() || !udpServer.start()) {
        LOG_WARN("Failed to start NetCore UDP Server. Continuing with TCP only.");
    }

    LOG_INFO("NetCore is active and serving traffic. Press Ctrl+C to stop.");

    // Main control loop - prints periodic statistics every 10 seconds
    int statsCounter = 0;
    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        statsCounter++;
        if (statsCounter >= 20) { // every 10s
            statsCounter = 0;
            std::cout << netcore::StatisticsManager::getInstance().formatSummary() << std::endl;
        }
    }

    // Graceful Shutdown Sequence
    std::cout << "\nInitiating graceful shutdown sequence:\n";
    std::cout << "1. Cease accepting incoming TCP/UDP connections...\n";
    std::cout << "2. Draining active connection write queues and in-flight tasks...\n";
    tcpServer.stop();
    udpServer.stop();

    std::cout << "3. Worker threads halted safely.\n";
    std::cout << "4. Sockets and epoll descriptors closed.\n";
    std::cout << "5. Final Run Statistics:\n";
    std::cout << netcore::StatisticsManager::getInstance().formatSummary() << std::endl;
    std::cout << "6. All resources cleanly released via RAII. NetCore shutdown complete.\n";

    return 0;
}
