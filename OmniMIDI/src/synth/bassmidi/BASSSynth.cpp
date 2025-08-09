#include "BASSSettings.hpp"
#include "bass/bass.h"
#include "bass/bassmidi.h"
#include <cstdint>
#ifdef _NONFREE

#include "../../ErrSys.hpp"
#include "../../KDMAPI.hpp"
#include "BASSSynth.hpp"
#include <exception>

OmniMIDI::BASSSynth::BASSSynth(ErrorSystem::Logger *PErr) : SynthModule(PErr) {
  _sfSystem = new SoundFontSystem(PErr);
}

OmniMIDI::BASSSynth::~BASSSynth() { delete _sfSystem; }

bool OmniMIDI::BASSSynth::LoadFuncs() {
  auto ptr = (LibImport *)LibImports;

  if (!BAudLib)
    BAudLib = new Lib("bass", nullptr, ErrLog, &ptr, LibImportsSize);

  if (!BMidLib)
    BMidLib = new Lib("bassmidi", nullptr, ErrLog, &ptr, LibImportsSize);

  // Plugins
  if (!BFlaLib)
    BFlaLib = new Lib("bassflac", nullptr, ErrLog);

  // Load required libs
  if (!BAudLib->LoadLib())
    return false;

  if (!BMidLib->LoadLib())
    return false;

  if (!BAudLib->IsSupported(BASS_GetVersion(), TGT_BASS) ||
      !BMidLib->IsSupported(BASS_MIDI_GetVersion(), TGT_BASSMIDI))
    return false;

#if defined(_WIN32)
  if (!BWasLib)
    BWasLib = new Lib("basswasapi", nullptr, ErrLog, &ptr, LibImportsSize);

  if (!BAsiLib)
    BAsiLib = new Lib("bassasio", nullptr, ErrLog, &ptr, LibImportsSize);

  switch (_bassConfig->AudioEngine) {
  case WASAPI:
    if (!BWasLib->LoadLib()) {
      Error("Failed to load BASSWASAPI, defaulting to internal output.", true);
      _bassConfig->AudioEngine = Internal;
    }
    break;

  case ASIO:
    if (!BAsiLib->LoadLib()) {
      Error("Failed to load BASSASIO, defaulting to internal output.", true);
      _bassConfig->AudioEngine = Internal;
    }
    break;
  default:
    break;
  }
#endif

  char *tmpUtils = new char[MAX_PATH_LONG]{0};
  if (BFlaLib->GetLibPath(tmpUtils)) {
#if defined(_WIN32)
    wchar_t *szPath = Utils.GetUTF16(tmpUtils);
    if (szPath != nullptr) {
      BFlaLibHandle = BASS_PluginLoad(szPath, BASS_UNICODE);
      delete[] szPath;
    }
#else
    BFlaLibHandle = BASS_PluginLoad(tmpUtils, 0);
#endif
  }
  delete[] tmpUtils;

  if (BFlaLibHandle) {
    Message("BASSFLAC loaded. BFlaLib --> 0x%08x", BFlaLibHandle);
  } else {
    Message(
        "No BASSFLAC, this could affect playback with FLAC based soundbanks.",
        BFlaLibHandle);
  }

  return true;
}

bool OmniMIDI::BASSSynth::ClearFuncs() {
  if (BFlaLibHandle) {
    BASS_PluginFree(BFlaLibHandle);
    Message("BASSFLAC freed.");
  }

#if defined(WIN32)
  if (!BWasLib->UnloadLib())
    return false;

  if (!BAsiLib->UnloadLib())
    return false;
#endif

  if (!BFlaLib->UnloadLib())
    return false;

  if (!BMidLib->UnloadLib())
    return false;

  if (!BAudLib->UnloadLib())
    return false;

  return true;
}

