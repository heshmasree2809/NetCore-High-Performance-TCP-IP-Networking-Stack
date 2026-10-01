#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <chrono>
#include <vector>
#include <mutex>

namespace netcore {

struct StatisticsSnapshot {
    uint64_t activeConnections{0};
    uint64_t totalConnections{0};
    uint64_t bytesReceived{0};
    uint64_t bytesSent{0};
    uint64_t messagesReceived{0};
    uint64_t messagesSent{0};
    uint64_t connectionErrors{0};
    double averageLatencyMs{0.0};
    double p50LatencyMs{0.0};
    double p95LatencyMs{0.0};
    double p99LatencyMs{0.0};
    double requestsPerSec{0.0};
    double throughputMBs{0.0};
    double uptimeSec{0.0};
};

class StatisticsManager {
public:
    static StatisticsManager& getInstance() {
        static StatisticsManager instance;
        return instance;
    }

    void reset();

    // Connection tracking
    void onConnectionOpened();
    void onConnectionClosed();
    void onConnectionError();

    // Data transfer tracking
    void onBytesReceived(size_t bytes);
    void onBytesSent(size_t bytes);
    void onMessageReceived();
    void onMessageSent();

    // Latency tracking (in microseconds)
    void recordLatencyUs(uint64_t latencyUs);

    // Get current calculated snapshot
    StatisticsSnapshot getSnapshot();

    // Formatted ASCII dashboard string
    std::string formatSummary();

    // JSON format string for telemetry / web dashboard
    std::string formatJson();

private:
    StatisticsManager();
    ~StatisticsManager() = default;

    std::atomic<uint64_t> activeConnections_{0};
    std::atomic<uint64_t> totalConnections_{0};
    std::atomic<uint64_t> totalBytesReceived_{0};
    std::atomic<uint64_t> totalBytesSent_{0};
    std::atomic<uint64_t> messagesReceived_{0};
    std::atomic<uint64_t> messagesSent_{0};
    std::atomic<uint64_t> connectionErrors_{0};

    // Latency metrics
    std::atomic<uint64_t> totalLatencyUs_{0};
    std::atomic<uint64_t> latencySamples_{0};
    mutable std::mutex latencyMutex_;
    std::vector<uint64_t> recentLatenciesUs_;
    static constexpr size_t kMaxLatencyHistory = 5000;

    // Throughput and rate calculations
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point lastSampleTime_;
    uint64_t lastMessages_{0};
    uint64_t lastBytes_{0};
    double cachedRps_{0.0};
    double cachedThroughputMBs_{0.0};

    static std::string formatBytes(uint64_t bytes);
};

} // namespace netcore
