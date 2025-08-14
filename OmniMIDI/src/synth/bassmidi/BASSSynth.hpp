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

#ifndef _BASSSYNTH_H
#define _BASSSYNTH_H

#include "../SynthModule.hpp"
#include <atomic>
#include <unordered_map>

#include "bass/bass.h"
#include "bass/bassmidi.h"

#include "BASSThreadMgr.hpp"

#define BASE_IMPORTS 38

#if defined(_WIN32)

#include "bass/bassasio.h"
#include "bass/basswasapi.h"
#define ADD_IMPORTS 32

#else

#define ADD_IMPORTS 0

#endif

#define FINAL_IMPORTS (BASE_IMPORTS + ADD_IMPORTS)

#define TGT_BASS 2 << 24 | 4 << 16 | 17 << 8 | 0
#define TGT_BASSMIDI 2 << 24 | 4 << 16 | 15 << 8 | 0

#define CCEvent(a)                                                             \
    evt = a;                                                                   \
    ev = param2;

#define CCEmptyEvent(a)                                                        \
    evt = a;                                                                   \
    ev = 0;

#define MIDI_EVENT_RAW 0xFFFF

namespace OmniMIDI {

class BASSSynth : public SynthModule {
  private:
    struct RealtimeStatistics {
        std::shared_ptr<std::atomic<uint64_t>> VoiceCount;
        std::shared_ptr<std::atomic<float>> RenderTime;
    };

    Lib *BAudLib = nullptr;
    Lib *BMidLib = nullptr;
    Lib *BFlaLib = nullptr;
    uint32_t BFlaLibHandle = 0;

#ifdef _WIN32
    Lib *BWasLib = nullptr;
    Lib *BAsiLib = nullptr;
    unsigned char ASIOBuf[2048] = {0};
#endif

    std::unordered_map<int, std::string> BASSErrReason{
        {BASS_ERROR_UNKNOWN, "Unknown"},
        {BASS_ERROR_MEM, "Memory error"},
        {BASS_ERROR_FILEOPEN, "Can't open file"},
        {BASS_ERROR_DRIVER, "Can't find a free/valid output device"},
        {BASS_ERROR_HANDLE, "Invalid stream handle"},
        {BASS_ERROR_FORMAT, "Unsupported sample format for output device"},
        {BASS_ERROR_INIT, "Init has not been successfully called"},
        {BASS_ERROR_REINIT, "Output device needs to be reinitialized"},
        {BASS_ERROR_ALREADY, "Output device already initialized/paused"},
        {BASS_ERROR_ILLTYPE, "Illegal type"},
        {BASS_ERROR_ILLPARAM, "Illegal parameter"},
        {BASS_ERROR_DEVICE, "Illegal device"},
        {BASS_ERROR_FREQ, "Illegal sample rate"},
        {BASS_ERROR_NOHW, "No hardware voices available"},
        {BASS_ERROR_NOTAVAIL, "Requested data/action is not available"},
        {BASS_ERROR_DECODE, "The channel is/isn't a \"decoding channel\""},
        {BASS_ERROR_FILEFORM, "Unsupported file format"},
        {BASS_ERROR_VERSION, "Invalid BASS version"},
        {BASS_ERROR_CODEC, "Requested audio codec is not available"},
        {BASS_ERROR_BUSY, "Output device is busy"}};

