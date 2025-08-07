#include "BASSSettings.hpp"

void OmniMIDI::BASSSettings::RewriteSynthConfig() {
  nlohmann::json DefConfig = {
      ConfGetVal(AsyncMode),        ConfGetVal(StreamDirectFeed),
      ConfGetVal(FloatRendering),   ConfGetVal(MonoRendering),
      ConfGetVal(DisableEffects),

      ConfGetVal(OneThreadMode),    ConfGetVal(ExperimentalMultiThreaded),
      ConfGetVal(ExpMTKeyboardDiv), ConfGetVal(FollowOverlaps),
      ConfGetVal(AudioEngine),      ConfGetVal(SampleRate),
      ConfGetVal(EvBufSize),        ConfGetVal(LoudMax),
      ConfGetVal(RenderTimeLimit),  ConfGetVal(VoiceLimit),
      ConfGetVal(AudioBuf),         ConfGetVal(ThreadCount),
      ConfGetVal(MaxInstanceNPS),

#if !defined(_WIN32)
      ConfGetVal(BufPeriod),
#else
      ConfGetVal(ASIODevice),       ConfGetVal(ASIOLCh),
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
    SynthSetVal(bool, AsyncMode);
    SynthSetVal(bool, LoudMax);
    SynthSetVal(bool, OneThreadMode);
    SynthSetVal(bool, ExperimentalMultiThreaded);
    SynthSetVal(char, ExpMTKeyboardDiv);
    SynthSetVal(bool, StreamDirectFeed);
    SynthSetVal(bool, FloatRendering);
    SynthSetVal(bool, MonoRendering);
    SynthSetVal(bool, FollowOverlaps);
    SynthSetVal(bool, DisableEffects);
    SynthSetVal(int32_t, AudioEngine);
    SynthSetVal(uint32_t, SampleRate);
    SynthSetVal(uint32_t, RenderTimeLimit);
    SynthSetVal(uint32_t, VoiceLimit);
    SynthSetVal(uint32_t, AudioBuf);
    SynthSetVal(size_t, EvBufSize);
    SynthSetVal(uint32_t, ThreadCount);
    SynthSetVal(uint32_t, MaxInstanceNPS);

#if !defined(_WIN32)
    SynthSetVal(uint32_t, BufPeriod);
#else
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

    if (ExpMTKeyboardDiv > 128)
      ExpMTKeyboardDiv = 128;

    // // Round to nearest power of 2, to avoid issues with pitch bends
    // if ((bool)ExpMTKeyboardDiv && !(ExpMTKeyboardDiv & (ExpMTKeyboardDiv -
    // 1))) { 	ExpMTKeyboardDiv = (uint8_t)pow(2,
    // ceil(log(ExpMTKeyboardDiv)/log(2)));
    // }

#if !defined(_WIN32)
    if (BufPeriod < 0 || BufPeriod > 4096)
      BufPeriod = 480;
#endif

    KeyboardChunk = 128 / ExpMTKeyboardDiv;
    ExperimentalAudioMultiplier = ChannelDiv * ExpMTKeyboardDiv;

    return;
  }

  if (IsConfigOpen() && !IsSynthConfigValid()) {
    RewriteSynthConfig();
  }
}