#pragma once

#include "hpblr/binary_log.hpp"
#include "hpblr/blocking_queue.hpp"
#include "hpblr/event.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <mutex>
#include <thread>

namespace hpblr {

struct AsyncRecorderOptions {
    std::size_t queue_capacity = 8192;
    WriterOptions writer_options = {};
};

struct AsyncRecorderStats {
    std::uint64_t submitted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t written = 0;
    std::size_t queue_depth = 0;
};

class AsyncRecorder {
public:
    explicit AsyncRecorder(const std::filesystem::path& output_path, AsyncRecorderOptions options = {});
    ~AsyncRecorder();

    AsyncRecorder(const AsyncRecorder&) = delete;
    AsyncRecorder& operator=(const AsyncRecorder&) = delete;

    bool submit(Event event);
    void stop();

    [[nodiscard]] AsyncRecorderStats stats() const;

private:
    void run();
    void set_writer_error(std::exception_ptr error);
    void rethrow_writer_error_if_any() const;

    BinaryLogWriter writer_;
    BlockingQueue<Event> queue_;
    std::jthread writer_thread_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> rejected_{0};
    std::atomic<std::uint64_t> written_{0};
    mutable std::mutex error_mutex_;
    std::exception_ptr writer_error_;
};

} // namespace hpblr
