/**
 * @file time_utils.hpp
 * @brief Déclare les utilitaires de timestamp nanoseconde et de formatage UTC.
 *
 * Ces fonctions donnent une représentation temporelle commune aux événements et aux journaux de diagnostic.
 */

#pragma once

#include <cstdint>
#include <string>

namespace hpblr {

[[nodiscard]] std::uint64_t now_unix_ns();
[[nodiscard]] std::string format_unix_ns_utc(std::uint64_t timestamp_ns);

} // namespace hpblr
