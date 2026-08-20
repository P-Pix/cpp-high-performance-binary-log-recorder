/**
 * @file binary_log.cpp
 * @brief Implémente la sérialisation et la lecture robuste du format HPBLR.
 *
 * Les entêtes et payloads sont bornés et vérifiés par CRC afin de refuser les journaux tronqués ou corrompus.
 */

#include "hpblr/binary_log.hpp"

#include "hpblr/crc32.hpp"
#include "hpblr/errors.hpp"
#include "hpblr/time_utils.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <sstream>

namespace hpblr {
namespace {

constexpr std::array<std::uint8_t, 8> kFileMagic{{'H', 'P', 'B', 'L', 'O', 'G', '1', '\0'}};
constexpr std::uint16_t kFileHeaderSize = 36;
constexpr std::uint16_t kRecordHeaderSize = 48;
constexpr std::uint16_t kEndianMarker = 0x0102;
constexpr std::uint32_t kRecordMagic = 0x524C4248U; // little-endian bytes: H B L R
constexpr std::size_t kRecordHeaderCrcOffset = 36;
constexpr std::size_t kFileHeaderCrcOffset = 32;

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
    }
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFULL));
    }
}

void write_u32_at(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        out.at(offset + i) = static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
    }
}

std::uint16_t read_u16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
}

std::uint32_t read_u32(const std::uint8_t* data) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(data[i]) << (8U * i);
    }
    return value;
}

std::uint64_t read_u64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[i]) << (8U * i);
    }
    return value;
}

std::string io_context(const std::filesystem::path& path, std::string_view action) {
    return std::string(action) + ": " + path.string();
}

void write_all(std::ofstream& out, const std::vector<std::uint8_t>& data, const std::filesystem::path& path) {
    if (data.empty()) {
        return;
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) {
        throw IoError(io_context(path, "failed to write binary log"));
    }
}

} // namespace

BinaryLogWriter::BinaryLogWriter(const std::filesystem::path& path, WriterOptions options)
    : path_(path), options_(options), out_(path, std::ios::binary | std::ios::trunc) {
    if (!out_) {
        throw IoError(io_context(path_, "failed to open output file"));
    }
    if (options_.flush_threshold_bytes == 0) {
        options_.flush_threshold_bytes = 1U << 20U;
    }
    buffer_.reserve(options_.flush_threshold_bytes + kMaxPayloadBytes + kRecordHeaderSize);
    write_file_header();
}

BinaryLogWriter::~BinaryLogWriter() {
    try {
        close();
    } catch (...) {
    }
}

void BinaryLogWriter::write_file_header() {
    std::vector<std::uint8_t> header;
    header.reserve(kFileHeaderSize);
    header.insert(header.end(), kFileMagic.begin(), kFileMagic.end());
    append_u16(header, kFileFormatVersion);
    append_u16(header, kFileHeaderSize);
    append_u16(header, kEndianMarker);
    append_u16(header, 0U); // flags
    append_u64(header, now_unix_ns());
    append_u64(header, 0U); // reserved
    append_u32(header, 0U); // header CRC placeholder

    if (header.size() != kFileHeaderSize) {
        throw FileFormatError("internal error: invalid file header size");
    }
    const auto checksum = crc32(std::span<const std::uint8_t>{header.data(), kFileHeaderCrcOffset});
    write_u32_at(header, kFileHeaderCrcOffset, checksum);
    write_all(out_, header, path_);
    stats_.bytes_written += static_cast<std::uint64_t>(header.size());
}

void BinaryLogWriter::append(const Event& event) {
    if (closed_) {
        throw IoError("cannot append to closed binary log");
    }
    if (event.payload.size() > kMaxPayloadBytes) {
        throw FileFormatError("payload too large: " + std::to_string(event.payload.size()));
    }
    if (event.payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw FileFormatError("payload cannot be represented by the file format");
    }

    const std::uint32_t payload_crc = crc32(std::span<const std::uint8_t>{event.payload.data(), event.payload.size()});
    std::vector<std::uint8_t> header;
    header.reserve(kRecordHeaderSize);
    append_u32(header, kRecordMagic);
    append_u16(header, kRecordHeaderSize);
    append_u16(header, kFileFormatVersion);
    append_u64(header, event.timestamp_ns);
    append_u64(header, event.sequence);
    append_u32(header, event.producer_id);
    append_u16(header, event.type);
    append_u16(header, severity_to_u16(event.severity));
    append_u32(header, static_cast<std::uint32_t>(event.payload.size()));
    append_u32(header, 0U); // record header CRC placeholder
    append_u32(header, payload_crc);
    append_u32(header, 0U); // flags

    if (header.size() != kRecordHeaderSize) {
        throw FileFormatError("internal error: invalid record header size");
    }
    const std::uint32_t header_crc = crc32(std::span<const std::uint8_t>{header.data(), header.size()});
    write_u32_at(header, kRecordHeaderCrcOffset, header_crc);

    buffer_.insert(buffer_.end(), header.begin(), header.end());
    buffer_.insert(buffer_.end(), event.payload.begin(), event.payload.end());
    stats_.events_written += 1U;
    stats_.bytes_written += static_cast<std::uint64_t>(header.size() + event.payload.size());

    write_buffer_if_needed(options_.flush_on_each_event || buffer_.size() >= options_.flush_threshold_bytes);
}

void BinaryLogWriter::write_buffer_if_needed(bool force) {
    if (!force || buffer_.empty()) {
        return;
    }
    write_all(out_, buffer_, path_);
    buffer_.clear();
}

