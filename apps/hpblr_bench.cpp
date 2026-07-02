#include "hpblr/async_recorder.hpp"
#include "hpblr/event_generator.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace {

struct Options {
    std::filesystem::path output = "benchmark.hpblr";
    std::filesystem::path report;
    std::size_t producers = 4;
    std::uint64_t events = 100000;
    std::size_t payload_size = 64;
    std::size_t queue_capacity = 32768;
    std::size_t flush_bytes = 4U << 20U;
    bool json_stdout = false;
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
              << "  --output <file>          Output .hpblr file (default: benchmark.hpblr)\n"
              << "  --report <file>          Write JSON benchmark report\n"
              << "  --json                   Print JSON report to stdout\n"
              << "  --producers <n>          Producer threads (default: 4)\n"
              << "  --events <n>             Total events (default: 100000)\n"
              << "  --payload-size <bytes>   Payload size per event (default: 64)\n"
              << "  --queue-capacity <n>     Queue capacity (default: 32768)\n"
              << "  --flush-bytes <n>        Writer flush threshold (default: 4194304)\n";
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
        } else if (arg == "--report") {
            opts.report = require_value("--report");
        } else if (arg == "--json") {
            opts.json_stdout = true;
        } else if (arg == "--producers") {
            if (!parse_size(require_value("--producers"), opts.producers) || opts.producers == 0) {
                throw std::runtime_error("invalid --producers value");
            }
        } else if (arg == "--events") {
            if (!parse_u64(require_value("--events"), opts.events) || opts.events == 0) {
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
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    return opts;
}

std::string make_json_report(const Options& opts,
                             std::uint64_t written,
                             double seconds,
                             std::uintmax_t file_size,
                             double events_per_second,
                             double mib_per_second) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{\n"
        << "  \"events\": " << written << ",\n"
        << "  \"producers\": " << opts.producers << ",\n"
        << "  \"payload_size\": " << opts.payload_size << ",\n"
        << "  \"queue_capacity\": " << opts.queue_capacity << ",\n"
        << "  \"flush_bytes\": " << opts.flush_bytes << ",\n"
        << "  \"duration_seconds\": " << seconds << ",\n"
        << "  \"events_per_second\": " << events_per_second << ",\n"
        << "  \"file_size_bytes\": " << file_size << ",\n"
        << "  \"mib_per_second\": " << mib_per_second << ",\n"
        << "  \"output\": \"" << opts.output.string() << "\"\n"
        << "}\n";
    return oss.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto opts = parse_options(argc, argv);
        hpblr::AsyncRecorderOptions recorder_options;
        recorder_options.queue_capacity = opts.queue_capacity;
        recorder_options.writer_options.flush_threshold_bytes = opts.flush_bytes;

        hpblr::AsyncRecorder recorder(opts.output, recorder_options);
        std::atomic<std::uint64_t> next_sequence{0};
        std::vector<std::jthread> producers;
        producers.reserve(opts.producers);

        const auto started = std::chrono::steady_clock::now();
        for (std::size_t p = 0; p < opts.producers; ++p) {
            producers.emplace_back([&, p] {
                hpblr::EventGenerator generator(hpblr::GeneratorConfig{
                    static_cast<std::uint32_t>(p),
                    static_cast<std::uint16_t>(100 + (p * 10)),
                    opts.payload_size,
                    0x123456780000ULL + static_cast<std::uint64_t>(p)});
                while (true) {
                    const auto sequence = next_sequence.fetch_add(1U, std::memory_order_relaxed);
                    if (sequence >= opts.events) {
                        break;
                    }
                    if (!recorder.submit(generator.next(sequence))) {
                        break;
                    }
                }
            });
        }

        producers.clear();
        recorder.stop();
        const auto stopped = std::chrono::steady_clock::now();
        const auto stats = recorder.stats();
        const double seconds = std::chrono::duration<double>(stopped - started).count();
        const auto file_size = std::filesystem::file_size(opts.output);
        const double events_per_second = seconds > 0.0 ? static_cast<double>(stats.written) / seconds : 0.0;
        const double mib_per_second = seconds > 0.0 ? (static_cast<double>(file_size) / (1024.0 * 1024.0)) / seconds : 0.0;
        const auto json = make_json_report(opts, stats.written, seconds, file_size, events_per_second, mib_per_second);

        if (!opts.report.empty()) {
            std::ofstream report(opts.report);
            if (!report) {
                throw std::runtime_error("failed to open benchmark report: " + opts.report.string());
            }
            report << json;
        }

        if (opts.json_stdout) {
            std::cout << json;
        } else {
            std::cout << "events=" << stats.written << '\n';
            std::cout << "duration_seconds=" << std::fixed << std::setprecision(3) << seconds << '\n';
            std::cout << "events_per_second=" << std::fixed << std::setprecision(0) << events_per_second << '\n';
            std::cout << "file_size_bytes=" << file_size << '\n';
            std::cout << "mib_per_second=" << std::fixed << std::setprecision(2) << mib_per_second << '\n';
            std::cout << "output=" << opts.output << '\n';
            if (!opts.report.empty()) {
                std::cout << "report=" << opts.report << '\n';
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "hpblr_bench: " << ex.what() << '\n';
        return 1;
    }
}
