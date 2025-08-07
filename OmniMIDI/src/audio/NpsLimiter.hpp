#ifndef NPS_LIMITER_H
#define NPS_LIMITER_H

#include <deque>
#include <memory>
#include <thread>

#define NPS_WINDOW_MS 20

namespace OmniMIDI {

class NpsLimiter {
public:
  NpsLimiter();
  ~NpsLimiter();

  // Disable copy/move to prevent issues with thread ownership.
  NpsLimiter(const NpsLimiter &) = delete;
  NpsLimiter &operator=(const NpsLimiter &) = delete;
  NpsLimiter(NpsLimiter &&) = delete;
  NpsLimiter &operator=(NpsLimiter &&) = delete;

  uint64_t calculate_nps();
  void add_note();

  static inline bool should_send_for_vel_and_nps(uint8_t vel, uint64_t nps, uint64_t max_nps) {
      if (max_nps == 0) return true; // Avoid division by zero
      return static_cast<uint64_t>(vel) * max_nps / 127 > nps;
  }

private:
  struct NpsWindow {
    uint64_t time;
    uint64_t notes;
  };

  void timer_loop();
  void check_time();

  std::shared_ptr<std::atomic<uint64_t>> rough_time_;
  uint64_t last_time_ = 0;
  std::deque<NpsWindow> windows_;
  uint64_t total_window_sum_ = 0;
  uint64_t current_window_sum_ = 0;

  std::shared_ptr<std::atomic<bool>> stop_flag_;
  std::thread timer_thread_;
};
} // namespace OmniMIDI

#endif