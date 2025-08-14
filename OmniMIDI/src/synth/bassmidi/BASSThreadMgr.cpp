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

#include "BASSThreadMgr.hpp"
#include "bass/bass.h"
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <sys/types.h>

void ThreadFunc(OmniMIDI::BASSThreadManager::ThreadInfo *info);

static size_t calc_render_size(uint32_t sample_rate, float buffer_ms) {
    float val = (float)sample_rate * buffer_ms / 1000.0;
    val = fmax(val, 1.0);
    return (size_t)val;
}

OmniMIDI::BASSThreadManager::BASSThreadManager(ErrorSystem::Logger *PErr,
                                               BASSSettings *bassConfig) {
    ErrLog = PErr;

    uint32_t sample_rate = bassConfig->SampleRate;
    uint16_t audio_channels = bassConfig->MonoRendering ? 1 : 2;

    Message("Initializing BASS");

    shared.num_instances = bassConfig->KeyboardDivisions * 16;
    if (bassConfig->ThreadCount == 0 ||
        bassConfig->ThreadCount > shared.num_instances) {
        shared.num_threads = shared.num_instances;
    } else {
        shared.num_threads = bassConfig->ThreadCount;
    }

    float buffer_ms = bassConfig->AudioBuf;
    kbdiv = (uint32_t)bassConfig->KeyboardDivisions;

    BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
    if (!BASS_Init(0, sample_rate, 0, NULL, NULL))
        throw std::runtime_error("Cannot start BASS");

    size_t render_size = calc_render_size(sample_rate, buffer_ms);
    size_t buffer_len = render_size * (size_t)audio_channels;

    shared.instance_buffers = new float*[shared.num_instances] { };

    Message("Creating %d BASSMIDI streams. Allocated buffer len: %zu",
            shared.num_instances, buffer_len);

    shared.instances = new BASSInstance *[shared.num_instances];

    for (uint32_t i = 0; i < shared.num_instances; i++) {
        shared.instances[i] = new BASSInstance(ErrLog, bassConfig, 1);
        shared.instance_buffers[i] = new float[buffer_len] { };
    }

    shared.nps =
        new NpsLimiter(shared.num_instances, bassConfig->MaxInstanceNPS);

    Message("Creating %d threads", shared.num_threads);

    shared.should_exit = 0;
    shared.work_in_progress = 0;
    shared.active_thread_count = 0;

    threads = new ThreadInfo[shared.num_threads];
    shared.thread_is_working = new uint8_t[shared.num_threads];

    for (uint32_t i = 0; i < shared.num_threads; i++) {
        ThreadInfo *t = &threads[i];
        t->thread_idx = i;
        t->shared = &shared;

        t->thread = std::jthread(ThreadFunc, t);
    }

    for (uint32_t i = 0; i < kbdiv; i++) {
        shared.instances[(9 * kbdiv) + i]->SetDrums(true);
    }

    AudioStreamParams stream_params{sample_rate, audio_channels};
    auto render_func = [self = this](std::vector<float> &buffer) mutable {
        self->ReadSamples(buffer.data(), buffer.size());
    };

    Message("Initializing buffered renderer: RenderSize=%dsamples",
            render_size);
    buffered = new BufferedRenderer(render_func, stream_params, render_size);

    Message("Initializing audio playback system");

    auto audio_pipe = [self = buffered](std::vector<float> &buffer) mutable {
        self->read(buffer);
    };
    audio_player = new MIDIAudioPlayer(PErr, sample_rate, audio_channels,
                                       bassConfig->AudioLimiter, audio_pipe);

    Message("BASSThreadManager intialization successful");
}

OmniMIDI::BASSThreadManager::~BASSThreadManager() {
    Message("Stopping BASSThreadManager");

    delete audio_player;
    delete buffered;

    {
        std::unique_lock<std::mutex> lck(shared.mutex);

        shared.should_exit = 1;
        for (uint32_t i = 0; i < shared.num_threads; i++)
            shared.thread_is_working[i] = 1;

        shared.work_available.notify_all();
    }

    Message("Waiting for threads to exit");
    for (uint32_t i = 0; i < shared.num_threads; i++) {
        threads[i].thread.join();
    }

    for (uint32_t i = 0; i < shared.num_instances; i++) {
        delete shared.instances[i];
        delete[] shared.instance_buffers[i];
    }

    delete shared.instances;
    delete shared.nps;

    delete[] shared.instance_buffers;
    delete shared.thread_is_working;

    // delete threads;

    Message("Freeing BASS");

    BASS_Free();
}

