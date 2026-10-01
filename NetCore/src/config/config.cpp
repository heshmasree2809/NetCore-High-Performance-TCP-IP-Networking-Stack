#include "config/config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace netcore {

std::string ConfigParser::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = str.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));

    return std::string(start, end + 1);
}

std::string ConfigParser::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "INFO";
    }
}

LogLevel ConfigParser::stringToLogLevel(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "DEBUG") return LogLevel::DEBUG;
    if (upper == "INFO") return LogLevel::INFO;
    if (upper == "WARNING" || upper == "WARN") return LogLevel::WARNING;
    if (upper == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;
}

bool ConfigParser::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, eqPos));
        std::string value = trim(line.substr(eqPos + 1));

        try {
            if (key == "server_port") {
                config_.serverPort = static_cast<uint16_t>(std::stoi(value));
            } else if (key == "udp_port") {
                config_.udpPort = static_cast<uint16_t>(std::stoi(value));
            } else if (key == "worker_threads") {
                config_.workerThreads = static_cast<size_t>(std::stoul(value));
            } else if (key == "max_connections") {
                config_.maxConnections = static_cast<size_t>(std::stoul(value));
            } else if (key == "connection_timeout") {
                config_.connectionTimeoutSec = static_cast<uint32_t>(std::stoul(value));
            } else if (key == "buffer_size") {
                config_.bufferSize = static_cast<size_t>(std::stoul(value));
            } else if (key == "backlog") {
                config_.backlog = static_cast<size_t>(std::stoul(value));
            } else if (key == "log_level") {
                config_.logLevel = stringToLogLevel(value);
            } else if (key == "log_file") {
                config_.logFile = value;
            } else if (key == "tcp_nodelay") {
                config_.tcpNoDelay = (value == "true" || value == "1" || value == "yes");
            } else if (key == "keepalive" || key == "keep_alive") {
                config_.keepAlive = (value == "true" || value == "1" || value == "yes");
            } else if (key == "bind_address") {
                config_.bindAddress = value;
            } else if (key == "stats_port") {
                config_.statsPort = static_cast<uint16_t>(std::stoi(value));
            }
        } catch (...) {
            return false;
        }
    }

    return validate();
}

bool ConfigParser::parseCommandLine(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            std::string cfgPath = argv[++i];
            loadFromFile(cfgPath);
        } else if (arg == "--server_port" || arg == "--server-port") {
            if (i + 1 < argc) config_.serverPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--udp_port" || arg == "--udp-port") {
            if (i + 1 < argc) config_.udpPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--worker_threads" || arg == "--worker-threads") {
            if (i + 1 < argc) config_.workerThreads = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--max_connections" || arg == "--max-connections") {
            if (i + 1 < argc) config_.maxConnections = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--backlog") {
            if (i + 1 < argc) config_.backlog = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--buffer_size" || arg == "--buffer-size") {
            if (i + 1 < argc) config_.bufferSize = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--keepalive") {
            if (i + 1 < argc) {
                std::string v = argv[++i];
                config_.keepAlive = (v == "true" || v == "1" || v == "yes");
            }
        } else if (arg == "--tcp_nodelay" || arg == "--tcp-nodelay") {
            if (i + 1 < argc) {
                std::string v = argv[++i];
                config_.tcpNoDelay = (v == "true" || v == "1" || v == "yes");
            }
        } else if (arg == "--log_level" || arg == "--log-level") {
            if (i + 1 < argc) config_.logLevel = stringToLogLevel(argv[++i]);
        } else if (arg == "--log_file" || arg == "--log-file") {
            if (i + 1 < argc) config_.logFile = argv[++i];
        }
    }
    return validate();
}

bool ConfigParser::validate() const {
    if (config_.serverPort == 0) return false;
    if (config_.workerThreads == 0 || config_.workerThreads > 256) return false;
    if (config_.maxConnections == 0 || config_.maxConnections > 1000000) return false;
    if (config_.bufferSize < 512 || config_.bufferSize > 10 * 1024 * 1024) return false;
    if (config_.connectionTimeoutSec > 86400) return false;
    return true;
}

} // namespace netcore
