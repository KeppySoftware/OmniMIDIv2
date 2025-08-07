#include "NpsLimiter.hpp"
#include <atomic>
#include <chrono>

OmniMIDI::NpsLimiter::NpsLimiter() {
    rough_time_ = std::make_shared<std::atomic<uint64_t>>(0);
    stop_flag_ = std::make_shared<std::atomic<bool>>(false);
    timer_thread_ = std::thread(&NpsLimiter::timer_loop, this);
}

OmniMIDI::NpsLimiter::~NpsLimiter() {
    stop_flag_->store(true);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void OmniMIDI::NpsLimiter::timer_loop() {
    uint64_t current_time = 0;
    auto last_tick = std::chrono::steady_clock::now();
    while (!stop_flag_->load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(NPS_WINDOW_MS));
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick);
        current_time += diff.count();
        rough_time_->store(current_time);
        last_tick = now;
    }
}

void OmniMIDI::NpsLimiter::check_time() {
    uint64_t time = rough_time_->load();
    if (time > last_time_) {
        windows_.push_back({last_time_, current_window_sum_});
        total_window_sum_ += current_window_sum_;
        current_window_sum_ = 0;
        last_time_ = time;
    }
}

uint64_t OmniMIDI::NpsLimiter::calculate_nps() {
    check_time();

    // Remove windows older than 1 second
    while (!windows_.empty()) {
        uint64_t cutoff = (last_time_ > 1000) ? last_time_ - 1000 : 0;
        if (windows_.front().time < cutoff) {
            total_window_sum_ -= windows_.front().notes;
            windows_.pop_front();
        } else {
            break;
        }
    }

    // Heuristic from original code
    uint64_t short_nps = current_window_sum_ * (1000 / NPS_WINDOW_MS) * 4 / 3;
    uint64_t long_nps = total_window_sum_;

    return std::max(short_nps, long_nps);
}

void OmniMIDI::NpsLimiter::add_note() {
    current_window_sum_++;
}