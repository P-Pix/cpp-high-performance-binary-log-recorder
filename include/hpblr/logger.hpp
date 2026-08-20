/**
 * @file logger.hpp
 * @brief Déclare le logger thread-safe des outils HPBLR.
 *
 * Il centralise le niveau minimal et la duplication éventuelle des diagnostics vers un fichier.
 */

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

/**
 * @brief Fournit une journalisation globale sérialisée pour les outils HPBLR.
 *
 * Le logger est partagé entre producteurs et thread writer ; son mutex protège à la fois la
 * configuration et l’émission des lignes console/fichier.
 */

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
