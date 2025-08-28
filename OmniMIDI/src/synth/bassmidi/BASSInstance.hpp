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

#include "BASSSettings.hpp"
#include "bass/bass.h"
#include "bass/bassmidi.h"
#include <mutex>
#include <vector>

namespace OmniMIDI {
class BASSInstance {
  public:
    BASSInstance(ErrorSystem::Logger *pErr, BASSSettings *bassConfig,
                 uint32_t channels);
    ~BASSInstance();

    void SendEvent(uint32_t event);
    bool SendDirectEvent(uint32_t chan, uint32_t evt, uint32_t param);
    void FlushEvents();
    void FlushEventBuffer();

    uint32_t GetHandle();
    void UpdateStream(uint32_t ms);
    int ReadData(void *buffer, size_t size);

    uint64_t GetActiveVoices();
    float GetRenderingTime();

    int SetSoundFonts(const std::vector<BASS_MIDI_FONTEX> &sfs);
    void SetDrums(bool isDrumsChan);
    void ResetStream(uint8_t bmType);

  private:
    uint32_t num_channels;

    ErrorSystem::Logger *ErrLog = nullptr;

    uint32_t *evbuf;
    uint32_t evbuf_len;
    uint32_t evbuf_capacity;

    std::mutex evbuf_mutex;

    HSTREAM stream;
    HFX audioLimiter;
};
} // namespace OmniMIDI