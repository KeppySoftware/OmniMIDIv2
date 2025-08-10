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
 *
 * This file contains the required code to run the driver under UNIX
 * OSes. This file is useful only if you want to compile the driver
 * under those OSes, it's not needed for Windows.
 */

#ifndef _WIN32

#ifdef __VSCODE_IDE__
#define OM_STANDALONE
#endif

#include "../Common.hpp"
#include "../KDMAPI.hpp"
#include "../synth/SynthHost.hpp"
#include <alsa/asoundlib.h>
#include <iostream>
#include <thread>
#include <unistd.h>

static ErrorSystem::Logger *ErrLog = nullptr;
static OmniMIDI::SynthHost *Host = nullptr;

#ifdef OM_STANDALONE
// Global objects
static int32_t in_port;
static snd_seq_t *seq_handle = nullptr;
static std::jthread seq_thread;

static OMShared::Funcs *Utils = nullptr;

void standalone();
snd_seq_event_t *readEvent();
void evThread();
#endif

void DESTRUCTOR stop();

int CONSTRUCTOR main(int argc, char *argv[]) {
    try {
        // Initialize logger
        ErrLog = new ErrorSystem::Logger();
        Host = new OmniMIDI::SynthHost(ErrLog);

#ifdef OM_STANDALONE
        Utils = new OMShared::Funcs();

        standalone();
        stop();

        Message("stop called. Terminating OmniMIDI...");
#else
        Message("main succeeded! OmniMIDI loaded.");
#endif

    } catch (const std::exception &e) {
        std::cerr << "Exception during initialization: " << e.what()
                  << std::endl;
        stop();
        return -1;
    }

    return 0;
}

void DESTRUCTOR stop() {
    if (Host) {
        Host->Stop();
        delete Host;
        Host = nullptr;
    }

    if (ErrLog) {
        delete ErrLog;
        ErrLog = nullptr;
    }

#ifdef OM_STANDALONE
    if (seq_handle) {
        snd_seq_delete_simple_port(seq_handle, in_port);
        snd_seq_close(seq_handle);
        seq_handle = nullptr;
    }

    if (Utils) {
        delete Utils;
        Utils = nullptr;
    }
#endif
}

extern "C" {
int32_t EXPORT IsKDMAPIAvailable() { return (int)Host->IsKDMAPIAvailable(); }

int32_t EXPORT InitializeKDMAPIStream() {
    if (Host->Start()) {
        Message("KDMAPI initialized.");
        return 1;
    }

    Message("KDMAPI failed to initialize.");
    return 0;
}

int32_t EXPORT TerminateKDMAPIStream() {
    if (Host->Stop()) {
        Message("KDMAPI freed.");
        return 1;
    }

    Message("KDMAPI failed to free its resources.");
    return 0;
}

void EXPORT ResetKDMAPIStream() { Host->PlayShortEvent(0xFF, 0x00, 0x00); }

void EXPORT SendDirectData(uint32_t ev) { Host->PlayShortEvent(ev); }

void EXPORT SendDirectDataNoBuf(uint32_t ev) {
    // Unsupported, forward to SendDirectData
    SendDirectData(ev);
}

uint32_t EXPORT SendDirectLongData(char *IIMidiHdr, uint32_t IIMidiHdrSize) {
    return Host->PlayLongEvent(IIMidiHdr, IIMidiHdrSize) == SYNTH_OK ? 0 : 11;
}

uint32_t EXPORT SendDirectLongDataNoBuf(char *IIMidiHdr,
                                        uint32_t IIMidiHdrSize) {
    // Unsupported, forward to SendDirectLongData
    return SendDirectLongData(IIMidiHdr, IIMidiHdrSize);
}

int32_t EXPORT SendCustomEvent(uint32_t evt, uint32_t chan, uint32_t param) {
    return Host->TalkToSynthDirectly(evt, chan, param);
}

int32_t EXPORT DriverSettings(uint32_t setting, uint32_t mode, void *value,
                              uint32_t cbValue) {
    return 1;
}

int32_t EXPORT LoadCustomSoundFontsList(char *Directory) {
    return DriverSettings(KDMAPI_SOUNDFONT, true, Directory, MAX_PATH_LONG);
}

float EXPORT GetRenderingTime() {
    if (Host == nullptr)
        return 0.0f;

    return Host->GetRenderingTime();
}

uint64_t EXPORT GetVoiceCount() {
    if (Host == nullptr)
        return 0;

    return Host->GetActiveVoices();
}
}

