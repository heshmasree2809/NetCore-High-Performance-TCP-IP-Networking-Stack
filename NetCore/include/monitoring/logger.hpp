#pragma once

#include "config/config.hpp"
#include <string>
#include <sstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

namespace netcore {

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) {
        currentLevel_ = level;
    }

    LogLevel getLogLevel() const {
        return currentLevel_;
    }

    void log(LogLevel level, const std::string& file, int line, const std::string& message) {
        if (static_cast<int>(level) < static_cast<int>(currentLevel_)) {
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tmNow{};
        localtime_r(&nowTime, &tmNow);

        std::ostringstream ss;
        ss << "[" << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S") << "."
           << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[" << logLevelToString(level) << "] "
           << "[tid:" << std::this_thread::get_id() << "] "
           << message;

        if (level == LogLevel::DEBUG) {
            ss << " (" << file << ":" << line << ")";
        }

        std::string formatted = ss.str();

        std::lock_guard<std::mutex> lock(mutex_);
        if (level == LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }

private:
    Logger() : currentLevel_(LogLevel::INFO) {}
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static const char* logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }

    LogLevel currentLevel_;
    std::mutex mutex_;
};

} // namespace netcore

#define LOG_DEBUG(msg) do { \
    std::ostringstream _ss; \
    _ss << msg; \
    netcore::Logger::getInstance().log(netcore::LogLevel::DEBUG, __FILE__, __LINE__, _ss.str()); \
} while(0)

#define LOG_INFO(msg) do { \
    std::ostringstream _ss; \
    _ss << msg; \
    netcore::Logger::getInstance().log(netcore::LogLevel::INFO, __FILE__, __LINE__, _ss.str()); \
} while(0)

#define LOG_WARN(msg) do { \
    std::ostringstream _ss; \
    _ss << msg; \
    netcore::Logger::getInstance().log(netcore::LogLevel::WARNING, __FILE__, __LINE__, _ss.str()); \
} while(0)

#define LOG_ERROR(msg) do { \
    std::ostringstream _ss; \
    _ss << msg; \
    netcore::Logger::getInstance().log(netcore::LogLevel::ERROR, __FILE__, __LINE__, _ss.str()); \
} while(0)
