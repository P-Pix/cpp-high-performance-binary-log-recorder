#include "test_framework.hpp"

#include "hpblr/async_recorder.hpp"
#include "hpblr/binary_log.hpp"
#include "hpblr/event_generator.hpp"
#include "hpblr/time_utils.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

std::filesystem::path temp_async_path() {
    return std::filesystem::temp_directory_path() / ("hpblr_async_" + std::to_string(hpblr::now_unix_ns()) + ".hpblr");
}

} // namespace

TEST_CASE("async_recorder_writes_all_submitted_events") {
    const auto path = temp_async_path();
    constexpr std::uint64_t total_events = 2000;
    constexpr std::size_t producer_count = 4;

    hpblr::AsyncRecorderOptions options;
    options.queue_capacity = 256;
    options.writer_options.flush_threshold_bytes = 4096;

    hpblr::AsyncRecorder recorder(path, options);
    std::atomic<std::uint64_t> next_sequence{0};
    std::atomic<bool> ok{true};

    std::vector<std::jthread> producers;
    producers.reserve(producer_count);
    for (std::size_t producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            hpblr::EventGenerator generator(hpblr::GeneratorConfig{
                static_cast<std::uint32_t>(producer),
                static_cast<std::uint16_t>(100U + producer),
                32,
                0xCAFE0000ULL + static_cast<std::uint64_t>(producer)});
            while (true) {
                const auto sequence = next_sequence.fetch_add(1U, std::memory_order_relaxed);
                if (sequence >= total_events) {
                    break;
                }
                if (!recorder.submit(generator.next(sequence))) {
                    ok.store(false, std::memory_order_relaxed);
                    break;
                }
            }
        });
    }

    producers.clear();
    recorder.stop();
    const auto stats = recorder.stats();
    REQUIRE_TRUE(ok.load(std::memory_order_relaxed));
    REQUIRE_EQ(stats.submitted, total_events);
    REQUIRE_EQ(stats.written, total_events);
    REQUIRE_EQ(stats.rejected, 0ULL);

    hpblr::BinaryLogReader reader(path);
    std::uint64_t count = 0;
    while (reader.next()) {
        ++count;
    }
    REQUIRE_EQ(count, total_events);
    std::filesystem::remove(path);
}
