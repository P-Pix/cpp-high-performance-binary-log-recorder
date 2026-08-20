/**
 * @file event_generator.hpp
 * @brief Déclare le générateur déterministe d’événements synthétiques.
 *
 * Il alimente les outils de démonstration et de benchmark sans introduire de dépendance vers une source métier externe.
 */

#pragma once

#include "hpblr/event.hpp"

#include <cstddef>
#include <cstdint>
#include <random>

namespace hpblr {

struct GeneratorConfig {
    std::uint32_t producer_id = 0;
    std::uint16_t event_type_base = 100;
    std::size_t payload_size = 64;
    std::uint64_t seed = 0xBADC0FFEEULL;
};

/**
 * @brief Produit une séquence reproductible d’événements synthétiques pour tests et benchmarks.
 *
 * Le seed est combiné à producer_id afin que plusieurs producteurs génèrent des flux distincts
 * tout en restant déterministes à configuration identique.
 */

class EventGenerator {
public:
    explicit EventGenerator(GeneratorConfig config);
/**
 * @brief Génère l’événement synthétique correspondant à une séquence donnée.
 * @param sequence Numéro fourni par le producteur appelant.
 * @return Événement horodaté avec payload de taille configurée.
 */

    [[nodiscard]] Event next(std::uint64_t sequence);

private:
    GeneratorConfig config_;
    std::mt19937_64 rng_;
};

} // namespace hpblr