    LibImport LibImports[FINAL_IMPORTS] = {
        // BASS
        ImpFunc(BASS_GetVersion), ImpFunc(BASS_ChannelFlags),
        ImpFunc(BASS_ChannelGetAttribute), ImpFunc(BASS_ChannelGetData),
        ImpFunc(BASS_ChannelGetLevelEx), ImpFunc(BASS_ChannelIsActive),
        ImpFunc(BASS_ChannelPlay), ImpFunc(BASS_ChannelRemoveFX),
        ImpFunc(BASS_ChannelSeconds2Bytes), ImpFunc(BASS_ChannelSetAttribute),
        ImpFunc(BASS_ChannelSetDevice), ImpFunc(BASS_ChannelSetFX),
        ImpFunc(BASS_ChannelStop), ImpFunc(BASS_ChannelUpdate),
        ImpFunc(BASS_ErrorGetCode), ImpFunc(BASS_Free), ImpFunc(BASS_Update),
        ImpFunc(BASS_GetDevice), ImpFunc(BASS_GetDeviceInfo),
        ImpFunc(BASS_GetInfo), ImpFunc(BASS_Init), ImpFunc(BASS_SetConfig),
        ImpFunc(BASS_Stop), ImpFunc(BASS_StreamFree), ImpFunc(BASS_PluginLoad),
        ImpFunc(BASS_PluginFree),

        // BASSMIDI
        ImpFunc(BASS_MIDI_FontFree), ImpFunc(BASS_MIDI_FontInit),
        ImpFunc(BASS_MIDI_FontLoadEx), ImpFunc(BASS_MIDI_StreamCreate),
        ImpFunc(BASS_MIDI_StreamEvent), ImpFunc(BASS_MIDI_StreamEvents),
        ImpFunc(BASS_MIDI_StreamGetEvent), ImpFunc(BASS_MIDI_StreamSetFonts),
        ImpFunc(BASS_MIDI_StreamGetChannel), ImpFunc(BASS_MIDI_GetVersion),
        ImpFunc(BASS_FXGetParameters), ImpFunc(BASS_FXSetParameters),

#ifdef _WIN32
        // BASSWASAPI
        ImpFunc(BASS_WASAPI_Init), ImpFunc(BASS_WASAPI_Free),
        ImpFunc(BASS_WASAPI_IsStarted), ImpFunc(BASS_WASAPI_Start),
        ImpFunc(BASS_WASAPI_Stop), ImpFunc(BASS_WASAPI_GetDeviceInfo),
        ImpFunc(BASS_WASAPI_GetInfo), ImpFunc(BASS_WASAPI_GetDevice),
        ImpFunc(BASS_WASAPI_GetLevelEx), ImpFunc(BASS_WASAPI_SetNotify),

        // BASSASIO
        ImpFunc(BASS_ASIO_CheckRate), ImpFunc(BASS_ASIO_ChannelEnable),
        ImpFunc(BASS_ASIO_ChannelEnableBASS),
        ImpFunc(BASS_ASIO_ChannelEnableMirror),
        ImpFunc(BASS_ASIO_ChannelGetLevel), ImpFunc(BASS_ASIO_ChannelJoin),
        ImpFunc(BASS_ASIO_ChannelSetFormat), ImpFunc(BASS_ASIO_ChannelSetRate),
        ImpFunc(BASS_ASIO_ChannelGetInfo), ImpFunc(BASS_ASIO_ControlPanel),
        ImpFunc(BASS_ASIO_ErrorGetCode), ImpFunc(BASS_ASIO_Free),
        ImpFunc(BASS_ASIO_GetDevice), ImpFunc(BASS_ASIO_GetDeviceInfo),
        ImpFunc(BASS_ASIO_GetLatency), ImpFunc(BASS_ASIO_GetRate),
        ImpFunc(BASS_ASIO_GetInfo), ImpFunc(BASS_ASIO_Init),
        ImpFunc(BASS_ASIO_SetRate), ImpFunc(BASS_ASIO_Start),
        ImpFunc(BASS_ASIO_Stop), ImpFunc(BASS_ASIO_IsStarted)
#endif
    };

    bool LoadFuncs();
    bool ClearFuncs();
    size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

    void RenderingThread();
    void ProcessingThread();
    void StatsThread();

    bool isActive = false;

    BASSSettings *_bassConfig = nullptr;
    SoundFontSystem *_sfSystem = nullptr;
    std::vector<BASS_MIDI_FONTEX> SoundFonts;

    BASSThreadManager *thread_mgr = nullptr;
    BASSInstance *standard_instance = nullptr;

    std::jthread _StatsThread;

  public:
    BASSSynth(ErrorSystem::Logger *PErr);
    ~BASSSynth();

    void LoadSoundFonts() override;
    bool LoadSynthModule() override;
    bool UnloadSynthModule() override;
    bool StartSynthModule() override;
    bool StopSynthModule() override;
    bool SettingsManager(uint32_t setting, bool get, void *var,
                         size_t size) override;
    uint32_t GetSampleRate() override {
        return _bassConfig != nullptr ? _bassConfig->SampleRate : 0;
    }
    bool IsSynthInitialized() override { return isActive; }

    uint32_t SynthID() override { return 0x1411BA55; }

    uint32_t PlayLongEvent(uint8_t *ev, uint32_t size) override;
    uint32_t UPlayLongEvent(uint8_t *ev, uint32_t size) override;

    SynthResult TalkToSynthDirectly(uint32_t evt, uint32_t chan,
                                    uint32_t param) override;
};

} // namespace OmniMIDI

#endif
#endif