#ifdef OM_STANDALONE
void standalone() {
    int32_t status = 0;
    uint32_t size = 1 << 30;

    try {
        status = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT,
                              SND_SEQ_NONBLOCK);
        if (status) {
            Error("snd_seq_open failed.", true);
            return;
        } else
            Message("snd_seq_open %x >> GOOD", status);

        status = snd_seq_set_client_name(seq_handle, NAME);
        if (status) {
            Error("snd_seq_set_client_name failed.", true);
            return;
        } else
            Message("snd_seq_set_client_name %x >> %s", status, NAME);

        in_port = snd_seq_create_simple_port(
            seq_handle, "virtual",
            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_SYNTHESIZER |
                SND_SEQ_PORT_TYPE_SOFTWARE | SND_SEQ_PORT_TYPE_MIDI_GM |
                SND_SEQ_PORT_TYPE_MIDI_GS | SND_SEQ_PORT_TYPE_MIDI_XG);
        if (in_port < 0) {
            Error("snd_seq_create_simple_port failed.", true);
            return;
        } else
            Message(
                "snd_seq_set_client_name %x >> virtual port with caps GM/GS/XG",
                in_port);

        status = snd_seq_set_input_buffer_size(seq_handle, size);
        if (status) {
            Error("snd_seq_set_client_name failed.", true);
            return;
        } else
            Message("snd_seq_set_input_buffer_size >> %x", size);

        if (Host->Start()) {
            seq_thread = std::jthread(&evThread);
            if (!seq_thread.joinable()) {
                Error("seq_thread failed. (ID: %x)", true, seq_thread.get_id());
                return;
            } else
                Message("ALSA MIDI thread allocated.");

            while (seq_thread.joinable()) {
                Utils->MicroSleep(SLEEPVAL(1));

                if (!seq_thread.joinable()) {
                    seq_thread = std::jthread(&evThread);
                    if (!seq_thread.joinable()) {
                        return;
                    } else
                        Message("ALSA MIDI thread allocated.");
                }
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception during initialization: " << e.what()
                  << std::endl;
    }
}

snd_seq_event_t *readEvent() {
    snd_seq_event_t *ev = NULL;
    int32_t ret = snd_seq_event_input(seq_handle, &ev);
    return ret < 0 ? nullptr : ev;
}

bool pBuf() {
    if (auto ev = readEvent()) {
        snd_seq_event_data_t evData = ev->data;
        uint32_t evDword = 0;

        switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
        case SND_SEQ_EVENT_NOTEOFF: {
            auto noteData = evData.note;

            auto vel = noteData.velocity;
            auto note = noteData.note;
            auto status = noteData.channel |
                          ((vel < 1 || ev->type == SND_SEQ_EVENT_NOTEOFF)
                               ? MIDI_CMD_NOTE_OFF
                               : MIDI_CMD_NOTE_ON);

            evDword = status | note << 8 | vel << 16;
            break;
        }

        case SND_SEQ_EVENT_KEYPRESS: {
            auto noteData = evData.note;

            auto pressure = noteData.velocity;
            auto note = noteData.note;
            auto status = noteData.channel | MIDI_CMD_NOTE_PRESSURE;

            evDword = status | note << 8 | pressure << 16;
            break;
        }

        case SND_SEQ_EVENT_CONTROLLER:
        case SND_SEQ_EVENT_CHANPRESS:
        case SND_SEQ_EVENT_PGMCHANGE:
        case SND_SEQ_EVENT_PITCHBEND:
        case SND_SEQ_EVENT_CONTROL14:
        case SND_SEQ_EVENT_REGPARAM:
        case SND_SEQ_EVENT_NONREGPARAM: {
            auto ctrlData = evData.control;
            auto cc = ctrlData.param;
            auto cv = ctrlData.value;
            auto status = ctrlData.channel;

            switch (ev->type) {
            case SND_SEQ_EVENT_CONTROLLER:
                status |= MIDI_CMD_CONTROL;
                break;

            case SND_SEQ_EVENT_CHANPRESS:
                status |= MIDI_CMD_CHANNEL_PRESSURE;
                break;

            default:
                break;
            }

            if (ev->type == SND_SEQ_EVENT_PGMCHANGE) {
                evDword = (status | MIDI_CMD_PGM_CHANGE) | cv << 8;
                break;
            }

            if (ev->type == SND_SEQ_EVENT_PITCHBEND) {
                evDword = (status | MIDI_CMD_BENDER) |
                          ((unsigned short)((short)cv + 0x2000) << 0x9);
                break;
            }

            evDword = status | cc << 8 | cv << 16;
            break;
        }

        case SND_SEQ_EVENT_SYSEX: {
            auto sysexData = evData.ext;
            char *ptr = (char *)sysexData.ptr;
            size_t len = sysexData.len;

            char *fixBuf = new char[len + 2]{0};
            fixBuf[0] = 0xF0;
            memcpy(fixBuf + 1, ptr, len);
            fixBuf[len] = 0xF7;

            Host->PlayLongEvent(fixBuf, len);
            delete[] fixBuf;

            return true;
        }

        case SND_SEQ_EVENT_RESET: {
            Host->Reset();
            return true;
        }

        case SND_SEQ_EVENT_PORT_SUBSCRIBED:
        case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: {
            auto subData = evData.connect;
            Message("Client %s: %x:%x",
                    ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED ? "connected"
                                                              : "disconnected",
                    subData.sender.client, subData.sender.port);
            Host->Reset();
            break;
        }

        default:
            Message("Unknown event %x (type %x)", evData.raw32.d[0], ev->type);
            return true;
        }

        Host->PlayShortEvent(evDword);
        return true;
    }

    return false;
}

void evThread() {
    while (!Host->IsSynthInitialized())
        ;

    auto ev = readEvent();
    do {

    } while (Host->IsSynthInitialized() && (ev = readEvent()) != nullptr);

    while (Host->IsSynthInitialized()) {
        if (!pBuf())
            Utils->MicroSleep(SLEEPVAL(1));
    }
}
#endif
#endif