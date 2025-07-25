#include "fluidconfig.h"

FluidConfig::FluidConfig(OMConfig *omconfig) : SynthConfig() {
    m_cfg = omconfig->getSynthJson(name());
    load();
}

void FluidConfig::load() {
    if (m_cfg.is_null()) return;

    try {
        EvBufSize = m_cfg.value("EvBufSize", EvBufSize);
        PeriodSize = m_cfg.value("PeriodSize", PeriodSize);
        Periods = m_cfg.value("Periods", Periods);
        ThreadsCount = m_cfg.value("ThreadsCount", ThreadsCount);
        MinimumNoteLength = m_cfg.value("MinimumNoteLength", MinimumNoteLength);
        ExperimentalMultiThreaded = m_cfg.value("ExperimentalMultiThreaded", ExperimentalMultiThreaded);
        OverflowVolume = m_cfg.value("OverflowVolume", OverflowVolume);
        OverflowPercussion = m_cfg.value("OverflowPercussion", OverflowPercussion);
        OverflowReleased = m_cfg.value("OverflowReleased", OverflowReleased);
        OverflowImportant = m_cfg.value("OverflowImportant", OverflowImportant);
        SampleFormat = m_cfg.value("SampleFormat", SampleFormat);
        Driver = m_cfg.value("Driver", Driver);
        SampleRate = m_cfg.value("SampleRate", SampleRate);
        VoiceLimit = m_cfg.value("VoiceLimit", VoiceLimit);
    } catch (const std::exception &e) {
        std::string s = "Error loading FluidSynth settings:";
        throw std::runtime_error(s + e.what());
    }
}

void FluidConfig::store() {
    if (m_cfg.is_null()) {
        m_cfg = json::parse("{}");
    }

    m_cfg["EvBufSize"] = EvBufSize;
    m_cfg["PeriodSize"] = PeriodSize;
    m_cfg["Periods"] = Periods;
    m_cfg["ThreadsCount"] = ThreadsCount;
    m_cfg["MinimumNoteLength"] = MinimumNoteLength;
    m_cfg["ExperimentalMultiThreaded"] = ExperimentalMultiThreaded;
    m_cfg["OverflowVolume"] = OverflowVolume;
    m_cfg["OverflowPercussion"] = OverflowPercussion;
    m_cfg["OverflowReleased"] = OverflowReleased;
    m_cfg["OverflowImportant"] = OverflowImportant;
    m_cfg["SampleFormat"] = SampleFormat;
    m_cfg["Driver"] = Driver;
    m_cfg["SampleRate"] = SampleRate;
    m_cfg["VoiceLimit"] = VoiceLimit;
}

json FluidConfig::getJson() {
    store();
    return m_cfg;
}
