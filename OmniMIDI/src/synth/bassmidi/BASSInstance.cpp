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

OmniMIDI::BASSInstance::BASSInstance(ErrorSystem::Logger *pErr,
                                     BASSSettings *bassConfig,
                                     uint32_t channels) {
    bool mtMode = bassConfig->Threading == Multithreaded;
    
    uint32_t deviceFlags =
                  (bassConfig->MonoRendering ? BASS_DEVICE_MONO : BASS_DEVICE_STEREO);
    
    uint32_t streamFlags = BASS_MIDI_DECAYEND | BASS_SAMPLE_FLOAT |
                  (mtMode ? BASS_STREAM_DECODE : 0) |
                  (bassConfig->MonoRendering ? BASS_SAMPLE_MONO : 0) |
                  (bassConfig->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0) |
                  (bassConfig->DisableEffects ? BASS_MIDI_NOFX : 0);

    
    ErrLog = pErr;
    singleInstance = !mtMode;
    audioLimiter = 0;
    evbuf_len = 0;
    evbuf_capacity = mtMode ? bassConfig->GlobalEvBufSize : bassConfig->InstanceEvBufSize;

    // This isn't under BASSThreadMgr, initialize BASS
    if (!mtMode) {
        BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);
        BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
        BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

#if !defined(_WIN32)
        // Only Linux and macOS can do this
        BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, bassConfig->BufPeriod * -1);
#else
        BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, bassConfig->AudioBuf);
#endif
        BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, bassConfig->AudioBuf);

        if (!BASS_Init(-1, bassConfig->SampleRate, deviceFlags, NULL, NULL)) {
            throw BASS_ErrorGetCode();
        }
    }

    stream = BASS_MIDI_StreamCreate(channels, streamFlags, bassConfig->SampleRate);
    if (stream == 0) {
        if (singleInstance)
            BASS_Free();

        throw BASS_ErrorGetCode();
    }

    evbuf = new uint32_t[evbuf_capacity] { };
    if (!evbuf) {
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
            if (!BASS_ChannelSetAttribute(
                    stream, BASS_ATTRIB_MIDI_QUEUE_ASYNC,
                    (float)evbuf_capacity * sizeof(uint32_t))) {
                BASS_StreamFree(stream);
                BASS_Free();
                
                Error("Failed to set async buffer size!", true);
            }
        }

        if (bassConfig->FloatRendering && bassConfig->AudioLimiter) {
            BASS_BFX_COMPRESSOR2 compressor;

            audioLimiter = BASS_ChannelSetFX(stream, BASS_FX_BFX_COMPRESSOR2, 0);

            BASS_FXGetParameters(audioLimiter, &compressor);
            BASS_FXSetParameters(audioLimiter, &compressor);

            Message("BASS audio limiter enabled.");
        }

        if (!BASS_ChannelPlay(stream, false)) {
            BASS_StreamFree(stream);
            BASS_Free();
            throw BASS_ErrorGetCode();
        }

        Message("BASS stream is now playing.");
    }
}

OmniMIDI::BASSInstance::~BASSInstance() {
    delete[] evbuf;
    BASS_StreamFree(stream);
    if (singleInstance)
        BASS_Free();
}

void OmniMIDI::BASSInstance::SendEvent(uint32_t event) {
    std::unique_lock<std::mutex> lck(evbuf_mutex);

    if (evbuf_len == evbuf_capacity) {
        lck.unlock();
        FlushEvents();
    }

    evbuf[evbuf_len++] = event;
}

bool OmniMIDI::BASSInstance::SendDirectEvent(uint32_t chan, uint32_t evt, uint32_t param) {
    return BASS_MIDI_StreamEvent(stream, chan, evt, param);
}

void OmniMIDI::BASSInstance::UpdateStream(uint32_t ms) {
    BASS_ChannelUpdate(stream, ms);
}

int OmniMIDI::BASSInstance::ReadSamples(float *buffer, size_t num_samples) {
    FlushEvents();
    return BASS_ChannelGetData(stream, buffer, num_samples * sizeof(float));
}

uint64_t OmniMIDI::BASSInstance::GetVoiceCount() {
    float val;
    BASS_ChannelGetAttribute(stream, BASS_ATTRIB_MIDI_VOICES_ACTIVE, &val);
    return (uint64_t)val;
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

    FlushEvents();

    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_NOTESOFF, 0);
    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SOUNDOFF, 0);
    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_RESET, 0);
    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEMEX, bmType);
}

void OmniMIDI::BASSInstance::FlushEvents() {
    std::unique_lock<std::mutex> lck(evbuf_mutex);

    BASS_MIDI_StreamEvents(stream,
                           BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS,
                           evbuf, evbuf_len * sizeof(uint32_t));
          
    evbuf_len = 0;
}