/**
 * @file event.hpp
 * @brief Définit le modèle d’événement enregistré dans les fichiers HPBLR.
 *
 * Le payload reste opaque pour conserver un cœur générique réutilisable pour télémétrie, instrumentation ou traces techniques.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hpblr
{

    enum class Severity : std::uint16_t
    {
        trace = 0,
        debug = 1,
        info = 2,
        warning = 3,
        error = 4,
        critical = 5
    };

    /**
     * @brief Représente l’unité persistée dans le journal binaire.
     *
     * Le sequence est fourni par l’application productrice et le payload n’est pas interprété par
     * la bibliothèque, ce qui permet de conserver HPBLR indépendant d’un domaine métier précis.
     */
    struct Event
    {
        std::uint64_t timestamp_ns = 0;
        std::uint64_t sequence = 0;
        std::uint32_t producer_id = 0;
        std::uint16_t type = 0;
        Severity severity = Severity::info;
        std::vector<std::uint8_t> payload;
    };

    [[nodiscard]] std::string_view severity_to_string(Severity severity) noexcept;
    [[nodiscard]] std::optional<Severity> severity_from_u16(std::uint16_t value) noexcept;
    [[nodiscard]] std::optional<Severity> severity_from_string(std::string_view value) noexcept;
    [[nodiscard]] std::uint16_t severity_to_u16(Severity severity) noexcept;

    [[nodiscard]] std::string bytes_to_hex(const std::vector<std::uint8_t> &bytes, std::size_t max_bytes = static_cast<std::size_t>(-1));
    [[nodiscard]] std::vector<std::uint8_t> string_to_bytes(std::string_view text);

} // namespace hpblr
