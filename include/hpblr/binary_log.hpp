/**
 * @file binary_log.hpp
 * @brief Déclare le lecteur et l’écrivain du format binaire HPBLR.
 *
 * Ces classes centralisent la sérialisation, le buffering, la validation de format et les contrôles d’intégrité lors du replay.
 */

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

/**
 * @brief Sérialise des événements dans un fichier HPBLR versionné et bufferisé.
 *
 * Le writer est destiné à être utilisé par un seul thread. Il écrit un entête de fichier puis
 * des enregistrements contenant des métadonnées protégées par CRC et un CRC de payload.
 */

class BinaryLogWriter {
public:
    explicit BinaryLogWriter(const std::filesystem::path& path, WriterOptions options = {});
    ~BinaryLogWriter();

    BinaryLogWriter(const BinaryLogWriter&) = delete;
    BinaryLogWriter& operator=(const BinaryLogWriter&) = delete;

/**
 * @brief Ajoute un événement au buffer de sortie selon le format HPBLR.
 * @param event Événement à sérialiser ; son payload doit respecter kMaxPayloadBytes.
 * @throws HpblrError En cas de taille invalide ou d’échec d’écriture.
 */

    void append(const Event& event);
/**
 * @brief Force l’écriture du buffer actuellement accumulé vers le fichier.
 * @throws IoError Si le flux de sortie signale un échec.
 */

    void flush();
/**
 * @brief Vide le buffer et ferme logiquement le writer de manière idempotente.
 * @throws IoError Si la dernière écriture ne peut pas être effectuée.
 */

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

/**
 * @brief Lit séquentiellement un fichier HPBLR en validant sa structure et ses CRC.
 *
 * Le lecteur échoue volontairement sur les corruptions plutôt que de tenter une reprise heuristique
 * qui pourrait masquer une perte d’intégrité dans un journal technique.
 */

class BinaryLogReader {
public:
    explicit BinaryLogReader(const std::filesystem::path& path);

    BinaryLogReader(const BinaryLogReader&) = delete;
    BinaryLogReader& operator=(const BinaryLogReader&) = delete;

    [[nodiscard]] const FileInfo& info() const noexcept { return info_; }
/**
 * @brief Lit et valide l’enregistrement suivant.
 * @return Événement reconstruit, ou std::nullopt à la fin normale du fichier.
 * @throws FileFormatError Si la structure est tronquée ou incohérente.
 * @throws CrcError Si un CRC d’entête ou de payload ne correspond pas.
 * @throws IoError En cas d’erreur d’I/O.
 */

    [[nodiscard]] std::optional<Event> next();

private:
    void read_file_header();

    std::filesystem::path path_;
    std::ifstream in_;
    FileInfo info_;
};

} // namespace hpblr
