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

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "../ErrSys.hpp"
#include "Limiter.hpp"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <miniaudio.h>

namespace OmniMIDI {

class MIDIAudioPlayer {
  public:
    using AudioPipe = std::function<void(std::vector<float> &)>;

    struct AudioPlayerArgument {
        uint16_t render_channels;
        uint16_t device_channels;
        AudioPipe audio_pipe;
        AudioLimiter *limiter;
    };

    MIDIAudioPlayer(ErrorSystem::Logger *PErr, uint32_t sample_rate,
                    uint16_t channels, bool enable_limiter,
                    AudioPipe audio_pipe);
    ~MIDIAudioPlayer();

  private:
    ErrorSystem::Logger *ErrLog = nullptr;

    ma_device device;

    AudioPlayerArgument arg;
};
} // namespace OmniMIDI

#endif