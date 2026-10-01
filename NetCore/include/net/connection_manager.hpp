#pragma once

#include "net/connection.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>

namespace netcore {

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    // Add a newly established connection
    bool addConnection(ConnectionPtr conn);

    // Remove connection by socket file descriptor
    ConnectionPtr removeConnection(int fd);

    // Lookup connection by socket file descriptor
    ConnectionPtr getConnection(int fd) const;

    // Lookup connection by 64-bit connection ID
    ConnectionPtr getConnectionById(uint64_t id) const;

    // Active connection count
    size_t getConnectionCount() const;

    // Get snapshot of all active connections (for inspection/telemetry)
    std::vector<ConnectionPtr> getAllConnections() const;

    // Identify and close connections inactive for longer than timeout
    std::vector<ConnectionPtr> cleanupTimedOutConnections(std::chrono::seconds timeout);

    // Close and remove all active connections
    void closeAll();

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, ConnectionPtr> fdMap_;
    std::unordered_map<uint64_t, ConnectionPtr> idMap_;
};

} // namespace netcore