void OmniMIDI::BASSThreadManager::SendEvent(uint32_t event) {
    const uint32_t head = event & 0xFF;
    const uint32_t channel = head & 0xF;
    const uint32_t code = head >> 4;
    uint32_t ev;

    switch (code) {
    case 0x8: { // Note Off
        ev = event & 0xFFFFF0;
        const uint32_t key = (ev >> 8) & 0xFF;
        const uint32_t idx = (channel * kbdiv) + (key % kbdiv);

        if (shared.nps->note_off(idx, key)) {
            shared.instances[idx]->SendEvent(ev);
        }
        break;
    }

    case 0x9: { // Note on
        ev = event & 0xFFFFF0;
        const uint32_t key = (ev >> 8) & 0xFF;
        const uint32_t vel = (ev >> 16) & 0xFF;
        const uint32_t idx = (channel * kbdiv) + (key % kbdiv);

        if (shared.nps->note_on(idx, key, vel)) {
            shared.instances[idx]->SendEvent(ev);
        }

        break;
    }

    case 0xF: { // System
        if (head == SystemReset) {
            ev = event & 0xFFFFF0;
            const uint32_t type = (ev >> 8) & 0xFF;

            shared.nps->reset();
            for (uint32_t i = 0; i < shared.num_instances; i++) {
                shared.instances[i]->ResetStream(type);
            }
        } else {
            for (uint32_t i = 0; i < shared.num_instances; i++) {
                shared.instances[i]->SendEvent(event);
            }
        }

        break;
    }

    default: { // Other channel specific messages
        ev = event & 0xFFFFF0;
        for (uint32_t i = 0; i < kbdiv; i++) {
            uint32_t idx = (channel * kbdiv) + i;
            shared.instances[idx]->SendEvent(ev);
        }
    }
    }
}

void OmniMIDI::BASSThreadManager::ReadSamples(float *buffer,
                                              size_t num_samples) {
    {
        std::unique_lock<std::mutex> lck(shared.mutex);

        shared.active_voices = 0;

        shared.should_exit = 0;
        shared.num_samples = num_samples;
        shared.active_thread_count = shared.num_threads;
        shared.work_in_progress = 1;
        for (uint32_t i = 0; i < shared.num_threads; i++)
            shared.thread_is_working[i] = 1;

        shared.work_available.notify_all();
        shared.work_done.wait(lck, [&] { return !shared.work_in_progress; });
    }

    memset(buffer, 0, num_samples * sizeof(float));
    for (uint32_t i = 0; i < shared.num_instances; i++) {
        float *curr = shared.instance_buffers[i];
        for (uint32_t s = 0; s < num_samples; s++) {
            buffer[s] += curr[s];
        }
    }

    ActiveVoices = shared.active_voices;
    RenderTime = buffered->average_renderer_load() * 100.0f;
}

int OmniMIDI::BASSThreadManager::SetSoundFonts(
    const std::vector<BASS_MIDI_FONTEX> &sfs) {
    for (uint32_t i = 0; i < shared.num_instances; i++) {
        int err = shared.instances[i]->SetSoundFonts(sfs);
        if (err < 0) {
            return BASS_ErrorGetCode();
        }
    }

    return 0;
}

void OmniMIDI::BASSThreadManager::ClearSoundFonts() {
    std::vector<BASS_MIDI_FONTEX> empty;
    for (uint32_t i = 0; i < shared.num_instances; i++) {
        shared.instances[i]->SetSoundFonts(empty);
    }
}

uint64_t OmniMIDI::BASSThreadManager::GetActiveVoices() { return ActiveVoices; }

float OmniMIDI::BASSThreadManager::GetRenderingTime() { return RenderTime; }

void ThreadFunc(OmniMIDI::BASSThreadManager::ThreadInfo *info) {
    using namespace OmniMIDI;

    OmniMIDI::BASSThreadManager::ThreadSharedInfo *shared = info->shared;
    uint32_t thread_idx = info->thread_idx;

    while (1) {
        {
            std::unique_lock<std::mutex> lck(shared->mutex);
            shared->work_available.wait(lck, [&] {
                return (shared->work_in_progress &&
                        shared->thread_is_working[thread_idx]) ||
                       shared->should_exit;
            });

            if (shared->should_exit) {
                break;
            }
        }

        uint64_t thread_active_voices = 0;

        for (uint32_t i = thread_idx; i < shared->num_instances;
             i += shared->num_threads) {
            BASSInstance *instance = shared->instances[i];

            memset(shared->instance_buffers[i], 0,
                   shared->num_samples * sizeof(float));
            instance->ReadSamples(shared->instance_buffers[i],
                                  shared->num_samples);
            thread_active_voices += instance->GetVoiceCount();
        }

        {
            std::unique_lock<std::mutex> lck(shared->mutex);
            shared->thread_is_working[thread_idx] = 0;
            shared->active_voices += thread_active_voices;
            if (--shared->active_thread_count == 0) {
                shared->work_in_progress = 0;
                shared->work_done.notify_one();
            }
        }
    }
}

#endif