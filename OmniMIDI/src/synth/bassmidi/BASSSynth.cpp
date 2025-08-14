/*
 * SPDX-License-Identifier: MIT
 *
 * OmniMIDI
 *
 * Copyright (c) 2024 Keppy's Software
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this
 * program.  If not, see <https://opensource.org/license/mit/>.
 */

#ifdef _NONFREE

#include "BASSSettings.hpp"
#include "bass/bass.h"
#include "bass/bass_fx.h"
#include "bass/bassmidi.h"
#include <cstdint>

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
            Error("Failed to load BASSWASAPI, defaulting to internal output.",
                  true);
            _bassConfig->AudioEngine = Internal;
        }
        break;

    case ASIO:
        if (!BAsiLib->LoadLib()) {
            Error("Failed to load BASSASIO, defaulting to internal output.",
                  true);
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
        Message("No BASSFLAC, this could affect playback with FLAC based "
                "soundbanks.",
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

void OmniMIDI::BASSSynth::RenderingThread() {
    int32_t sleepRate = -1;
	uint32_t updRate = 1;

#ifndef _WIN32
	// Linux/macOS don't, so let's do the math ourselves
	sleepRate = (int32_t)(((double)_bassConfig->BufPeriod / (double)_bassConfig->SampleRate) * 10000000.0);
#endif

    // Spin while waiting for the stream to go online
    while (!isActive)
        Utils.MicroSleep(SLEEPVAL(1));

    Message("RenderingThread spinned up.");

    switch (_bassConfig->Threading) {
    case SingleThread:
        while (IsSynthInitialized()) {
            do standard_instance->SendEvent(ShortEvents->Read());
	        while (ShortEvents->NewEventsAvailable());

            standard_instance->FlushEvents();
            standard_instance->UpdateStream(updRate);
            Utils.MicroSleep(sleepRate);
        }
        break;

    case Standard:
        while (IsSynthInitialized()) {
            standard_instance->UpdateStream(updRate);
            Utils.MicroSleep(sleepRate);
        }
        break;

    default:
        break;
    }
}

void OmniMIDI::BASSSynth::ProcessingThread() {
    // Spin while waiting for the stream to go online
    while (!isActive)
        Utils.MicroSleep(SLEEPVAL(1));

    Message("ProcessingThread spinned up.");

    switch (_bassConfig->Threading) {
    case Standard:
        while (IsSynthInitialized()) {
            do standard_instance->SendEvent(ShortEvents->Read());
	        while (ShortEvents->NewEventsAvailable());
            
            standard_instance->FlushEvents();
            Utils.MicroSleep(SLEEPVAL(1));
        }
        break;

    case Multithreaded:
        while (IsSynthInitialized()) {
            do thread_mgr->SendEvent(ShortEvents->Read());
	        while (ShortEvents->NewEventsAvailable());

            Utils.MicroSleep(SLEEPVAL(1));
        }
        break;

    default:
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
            RenderingTime = standard_instance->GetRenderingTime();
            ActiveVoices = standard_instance->GetVoiceCount();

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
        Error(
            "Something went wrong while importing the libraries' functions!!!",
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
    case Standard: {
        standard_instance->SetSoundFonts(std::vector<BASS_MIDI_FONTEX>());
        break;
    }

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
        bmfiflags |= sfItem.nolimits ? BASS_MIDI_FONT_SBLIMITS
                                     : BASS_MIDI_FONT_NOSBLIMITS;
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

    int err = 0;
    switch (_bassConfig->Threading) {
    case SingleThread:
    case Standard:
        err = standard_instance->SetSoundFonts(SoundFonts) == false;
        break;

    case Multithreaded:
        err = thread_mgr->SetSoundFonts(SoundFonts);
        break;
    }

    if (err != 0) {
        Error("Error setting soundfonts | ERR:%d", false, err);
    }
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
    if (isActive)
        return true;

    Message("Starting BASSMIDI.");

    switch (_bassConfig->Threading) {
    case SingleThread:
    case Standard: {
         try {
            standard_instance = new BASSInstance(ErrLog, _bassConfig, 16);

            _AudThread = std::jthread(&BASSSynth::RenderingThread, this);
            if (!_AudThread.joinable()) {
                Error("_AudThread failed. (ID: %x)", true, _AudThread.get_id());
                return false;
            }
            Message("Rendering thread started.");
        } catch (const std::exception &e) {
            Error("BASSInstance intialization failed: %s", 0, e.what());
            return false;
        }
        break;
    }

    case Multithreaded: {
        try {
            thread_mgr = new BASSThreadManager(ErrLog, _bassConfig);
        } catch (const std::exception &e) {
            Error("BASSThreadManager intialization failed: %s", 0, e.what());
            return false;
        }
        break;
    }
    };

    LoadSoundFonts();
    _sfSystem->RegisterCallback(this);

    _EvtThread = std::jthread(&BASSSynth::ProcessingThread, this);
    if (!_EvtThread.joinable()) {
        Error("_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
        return false;
    }
    Message("Processing thread started.");

    _StatsThread = std::jthread(&BASSSynth::StatsThread, this);
    if (!_StatsThread.joinable()) {
        Error("_StatsThread failed. (ID: %x)", true, _StatsThread.get_id());
        return false;
    }
    Message("Stats thread started.");

    ShortEvents->ResetHeads();

    StartDebugOutput();

    isActive = true;

    return true;
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
    isActive = false;

    _sfSystem->ClearList();
    _sfSystem->RegisterCallback();

    if (_AudThread.joinable()) {
        _AudThread.join();
        Message("_AudThread freed.");
    }

    if (_EvtThread.joinable()) {
        _EvtThread.join();
        Message("_EvtThread freed.");
    }

    if (_StatsThread.joinable()) {
        _StatsThread.join();
        Message("_StatsThread freed.");
    }

    switch (_bassConfig->Threading) {
    case SingleThread:
    case Standard:
        delete standard_instance;
        Message("Deleted BASSMIDI instance.");
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
            Error(
                "You can not control the settings while the driver is open and "
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
                Error("You can not control the settings while the driver is "
                      "open and "
                      "running! Call TerminateKDMAPIStream() first then try "
                      "again.",
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
        Message("Unknown setting passed to SettingsManager. (VAL: 0x%x)",
                setting);
        return false;
    }

    return true;
}

uint32_t OmniMIDI::BASSSynth::PlayLongEvent(uint8_t *ev, uint32_t size) {
    if (!BMidLib || !BMidLib->IsOnline())
        return 0;

    // The size has to be more than 1B!
    if (size < 1)
        return 0;

    return UPlayLongEvent(ev, size);
}

uint32_t OmniMIDI::BASSSynth::UPlayLongEvent(uint8_t *ev, uint32_t size) {
    // Converting the byte array to uint array is supported since it will be
    // deconstructed again when sending to BASSMIDI. This is not supported for
    // multithreading since we have to process each event before BASSMIDI.

    if (_bassConfig->Threading == Multithreaded)
        return 0;

    uint32_t out_events = (size + 3) / 4;
    uint32_t curr_evt = 0;

    for (uint32_t i = 0; i < out_events; i++) {
        for (uint32_t k = 0; k < 4; k++) {
            size_t current_index = i * 4 + k;
            if (current_index < size) {
                curr_evt |= (uint32_t)(ev[current_index]) << (24 - i * 8);
            } else {
                // Pad with 0s if we've reached the end of the input array
                curr_evt |= 0x00 << (24 - i * 8);
            }
        }

        ShortEvents->Write(curr_evt);
    }

    return out_events;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::TalkToSynthDirectly(uint32_t evt,
                                                               uint32_t chan,
                                                               uint32_t param) {
    if (!BMidLib || !BMidLib->IsOnline())
        return LibrariesOffline;

    if (_bassConfig->Threading == Multithreaded)
        return NotSupported;

    if (standard_instance == nullptr)
        return NotInitialized;
    
    return standard_instance->SendDirectEvent(chan, evt, param) ? Ok
                                                             : InvalidParameter;
}

#endif