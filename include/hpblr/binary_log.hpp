#pragma once

#include "hpblr/event.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace hpblr {

inline constexpr std::uint16_t kFileFormatVersion = 1;
inline constexpr std::size_t kMaxPayloadBytes = 16U * 1024U * 1024U;

struct FileInfo {
    std::uint16_t version = 0;
    std::uint64_t created_unix_ns = 0;
    std::uint64_t first_timestamp_ns = 0;
    std::uint64_t last_timestamp_ns = 0;
};

struct WriterOptions {
    std::size_t flush_threshold_bytes = 1U << 20U;
    bool flush_on_each_event = false;
};

struct WriterStats {
    std::uint64_t events_written = 0;
    std::uint64_t bytes_written = 0;
};

class BinaryLogWriter {
public:
    explicit BinaryLogWriter(const std::filesystem::path& path, WriterOptions options = {});
    ~BinaryLogWriter();

    BinaryLogWriter(const BinaryLogWriter&) = delete;
    BinaryLogWriter& operator=(const BinaryLogWriter&) = delete;

    void append(const Event& event);
    void flush();
    void close();

    [[nodiscard]] WriterStats stats() const noexcept { return stats_; }

private:
    void write_file_header();
    void write_buffer_if_needed(bool force);

    std::filesystem::path path_;
    WriterOptions options_;
    std::ofstream out_;
    std::vector<std::uint8_t> buffer_;
    WriterStats stats_;
    bool closed_ = false;
};

class BinaryLogReader {
public:
    explicit BinaryLogReader(const std::filesystem::path& path);

    BinaryLogReader(const BinaryLogReader&) = delete;
    BinaryLogReader& operator=(const BinaryLogReader&) = delete;

    [[nodiscard]] const FileInfo& info() const noexcept { return info_; }
    [[nodiscard]] std::optional<Event> next();

private:
    void read_file_header();

    std::filesystem::path path_;
    std::ifstream in_;
    FileInfo info_;
};

} // namespace hpblr
