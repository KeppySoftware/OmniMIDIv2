#ifndef XSYNTHCONFIG_H
#define XSYNTHCONFIG_H

#include "omconfig.h"

class XSynthConfig : public SynthConfig
{
public:
    XSynthConfig(OMConfig *omconfig);

    void load() override;
    void store() override;
    json getJson() override;
    std::string name() override {
        return "XSynth";
    }

    bool FadeOutKilling = false;
    uint8_t Interpolation = 0;
    uint64_t LayerCount = 4;
    double RenderWindow = 10.0;
    int32_t ThreadsCount = 0;

private:
    json m_cfg;
};

#endif // XSYNTHCONFIG_H
