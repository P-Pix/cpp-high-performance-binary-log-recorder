/**
 * @file event.cpp
 * @brief Implémente les conversions et représentations utilitaires du modèle Event.
 *
 * Ces helpers sont partagés par le lecteur, les filtres et les outils d’export.
 */

#include "hpblr/event.hpp"

#include <array>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace hpblr {

std::string_view severity_to_string(Severity severity) noexcept {
    switch (severity) {
    case Severity::trace:
        return "trace";
    case Severity::debug:
        return "debug";
    case Severity::info:
        return "info";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    case Severity::critical:
        return "critical";
    }
    return "unknown";
}

std::optional<Severity> severity_from_u16(std::uint16_t value) noexcept {
    switch (value) {
    case 0:
        return Severity::trace;
    case 1:
        return Severity::debug;
    case 2:
        return Severity::info;
    case 3:
        return Severity::warning;
    case 4:
        return Severity::error;
    case 5:
        return Severity::critical;
    default:
        return std::nullopt;
    }
}

std::optional<Severity> severity_from_string(std::string_view value) noexcept {
    if (value == "trace") {
        return Severity::trace;
    }
    if (value == "debug") {
        return Severity::debug;
    }
    if (value == "info") {
        return Severity::info;
    }
    if (value == "warning" || value == "warn") {
        return Severity::warning;
    }
    if (value == "error") {
        return Severity::error;
    }
    if (value == "critical" || value == "crit") {
        return Severity::critical;
    }

    unsigned int numeric = 0;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto result = std::from_chars(begin, end, numeric);
    if (result.ec == std::errc{} && result.ptr == end && numeric <= 5U) {
        return severity_from_u16(static_cast<std::uint16_t>(numeric));
    }
    return std::nullopt;
}

std::uint16_t severity_to_u16(Severity severity) noexcept {
    return static_cast<std::uint16_t>(severity);
}

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes, std::size_t max_bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    const std::size_t count = bytes.size() < max_bytes ? bytes.size() : max_bytes;
    for (std::size_t i = 0; i < count; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    if (count < bytes.size()) {
        oss << "...";
    }
    return oss.str();
}

std::vector<std::uint8_t> string_to_bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

} // namespace hpblr
