#include "hpblr/crc32.hpp"

#include <array>

namespace hpblr {
namespace {

constexpr std::uint32_t kPolynomial = 0xEDB88320U;

constexpr std::array<std::uint32_t, 256> make_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        std::uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ kPolynomial : (crc >> 1U);
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto kTable = make_table();

} // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        const auto index = static_cast<std::uint8_t>((crc ^ byte) & 0xFFU);
        crc = (crc >> 8U) ^ kTable[index];
    }
    return crc ^ 0xFFFFFFFFU;
}

std::uint32_t crc32(std::string_view text) noexcept {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(text.data());
    return crc32(std::span<const std::uint8_t>{ptr, text.size()});
}

} // namespace hpblr
