#pragma once

#include "hpblr/event.hpp"

#include <cstddef>
#include <cstdint>
#include <random>

namespace hpblr {

struct GeneratorConfig {
    std::uint32_t producer_id = 0;
    std::uint16_t event_type_base = 100;
    std::size_t payload_size = 64;
    std::uint64_t seed = 0xBADC0FFEEULL;
};

class EventGenerator {
public:
    explicit EventGenerator(GeneratorConfig config);
    [[nodiscard]] Event next(std::uint64_t sequence);

private:
    GeneratorConfig config_;
    std::mt19937_64 rng_;
};

} // namespace hpblr
