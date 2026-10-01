#include "net/socket.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

using namespace netcore;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    int durationSec = 5;
    int numThreads = 8;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--duration" && i + 1 < argc) durationSec = std::stoi(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) numThreads = std::stoi(argv[++i]);
    }

    std::cout << "Testing Connection Rate on " << host << ":" << port 
              << " for " << durationSec << "s with " << numThreads << " threads..." << std::endl;

    std::atomic<bool> running{true};
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> failedConnections{0};
    std::vector<std::thread> workers;

    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                Socket sock(Socket::Type::TCP);
                if (sock.create(Socket::Type::TCP) && sock.connect(host, port, 1000)) {
                    totalConnections.fetch_add(1, std::memory_order_relaxed);
                    sock.close();
                } else {
                    failedConnections.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(durationSec));
    running.store(false, std::memory_order_release);

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    double cps = static_cast<double>(totalConnections.load()) / durationSec;
    std::cout << "\n================ Connection Rate Results ================\n"
              << "Duration            : " << durationSec << " s\n"
              << "Total Established   : " << totalConnections.load() << "\n"
              << "Failed              : " << failedConnections.load() << "\n"
              << "Connection Rate     : " << std::fixed << std::setprecision(1) << cps << " conn/sec\n"
              << "=========================================================\n";
    return 0;
}
