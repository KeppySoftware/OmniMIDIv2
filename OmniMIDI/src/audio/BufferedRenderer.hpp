/*
 * SPDX-License-Identifier: MIT
 *
 * OmniMIDI
 *
 * Copyright (c) 2024 Keppy's Software
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this
 * program.  If not, see <https://opensource.org/license/mit/>.
 */

#ifndef BUFFERED_RENDERER_H
#define BUFFERED_RENDERER_H

// Original code:
// https://github.com/BlackMIDIDevs/xsynth/blob/master/core/src/buffered_renderer.rs
// Written by arduano

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "../Common.hpp"

template <typename T> class ThreadSafeQueue {
  public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_var_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool try_pop(T &value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

  private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cond_var_;
};

class BufferedRenderer;
class BufferedRendererStatsReader;

struct BufferedRendererStats {
    std::shared_ptr<std::atomic<int64_t>> samples;
    std::shared_ptr<std::atomic<int64_t>> last_samples_after_read;
    std::shared_ptr<std::atomic<int64_t>> last_request_samples;
    std::shared_ptr<std::deque<double>> render_time_queue;
    std::shared_ptr<std::shared_mutex> render_time_mutex;
    std::shared_ptr<std::atomic<size_t>> render_size;
};

class BufferedRendererStatsReader {
  public:
    explicit BufferedRendererStatsReader(BufferedRendererStats stats);

    // The number of samples currently buffered.
    int64_t samples() const;

    // The number of samples that were in the buffer after the last read.
    int64_t last_samples_after_read() const;

    // The last number of samples requested by the read command.
    int64_t last_request_samples() const;

    // The number of samples to render each iteration.
    size_t render_size() const;

    // The average load of the renderer (0.0 to 1.0).
    double average_renderer_load() const;

    // The most recent renderer load (0.0 to 1.0).
    double last_renderer_load() const;

  private:
    BufferedRendererStats stats_;
};

class BufferedRenderer {
  public:
    using AudioPipe = std::function<void(std::vector<float> &)>;

    BufferedRenderer(AudioPipe render_function, AudioStreamParams params,
                     size_t initial_render_size);

    ~BufferedRenderer();

    BufferedRenderer(const BufferedRenderer &) = delete;
    BufferedRenderer &operator=(const BufferedRenderer &) = delete;
    BufferedRenderer(BufferedRenderer &&) = delete;
    BufferedRenderer &operator=(BufferedRenderer &&) = delete;

    void read(std::vector<float> &dest);
    void set_render_size(size_t size);

    BufferedRendererStatsReader get_buffer_stats() const;

  private:
    void render_loop();

    BufferedRendererStats stats_;
    ThreadSafeQueue<std::vector<float>> receive_queue_;
    std::vector<float> remainder_;
    std::shared_ptr<std::atomic<bool>> killed_;
    std::thread render_thread_;
    AudioStreamParams stream_params_;
    AudioPipe audio_pipe_;
};

#endif // BUFFERED_RENDERER_H