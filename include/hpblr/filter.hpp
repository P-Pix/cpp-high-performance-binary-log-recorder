#pragma once

#include "hpblr/event.hpp"

#include <cstdint>
#include <optional>

namespace hpblr {

struct EventFilter {
    std::optional<std::uint32_t> producer_id;
    std::optional<std::uint16_t> type;
    std::optional<Severity> severity;
    std::optional<std::uint64_t> from_timestamp_ns;
    std::optional<std::uint64_t> to_timestamp_ns;

    [[nodiscard]] bool matches(const Event& event) const noexcept {
        if (producer_id && event.producer_id != *producer_id) {
            return false;
        }
        if (type && event.type != *type) {
            return false;
        }
        if (severity && event.severity != *severity) {
            return false;
        }
        if (from_timestamp_ns && event.timestamp_ns < *from_timestamp_ns) {
            return false;
        }
        if (to_timestamp_ns && event.timestamp_ns > *to_timestamp_ns) {
            return false;
        }
        return true;
    }
};

} // namespace hpblr
