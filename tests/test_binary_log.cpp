#include "test_framework.hpp"

#include "hpblr/binary_log.hpp"
#include "hpblr/errors.hpp"
#include "hpblr/event.hpp"
#include "hpblr/time_utils.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path temp_path(const std::string& suffix) {
    return std::filesystem::temp_directory_path() / ("hpblr_test_" + std::to_string(hpblr::now_unix_ns()) + suffix);
}

hpblr::Event make_event(std::uint64_t sequence) {
    hpblr::Event event;
    event.timestamp_ns = 1000000000ULL + sequence;
    event.sequence = sequence;
    event.producer_id = static_cast<std::uint32_t>(sequence % 3ULL);
    event.type = static_cast<std::uint16_t>(100U + sequence);
    event.severity = sequence % 2ULL == 0ULL ? hpblr::Severity::info : hpblr::Severity::warning;
    event.payload = hpblr::string_to_bytes("payload-" + std::to_string(sequence));
    return event;
}

} // namespace

TEST_CASE("binary_log_roundtrip") {
    const auto path = temp_path("_roundtrip.hpblr");
    {
        hpblr::BinaryLogWriter writer(path);
        for (std::uint64_t i = 0; i < 128; ++i) {
            writer.append(make_event(i));
        }
        writer.close();
    }

    hpblr::BinaryLogReader reader(path);
    REQUIRE_EQ(reader.info().version, hpblr::kFileFormatVersion);

    std::uint64_t count = 0;
    while (auto event = reader.next()) {
        REQUIRE_EQ(event->sequence, count);
        REQUIRE_EQ(event->producer_id, static_cast<std::uint32_t>(count % 3ULL));
        REQUIRE_EQ(event->payload, hpblr::string_to_bytes("payload-" + std::to_string(count)));
        ++count;
    }
    REQUIRE_EQ(count, 128ULL);
    REQUIRE_EQ(reader.info().first_timestamp_ns, 1000000000ULL);
    REQUIRE_EQ(reader.info().last_timestamp_ns, 1000000000ULL + 127ULL);
    std::filesystem::remove(path);
}

TEST_CASE("binary_log_rejects_truncated_file") {
    const auto path = temp_path("_truncated.hpblr");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "short";
    }
    REQUIRE_THROWS_AS(hpblr::BinaryLogReader(path), hpblr::FileFormatError);
    std::filesystem::remove(path);
}

TEST_CASE("binary_log_detects_payload_corruption") {
    const auto path = temp_path("_corrupt.hpblr");
    {
        hpblr::BinaryLogWriter writer(path);
        auto event = make_event(7);
        event.payload = hpblr::string_to_bytes("abcdef");
        writer.append(event);
        writer.close();
    }

    constexpr std::streamoff file_header_size = 36;
    constexpr std::streamoff record_header_size = 48;
    constexpr std::streamoff payload_offset = file_header_size + record_header_size + 2;
    {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE_TRUE(file.good());
        file.seekg(payload_offset);
        char original = 0;
        file.read(&original, 1);
        REQUIRE_TRUE(file.good());
        file.seekp(payload_offset);
        const char corrupted = static_cast<char>(original ^ 0x7F);
        file.write(&corrupted, 1);
        REQUIRE_TRUE(file.good());
    }

    hpblr::BinaryLogReader reader(path);
    REQUIRE_THROWS_AS(reader.next(), hpblr::CrcError);
    std::filesystem::remove(path);
}
