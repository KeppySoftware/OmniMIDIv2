#ifndef FLUIDCONFIG_H
#define FLUIDCONFIG_H

#include "omconfig.h"

static std::map<std::string, std::string> audioDrivers  = {
#if defined(__linux__)
    {"pipewire", "PipeWire"},
    {"alsa", "ALSA"},
    {"oss", "OSS"},
#elif defined(_WIN32)
    {"wasapi", "WASAPI"},
    {"dsound", "DirectSound"},
    {"waveout", "WaveOut"},
#elif defined(__APPLE__)
    {"coreaudio", "CoreAudio"},
#endif
    {"jack", "JACK"},
    {"pulseaudio", "PulseAudio"},
    {"portaudio", "PortAudio"},
    {"file", "Output to file"}
};

class FluidConfig : public SynthConfig
{
public:
    FluidConfig(OMConfig *omconfig);

    void load() override;
    void store() override;
    json getJson() override;
    std::string name() override {
        return "FluidSynth";
    }

    uint64_t EvBufSize = 32768;
    uint32_t PeriodSize = 64;
    uint32_t Periods = 2;
    uint32_t ThreadsCount = 1;
    uint32_t MinimumNoteLength = 10;
    bool ExperimentalMultiThreaded = false;
    double OverflowVolume = 10000.0;
    double OverflowPercussion = 10000.0;
    double OverflowReleased = -10000.0;
    double OverflowImportant = 0.0;
    std::string SampleFormat = "float";
    std::string Driver = audioDrivers.begin()->first;
    uint32_t SampleRate = 48000;
    uint32_t VoiceLimit = 1024;

private:
    json m_cfg;
};

#endif // FLUIDCONFIG_H
