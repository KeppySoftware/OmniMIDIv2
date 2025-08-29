#include "bassconfig.h"

BASSConfig::BASSConfig(OMConfig *omconfig) : SynthConfig() {
    m_cfg = omconfig->getSynthJson(name());
    load();
}

void BASSConfig::load() {
    if (m_cfg.is_null()) return;

    try {
        GlobalEvBufSize = m_cfg.value("GlobalEvBufSize", GlobalEvBufSize);
        RenderTimeLimit = m_cfg.value("RenderTimeLimit", RenderTimeLimit);
        SampleRate = m_cfg.value("SampleRate", SampleRate);
        VoiceLimit = m_cfg.value("VoiceLimit", VoiceLimit);
        FollowOverlaps = m_cfg.value("FollowOverlaps", FollowOverlaps);
        AudioLimiter = m_cfg.value("AudioLimiter", AudioLimiter);
        FloatRendering = m_cfg.value("FloatRendering", FloatRendering);
        MonoRendering = m_cfg.value("MonoRendering", MonoRendering);
        DisableEffects = m_cfg.value("DisableEffects", DisableEffects);
        Threading = m_cfg.value("Threading", Threading);
        KeyboardDivisions = m_cfg.value("KeyboardDivisions", KeyboardDivisions);
        ThreadCount = m_cfg.value("ThreadCount", ThreadCount);
        MaxInstanceNPS = m_cfg.value("MaxInstanceNPS", MaxInstanceNPS);
        InstanceEvBufSize = m_cfg.value("InstanceEvBufSize", InstanceEvBufSize);
        AudioEngine = m_cfg.value("AudioEngine", AudioEngine);
        AudioBuf = m_cfg.value("AudioBuf", AudioBuf);
#if defined(__linux__)
        BufPeriod = m_cfg.value("BufPeriod", BufPeriod);
#elif defined(_WIN32)
        StreamDirectFeed = m_cfg.value("StreamDirectFeed", StreamDirectFeed);
        WASAPIBuf = m_cfg.value("WASAPIBuf", WASAPIBuf);
        ASIOChunksDivision = m_cfg.value("ASIOChunksDivision", ASIOChunksDivision);
        ASIODevice = m_cfg.value("ASIODevice", ASIODevice);
        ASIOLCh = m_cfg.value("ASIOLCh", ASIOLCh);
        ASIORCh = m_cfg.value("ASIORCh", ASIORCh);
#endif
    } catch (const std::exception &e) {
        std::string s = "Error loading BASS settings:";
        throw std::runtime_error(s + e.what());
    }
}

void BASSConfig::store() {
    if (m_cfg.is_null()) {
        m_cfg = json::parse("{}");
    }


    m_cfg["SampleRate"] = SampleRate;
    m_cfg["VoiceLimit"] = VoiceLimit;
    m_cfg["GlobalEvBufSize"] = GlobalEvBufSize;
    m_cfg["RenderTimeLimit"] = RenderTimeLimit;
    m_cfg["FollowOverlaps"] = FollowOverlaps;
    m_cfg["AudioLimiter"] = AudioLimiter;
    m_cfg["FloatRendering"] = FloatRendering;
    m_cfg["MonoRendering"] = MonoRendering;
    m_cfg["DisableEffects"] = DisableEffects;
    m_cfg["Threading"] = Threading;
    m_cfg["KeyboardDivisions"] = KeyboardDivisions;
    m_cfg["ThreadCount"] = ThreadCount;
    m_cfg["MaxInstanceNPS"] = MaxInstanceNPS;
    m_cfg["InstanceEvBufSize"] = InstanceEvBufSize;
    m_cfg["AudioEngine"] = AudioEngine;
    m_cfg["AudioBuf"] = AudioBuf;
#if defined(__linux__)
    m_cfg["BufPeriod"] = BufPeriod;
#elif defined(_WIN32)
    m_cfg["StreamDirectFeed"] = StreamDirectFeed;
    m_cfg["WASAPIBuf"] = WASAPIBuf;
    m_cfg["ASIOChunksDivision"] = ASIOChunksDivision;
    m_cfg["ASIODevice"] = ASIODevice;
    m_cfg["ASIOLCh"] = ASIOLCh;
    m_cfg["ASIORCh"] = ASIORCh;
#endif
}

json BASSConfig::getJson() {
    store();
    return m_cfg;
}
