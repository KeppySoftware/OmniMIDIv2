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

#include "BASSInstance.hpp"
#include "bass/bass_fx.h"
#include "bass/bassmidi.h"
#include <cstdint>

OmniMIDI::BASSInstance::BASSInstance(ErrorSystem::Logger *pErr,
                                     BASSSettings *bassConfig,
                                     uint32_t channels) {
    bool mtMode = bassConfig->Threading == Multithreaded;

    ErrLog = pErr;
    num_channels = channels;
    audioLimiter = 0;
    evbuf_len = 0;
    evbuf_capacity =
        mtMode ? bassConfig->InstanceEvBufSize : bassConfig->GlobalEvBufSize;

    bool decodeMode = mtMode || bassConfig->AudioEngine != Internal;

    uint32_t streamFlags =
        BASS_MIDI_DECAYEND | BASS_SAMPLE_FLOAT |
        (decodeMode ? BASS_STREAM_DECODE : 0) |
        (bassConfig->Threading == Standard ? BASS_MIDI_ASYNC : 0) |
        (bassConfig->MonoRendering ? BASS_SAMPLE_MONO : 0) |
        (bassConfig->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0) |
        (bassConfig->DisableEffects ? BASS_MIDI_NOFX : 0);

    stream =
        BASS_MIDI_StreamCreate(channels, streamFlags, bassConfig->SampleRate);
    if (stream == 0) {
        throw BASS_ErrorGetCode();
    }

    evbuf = new uint32_t[evbuf_capacity]{};
    if (!evbuf) {
        BASS_StreamFree(stream);
        throw std::runtime_error("");
    }

    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0);
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_VOICES,
                             (float)bassConfig->VoiceLimit);

    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 0);

    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_KILL, 1);

    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_CPU,
                             (float)bassConfig->RenderTimeLimit);

    if (!mtMode) {
        if (bassConfig->Threading == Standard) {
            if (!BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_QUEUE_ASYNC,
                                          (float)evbuf_capacity *
                                              sizeof(uint32_t))) {
                BASS_StreamFree(stream);
                throw std::runtime_error("Failed to set async buffer size!");
            }
        }

        if (bassConfig->FloatRendering && bassConfig->AudioLimiter) {
            BASS_BFX_COMPRESSOR2 compressor;

            audioLimiter =
                BASS_ChannelSetFX(stream, BASS_FX_BFX_COMPRESSOR2, 0);

            BASS_FXGetParameters(audioLimiter, &compressor);
            BASS_FXSetParameters(audioLimiter, &compressor);

            Message("BASS audio limiter enabled.");
        }

        if (bassConfig->AudioEngine == Internal) {
            if (!BASS_ChannelPlay(stream, false)) {
                BASS_StreamFree(stream);
                throw BASS_ErrorGetCode();
            }
        }

        Message("BASS stream is now playing.");
    }
}

OmniMIDI::BASSInstance::~BASSInstance() {
    delete[] evbuf;

    BASS_ChannelStop(stream);
    BASS_StreamFree(stream);
}

void OmniMIDI::BASSInstance::SendEvent(uint32_t event) {
    std::unique_lock<std::mutex> lck(evbuf_mutex);

    if (evbuf_len == evbuf_capacity) {
        lck.unlock();
        FlushEvents();
    }

    evbuf[evbuf_len++] = event;
}

bool OmniMIDI::BASSInstance::SendDirectEvent(uint32_t chan, uint32_t evt,
                                             uint32_t param) {
    return BASS_MIDI_StreamEvent(stream, chan, evt, param);
}

uint32_t OmniMIDI::BASSInstance::GetHandle() { return stream; }

void OmniMIDI::BASSInstance::UpdateStream(uint32_t ms) {
    BASS_ChannelUpdate(stream, ms);
}

int OmniMIDI::BASSInstance::ReadData(void *buffer, size_t size) {
    FlushEvents();
    return BASS_ChannelGetData(stream, buffer, size);
}

uint64_t OmniMIDI::BASSInstance::GetActiveVoices() {
    uint64_t val = 0;
    for (uint32_t i = 0; i < num_channels; i++) {
        int res = BASS_MIDI_StreamGetEvent(stream, i, MIDI_EVENT_VOICES);
        if (res != -1)
            val += res;
    }
    return val;
}

float OmniMIDI::BASSInstance::GetRenderingTime() {
    float val;
    BASS_ChannelGetAttribute(stream, BASS_ATTRIB_CPU, &val);
    return val;
}

int OmniMIDI::BASSInstance::SetSoundFonts(
    const std::vector<BASS_MIDI_FONTEX> &sfs) {
    return BASS_MIDI_StreamSetFonts(stream, sfs.data(),
                                    (uint32_t)sfs.size() | BASS_MIDI_FONT_EX);
}

void OmniMIDI::BASSInstance::SetDrums(bool isDrumsChan) {
    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_DEFDRUMS, (float)isDrumsChan);
}

void OmniMIDI::BASSInstance::ResetStream(uint8_t type) {
    uint32_t bmType = 0;

    switch (type) {
    case 0x01:
        bmType = MIDI_SYSTEM_GS;
        break;

    case 0x02:
        bmType = MIDI_SYSTEM_GM1;
        break;

    case 0x03:
        bmType = MIDI_SYSTEM_GM2;
        break;

    case 0x04:
        bmType = MIDI_SYSTEM_XG;
        break;

    default:
        bmType = MIDI_SYSTEM_DEFAULT;
        break;
    }

    {
        std::unique_lock<std::mutex> lck(evbuf_mutex);
        evbuf_len = 0;
    }

    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEMEX, bmType);
}

void OmniMIDI::BASSInstance::FlushEvents() {
    std::unique_lock<std::mutex> lck(evbuf_mutex);

    BASS_MIDI_StreamEvents(stream,
                           BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS,
                           evbuf, evbuf_len * sizeof(uint32_t));

    evbuf_len = 0;
}