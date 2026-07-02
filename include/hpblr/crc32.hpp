#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace hpblr {

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] std::uint32_t crc32(std::string_view text) noexcept;

} // namespace hpblr
