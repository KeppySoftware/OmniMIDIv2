#ifndef BASS_SETTINGS_H
#define BASS_SETTINGS_H

#include "../SynthModule.hpp"

#if defined(_WIN32)
#define DEFAULT_ENGINE WASAPI
#else
#define DEFAULT_ENGINE Internal
#endif

#define BASSSYNTH_STR "BASSSynth"

namespace OmniMIDI {
enum BASSEngine {
  Invalid = -1,
  Internal = 0,
  WASAPI = 1,
  ASIO = 2,
  BASSENGINE_COUNT = ASIO
};

class BASSSettings : public SettingsModule {
public:
  uint64_t EvBufSize = 32768;
  uint32_t RenderTimeLimit = 95;
  int32_t AudioEngine = (int)DEFAULT_ENGINE;

  bool FollowOverlaps = false;
  bool LoudMax = false;
  bool AsyncMode = true;
  bool FloatRendering = true;
  bool MonoRendering = false;
  bool OneThreadMode = false;
  bool StreamDirectFeed = false;
  bool DisableEffects = false;

  // EXP
  uint32_t ThreadCount = 4;

  const uint8_t ChannelDiv = 16;
  uint8_t ExpMTKeyboardDiv = 4;
  uint8_t KeyboardChunk = 128 / ExpMTKeyboardDiv;
  uint64_t MaxInstanceNPS = 100000;

  bool ExperimentalMultiThreaded = false;
  uint32_t ExperimentalAudioMultiplier = ChannelDiv * ExpMTKeyboardDiv;

  uint32_t AudioBuf = 10;

#if !defined(_WIN32)
  uint32_t BufPeriod = 480;
#else
  // WASAPI
  float WASAPIBuf = 32.0f;

  // ASIO
  uint32_t ASIOChunksDivision = 4;
  std::string ASIODevice = "None";
  std::string ASIOLCh = "0";
  std::string ASIORCh = "0";
#endif

  BASSSettings(ErrorSystem::Logger *PErr) : SettingsModule(PErr) {}
  void RewriteSynthConfig() override;
  void LoadSynthConfig() override;
};
}

#endif