void OmniMIDI::BASSSynth::EventsThread() {
  // Spin while waiting for the stream to go online
  while (!isActive)
    Utils.MicroSleep(SLEEPVAL(1));

  Message("EvtThread spinned up.");

  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard:
    while (IsSynthInitialized()) {
      if (ShortEvents->NewEventsAvailable()) {
        BASS_MIDI_StreamEvents(
            bassmidi, BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS,
            ShortEvents->ReadPtr(), sizeof(uint32_t));
      } else {
        Utils.MicroSleep(SLEEPVAL(1));
      }

      if (_bassConfig->Threading == SingleThread) {
        float voices;
        BASS_ChannelGetAttribute(bassmidi, BASS_ATTRIB_CPU, &RenderingTime);
        BASS_ChannelGetAttribute(bassmidi, BASS_ATTRIB_MIDI_VOICES_ACTIVE,
                                 &voices);
        ActiveVoices = (uint64_t)voices;
      }
    }
    break;

  case Multithreaded:
    while (IsSynthInitialized()) {
      if (ShortEvents->NewEventsAvailable()) {
        thread_mgr->SendEvent(ShortEvents->Read());
      } else {
        Utils.MicroSleep(SLEEPVAL(1));
      }
    }
    break;
  }
}

void OmniMIDI::BASSSynth::StatsThread() {
  // Spin while waiting for the stream to go online
  while (!isActive)
    Utils.MicroSleep(SLEEPVAL(1));

  Message("StatsThread spinned up.");

  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard: {
    while (IsSynthInitialized()) {
      float voices;

      BASS_ChannelGetAttribute(bassmidi, BASS_ATTRIB_CPU, &RenderingTime);
      BASS_ChannelGetAttribute(bassmidi, BASS_ATTRIB_MIDI_VOICES_ACTIVE,
                               &voices);

      ActiveVoices = (uint64_t)voices;

      Utils.MicroSleep(SLEEPVAL(100000));
    }
    break;
  }

  case Multithreaded:
    while (IsSynthInitialized()) {
      RenderingTime = thread_mgr->GetRenderingTime();
      ActiveVoices = thread_mgr->GetActiveVoices();

      Utils.MicroSleep(SLEEPVAL(100000));
    }
  }
}

bool OmniMIDI::BASSSynth::LoadSynthModule() {
  _bassConfig = LoadSynthConfig<BASSSettings>();

  if (_bassConfig == nullptr)
    return false;

  if (!LoadFuncs()) {
    Error("Something went wrong while importing the libraries' functions!!!",
          true);
    return false;
  } else {
    Message("Libraries loaded.");
  }

  if (!AllocateShortEvBuf(_bassConfig->GlobalEvBufSize)) {
    Error("AllocateShortEvBuf failed.", true);
    return false;
  } else {
    Message("Short events buffer allocated for %llu events.",
            _bassConfig->GlobalEvBufSize);
  }

  return true;
}

bool OmniMIDI::BASSSynth::UnloadSynthModule() {
  if (ClearFuncs()) {
    SoundFonts.clear();

    FreeShortEvBuf();
    FreeSynthConfig(_bassConfig);

    return true;
  }

  Error("Something went wrong here!!!", true);
  return false;
}

