/**
 * @file hpblr_tool.cpp
 * @brief Implémente l’outil d’inspection, filtrage et export des fichiers HPBLR.
 *
 * Tous les modes de lecture passent par BinaryLogReader afin de conserver les mêmes validations de format et de CRC.
 */

#include "hpblr/binary_log.hpp"
#include "hpblr/event.hpp"
#include "hpblr/filter.hpp"
#include "hpblr/time_utils.hpp"

#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

struct CommonOptions {
    std::filesystem::path input;
    hpblr::EventFilter filter;
    std::uint64_t limit = 0;
};

[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool parse_u32(const std::string& text, std::uint32_t& value) {
    std::uint64_t tmp = 0;
    if (!parse_u64(text, tmp) || tmp > 0xFFFFFFFFULL) {
        return false;
    }
    value = static_cast<std::uint32_t>(tmp);
    return true;
}

[[nodiscard]] bool parse_u16(const std::string& text, std::uint16_t& value) {
    std::uint64_t tmp = 0;
    if (!parse_u64(text, tmp) || tmp > 0xFFFFULL) {
        return false;
    }
    value = static_cast<std::uint16_t>(tmp);
    return true;
}

void usage(const char* argv0) {
    std::cerr << "Usage:\n"
              << "  " << argv0 << " inspect --input <file>\n"
              << "  " << argv0 << " dump --input <file> [filters] [--limit <n>]\n"
              << "  " << argv0 << " export --input <file> --format csv|json [--output <file>] [filters] [--limit <n>]\n\n"
              << "Filters:\n"
              << "  --producer <id>       Keep only one producer id\n"
              << "  --type <id>           Keep only one event type\n"
              << "  --severity <name|id>  trace, debug, info, warning, error, critical\n"
              << "  --from-ns <ns>        Inclusive timestamp lower bound\n"
              << "  --to-ns <ns>          Inclusive timestamp upper bound\n";
}

std::string require_value(int& i, int argc, char** argv, const char* option) {
    if (i + 1 >= argc) {
        throw std::runtime_error(std::string("missing value for ") + option);
    }
    return argv[++i];
}

void parse_common_option(CommonOptions& opts, const std::string& arg, int& i, int argc, char** argv) {
    if (arg == "--input") {
        opts.input = require_value(i, argc, argv, "--input");
    } else if (arg == "--producer") {
        std::uint32_t value = 0;
        if (!parse_u32(require_value(i, argc, argv, "--producer"), value)) {
            throw std::runtime_error("invalid --producer value");
        }
        opts.filter.producer_id = value;
    } else if (arg == "--type") {
        std::uint16_t value = 0;
        if (!parse_u16(require_value(i, argc, argv, "--type"), value)) {
            throw std::runtime_error("invalid --type value");
        }
        opts.filter.type = value;
    } else if (arg == "--severity") {
        const auto severity = hpblr::severity_from_string(require_value(i, argc, argv, "--severity"));
        if (!severity) {
            throw std::runtime_error("invalid --severity value");
        }
        opts.filter.severity = *severity;
    } else if (arg == "--from-ns") {
        std::uint64_t value = 0;
        if (!parse_u64(require_value(i, argc, argv, "--from-ns"), value)) {
            throw std::runtime_error("invalid --from-ns value");
        }
        opts.filter.from_timestamp_ns = value;
    } else if (arg == "--to-ns") {
        std::uint64_t value = 0;
        if (!parse_u64(require_value(i, argc, argv, "--to-ns"), value)) {
            throw std::runtime_error("invalid --to-ns value");
        }
        opts.filter.to_timestamp_ns = value;
    } else if (arg == "--limit") {
        if (!parse_u64(require_value(i, argc, argv, "--limit"), opts.limit)) {
            throw std::runtime_error("invalid --limit value");
        }
    } else {
        throw std::runtime_error("unknown option: " + arg);
    }
}

CommonOptions parse_common(int start, int argc, char** argv) {
    CommonOptions opts;
    for (int i = start; i < argc; ++i) {
        parse_common_option(opts, argv[i], i, argc, argv);
    }
    if (opts.input.empty()) {
        throw std::runtime_error("--input is required");
    }
    return opts;
}

void inspect(const CommonOptions& opts) {
    hpblr::BinaryLogReader reader(opts.input);
    std::uint64_t count = 0;
    std::uint64_t payload_bytes = 0;
    std::array<std::uint64_t, 6> severities{};

    while (auto event = reader.next()) {
        ++count;
        payload_bytes += static_cast<std::uint64_t>(event->payload.size());
        severities.at(hpblr::severity_to_u16(event->severity)) += 1U;
    }

    const auto info = reader.info();
    std::cout << "file=" << opts.input << '\n';
    std::cout << "version=" << info.version << '\n';
    std::cout << "created_unix_ns=" << info.created_unix_ns << '\n';
    std::cout << "created_utc=" << hpblr::format_unix_ns_utc(info.created_unix_ns) << '\n';
    std::cout << "events=" << count << '\n';
    std::cout << "payload_bytes=" << payload_bytes << '\n';
    if (count > 0) {
        std::cout << "first_timestamp_ns=" << info.first_timestamp_ns << '\n';
        std::cout << "last_timestamp_ns=" << info.last_timestamp_ns << '\n';
    }
    for (std::uint16_t i = 0; i < severities.size(); ++i) {
        const auto severity = hpblr::severity_from_u16(i).value();
        std::cout << "severity_" << hpblr::severity_to_string(severity) << '=' << severities[i] << '\n';
    }
}

void dump(const CommonOptions& opts) {
    hpblr::BinaryLogReader reader(opts.input);
    std::uint64_t emitted = 0;
    while (auto event = reader.next()) {
        if (!opts.filter.matches(*event)) {
            continue;
        }
        std::cout << "seq=" << event->sequence << " ts_ns=" << event->timestamp_ns << " utc="
                  << hpblr::format_unix_ns_utc(event->timestamp_ns) << " producer=" << event->producer_id
                  << " type=" << event->type << " severity=" << hpblr::severity_to_string(event->severity)
                  << " payload_size=" << event->payload.size() << " payload_hex=" << hpblr::bytes_to_hex(event->payload, 32)
                  << '\n';
        ++emitted;
        if (opts.limit != 0 && emitted >= opts.limit) {
            break;
        }
    }
}

void write_csv(std::ostream& out, const CommonOptions& opts) {
    hpblr::BinaryLogReader reader(opts.input);
    out << "sequence,timestamp_ns,timestamp_utc,producer_id,type,severity,payload_size,payload_hex\n";
    std::uint64_t emitted = 0;
    while (auto event = reader.next()) {
        if (!opts.filter.matches(*event)) {
            continue;
        }
        out << event->sequence << ',' << event->timestamp_ns << ',' << hpblr::format_unix_ns_utc(event->timestamp_ns) << ','
            << event->producer_id << ',' << event->type << ',' << hpblr::severity_to_string(event->severity) << ','
            << event->payload.size() << ',' << hpblr::bytes_to_hex(event->payload) << '\n';
        ++emitted;
        if (opts.limit != 0 && emitted >= opts.limit) {
            break;
        }
    }
}

void write_json(std::ostream& out, const CommonOptions& opts) {
    hpblr::BinaryLogReader reader(opts.input);
    out << "[\n";
    bool first = true;
    std::uint64_t emitted = 0;
    while (auto event = reader.next()) {
        if (!opts.filter.matches(*event)) {
            continue;
        }
        if (!first) {
            out << ",\n";
        }
        first = false;
        out << "  {\"sequence\":" << event->sequence << ",\"timestamp_ns\":" << event->timestamp_ns
            << ",\"timestamp_utc\":\"" << hpblr::format_unix_ns_utc(event->timestamp_ns) << "\""
            << ",\"producer_id\":" << event->producer_id << ",\"type\":" << event->type
            << ",\"severity\":\"" << hpblr::severity_to_string(event->severity) << "\""
            << ",\"payload_size\":" << event->payload.size() << ",\"payload_hex\":\""
            << hpblr::bytes_to_hex(event->payload) << "\"}";
        ++emitted;
        if (opts.limit != 0 && emitted >= opts.limit) {
            break;
        }
    }
    out << "\n]\n";
}

void export_events(int start, int argc, char** argv) {
    CommonOptions opts;
    std::string format;
    std::filesystem::path output;

    for (int i = start; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--format") {
            format = require_value(i, argc, argv, "--format");
        } else if (arg == "--output") {
            output = require_value(i, argc, argv, "--output");
        } else {
            parse_common_option(opts, arg, i, argc, argv);
        }
    }

    if (opts.input.empty()) {
        throw std::runtime_error("--input is required");
    }
    if (format != "csv" && format != "json") {
        throw std::runtime_error("--format must be csv or json");
    }

    if (output.empty()) {
        if (format == "csv") {
            write_csv(std::cout, opts);
        } else {
            write_json(std::cout, opts);
        }
        return;
    }

    std::ofstream out(output);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + output.string());
    }
    if (format == "csv") {
        write_csv(out, opts);
    } else {
        write_json(out, opts);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            usage(argv[0]);
            return argc < 2 ? 1 : 0;
        }
        const std::string command = argv[1];
        if (command == "inspect") {
            inspect(parse_common(2, argc, argv));
        } else if (command == "dump") {
            dump(parse_common(2, argc, argv));
        } else if (command == "export") {
            export_events(2, argc, argv);
        } else {
            throw std::runtime_error("unknown command: " + command);
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "hpblr_tool: " << ex.what() << '\n';
        return 1;
    }
}
