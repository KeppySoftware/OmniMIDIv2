#ifndef BASS_THREAD_MGR_H
#define BASS_THREAD_MGR_H

#include "../../audio/AudioPlayer.hpp"
#include "../../audio/BufferedRenderer.hpp"
#include "../../audio/NpsLimiter.hpp"
#include "BASSSettings.hpp"
#include "bass/bass.h"
#include "bass/bassmidi.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace OmniMIDI {
class BASSInstance {
public:
  BASSInstance(BASSSettings *bassConfig, uint32_t channels);
  ~BASSInstance();

  void SendEvent(uint32_t event);
  int ReadSamples(float *buffer, size_t num_samples);
  uint64_t GetVoiceCount();

  int SetSoundFonts(const std::vector<BASS_MIDI_FONTEX> &sfs);
  void SetDrums(bool isDrumsChan);
  void ResetStream();

private:
  void FlushEvents();

  uint32_t *evbuf;
  uint32_t evbuf_len;
  uint32_t evbuf_capacity;

  HSTREAM stream;
};

class BASSThreadManager {
public:
  struct NpsInstance {
    NpsLimiter *nps;
    uint64_t max_nps;
    uint64_t skipped_notes_[128 * 16];
  };

  struct ThreadSharedInfo {
    uint32_t num_threads;

    BASSInstance **instances;
    NpsInstance *nps_insts;
    uint32_t num_instances;

    uint32_t num_samples;
    float **instance_buffers;

    std::mutex mutex;
    std::condition_variable work_available;
    std::condition_variable work_done;

    int work_in_progress;
    int active_thread_count;
    int should_exit;
    uint8_t *thread_is_working;

    uint64_t active_voices;
  };

  struct ThreadInfo {
    std::jthread thread;
    uint32_t thread_idx;

    ThreadSharedInfo *shared;
  };

  BASSThreadManager(ErrorSystem::Logger *PErr, BASSSettings *bassConfig);
  ~BASSThreadManager();
  void SendEvent(uint32_t event);
  void ReadSamples(float *buffer, size_t num_samples);
  int SetSoundFonts(const std::vector<BASS_MIDI_FONTEX> &sfs);
  void ClearSoundFonts();

  uint64_t GetActiveVoices();
  float GetRenderingTime();

private:
  ErrorSystem::Logger *ErrLog = nullptr;

  uint32_t kbdiv;

  ThreadInfo *threads;
  ThreadSharedInfo shared;

  BufferedRenderer *buffered = nullptr;

  uint64_t ActiveVoices = 0;
  float RenderTime = 0.0;

  MIDIAudioPlayer audio_player;
};
} // namespace OmniMIDI

#endif