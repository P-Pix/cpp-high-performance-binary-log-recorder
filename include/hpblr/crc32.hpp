/**
 * @file crc32.hpp
 * @brief Déclare le CRC-32 utilisé pour protéger les métadonnées et payloads du journal.
 *
 * Le lecteur s’appuie sur ce checksum pour détecter les corruptions avant de reconstruire un événement.
 */

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace hpblr {

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] std::uint32_t crc32(std::string_view text) noexcept;

} // namespace hpblr
