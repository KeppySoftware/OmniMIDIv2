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

#include "../../Common.hpp"

// Not supported on ARM Thumb-2!
#ifndef _M_ARM

#ifndef _XSYNTHM_H
#define _XSYNTHM_H

#include "../SynthModule.hpp"

#include "xsynth.h"

#define XSYNTH_STR "XSynth"

namespace OmniMIDI {
class XSynthSettings : public SettingsModule {
  public:
    // Global settings
    bool FadeOutKilling = false;
    double RenderWindow = 10.0;
    uint16_t LayerCount = 4;
    int32_t ThreadsCount = 0;
    uint8_t Interpolation = XSYNTH_INTERPOLATION_NEAREST;

    XSynthSettings(ErrorSystem::Logger *PErr) : SettingsModule(PErr) {}

    void RewriteSynthConfig() {
        nlohmann::json DefConfig = {
            ConfGetVal(FadeOutKilling), ConfGetVal(RenderWindow),
            ConfGetVal(ThreadsCount),   ConfGetVal(LayerCount),
            ConfGetVal(Interpolation),
        };

        if (AppendToConfig(DefConfig))
            WriteConfig();

        CloseConfig();
        InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR));
    }

    // Here you can load your own JSON, it will be tied to ChangeSetting()
    void LoadSynthConfig() {
        if (InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR))) {
            SynthSetVal(bool, FadeOutKilling);
            SynthSetVal(double, RenderWindow);
            SynthSetVal(int32_t, ThreadsCount);
            SynthSetVal(uint16_t, LayerCount);
            SynthSetVal(uint8_t, Interpolation);

            if (!RANGE(ThreadsCount, -1,
                       (int32_t)std::thread::hardware_concurrency()))
                ThreadsCount = -1;

            if (!RANGE(LayerCount, 1, UINT16_MAX))
                LayerCount = 4;

            return;
        }

        if (IsConfigOpen() && !IsSynthConfigValid()) {
            RewriteSynthConfig();
        }
    }
};

class XSynth : public SynthModule {
  private:
    Lib *XLib = nullptr;

    std::vector<XSynth_Soundfont> SoundFonts;
    XSynth_RealtimeSynth realtimeSynth;
    XSynth_RealtimeConfig realtimeConf;
    XSynth_RealtimeStats realtimeStats;
    std::jthread _XSyThread;

    LibImport xLibImp[14] = {ImpFunc(XSynth_GetVersion),
                             ImpFunc(XSynth_GenDefault_RealtimeConfig),
                             ImpFunc(XSynth_GenDefault_SoundfontOptions),
                             ImpFunc(XSynth_Realtime_Drop),
                             ImpFunc(XSynth_Realtime_GetStats),
                             ImpFunc(XSynth_Realtime_GetStreamParams),
                             ImpFunc(XSynth_Realtime_Create),
                             ImpFunc(XSynth_Realtime_Reset),
                             ImpFunc(XSynth_Realtime_SendEventU32),
                             ImpFunc(XSynth_Realtime_SendConfigEventAll),
                             ImpFunc(XSynth_Realtime_SetSoundfonts),
                             ImpFunc(XSynth_Realtime_ClearSoundfonts),
                             ImpFunc(XSynth_Soundfont_LoadNew),
                             ImpFunc(XSynth_Soundfont_Remove)};
    size_t xLibImpLen = sizeof(xLibImp) / sizeof(xLibImp[0]);

    XSynthSettings *_XSyConfig = nullptr;
    SoundFontSystem _sfSystem;
    bool Running = false;

    void XSynthThread();
    void UnloadSoundfonts();

  public:
    XSynth(ErrorSystem::Logger *PErr) : SynthModule(PErr) {}
    bool LoadSynthModule() override;
    bool UnloadSynthModule() override;
    bool StartSynthModule() override;
    bool StopSynthModule() override;
    bool SettingsManager(uint32_t setting, bool get, void *var,
                         size_t size) override {
        return false;
    }
    uint32_t GetSampleRate() override { return 48000; }
    bool IsSynthInitialized() override;
    uint32_t SynthID() override { return 0x9AF3812A; }
    void LoadSoundFonts() override;

    // Event handling system
    void PlayShortEvent(uint32_t ev) override;
    void UPlayShortEvent(uint32_t ev) override;

    // Not supported in XSynth
    SynthResult TalkToSynthDirectly(uint32_t evt, uint32_t chan,
                                    uint32_t param) override {
        return Ok;
    }
};
} // namespace OmniMIDI

#endif

#endif