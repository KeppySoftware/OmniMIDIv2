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

#include "BASSSettings.hpp"

void OmniMIDI::BASSSettings::RewriteSynthConfig() {
    nlohmann::json DefConfig = {
        ConfGetVal(FloatRendering),    ConfGetVal(MonoRendering),
        ConfGetVal(DisableEffects),    ConfGetVal(Threading),
        ConfGetVal(KeyboardDivisions), ConfGetVal(FollowOverlaps),
        ConfGetVal(AudioEngine),       ConfGetVal(SampleRate),
        ConfGetVal(GlobalEvBufSize),   ConfGetVal(AudioLimiter),
        ConfGetVal(RenderTimeLimit),   ConfGetVal(VoiceLimit),
        ConfGetVal(AudioBuf),          ConfGetVal(ThreadCount),
        ConfGetVal(MaxInstanceNPS),    ConfGetVal(InstanceEvBufSize),

#if !defined(_WIN32)
        ConfGetVal(BufPeriod),
#else
      ConfGetVal(StreamDirectFeed), ConfGetVal(ASIODevice),       ConfGetVal(ASIOLCh),
      ConfGetVal(ASIORCh),          ConfGetVal(ASIOChunksDivision),
      ConfGetVal(WASAPIBuf),
#endif
    };

    if (AppendToConfig(DefConfig))
        WriteConfig();

    CloseConfig();
    InitConfig(false, BASSSYNTH_STR, sizeof(BASSSYNTH_STR));
}

void OmniMIDI::BASSSettings::LoadSynthConfig() {
    if (InitConfig(false, BASSSYNTH_STR, sizeof(BASSSYNTH_STR))) {
        SynthSetVal(bool, AudioLimiter);
        SynthSetVal(BASSMultithreadingType, Threading);
        SynthSetVal(char, KeyboardDivisions);
        SynthSetVal(bool, FloatRendering);
        SynthSetVal(bool, MonoRendering);
        SynthSetVal(bool, FollowOverlaps);
        SynthSetVal(bool, DisableEffects);
        SynthSetVal(int32_t, AudioEngine);
        SynthSetVal(uint32_t, SampleRate);
        SynthSetVal(uint32_t, RenderTimeLimit);
        SynthSetVal(uint32_t, VoiceLimit);
        SynthSetVal(float, AudioBuf);
        SynthSetVal(size_t, GlobalEvBufSize);
        SynthSetVal(uint32_t, ThreadCount);
        SynthSetVal(uint32_t, MaxInstanceNPS);
        SynthSetVal(uint64_t, InstanceEvBufSize);

#if !defined(_WIN32)
        SynthSetVal(uint32_t, BufPeriod);
#else
        SynthSetVal(bool, StreamDirectFeed);
        SynthSetVal(std::string, ASIODevice);
        SynthSetVal(std::string, ASIOLCh);
        SynthSetVal(std::string, ASIORCh);
        SynthSetVal(uint32_t, ASIOChunksDivision);
        SynthSetVal(float, WASAPIBuf);
#endif

        if (SampleRate == 0 || SampleRate > 384000)
            SampleRate = 48000;

        if (VoiceLimit < 1 || VoiceLimit > 100000)
            VoiceLimit = 1024;

        if (AudioEngine < Internal || AudioEngine > BASSENGINE_COUNT)
            AudioEngine = DEFAULT_ENGINE;

        if (KeyboardDivisions > 128)
            KeyboardDivisions = 128;

#if !defined(_WIN32)
        if (BufPeriod < 0 || BufPeriod > 4096)
            BufPeriod = 480;
#endif

        return;
    }

    if (IsConfigOpen() && !IsSynthConfigValid()) {
        RewriteSynthConfig();
    }
}

#endif