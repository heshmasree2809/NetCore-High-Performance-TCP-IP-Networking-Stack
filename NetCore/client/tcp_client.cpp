#include "net/socket.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --host <ip>         Target IP address (default: 127.0.0.1)\n"
              << "  --port <port>       Target port (default: 8080)\n"
              << "  --interactive       Launch interactive interactive terminal session\n"
              << "  --batch <count>     Run batch ping test for <count> iterations\n"
              << "  --msg <string>      Custom message for single/batch send (default: PING)\n"
              << "  --help              Display this help message\n";
}

int runInteractive(const std::string& host, uint16_t port) {
    netcore::Socket sock(netcore::Socket::Type::TCP);
    std::cout << "Connecting to " << host << ":" << port << "..." << std::flush;
    if (!sock.connect(host, port, 5000)) {
        std::cerr << " FAILED! Could not connect.\n";
        return 1;
    }
    std::cout << " CONNECTED!\n";
    sock.setTcpNoDelay(true);
    sock.setNonBlocking(false);

    std::cout << "Type message and press Enter (commands: 'QUIT' to exit, 'STATS' for server stats):\n";
    std::string line;
    std::vector<char> buffer(8192);

    while (true) {
        std::cout << "netcore> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) continue;
        if (line == "QUIT" || line == "quit" || line == "exit") {
            std::cout << "Closing connection.\n";
            break;
        }

        auto start = std::chrono::steady_clock::now();
        ssize_t sent = sock.send(line.data(), line.size());
        if (sent <= 0) {
            std::cerr << "Send failed (connection lost)\n";
            break;
        }

        ssize_t recvd = sock.recv(buffer.data(), buffer.size() - 1);
        auto end = std::chrono::steady_clock::now();

        if (recvd <= 0) {
            std::cerr << "Connection closed by remote peer.\n";
            break;
        }

        buffer[static_cast<size_t>(recvd)] = '\0';
        auto rttUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout << "< [RTT: " << std::fixed << std::setprecision(2) << (static_cast<double>(rttUs) / 1000.0) 
                  << " ms | " << recvd << " bytes] " << buffer.data() << "\n";
    }

    sock.close();
    return 0;
}

int runBatch(const std::string& host, uint16_t port, size_t count, const std::string& msg) {
    netcore::Socket sock(netcore::Socket::Type::TCP);
    std::cout << "Connecting to " << host << ":" << port << " for " << count << " batch requests...\n";
    if (!sock.connect(host, port, 5000)) {
        std::cerr << "Connection failed!\n";
        return 1;
    }
    sock.setTcpNoDelay(true);
    sock.setNonBlocking(false);

    std::vector<char> buffer(8192);
    size_t successful = 0;
    uint64_t totalRttUs = 0;
    uint64_t minRttUs = UINT64_MAX;
    uint64_t maxRttUs = 0;

    auto batchStart = std::chrono::steady_clock::now();

    for (size_t i = 0; i < count; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        ssize_t sent = sock.send(msg.data(), msg.size());
        if (sent <= 0) break;

        ssize_t recvd = sock.recv(buffer.data(), buffer.size());
        if (recvd <= 0) break;

        auto t1 = std::chrono::steady_clock::now();
        uint64_t rtt = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());

        successful++;
        totalRttUs += rtt;
        if (rtt < minRttUs) minRttUs = rtt;
        if (rtt > maxRttUs) maxRttUs = rtt;
    }

    auto batchEnd = std::chrono::steady_clock::now();
    double totalSec = std::chrono::duration<double>(batchEnd - batchStart).count();

    sock.close();

    std::cout << "\n--- Batch Run Summary ---\n"
              << "Requests Attempted: " << count << "\n"
              << "Requests Completed: " << successful << "\n"
              << "Elapsed Time      : " << std::fixed << std::setprecision(3) << totalSec << " s\n";

    if (successful > 0) {
        double avgRttMs = (static_cast<double>(totalRttUs) / static_cast<double>(successful)) / 1000.0;
        double rps = static_cast<double>(successful) / totalSec;
        std::cout << "Throughput        : " << std::fixed << std::setprecision(1) << rps << " req/s\n"
                  << "Min Latency       : " << (static_cast<double>(minRttUs) / 1000.0) << " ms\n"
                  << "Avg Latency       : " << avgRttMs << " ms\n"
                  << "Max Latency       : " << (static_cast<double>(maxRttUs) / 1000.0) << " ms\n";
    }

    return (successful == count) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    bool interactive = false;
    size_t batchCount = 0;
    std::string message = "PING";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        } else if (arg == "--batch" && i + 1 < argc) {
            batchCount = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--msg" && i + 1 < argc) {
            message = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (interactive) {
        return runInteractive(host, port);
    } else if (batchCount > 0) {
        return runBatch(host, port, batchCount, message);
    } else {
        // Default single ping test
        return runBatch(host, port, 1, message);
    }
}
