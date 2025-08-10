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

#ifndef NPS_LIMITER_H
#define NPS_LIMITER_H

// Original code:
// https://github.com/BlackMIDIDevs/xsynth/blob/master/realtime/src/event_senders.rs
// Written by arduano

#include <deque>
#include <memory>
#include <thread>
#include <vector>

#define NPS_WINDOW_MS 1

namespace OmniMIDI {

class NpsLimiter {
  private:
    struct NpsWindow {
        uint64_t time;
        uint64_t notes;
    };

    struct ChannelNpsLimiter {
        std::shared_ptr<std::atomic<uint64_t>> rough_time_;
        uint64_t last_time_ = 0;
        std::deque<NpsWindow> windows_;
        uint64_t total_window_sum_ = 0;
        uint64_t current_window_sum_ = 0;

        uint64_t missed_notes_[128] = {0};

        ChannelNpsLimiter(std::shared_ptr<std::atomic<uint64_t>> rough_time)
            : rough_time_(rough_time) {}
        uint64_t calculate_nps();
        void add_note();
        void check_time();
        void reset();
    };

    void timer_loop();

    static inline bool should_send_for_vel_and_nps(uint8_t vel, uint64_t nps,
                                                   uint64_t max_nps) {
        return static_cast<uint64_t>(vel) * max_nps / 127 > nps;
    }

    std::shared_ptr<std::atomic<uint64_t>> rough_time_;
    std::shared_ptr<std::atomic<bool>> stop_flag_;
    std::thread timer_thread_;
    uint64_t max_nps_;

    std::vector<ChannelNpsLimiter> channels_;

  public:
    NpsLimiter(uint32_t channels, uint64_t max_nps);
    ~NpsLimiter();

    // Disable copy/move to prevent issues with thread ownership.
    NpsLimiter(const NpsLimiter &) = delete;
    NpsLimiter &operator=(const NpsLimiter &) = delete;
    NpsLimiter(NpsLimiter &&) = delete;
    NpsLimiter &operator=(NpsLimiter &&) = delete;

    bool note_on(uint32_t channel, uint8_t key, uint8_t vel);
    bool note_off(uint32_t channel, uint8_t key);
    void reset();
};
} // namespace OmniMIDI

#endif