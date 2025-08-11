#ifndef BASSCONFIG_H
#define BASSCONFIG_H

#include "omconfig.h"

#include <iostream>

class BASSConfig : public SynthConfig
{
public:
    enum BASSMultithreadingType {
        SingleThread = 0,
        Standard = 1,
        Multithreaded = 2
    };

    BASSConfig(OMConfig *omconfig);

    void load() override;
    void store() override;
    json getJson() override;
    std::string name() override {
        return "BASSSynth";
    }

    uint32_t SampleRate = 48000;
    uint32_t VoiceLimit = 1024;

    uint64_t GlobalEvBufSize = 65536;
    uint32_t RenderTimeLimit = 95;

    bool FollowOverlaps = false;
    bool AudioLimiter = false;
    bool FloatRendering = true;
    bool MonoRendering = false;
    bool DisableEffects = false;

    BASSMultithreadingType Threading = Standard;
    uint8_t KeyboardDivisions = 4;
    uint32_t ThreadCount = 0;
    uint64_t MaxInstanceNPS = 10000;
    uint64_t InstanceEvBufSize = 8192;

    int32_t AudioEngine = 0;
    float AudioBuf = 10.0f;
#ifndef _WIN32
    uint32_t BufPeriod = 480;
#else
    // WASAPI
    float WASAPIBuf = 32.0f;

    // ASIO
    bool StreamDirectFeed = false;
    uint32_t ASIOChunksDivision = 4;
    std::string ASIODevice = "None";
    std::string ASIOLCh = "0";
    std::string ASIORCh = "0";
#endif

private:
    json m_cfg;
};

#endif // BASSCONFIG_H
