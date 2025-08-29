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

#include "NpsLimiter.hpp"
#include <atomic>
#include <chrono>
#include <cstring>

OmniMIDI::NpsLimiter::NpsLimiter(uint32_t channels, uint64_t max_nps) {
    rough_time_ = std::make_shared<std::atomic<uint64_t>>(0);
    stop_flag_ = std::make_shared<std::atomic<bool>>(false);
    timer_thread_ = std::thread(&NpsLimiter::timer_loop, this);
    max_nps_ = max_nps;

    for (uint32_t i = 0; i < channels; i++) {
        channels_.push_back(ChannelNpsLimiter(rough_time_));
    }
}

OmniMIDI::NpsLimiter::~NpsLimiter() {
    stop_flag_->store(true);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    channels_.clear();
}

void OmniMIDI::NpsLimiter::timer_loop() {
    uint64_t current_time = 0;
    auto last_tick = std::chrono::steady_clock::now();
    while (!stop_flag_->load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(NPS_WINDOW_MS));
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_tick);
        current_time += diff.count();
        rough_time_->store(current_time);
        last_tick = now;
    }
}

void OmniMIDI::NpsLimiter::ChannelNpsLimiter::check_time() {
    uint64_t time = rough_time_->load();
    if (time > last_time_) {
        windows_.push_back({last_time_, current_window_sum_});
        total_window_sum_ += current_window_sum_;
        current_window_sum_ = 0;
        last_time_ = time;
    }
}

uint64_t OmniMIDI::NpsLimiter::ChannelNpsLimiter::calculate_nps() {
    check_time();

    while (!windows_.empty()) {
        uint64_t cutoff = (last_time_ > 1000) ? last_time_ - 1000 : 0;
        if (windows_.front().time < cutoff) {
            total_window_sum_ -= windows_.front().notes;
            windows_.pop_front();
        } else {
            break;
        }
    }

    uint64_t short_nps = current_window_sum_ * (1000 / NPS_WINDOW_MS) * 4 / 3;
    uint64_t long_nps = total_window_sum_;

    return std::max(short_nps, long_nps);
}

void OmniMIDI::NpsLimiter::ChannelNpsLimiter::add_note() {
    current_window_sum_++;
}

bool OmniMIDI::NpsLimiter::note_on(uint32_t channel, uint8_t key, uint8_t vel) {
    if (key > 127)
        return false;

    ChannelNpsLimiter *ch = &channels_[channel];
    uint64_t curr_nps = ch->calculate_nps();

    if (NpsLimiter::should_send_for_vel_and_nps(vel, curr_nps, max_nps_)) {
        ch->add_note();
        return true;
    } else {
        ch->missed_notes_[key]++;
        return false;
    }
}

void OmniMIDI::NpsLimiter::ChannelNpsLimiter::reset() {
    memset(&missed_notes_, 0, sizeof(missed_notes_));
}

bool OmniMIDI::NpsLimiter::note_off(uint32_t channel, uint8_t key) {
    if (key > 127)
        return false;

    ChannelNpsLimiter *ch = &channels_[channel];
    if (ch->missed_notes_[key] > 0) {
        ch->missed_notes_[key]--;
        return false;
    } else {
        return true;
    }
}

void OmniMIDI::NpsLimiter::reset() {
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
        it->reset();
    }
}