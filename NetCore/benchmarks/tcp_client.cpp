#include "net/socket.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <cstring>

struct BenchmarkConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    size_t clients = 100;
    size_t requestsPerClient = 100;
    size_t payloadSize = 64;
    int timeoutMs = 5000;
};

struct ClientStats {
    uint64_t successfulRequests{0};
    uint64_t failedRequests{0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    std::vector<uint64_t> latenciesUs;
};

void runClientThread(const BenchmarkConfig& config, ClientStats& stats, std::atomic<bool>& startGate) {
    // Wait for synchronized start
    while (!startGate.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    netcore::Socket sock(netcore::Socket::Type::TCP);
    if (!sock.connect(config.host, config.port, config.timeoutMs)) {
        stats.failedRequests += config.requestsPerClient;
        return;
    }

    sock.setNonBlocking(false);
    sock.setTcpNoDelay(true);

    std::string payload(config.payloadSize, 'A');
    std::vector<uint8_t> recvBuf(config.payloadSize + 128);

    stats.latenciesUs.reserve(config.requestsPerClient);

    for (size_t i = 0; i < config.requestsPerClient; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        ssize_t sent = sock.send(payload.data(), payload.size());
        if (sent != static_cast<ssize_t>(payload.size())) {
            stats.failedRequests++;
            break;
        }
        stats.bytesSent += static_cast<uint64_t>(sent);

        // Read response (echo)
        size_t totalReceived = 0;
        bool error = false;
        while (totalReceived < payload.size()) {
            ssize_t recvd = sock.recv(recvBuf.data() + totalReceived, payload.size() - totalReceived);
            if (recvd <= 0) {
                error = true;
                break;
            }
            totalReceived += static_cast<size_t>(recvd);
        }

        if (error || totalReceived < payload.size()) {
            stats.failedRequests++;
            break;
        }

        auto t1 = std::chrono::steady_clock::now();
        uint64_t latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        stats.latenciesUs.push_back(latencyUs);
        stats.bytesReceived += totalReceived;
        stats.successfulRequests++;
    }

    sock.close();
}

int main(int argc, char* argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--clients" && i + 1 < argc) {
            config.clients = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--requests" && i + 1 < argc) {
            config.requestsPerClient = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--payload" && i + 1 < argc) {
            config.payloadSize = static_cast<size_t>(std::stoul(argv[++i]));
        }
    }

    std::cout << "\n======================================================\n"
              << "       NetCore TCP Stress & Benchmark Tool            \n"
              << "======================================================\n"
              << "Target Host       : " << config.host << ":" << config.port << "\n"
              << "Concurrent Clients: " << config.clients << "\n"
              << "Requests / Client : " << config.requestsPerClient << "\n"
              << "Total Requests    : " << (config.clients * config.requestsPerClient) << "\n"
              << "Payload Size      : " << config.payloadSize << " bytes\n"
              << "======================================================\n\n";

    std::vector<ClientStats> allStats(config.clients);
    std::vector<std::thread> threads;
    std::atomic<bool> startGate{false};

    threads.reserve(config.clients);
    for (size_t i = 0; i < config.clients; ++i) {
        threads.emplace_back(runClientThread, std::cref(config), std::ref(allStats[i]), std::ref(startGate));
    }

    // Warm-up and trigger threads simultaneously
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto wallClockStart = std::chrono::steady_clock::now();
    startGate.store(true, std::memory_order_release);

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    auto wallClockEnd = std::chrono::steady_clock::now();
    double totalSeconds = std::chrono::duration<double>(wallClockEnd - wallClockStart).count();

    // Aggregate statistics
    uint64_t totalSuccess = 0;
    uint64_t totalFail = 0;
    uint64_t totalBytesSent = 0;
    uint64_t totalBytesRecv = 0;
    std::vector<uint64_t> allLatencies;

    for (const auto& s : allStats) {
        totalSuccess += s.successfulRequests;
        totalFail += s.failedRequests;
        totalBytesSent += s.bytesSent;
        totalBytesRecv += s.bytesReceived;
        allLatencies.insert(allLatencies.end(), s.latenciesUs.begin(), s.latenciesUs.end());
    }

    std::sort(allLatencies.begin(), allLatencies.end());

    double rps = totalSeconds > 0 ? (static_cast<double>(totalSuccess) / totalSeconds) : 0;
    double throughputMBs = totalSeconds > 0 ? ((static_cast<double>(totalBytesSent + totalBytesRecv) / (1024.0 * 1024.0)) / totalSeconds) : 0;

    double avgLatencyMs = 0;
    double p50Ms = 0, p95Ms = 0, p99Ms = 0, minMs = 0, maxMs = 0;

    if (!allLatencies.empty()) {
        uint64_t sumUs = std::accumulate(allLatencies.begin(), allLatencies.end(), 0ULL);
        avgLatencyMs = (static_cast<double>(sumUs) / static_cast<double>(allLatencies.size())) / 1000.0;
        minMs = static_cast<double>(allLatencies.front()) / 1000.0;
        maxMs = static_cast<double>(allLatencies.back()) / 1000.0;
        p50Ms = static_cast<double>(allLatencies[allLatencies.size() * 50 / 100]) / 1000.0;
        p95Ms = static_cast<double>(allLatencies[allLatencies.size() * 95 / 100]) / 1000.0;
        p99Ms = static_cast<double>(allLatencies[allLatencies.size() * 99 / 100]) / 1000.0;
    }

    std::cout << "\n================ Benchmark Results ================\n"
              << "Duration          : " << std::fixed << std::setprecision(2) << totalSeconds << " s\n"
              << "Successful Reqs   : " << totalSuccess << "\n"
              << "Failed Reqs       : " << totalFail << "\n"
              << "Requests / Sec    : " << std::fixed << std::setprecision(1) << rps << " req/s\n"
              << "Network Throughput: " << std::fixed << std::setprecision(2) << throughputMBs << " MB/s\n"
              << "Latency Min       : " << std::fixed << std::setprecision(3) << minMs << " ms\n"
              << "Latency Avg       : " << std::fixed << std::setprecision(3) << avgLatencyMs << " ms\n"
              << "Latency P50       : " << std::fixed << std::setprecision(3) << p50Ms << " ms\n"
              << "Latency P95       : " << std::fixed << std::setprecision(3) << p95Ms << " ms\n"
              << "Latency P99       : " << std::fixed << std::setprecision(3) << p99Ms << " ms\n"
              << "Latency Max       : " << std::fixed << std::setprecision(3) << maxMs << " ms\n"
              << "====================================================\n\n";

    // Output single CSV line for automated scripting:
    // Clients,RequestsPerClient,TotalReqs,DurationSec,RPS,ThroughputMBs,AvgLatencyMs,P99LatencyMs,Errors
    std::cout << "CSV:" << config.clients << "," << config.requestsPerClient << "," 
              << (config.clients * config.requestsPerClient) << ","
              << totalSeconds << "," << rps << "," << throughputMBs << ","
              << avgLatencyMs << "," << p99Ms << "," << totalFail << std::endl;

    return (totalFail > 0 && totalSuccess == 0) ? 1 : 0;
}
