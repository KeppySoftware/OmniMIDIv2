#include "omconfig.h"
#include <fstream>
#include <iostream>
#include "utils.h"

OMConfig::OMConfig() {
    m_cfgPath = Utils::GetConfigFolder() / "settings.json";
}

void OMConfig::loadFromDisk() {
    std::ifstream ifs(m_cfgPath);
    if (!ifs)
        throw std::runtime_error("Error opening settings file.");

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    try {
        m_cfg = json::parse(content, nullptr, false, true)["OmniMIDI"];

        Renderer = m_cfg.value("Renderer", Renderer);
        DebugMode = m_cfg.value("DebugMode", DebugMode);
        KDMAPIEnabled = m_cfg.value("KDMAPIEnabled", KDMAPIEnabled);
        CustomRenderer = m_cfg.value("CustomRenderer", CustomRenderer);
    } catch (const std::exception &e) {
        std::string s = "Error loading settings:";
        throw std::runtime_error(s + e.what());
    }
}

void OMConfig::storeToDisk() {
    m_cfg["Renderer"] = Renderer;
    m_cfg["DebugMode"] = DebugMode;
    m_cfg["KDMAPIEnabled"] = KDMAPIEnabled;
    m_cfg["CustomRenderer"] = CustomRenderer;
    if (m_synthCfg != nullptr)
        m_cfg["SynthModules"][m_synthCfg->name()] = m_synthCfg->getJson();

    json outCfg;
    outCfg["OmniMIDI"] = m_cfg;

    std::ofstream out(m_cfgPath);
    if (!out)
        throw std::runtime_error("Error creating settings file.");

    out << outCfg.dump(4);
    out.close();

    if (out.bad() || out.fail())
        throw std::runtime_error("Error writing to settings file.");
}

void OMConfig::resetConfig() {
    // ToDo
}

void OMConfig::resetSynthDefaults() {
    // ToDo
}

void OMConfig::setSynthConfig(SynthConfig *cfg) {
    m_synthCfg = cfg;
}

json OMConfig::getSynthJson(std::string synthName) {
    return m_cfg["SynthModules"][synthName];
}
