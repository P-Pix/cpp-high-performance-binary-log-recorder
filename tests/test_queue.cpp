#include "test_framework.hpp"

#include "hpblr/blocking_queue.hpp"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

TEST_CASE("blocking_queue_basic_close") {
    hpblr::BlockingQueue<int> queue(2);
    REQUIRE_TRUE(queue.push(10));
    REQUIRE_TRUE(queue.push(20));
    REQUIRE_FALSE(queue.try_push(30));

    int value = 0;
    REQUIRE_TRUE(queue.pop(value));
    REQUIRE_EQ(value, 10);
    REQUIRE_TRUE(queue.pop(value));
    REQUIRE_EQ(value, 20);

    queue.close();
    REQUIRE_FALSE(queue.push(40));
    REQUIRE_FALSE(queue.pop(value));
}

TEST_CASE("blocking_queue_multi_producer_consumer") {
    hpblr::BlockingQueue<std::uint64_t> queue(128);
    constexpr std::uint64_t producer_count = 4;
    constexpr std::uint64_t per_producer = 1000;
    constexpr std::uint64_t expected_count = producer_count * per_producer;

    std::atomic<bool> ok{true};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::uint64_t> sum{0};

    std::jthread consumer([&] {
        std::uint64_t value = 0;
        while (queue.pop(value)) {
            consumed.fetch_add(1U, std::memory_order_relaxed);
            sum.fetch_add(value, std::memory_order_relaxed);
        }
    });

    std::vector<std::jthread> producers;
    producers.reserve(static_cast<std::size_t>(producer_count));
    for (std::uint64_t producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            for (std::uint64_t i = 0; i < per_producer; ++i) {
                if (!queue.push(producer * per_producer + i)) {
                    ok.store(false, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }

    producers.clear();
    queue.close();
    consumer.join();

    const std::uint64_t expected_sum = (expected_count - 1U) * expected_count / 2U;
    REQUIRE_TRUE(ok.load(std::memory_order_relaxed));
    REQUIRE_EQ(consumed.load(std::memory_order_relaxed), expected_count);
    REQUIRE_EQ(sum.load(std::memory_order_relaxed), expected_sum);
}
