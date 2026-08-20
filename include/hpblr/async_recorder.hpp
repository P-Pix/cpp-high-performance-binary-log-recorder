/**
 * @file async_recorder.hpp
 * @brief Déclare l’enregistreur asynchrone producteur-consommateur.
 *
 * Il isole les producteurs de l’écriture disque à l’aide d’une file bornée et d’un thread dédié.
 */

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

namespace hpblr
{

    /**
     * @brief Paramètres de dimensionnement du pipeline asynchrone.
     *
     * La capacité de file pilote la backpressure ; WriterOptions contrôle le buffering disque.
     */
    struct AsyncRecorderOptions
    {
        std::size_t queue_capacity = 8192;
        WriterOptions writer_options = {};
    };

    struct AsyncRecorderStats
    {
        std::uint64_t submitted = 0;
        std::uint64_t rejected = 0;
        std::uint64_t written = 0;
        std::size_t queue_depth = 0;
    };

    /**
     * @brief Enregistre des Event via un unique thread d’écriture de fond.
     *
     * Objectif projet :
     * Permettre à plusieurs producteurs de soumettre des événements sans partager directement le
     * flux fichier. La file bornée limite la croissance mémoire et l’arrêt draine les événements
     * acceptés avant de fermer BinaryLogWriter.
     *
     * Interagit avec :
     * - BlockingQueue<Event> pour la synchronisation et la backpressure ;
     * - BinaryLogWriter pour la sérialisation persistante ;
     * - std::jthread pour la durée de vie du consommateur.
     */
    class AsyncRecorder
    {
    public:
        explicit AsyncRecorder(const std::filesystem::path &output_path, AsyncRecorderOptions options = {});
        ~AsyncRecorder();

        AsyncRecorder(const AsyncRecorder &) = delete;
        AsyncRecorder &operator=(const AsyncRecorder &) = delete;

        /**
         * @brief Soumet un événement à la file d’écriture.
         * @param event Événement transféré vers le pipeline asynchrone.
         * @return true si l’événement est accepté, false si l’enregistreur est en cours d’arrêt.
         * @throws std::exception Une erreur différée du thread writer peut être retransmise à l’appelant.
         */
        bool submit(Event event);

        /**
         * @brief Ferme la file, draine les événements déjà acceptés puis joint le thread writer.
         *
         * L’opération est conçue pour être idempotente et constitue la barrière de persistance avant
         * la destruction de l’enregistreur.
         * @throws std::exception Si le thread d’écriture a rencontré une erreur persistante.
         */
        void stop();

        /**
         * @brief Capture les compteurs de soumission, rejet, écriture et profondeur de file.
         * @return Snapshot de diagnostic sans modifier le pipeline.
         */

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
