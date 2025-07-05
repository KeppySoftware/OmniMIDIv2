#include "xsynthconfig.h"

XSynthConfig::XSynthConfig(OMConfig *omconfig) : SynthConfig() {
    m_cfg = omconfig->getSynthJson(name());
    load();
}

void XSynthConfig::load() {
    if (m_cfg.is_null()) return;

    try {
        FadeOutKilling = m_cfg.value("FadeOutKilling", FadeOutKilling);
        Interpolation = m_cfg.value("Interpolation", Interpolation);
        LayerCount = m_cfg.value("LayerCount", LayerCount);
        RenderWindow = m_cfg.value("RenderWindow", RenderWindow);
        ThreadsCount = m_cfg.value("ThreadsCount", ThreadsCount);
    } catch (const std::exception &e) {
        std::string s = "Error loading XSynth settings:";
        throw std::runtime_error(s + e.what());
    }
}

void XSynthConfig::store() {
    if (m_cfg.is_null()) return;

    m_cfg["FadeOutKilling"] = FadeOutKilling;
    m_cfg["Interpolation"] = Interpolation;
    m_cfg["LayerCount"] = LayerCount;
    m_cfg["RenderWindow"] = RenderWindow;
    m_cfg["ThreadsCount"] = ThreadsCount;
}

json XSynthConfig::getJson() {
    store();
    return m_cfg;
}