void OmniMIDI::BASSSynth::LoadSoundFonts() {
  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard:
    BASS_MIDI_StreamSetFonts(bassmidi, NULL, 0);
    break;

  case Multithreaded: {
    thread_mgr->ClearSoundFonts();
    break;
  }
  }

  if (SoundFonts.size() > 0) {
    for (size_t i = 0; i < SoundFonts.size(); i++) {
      if (SoundFonts[i].font)
        BASS_MIDI_FontFree(SoundFonts[i].font);
    }
  }
  SoundFonts.clear();

  _sfSystem->ClearList();
  _sfVec = _sfSystem->LoadList();

  if (_sfVec == nullptr)
    return;

  for (auto sfItem : *_sfVec) {
    if (!sfItem.enabled)
      continue;

    const char *sfPath = sfItem.path.c_str();
    uint32_t bmfiflags = 0;
    BASS_MIDI_FONTEX sf = BASS_MIDI_FONTEX();

    sf.spreset = sfItem.spreset;
    sf.sbank = sfItem.sbank;
    sf.dpreset = sfItem.dpreset;
    sf.dbank = sfItem.dbank;
    sf.dbanklsb = sfItem.dbanklsb;

    bmfiflags |= sfItem.xgdrums ? BASS_MIDI_FONT_XGDRUMS : 0;
    bmfiflags |= sfItem.linattmod ? BASS_MIDI_FONT_LINATTMOD : 0;
    bmfiflags |= sfItem.lindecvol ? BASS_MIDI_FONT_LINDECVOL : 0;
    bmfiflags |= sfItem.nofx ? BASS_MIDI_FONT_NOFX : 0;
    bmfiflags |= sfItem.minfx ? BASS_MIDI_FONT_MINFX : 0;
    bmfiflags |=
        sfItem.nolimits ? BASS_MIDI_FONT_SBLIMITS : BASS_MIDI_FONT_NOSBLIMITS;
    bmfiflags |= sfItem.norampin ? BASS_MIDI_FONT_NORAMPIN : 0;

#if defined(_WIN32)
    wchar_t *szPath = Utils.GetUTF16((char *)sfPath);
    if (szPath != nullptr) {
      sf.font = BASS_MIDI_FontInit(szPath, bmfiflags | BASS_UNICODE);
      delete[] szPath;
    }
#else
    sf.font = BASS_MIDI_FontInit(sfPath, bmfiflags);
#endif

    if (sf.font != 0) {
      if (BASS_MIDI_FontLoadEx(sf.font, -1, -1, 0, 0)) {
        SoundFonts.push_back(sf);
        Message("\"%s\" loaded!", sfPath);
      } else {
        Error("Error 0x%x occurred while loading \"%s\"!", false,
              BASS_ErrorGetCode(), sfPath);
      }
    } else {
      Error("Error 0x%x occurred while initializing \"%s\"!", false,
            BASS_ErrorGetCode(), sfPath);
    }
  }

  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard:
    BASS_MIDI_StreamSetFonts(bassmidi, SoundFonts.data(),
                             (uint32_t)SoundFonts.size() | BASS_MIDI_FONT_EX);
    break;

  case Multithreaded: {
    int err = thread_mgr->SetSoundFonts(SoundFonts);
    if (err != 0) {
      Error("Error setting soundfonts | ERR:%d", false, err);
    }
    break;
  }
  }
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
  if (isActive)
    return true;

  Message("Starting BASSMIDI.");

  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard: {
    uint32_t deviceFlags =
        (_bassConfig->MonoRendering ? BASS_DEVICE_MONO : BASS_DEVICE_STEREO);

    uint32_t streamFlags =
        (_bassConfig->MonoRendering ? BASS_SAMPLE_MONO : 0) |
        (_bassConfig->Threading == Standard ? BASS_MIDI_ASYNC : 0) |
        (_bassConfig->FloatRendering ? BASS_SAMPLE_FLOAT : 0) |
        (_bassConfig->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0) |
        (_bassConfig->DisableEffects ? BASS_MIDI_NOFX : 0);

    BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);
    BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
    BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

#if !defined(_WIN32)
    // Only Linux and macOS can do this
    BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, _bassConfig->BufPeriod * -1);
#else
    BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, _bassConfig->AudioBuf);
