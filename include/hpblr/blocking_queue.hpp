/**
 * @file blocking_queue.hpp
 * @brief Définit la file bornée thread-safe utilisée pour la backpressure.
 *
 * La fermeture réveille producteurs et consommateur afin de permettre un arrêt déterministe sans perdre les événements déjà acceptés.
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace hpblr {

/**
 * @brief File FIFO bornée fournissant backpressure et fermeture coordonnée.
 *
 * push() bloque tant que la capacité est atteinte, alors que try_push() permet une stratégie
 * non bloquante. close() réveille tous les waiters ; pop() continue de drainer les éléments
 * déjà présents avant de signaler la fin.
 * @tparam T Type d’élément transféré entre producteurs et consommateur.
 */

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1U : capacity) {}

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        queue_.push_back(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    bool try_push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    bool closed_ = false;
};

} // namespace hpblr
