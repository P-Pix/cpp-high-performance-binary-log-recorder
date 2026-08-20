/**
 * @file logger.cpp
 * @brief Implémente la sortie de diagnostic synchronisée des outils HPBLR.
 *
 * Le verrou protège la configuration et évite l’entrelacement des lignes écrites par plusieurs threads.
 */

#include "hpblr/logger.hpp"

#include "hpblr/time_utils.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace hpblr {
namespace {

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::debug:
        return "DEBUG";
    case LogLevel::info:
        return "INFO";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::error:
        return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::set_file(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(path, std::ios::app);
    if (!file_) {
        throw std::runtime_error("failed to open log file: " + path.string());
    }
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(min_level_)) {
        return;
    }

    const std::string line = format_unix_ns_utc(now_unix_ns()) + " [" + std::string(to_string(level)) + "] [" +
                             std::string(component) + "] " + std::string(message);

    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << line << '\n';
    if (file_) {
        file_ << line << '\n';
        file_.flush();
    }
}

} // namespace hpblr
