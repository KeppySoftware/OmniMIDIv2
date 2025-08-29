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
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "../Common.hpp"

class BufferedRenderer {
    using AudioPipe = std::function<void(std::vector<float> &)>;

  private:
    struct BufferedRendererStats {
        std::shared_ptr<std::atomic<int64_t>> samples;
        std::shared_ptr<std::atomic<int64_t>> last_samples_after_read;
        std::shared_ptr<std::atomic<int64_t>> last_request_samples;
        std::shared_ptr<std::deque<double>> render_time_queue;
        std::shared_ptr<std::shared_mutex> render_time_mutex;
        std::shared_ptr<std::atomic<size_t>> render_size;
    };

    void render_loop();

    BufferedRendererStats stats_;
    std::queue<std::vector<float>> receive_queue_;
    std::mutex queue_mutex;
    std::condition_variable queue_cond;
    std::vector<float> remainder_;
    std::shared_ptr<std::atomic<bool>> killed_;
    std::thread render_thread_;
    AudioStreamParams stream_params_;
    AudioPipe audio_pipe_;

  public:
    BufferedRenderer(AudioPipe render_function, AudioStreamParams params,
                     size_t initial_render_size);

    ~BufferedRenderer();

    BufferedRenderer(const BufferedRenderer &) = delete;
    BufferedRenderer &operator=(const BufferedRenderer &) = delete;
    BufferedRenderer(BufferedRenderer &&) = delete;
    BufferedRenderer &operator=(BufferedRenderer &&) = delete;

    void read(std::vector<float> &dest);
    void set_render_size(size_t size);

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
};

#endif // BUFFERED_RENDERER_H