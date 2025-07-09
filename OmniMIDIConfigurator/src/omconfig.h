#ifndef OMCONFIG_H
#define OMCONFIG_H

#include "nlohmann/json.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

class Synthesizers {
public:
    enum engineID {
        External = -1,
        BASSMIDI,
        FluidSynth,
        XSynth,
        ShakraPipe
    };
};

class SynthConfig
{
public:
    virtual void load() = 0;
    virtual void store() = 0;
    virtual json getJson() = 0;
    virtual std::string name() = 0;
};

class OMConfig
{
public:
    OMConfig();
    void loadFromDisk();
    void storeToDisk();
    void resetConfig();
    void resetSynthDefaults();
    json getSynthJson(std::string synthName);
    void setSynthConfig(SynthConfig *cfg);

    std::string CustomRenderer = "";
    bool DebugMode = false;
    bool KDMAPIEnabled = true;
    char Renderer = Synthesizers::BASSMIDI;

private:
    fs::path m_cfgPath;
    json m_cfg;
    SynthConfig *m_synthCfg = nullptr;
};

#endif // OMCONFIG_H
