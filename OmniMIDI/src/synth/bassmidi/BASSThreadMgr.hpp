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

#ifdef _NONFREE

#ifndef BASS_THREAD_MGR_H
#define BASS_THREAD_MGR_H

#include "../../audio/AudioPlayer.hpp"
#include "../../audio/BufferedRenderer.hpp"
#include "../../audio/NpsLimiter.hpp"
#include "BASSSettings.hpp"
#include "BASSInstance.hpp"
#include "bass/bass.h"
#include "bass/bassmidi.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace OmniMIDI {
class BASSThreadManager {
  public:
    struct ThreadSharedInfo {
        uint32_t num_threads;

        BASSInstance **instances;
        uint32_t num_instances;

        NpsLimiter *nps = nullptr;

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

    MIDIAudioPlayer *audio_player = nullptr;
};
} // namespace OmniMIDI

#endif

#endif