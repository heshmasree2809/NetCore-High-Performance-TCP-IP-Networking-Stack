#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

namespace netcore {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

struct ServerConfig {
    uint16_t serverPort = 8080;
    uint16_t udpPort = 8081;
    size_t workerThreads = 8;
    size_t maxConnections = 10000;
    uint32_t connectionTimeoutSec = 30;
    size_t bufferSize = 8192;
    size_t backlog = 4096;
    LogLevel logLevel = LogLevel::INFO;
    std::string logFile = "logs/netcore.log";
    bool tcpNoDelay = true;
    bool keepAlive = true;
    int keepAliveIdle = 60;
    int keepAliveInterval = 5;
    int keepAliveCount = 3;
    bool enableStatsEndpoint = true;
    uint16_t statsPort = 8082;
    std::string bindAddress = "0.0.0.0";
};

class ConfigParser {
public:
    ConfigParser() = default;
    
    // Load config from key=value file
    bool loadFromFile(const std::string& filepath);
    
    // Command line argument override
    bool parseCommandLine(int argc, char* argv[]);

    // Validate loaded values against safe operational bounds
    bool validate() const;

    const ServerConfig& getConfig() const { return config_; }
    ServerConfig& getMutableConfig() { return config_; }

    static std::string logLevelToString(LogLevel level);
    static LogLevel stringToLogLevel(const std::string& str);

private:
    ServerConfig config_;
    static std::string trim(const std::string& str);
};

} // namespace netcore