#endif
    BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, _bassConfig->AudioBuf);

    if (!BASS_Init(-1, _bassConfig->SampleRate, deviceFlags, NULL, NULL)) {
      Error("Error initializing BASS : %x", true, BASS_ErrorGetCode());
      return false;
    }

    bassmidi = BASS_MIDI_StreamCreate(16, streamFlags, _bassConfig->SampleRate);
    if (bassmidi == 0) {
      Error("Failed to create BASSMIDI stream : %x", true, BASS_ErrorGetCode());
      BASS_Free();
      return false;
    }

    BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_BUFFER, 0);
    BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_MIDI_VOICES,
                             (float)_bassConfig->VoiceLimit);

    BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_MIDI_SRC, 0.0);

    BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_MIDI_KILL, 1.0);

    BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_MIDI_CPU,
                             (float)_bassConfig->RenderTimeLimit);

    if (_bassConfig->Threading == Standard) {
      if (!BASS_ChannelSetAttribute(bassmidi, BASS_ATTRIB_MIDI_QUEUE_ASYNC,
                                    (float)_bassConfig->GlobalEvBufSize *
                                        sizeof(uint32_t))) {
        BASS_StreamFree(bassmidi);
        BASS_Free();
        Error("Failed to set async buffer size!", true);
      }
    }

    if (_bassConfig->FloatRendering && _bassConfig->AudioLimiter) {
      BASS_BFX_COMPRESSOR2 compressor;

      audioLimiter = BASS_ChannelSetFX(bassmidi, BASS_FX_BFX_COMPRESSOR2, 0);

      BASS_FXGetParameters(audioLimiter, &compressor);
      BASS_FXSetParameters(audioLimiter, &compressor);
      Message("BASS audio limiter enabled.");
    }

    if (!BASS_ChannelPlay(bassmidi, false)) {
      Error("BASS_ChannelPlay failed with error 0x%x.", true,
            BASS_ErrorGetCode());
      return false;
    }
    break;
  }

  case Multithreaded: {
    try {
      thread_mgr = new BASSThreadManager(ErrLog, _bassConfig);
    } catch (const std::exception &e) {
      Error("FAILED", 0);
      return false;
    }
    Message("BASSMIDI thread manager initialized.");
    break;
  }
  };

  LoadSoundFonts();
  _sfSystem->RegisterCallback(this);

  _EvtThread = std::jthread(&BASSSynth::EventsThread, this);
  if (!_EvtThread.joinable()) {
    Error("_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
    return false;
  }
  Message("Event thread started.");

  if (_bassConfig->Threading != SingleThread) {
    _StatsThread = std::jthread(&BASSSynth::StatsThread, this);
    if (!_StatsThread.joinable()) {
      Error("_StatsThread failed. (ID: %x)", true, _StatsThread.get_id());
      return false;
    }
    Message("Stats thread started.");
  }

  ShortEvents->ResetHeads();

  StartDebugOutput();

  isActive = true;

  return true;
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
  isActive = false;

  _sfSystem->ClearList();
  _sfSystem->RegisterCallback();

  if (_EvtThread.joinable()) {
    _EvtThread.join();
    Message("_EvtThread freed.");
  }

  if (_bassConfig->Threading != SingleThread && _StatsThread.joinable()) {
    _StatsThread.join();
    Message("_StatsThread freed.");
  }

  switch (_bassConfig->Threading) {
  case SingleThread:
  case Standard:
    BASS_StreamFree(bassmidi);
    BASS_Free();
    break;

  case Multithreaded:
    delete thread_mgr;
    Message("Deleted BASSMIDI thread manager.");
    break;
  }

  return true;
}

bool OmniMIDI::BASSSynth::SettingsManager(uint32_t setting, bool get, void *var,
                                          size_t size) {
  switch (setting) {

  case KDMAPI_MANAGE: {
    if (_bassConfig || IsSynthInitialized()) {
      Error("You can not control the settings while the driver is open and "
            "running! Call TerminateKDMAPIStream() first then try again.",
            true);
      return false;
    }

    Message("KDMAPI REQUEST: The MIDI app wants to manage the settings.");
    _bassConfig = new BASSSettings(ErrLog);

    break;
  }

  case KDMAPI_LEAVE: {
    if (_bassConfig) {
      if (IsSynthInitialized()) {
        Error("You can not control the settings while the driver is open and "
              "running! Call TerminateKDMAPIStream() first then try again.",
              true);
        return false;
      }

      Message("KDMAPI REQUEST: The MIDI app does not want to manage the "
              "settings anymore.");
      delete _bassConfig;
      _bassConfig = nullptr;

      _bassConfig = (BASSSettings *)_synthConfig;
    }
    break;
  }

  case WINMMWRP_ON:
  case WINMMWRP_OFF:
    // Old WinMMWRP code, ignore
    break;

    SettingsManagerCase(KDMAPI_AUDIOFREQ, get, uint32_t,
                        _bassConfig->SampleRate, var, size);
    SettingsManagerCase(KDMAPI_CURRENTENGINE, get, int32_t,
                        _bassConfig->AudioEngine, var, size);
    SettingsManagerCase(KDMAPI_MAXVOICES, get, uint32_t,
                        _bassConfig->VoiceLimit, var, size);
    SettingsManagerCase(KDMAPI_MAXRENDERINGTIME, get, uint32_t,
                        _bassConfig->RenderTimeLimit, var, size);

  default:
    Message("Unknown setting passed to SettingsManager. (VAL: 0x%x)", setting);
    return false;
  }

  return true;
}

#endif