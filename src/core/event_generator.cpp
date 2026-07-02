#include "hpblr/event_generator.hpp"

#include "hpblr/time_utils.hpp"

#include <algorithm>

namespace hpblr {

EventGenerator::EventGenerator(GeneratorConfig config)
    : config_(config), rng_(config.seed ^ (static_cast<std::uint64_t>(config.producer_id) << 32U)) {}

Event EventGenerator::next(std::uint64_t sequence) {
    Event event;
    event.timestamp_ns = now_unix_ns();
    event.sequence = sequence;
    event.producer_id = config_.producer_id;
    event.type = static_cast<std::uint16_t>(config_.event_type_base + (sequence % 8ULL));
    event.severity = static_cast<Severity>(static_cast<std::uint16_t>(sequence % 6ULL));
    event.payload.resize(config_.payload_size);

    std::uint64_t metadata[] = {sequence, static_cast<std::uint64_t>(config_.producer_id), event.timestamp_ns};
    std::size_t offset = 0;
    for (const auto value : metadata) {
        for (std::size_t byte = 0; byte < sizeof(value) && offset < event.payload.size(); ++byte) {
            event.payload[offset++] = static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU);
        }
    }

    while (offset < event.payload.size()) {
        const auto value = rng_();
        for (std::size_t byte = 0; byte < sizeof(value) && offset < event.payload.size(); ++byte) {
            event.payload[offset++] = static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU);
        }
    }

    return event;
}

} // namespace hpblr
