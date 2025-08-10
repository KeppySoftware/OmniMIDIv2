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
#include <portaudio.h>
#include <stdexcept>
#include <vector>

using namespace OmniMIDI;

static int paCallback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo,
                      PaStreamCallbackFlags statusFlags, void *userData) {
    OmniMIDI::MIDIAudioPlayer::AudioPlayerArgument *argument =
        (OmniMIDI::MIDIAudioPlayer::AudioPlayerArgument *)userData;
    OmniMIDI::MIDIAudioPlayer::AudioPipe audio_pipe = argument->audio_pipe;
    OmniMIDI::AudioLimiter *limiter = argument->limiter;

    float *out = (float *)outputBuffer;

    std::vector<float> outVec(framesPerBuffer * argument->render_channels);

    audio_pipe(outVec);
    if (limiter) {
        limiter->process(outVec);
    }

    if (argument->render_channels == argument->device_channels) {
        std::copy(outVec.begin(), outVec.end(), out);
    } else if (argument->render_channels == 2 &&
               argument->device_channels == 1) {
        for (uint16_t c = 0; c < framesPerBuffer; c++) {
            out[c] = (outVec[c * 2] + outVec[c * 2 + 1]) / 2.0f;
        }
    } else if (argument->render_channels == 1 &&
               argument->device_channels == 2) {
        for (uint16_t c = 0; c < framesPerBuffer; c++) {
            out[c * 2] = outVec[c];
            out[c * 2 + 1] = outVec[c];
        }
    }

    return 0;
}

OmniMIDI::AudioLimiter::AudioLimiter(uint16_t channels, uint32_t sample_rate) {
    num_channels = channels;
    compressors = (Compressor *)malloc(channels * sizeof(Compressor));
    for (uint16_t i = 0; i < channels; i++) {
        if (!compressor_create(&compressors[i], sample_rate, 0.3, 1000.0, 10.0,
                               50.0, 10.0))
            throw std::runtime_error("Error initializing audio limiter");
    }
}

OmniMIDI::AudioLimiter::~AudioLimiter() {
    for (uint16_t i = 0; i < num_channels; i++) {
        compressor_destroy(&compressors[i]);
    }
    free(compressors);
}

void OmniMIDI::AudioLimiter::process(std::vector<float> &samples) {
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] =
            compressor_process(&compressors[i % num_channels], samples[i]);
    }
}

OmniMIDI::MIDIAudioPlayer::MIDIAudioPlayer(ErrorSystem::Logger *PErr,
                                           uint32_t sample_rate,
                                           uint16_t channels,
                                           bool enable_limiter,
                                           AudioPipe audio_pipe)
    : ErrLog(PErr) {
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error(Pa_GetErrorText(err));
    }

    Message("Initialized PortAudio");

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        throw std::runtime_error("Error: No default output device.");
    }

    Message("Found audio output device: ID%d", outputParameters.device);

    const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(outputParameters.device);
    outputParameters.channelCount = devInfo->maxOutputChannels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = devInfo->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    arg.audio_pipe = audio_pipe;
    arg.limiter = NULL;
    arg.render_channels = channels;
    arg.device_channels = devInfo->maxOutputChannels;
    if (enable_limiter) {
        arg.limiter = new AudioLimiter(channels, sample_rate);
    }

    Message("Opening audio stream: SampleRate=%d | DeviceChannels=%d | "
            "RenderChannels=%d",
            sample_rate, outputParameters.channelCount, channels);

    err = Pa_OpenStream(&stream, NULL, &outputParameters, (double)sample_rate,
                        paFramesPerBufferUnspecified, paClipOff, paCallback,
                        &arg);
    if (err != paNoError) {
        throw std::runtime_error(Pa_GetErrorText(err));
    }

    Pa_StartStream(stream);

    Message("PortAudio initialization complete");
}

OmniMIDI::MIDIAudioPlayer::~MIDIAudioPlayer() {
    Message("Closing PortAudio stream");

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    if (arg.limiter)
        delete arg.limiter;

    Message("Audio player cleanup complete");
}