void BinaryLogWriter::flush() {
    if (closed_) {
        return;
    }
    write_buffer_if_needed(true);
    out_.flush();
    if (!out_) {
        throw IoError(io_context(path_, "failed to flush binary log"));
    }
}

void BinaryLogWriter::close() {
    if (closed_) {
        return;
    }
    flush();
    out_.close();
    if (!out_) {
        throw IoError(io_context(path_, "failed to close binary log"));
    }
    closed_ = true;
}

BinaryLogReader::BinaryLogReader(const std::filesystem::path& path) : path_(path), in_(path, std::ios::binary) {
    if (!in_) {
        throw IoError(io_context(path_, "failed to open input file"));
    }
    read_file_header();
}

void BinaryLogReader::read_file_header() {
    std::array<std::uint8_t, kFileHeaderSize> header{};
    in_.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (in_.gcount() != static_cast<std::streamsize>(header.size())) {
        throw FileFormatError("file is too short to contain an HPBLR header");
    }

    if (!std::equal(kFileMagic.begin(), kFileMagic.end(), header.begin())) {
        throw FileFormatError("invalid HPBLR file magic");
    }

    const auto version = read_u16(header.data() + 8);
    const auto header_size = read_u16(header.data() + 10);
    const auto endian_marker = read_u16(header.data() + 12);
    const auto created_ns = read_u64(header.data() + 16);
    const auto stored_crc = read_u32(header.data() + kFileHeaderCrcOffset);
    const auto computed_crc = crc32(std::span<const std::uint8_t>{header.data(), kFileHeaderCrcOffset});

    if (version != kFileFormatVersion) {
        throw FileFormatError("unsupported HPBLR file version: " + std::to_string(version));
    }
    if (header_size != kFileHeaderSize) {
        throw FileFormatError("unsupported HPBLR header size: " + std::to_string(header_size));
    }
    if (endian_marker != kEndianMarker) {
        throw FileFormatError("unsupported HPBLR endian marker");
    }
    if (stored_crc != computed_crc) {
        throw CrcError("HPBLR file header CRC mismatch");
    }

    info_.version = version;
    info_.created_unix_ns = created_ns;
}

/**
 * @brief Décode un record HPBLR en vérifiant les limites avant toute confiance dans le fichier.
 *
 * La taille de payload est bornée avant allocation, les CRC des métadonnées et du payload sont
 * vérifiés séparément, puis la sévérité est validée avant construction de Event.
 * @return Événement validé ou std::nullopt à EOF.
 */

std::optional<Event> BinaryLogReader::next() {
    std::array<std::uint8_t, kRecordHeaderSize> header{};
    in_.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));

    const auto count = in_.gcount();
    if (count == 0 && in_.eof()) {
        return std::nullopt;
    }
    if (count != static_cast<std::streamsize>(header.size())) {
        throw FileFormatError("truncated HPBLR record header");
    }

    const auto magic = read_u32(header.data());
    const auto header_size = read_u16(header.data() + 4);
    const auto version = read_u16(header.data() + 6);
    const auto timestamp_ns = read_u64(header.data() + 8);
    const auto sequence = read_u64(header.data() + 16);
    const auto producer_id = read_u32(header.data() + 24);
    const auto event_type = read_u16(header.data() + 28);
    const auto severity_raw = read_u16(header.data() + 30);
    const auto payload_size = read_u32(header.data() + 32);
    const auto stored_header_crc = read_u32(header.data() + 36);
    const auto stored_payload_crc = read_u32(header.data() + 40);

    if (magic != kRecordMagic) {
        throw FileFormatError("invalid HPBLR record magic near sequence " + std::to_string(sequence));
    }
    if (header_size != kRecordHeaderSize) {
        throw FileFormatError("unsupported HPBLR record header size: " + std::to_string(header_size));
    }
    if (version != kFileFormatVersion) {
        throw FileFormatError("unsupported HPBLR record version: " + std::to_string(version));
    }
    if (payload_size > kMaxPayloadBytes) {
        throw FileFormatError("record payload too large: " + std::to_string(payload_size));
    }

    auto header_for_crc = header;
    for (std::size_t i = 0; i < 4; ++i) {
        header_for_crc[kRecordHeaderCrcOffset + i] = 0U;
    }
    const auto computed_header_crc = crc32(std::span<const std::uint8_t>{header_for_crc.data(), header_for_crc.size()});
    if (stored_header_crc != computed_header_crc) {
        throw CrcError("HPBLR record header CRC mismatch near sequence " + std::to_string(sequence));
    }

    std::vector<std::uint8_t> payload(payload_size);
    if (payload_size > 0) {
        in_.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        if (in_.gcount() != static_cast<std::streamsize>(payload.size())) {
            throw FileFormatError("truncated HPBLR record payload near sequence " + std::to_string(sequence));
        }
    }

    const auto computed_payload_crc = crc32(std::span<const std::uint8_t>{payload.data(), payload.size()});
    if (stored_payload_crc != computed_payload_crc) {
        throw CrcError("HPBLR payload CRC mismatch near sequence " + std::to_string(sequence));
    }

    const auto severity = severity_from_u16(severity_raw);
    if (!severity) {
        throw FileFormatError("invalid severity value in HPBLR record: " + std::to_string(severity_raw));
    }

    Event event;
    event.timestamp_ns = timestamp_ns;
    event.sequence = sequence;
    event.producer_id = producer_id;
    event.type = event_type;
    event.severity = *severity;
    event.payload = std::move(payload);

    if (info_.first_timestamp_ns == 0) {
        info_.first_timestamp_ns = timestamp_ns;
    }
    info_.last_timestamp_ns = timestamp_ns;
    return event;
}

} // namespace hpblr
