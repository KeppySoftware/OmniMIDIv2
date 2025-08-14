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

#include "AudioPlayer.hpp"
#include <stdexcept>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

using namespace OmniMIDI;

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
    using namespace OmniMIDI;

    MIDIAudioPlayer::AudioPlayerArgument *argument =
        (MIDIAudioPlayer::AudioPlayerArgument *)pDevice->pUserData;

    MIDIAudioPlayer::AudioPipe audio_pipe = argument->audio_pipe;

    AudioLimiter *limiter = argument->limiter;

    float *out = (float *)pOutput;
    std::vector<float> outVec(frameCount * argument->render_channels);
    audio_pipe(outVec);
    if (limiter) {
        limiter->process(outVec);
    }

    std::copy(outVec.begin(), outVec.end(), out);
}

OmniMIDI::MIDIAudioPlayer::MIDIAudioPlayer(ErrorSystem::Logger *PErr,
                                           uint32_t sample_rate,
                                           uint16_t channels,
                                           bool enable_limiter,
                                           AudioPipe audio_pipe)
    : ErrLog(PErr) {

    arg.audio_pipe = audio_pipe;
    arg.limiter = NULL;
    arg.render_channels = channels;
    arg.device_channels = channels;
    if (enable_limiter) {
        arg.limiter = new AudioLimiter(channels, sample_rate);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sample_rate;
    config.dataCallback = data_callback;
    config.pUserData = &arg;

    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize audio device");
    }

    ma_device_start(&device);

    Message("MIDIAudioPlayer stream initialized.");
}

OmniMIDI::MIDIAudioPlayer::~MIDIAudioPlayer() {
    Message("Closing MIDIAudioPlayer stream");

    ma_device_uninit(&device);

    if (arg.limiter)
        delete arg.limiter;

    Message("MIDIAudioPlayer cleanup complete");
}