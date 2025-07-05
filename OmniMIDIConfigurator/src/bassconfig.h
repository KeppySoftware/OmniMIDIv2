#ifndef BASSCONFIG_H
#define BASSCONFIG_H

#include "omconfig.h"

#include <iostream>

class BASSConfig : public SynthConfig
{
public:
    BASSConfig(OMConfig *omconfig);

    void load() override;
    void store() override;
    json getJson() override;
    std::string name() override {
        return "BASSSynth";
    }

    uint64_t EvBufSize = 32768;
    uint32_t RenderTimeLimit = 95;
    uint32_t SampleRate = 48000;
    uint32_t VoiceLimit = 1024;
    int32_t AudioEngine = 0;
#ifndef _WIN32
    uint32_t BufPeriod = 480;
#else
    uint32_t AudioBuf = 10;

    // WASAPI
    float WASAPIBuf = 32.0f;

    // ASIO
    bool StreamDirectFeed = false;
    uint32_t ASIOChunksDivision = 4;
    std::string ASIODevice = "None";
    std::string ASIOLCh = "0";
    std::string ASIORCh = "0";
#endif

    bool FollowOverlaps = false;
    bool LoudMax = false;
    bool AsyncMode = true;
    bool FloatRendering = true;
    bool MonoRendering = false;
    bool OneThreadMode = false;
    bool DisableEffects = false;

    uint8_t ExpMTKeyboardDiv = 4;
    uint32_t ExpMTVoiceLimit = 128;
    bool ExperimentalMultiThreaded = false;

private:
    json m_cfg;
};

#endif // BASSCONFIG_H
