#ifdef _NONFREE

#include "BASSSynth.hpp"
#include "../../ErrSys.hpp"
#include "../../KDMAPI.hpp"
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

  return true;
}

bool OmniMIDI::BASSSynth::ClearFuncs() {
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
  while (!thread_mgr)
    Utils.MicroSleep(SLEEPVAL(1));

  Message("EvtThread spinned up.");

  while (IsSynthInitialized()) {
    if (ShortEvents->NewEventsAvailable()) {
      auto pEvt = ShortEvents->ReadPtr();
      thread_mgr->SendEvent(*pEvt);
    } else {
      Utils.MicroSleep(SLEEPVAL(1));
    }
  }
}

void OmniMIDI::BASSSynth::StatsThread() {
  // Spin while waiting for the stream to go online
  while (!thread_mgr)
    Utils.MicroSleep(SLEEPVAL(1));

  Message("StatsThread spinned up.");

  while (IsSynthInitialized()) {
    RenderingTime = thread_mgr->GetRenderingTime();
    ActiveVoices = thread_mgr->GetActiveVoices();

    Utils.MicroSleep(SLEEPVAL(100000));
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

  if (!AllocateShortEvBuf(_bassConfig->EvBufSize)) {
    Error("AllocateShortEvBuf failed.", true);
    return false;
  } else {
    Message("Short events buffer allocated for %llu events.",
            _bassConfig->EvBufSize);
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

  int err = thread_mgr->SetSoundFonts(SoundFonts);
  if (err != 0) {
    Error("Error setting soundfonts | ERR:%d", false, err);
  }
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
  Message("Starting BASSMIDI.");
  try {
    thread_mgr = new BASSThreadManager(ErrLog, _bassConfig);
  } catch (const std::exception &e) {
    Error("FAILED", 0);
    return false;
  }
  Message("BASSMIDI thread manager initialized.");

  LoadSoundFonts();
  _sfSystem->RegisterCallback(this);

  _EvtThread = std::jthread(&BASSSynth::EventsThread, this);
  if (!_EvtThread.joinable()) {
    Error("_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
    return false;
  }
  Message("Event thread started.");

  _StatsThread = std::jthread(&BASSSynth::StatsThread, this);
  if (!_StatsThread.joinable()) {
    Error("_StatsThread failed. (ID: %x)", true, _StatsThread.get_id());
    return false;
  }
  Message("Stats thread started.");

  ShortEvents->ResetHeads();

  isActive = true;

  return true;
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
  Message("Stopping BASSSynth.");
  isActive = false;

  _sfSystem->ClearList();
  _sfSystem->RegisterCallback();

  if (_EvtThread.joinable()) {
    _EvtThread.join();
    Message("_EvtThread freed.");
  }

  if (_StatsThread.joinable()) {
    _StatsThread.join();
    Message("_StatsThread freed.");
  }

  delete thread_mgr;
  Message("Deleted BASSMIDI thread manager.");

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