#include "monitoring/statistics.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace netcore {

StatisticsManager::StatisticsManager() {
    startTime_ = std::chrono::steady_clock::now();
    lastSampleTime_ = startTime_;
    reset();
}

void StatisticsManager::reset() {
    activeConnections_.store(0);
    totalConnections_.store(0);
    totalBytesReceived_.store(0);
    totalBytesSent_.store(0);
    messagesReceived_.store(0);
    messagesSent_.store(0);
    connectionErrors_.store(0);
    totalLatencyUs_.store(0);
    latencySamples_.store(0);
    {
        std::lock_guard<std::mutex> lock(latencyMutex_);
        recentLatenciesUs_.clear();
    }
    startTime_ = std::chrono::steady_clock::now();
    lastSampleTime_ = startTime_;
    lastMessages_ = 0;
    lastBytes_ = 0;
    cachedRps_ = 0.0;
    cachedThroughputMBs_ = 0.0;
}

void StatisticsManager::onConnectionOpened() {
    activeConnections_.fetch_add(1, std::memory_order_relaxed);
    totalConnections_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsManager::onConnectionClosed() {
    uint64_t current = activeConnections_.load(std::memory_order_relaxed);
    while (current > 0 && !activeConnections_.compare_exchange_weak(current, current - 1, std::memory_order_relaxed)) {
        // retry
    }
}

void StatisticsManager::onConnectionError() {
    connectionErrors_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsManager::onBytesReceived(size_t bytes) {
    totalBytesReceived_.fetch_add(bytes, std::memory_order_relaxed);
}

void StatisticsManager::onBytesSent(size_t bytes) {
    totalBytesSent_.fetch_add(bytes, std::memory_order_relaxed);
}

void StatisticsManager::onMessageReceived() {
    messagesReceived_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsManager::onMessageSent() {
    messagesSent_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsManager::recordLatencyUs(uint64_t latencyUs) {
    totalLatencyUs_.fetch_add(latencyUs, std::memory_order_relaxed);
    latencySamples_.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(latencyMutex_);
    if (recentLatenciesUs_.size() >= kMaxLatencyHistory) {
        recentLatenciesUs_.erase(recentLatenciesUs_.begin());
    }
    recentLatenciesUs_.push_back(latencyUs);
}

std::string StatisticsManager::formatBytes(uint64_t bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        ss << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else if (bytes >= 1024ULL * 1024ULL) {
        ss << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024ULL) {
        ss << (static_cast<double>(bytes) / 1024.0) << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

StatisticsSnapshot StatisticsManager::getSnapshot() {
    auto now = std::chrono::steady_clock::now();
    double totalUptimeSec = std::chrono::duration<double>(now - startTime_).count();
    if (totalUptimeSec < 0.001) totalUptimeSec = 0.001;

    double dt = std::chrono::duration<double>(now - lastSampleTime_).count();
    uint64_t curMessages = messagesReceived_.load(std::memory_order_relaxed);
    uint64_t curBytes = totalBytesReceived_.load(std::memory_order_relaxed) + totalBytesSent_.load(std::memory_order_relaxed);

    if (dt >= 0.25) { // update rate estimates every 250ms
        uint64_t diffMessages = (curMessages >= lastMessages_) ? (curMessages - lastMessages_) : 0;
        uint64_t diffBytes = (curBytes >= lastBytes_) ? (curBytes - lastBytes_) : 0;

        cachedRps_ = static_cast<double>(diffMessages) / dt;
        cachedThroughputMBs_ = (static_cast<double>(diffBytes) / (1024.0 * 1024.0)) / dt;

        lastMessages_ = curMessages;
        lastBytes_ = curBytes;
        lastSampleTime_ = now;
    }

    StatisticsSnapshot snap;
    snap.activeConnections = activeConnections_.load(std::memory_order_relaxed);
    snap.totalConnections = totalConnections_.load(std::memory_order_relaxed);
    snap.bytesReceived = totalBytesReceived_.load(std::memory_order_relaxed);
    snap.bytesSent = totalBytesSent_.load(std::memory_order_relaxed);
    snap.messagesReceived = curMessages;
    snap.messagesSent = messagesSent_.load(std::memory_order_relaxed);
    snap.connectionErrors = connectionErrors_.load(std::memory_order_relaxed);
    snap.uptimeSec = totalUptimeSec;

    uint64_t samples = latencySamples_.load(std::memory_order_relaxed);
    uint64_t totalUs = totalLatencyUs_.load(std::memory_order_relaxed);
    if (samples > 0) {
        snap.averageLatencyMs = (static_cast<double>(totalUs) / static_cast<double>(samples)) / 1000.0;
    } else {
        snap.averageLatencyMs = 0.0;
    }

    // Calculate percentiles
    std::vector<uint64_t> latCopy;
    {
        std::lock_guard<std::mutex> lock(latencyMutex_);
        latCopy = recentLatenciesUs_;
    }

    if (!latCopy.empty()) {
        std::sort(latCopy.begin(), latCopy.end());
        snap.p50LatencyMs = static_cast<double>(latCopy[latCopy.size() * 50 / 100]) / 1000.0;
        snap.p95LatencyMs = static_cast<double>(latCopy[latCopy.size() * 95 / 100]) / 1000.0;
        snap.p99LatencyMs = static_cast<double>(latCopy[latCopy.size() * 99 / 100]) / 1000.0;
    } else {
        snap.p50LatencyMs = snap.averageLatencyMs;
        snap.p95LatencyMs = snap.averageLatencyMs;
        snap.p99LatencyMs = snap.averageLatencyMs;
    }

    snap.requestsPerSec = cachedRps_;
    snap.throughputMBs = cachedThroughputMBs_;
    return snap;
}

std::string StatisticsManager::formatSummary() {
    auto snap = getSnapshot();
    std::ostringstream ss;
    ss << "\n================ NetCore Statistics ================\n";
    ss << "Uptime             : " << std::fixed << std::setprecision(1) << snap.uptimeSec << " s\n";
    ss << "Active Connections : " << snap.activeConnections << "\n";
    ss << "Total Connections  : " << snap.totalConnections << "\n";
    ss << "Bytes Received     : " << formatBytes(snap.bytesReceived) << "\n";
    ss << "Bytes Sent         : " << formatBytes(snap.bytesSent) << "\n";
    ss << "Requests/sec       : " << std::fixed << std::setprecision(1) << snap.requestsPerSec << " req/s\n";
    ss << "Throughput         : " << std::fixed << std::setprecision(2) << snap.throughputMBs << " MB/s\n";
    ss << "Average Latency    : " << std::fixed << std::setprecision(3) << snap.averageLatencyMs << " ms\n";
    ss << "Latency p50        : " << std::fixed << std::setprecision(3) << snap.p50LatencyMs << " ms\n";
    ss << "Latency p95        : " << std::fixed << std::setprecision(3) << snap.p95LatencyMs << " ms\n";
    ss << "Latency p99        : " << std::fixed << std::setprecision(3) << snap.p99LatencyMs << " ms\n";
    ss << "Connection Errors  : " << snap.connectionErrors << "\n";
    ss << "====================================================\n";
    return ss.str();
}

std::string StatisticsManager::formatJson() {
    auto snap = getSnapshot();
    std::ostringstream ss;
    ss << "{"
       << "\"activeConnections\":" << snap.activeConnections << ","
       << "\"totalConnections\":" << snap.totalConnections << ","
       << "\"bytesReceived\":" << snap.bytesReceived << ","
       << "\"bytesSent\":" << snap.bytesSent << ","
       << "\"messagesReceived\":" << snap.messagesReceived << ","
       << "\"messagesSent\":" << snap.messagesSent << ","
       << "\"connectionErrors\":" << snap.connectionErrors << ","
       << "\"averageLatencyMs\":" << std::fixed << std::setprecision(3) << snap.averageLatencyMs << ","
       << "\"p50LatencyMs\":" << std::fixed << std::setprecision(3) << snap.p50LatencyMs << ","
       << "\"p95LatencyMs\":" << std::fixed << std::setprecision(3) << snap.p95LatencyMs << ","
       << "\"p99LatencyMs\":" << std::fixed << std::setprecision(3) << snap.p99LatencyMs << ","
       << "\"requestsPerSec\":" << std::fixed << std::setprecision(2) << snap.requestsPerSec << ","
       << "\"throughputMBs\":" << std::fixed << std::setprecision(2) << snap.throughputMBs << ","
       << "\"uptimeSec\":" << std::fixed << std::setprecision(1) << snap.uptimeSec
       << "}";
    return ss.str();
}

} // namespace netcore
