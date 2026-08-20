/**
 * @file async_recorder.cpp
 * @brief Implémente le pipeline asynchrone d’enregistrement HPBLR.
 *
 * Le thread d’écriture draine la file jusqu’à sa fermeture et propage ses erreurs vers les appels synchrones de contrôle.
 */

#include "hpblr/async_recorder.hpp"

namespace hpblr {

AsyncRecorder::AsyncRecorder(const std::filesystem::path& output_path, AsyncRecorderOptions options)
    : writer_(output_path, options.writer_options), queue_(options.queue_capacity) {
    writer_thread_ = std::jthread([this] { run(); });
}

AsyncRecorder::~AsyncRecorder() {
    try {
        stop();
    } catch (...) {
    }
}

bool AsyncRecorder::submit(Event event) {
    rethrow_writer_error_if_any();
    if (stopping_.load(std::memory_order_acquire)) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
    if (!queue_.push(std::move(event))) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
    submitted_.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

void AsyncRecorder::stop() {
    const bool already_stopping = stopping_.exchange(true, std::memory_order_acq_rel);
    if (!already_stopping) {
        queue_.close();
    }
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    rethrow_writer_error_if_any();
}

AsyncRecorderStats AsyncRecorder::stats() const {
    return AsyncRecorderStats{
        submitted_.load(std::memory_order_relaxed),
        rejected_.load(std::memory_order_relaxed),
        written_.load(std::memory_order_relaxed),
        queue_.size()};
}

/**
 * @brief Boucle du consommateur qui draine la file vers BinaryLogWriter.
 *
 * Toute exception du chemin disque est capturée et mémorisée : les producteurs ne lancent pas
 * depuis ce thread, mais stop()/submit() peuvent ensuite retransmettre l’échec de façon contrôlée.
 */

void AsyncRecorder::run() {
    try {
        Event event;
        while (queue_.pop(event)) {
            writer_.append(event);
            written_.fetch_add(1U, std::memory_order_relaxed);
        }
        writer_.flush();
    } catch (...) {
        set_writer_error(std::current_exception());
        queue_.close();
    }
}

void AsyncRecorder::set_writer_error(std::exception_ptr error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (!writer_error_) {
        writer_error_ = error;
    }
}

void AsyncRecorder::rethrow_writer_error_if_any() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (writer_error_) {
        std::rethrow_exception(writer_error_);
    }
}

} // namespace hpblr
