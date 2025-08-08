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

enum BASSMultithreadingType {
  SingleThread = 0,
  Standard = 1,
  Multithreaded = 2
};

class BASSSettings : public SettingsModule {
public:
  uint64_t GlobalEvBufSize = 32768;
  uint32_t RenderTimeLimit = 95;

  bool FollowOverlaps = false;
  bool AudioLimiter = false;
  bool FloatRendering = true;
  bool MonoRendering = false;
  bool DisableEffects = false;

  BASSMultithreadingType Threading = Standard;
  uint8_t KeyboardDivisions = 4;
  uint32_t ThreadCount = 4;
  uint64_t MaxInstanceNPS = 10000;
  uint64_t InstanceEvBufSize = 2048;

  int32_t AudioEngine = (int)DEFAULT_ENGINE;
  float AudioBuf = 10.0f;

#if !defined(_WIN32)
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

  BASSSettings(ErrorSystem::Logger *PErr) : SettingsModule(PErr) {}
  void RewriteSynthConfig() override;
  void LoadSynthConfig() override;
};
} // namespace OmniMIDI

#endif