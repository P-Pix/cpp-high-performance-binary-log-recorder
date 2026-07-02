#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace hpblr {

enum class LogLevel {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3
};

class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level);
    void set_file(const std::filesystem::path& path);
    void log(LogLevel level, std::string_view component, std::string_view message);

private:
    Logger() = default;

    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::info;
    std::ofstream file_;
};

} // namespace hpblr
