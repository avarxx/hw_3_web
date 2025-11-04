#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace network {

class Logger {
   public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static void log(Level level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        const char* level_str = levelToString(level);
        std::cout << "[" << ss.str() << "] [" << level_str << "] " << message << std::endl;
    }

    static void debug(const std::string& message) { log(Level::DEBUG, message); }
    static void info(const std::string& message) { log(Level::INFO, message); }
    static void warning(const std::string& message) { log(Level::WARNING, message); }
    static void error(const std::string& message) { log(Level::ERROR, message); }

   private:
    static const char* levelToString(Level level) {
        switch (level) {
            case Level::DEBUG:
                return "DEBUG";
            case Level::INFO:
                return "INFO";
            case Level::WARNING:
                return "WARN";
            case Level::ERROR:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }
};

}  // namespace network
