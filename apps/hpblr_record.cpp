/**
 * @file hpblr_record.cpp
 * @brief Implémente l’outil de génération et d’enregistrement multi-producteurs.
 *
 * L’application pilote plusieurs producteurs synthétiques et confie la persistance à AsyncRecorder.
 */

#include "hpblr/async_recorder.hpp"
#include "hpblr/event_generator.hpp"
#include "hpblr/logger.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void signal_handler(int) {
    g_stop_requested = 1;
}

struct Options {
    std::filesystem::path output = "recording.hpblr";
    std::size_t producers = 4;
    std::uint64_t events = 10000;
    std::size_t payload_size = 64;
    std::size_t queue_capacity = 8192;
    std::size_t flush_bytes = 1U << 20U;
    std::uint64_t sleep_us = 0;
    std::filesystem::path log_file;
};

[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool parse_size(const std::string& text, std::size_t& value) {
    std::uint64_t tmp = 0;
    if (!parse_u64(text, tmp)) {
        return false;
    }
    value = static_cast<std::size_t>(tmp);
    return static_cast<std::uint64_t>(value) == tmp;
}

void usage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " [options]\n"
              << "  --output <file>          Output .hpblr file (default: recording.hpblr)\n"
              << "  --producers <n>         Number of producer threads (default: 4)\n"
              << "  --events <n>            Total events to generate, 0 means until Ctrl-C (default: 10000)\n"
              << "  --payload-size <bytes>  Payload size per event (default: 64)\n"
              << "  --queue-capacity <n>    Bounded queue capacity (default: 8192)\n"
              << "  --flush-bytes <n>       Writer flush threshold (default: 1048576)\n"
              << "  --sleep-us <n>          Optional producer pacing delay in microseconds\n"
              << "  --log-file <file>       Append logs to a text file\n";
}

Options parse_options(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else if (arg == "--output") {
            opts.output = require_value("--output");
        } else if (arg == "--producers") {
            if (!parse_size(require_value("--producers"), opts.producers) || opts.producers == 0) {
                throw std::runtime_error("invalid --producers value");
            }
        } else if (arg == "--events") {
            if (!parse_u64(require_value("--events"), opts.events)) {
                throw std::runtime_error("invalid --events value");
            }
        } else if (arg == "--payload-size") {
            if (!parse_size(require_value("--payload-size"), opts.payload_size)) {
                throw std::runtime_error("invalid --payload-size value");
            }
        } else if (arg == "--queue-capacity") {
            if (!parse_size(require_value("--queue-capacity"), opts.queue_capacity) || opts.queue_capacity == 0) {
                throw std::runtime_error("invalid --queue-capacity value");
            }
        } else if (arg == "--flush-bytes") {
            if (!parse_size(require_value("--flush-bytes"), opts.flush_bytes) || opts.flush_bytes == 0) {
                throw std::runtime_error("invalid --flush-bytes value");
            }
        } else if (arg == "--sleep-us") {
            if (!parse_u64(require_value("--sleep-us"), opts.sleep_us)) {
                throw std::runtime_error("invalid --sleep-us value");
            }
        } else if (arg == "--log-file") {
            opts.log_file = require_value("--log-file");
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto opts = parse_options(argc, argv);
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        if (!opts.log_file.empty()) {
            hpblr::Logger::instance().set_file(opts.log_file);
        }

        hpblr::AsyncRecorderOptions recorder_options;
        recorder_options.queue_capacity = opts.queue_capacity;
        recorder_options.writer_options.flush_threshold_bytes = opts.flush_bytes;

        hpblr::Logger::instance().log(hpblr::LogLevel::info, "record", "starting event recorder");
        hpblr::AsyncRecorder recorder(opts.output, recorder_options);

        std::atomic<std::uint64_t> next_sequence{0};
        std::atomic<std::uint64_t> generated{0};
        std::vector<std::jthread> producers;
        producers.reserve(opts.producers);

        for (std::size_t p = 0; p < opts.producers; ++p) {
            producers.emplace_back([&, p] {
                hpblr::EventGenerator generator(hpblr::GeneratorConfig{
                    static_cast<std::uint32_t>(p),
                    static_cast<std::uint16_t>(100 + (p * 10)),
                    opts.payload_size,
                    0xBADC0FFEEULL + static_cast<std::uint64_t>(p)});

                while (g_stop_requested == 0) {
                    const auto sequence = next_sequence.fetch_add(1U, std::memory_order_relaxed);
                    if (opts.events != 0 && sequence >= opts.events) {
                        break;
                    }
                    if (!recorder.submit(generator.next(sequence))) {
                        break;
                    }
                    generated.fetch_add(1U, std::memory_order_relaxed);
                    if (opts.sleep_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(opts.sleep_us));
                    }
                }
            });
        }

        producers.clear();
        recorder.stop();
        const auto stats = recorder.stats();
        std::ostringstream msg;
        msg << "finished: generated=" << generated.load() << " submitted=" << stats.submitted
            << " written=" << stats.written << " rejected=" << stats.rejected;
        hpblr::Logger::instance().log(hpblr::LogLevel::info, "record", msg.str());
        std::cout << msg.str() << '\n';
        std::cout << "output=" << opts.output << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "hpblr_record: " << ex.what() << '\n';
        return 1;
    }
}
