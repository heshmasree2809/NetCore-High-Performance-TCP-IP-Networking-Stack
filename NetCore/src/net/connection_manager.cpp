#include "net/connection_manager.hpp"
#include "monitoring/logger.hpp"

namespace netcore {

ConnectionManager::ConnectionManager() = default;

ConnectionManager::~ConnectionManager() {
    closeAll();
}

bool ConnectionManager::addConnection(ConnectionPtr conn) {
    if (!conn) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    fdMap_[conn->getFd()] = conn;
    idMap_[conn->getId()] = conn;
    return true;
}

ConnectionPtr ConnectionManager::removeConnection(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fdMap_.find(fd);
    if (it == fdMap_.end()) {
        return nullptr;
    }

    ConnectionPtr conn = it->second;
    fdMap_.erase(it);
    idMap_.erase(conn->getId());
    return conn;
}

ConnectionPtr ConnectionManager::getConnection(int fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fdMap_.find(fd);
    return (it != fdMap_.end()) ? it->second : nullptr;
}

ConnectionPtr ConnectionManager::getConnectionById(uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = idMap_.find(id);
    return (it != idMap_.end()) ? it->second : nullptr;
}

size_t ConnectionManager::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fdMap_.size();
}

std::vector<ConnectionPtr> ConnectionManager::getAllConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ConnectionPtr> list;
    list.reserve(fdMap_.size());
    for (const auto& pair : fdMap_) {
        list.push_back(pair.second);
    }
    return list;
}

std::vector<ConnectionPtr> ConnectionManager::cleanupTimedOutConnections(std::chrono::seconds timeout) {
    std::vector<ConnectionPtr> timedOut;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = fdMap_.begin(); it != fdMap_.end(); ) {
            auto conn = it->second;
            auto idleSec = std::chrono::duration_cast<std::chrono::seconds>(now - conn->getLastActivityTime());
            if (idleSec >= timeout && !conn->isClosed()) {
                timedOut.push_back(conn);
                idMap_.erase(conn->getId());
                it = fdMap_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& conn : timedOut) {
        LOG_INFO("Connection timeout for " << conn->getPeerEndpoint().toString() 
                 << " (idle > " << timeout.count() << "s)");
        conn->close();
    }

    return timedOut;
}

void ConnectionManager::closeAll() {
    std::vector<ConnectionPtr> allConns;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : fdMap_) {
            allConns.push_back(pair.second);
        }
        fdMap_.clear();
        idMap_.clear();
    }

    for (auto& conn : allConns) {
        conn->close();
    }
}

} // namespace netcore
