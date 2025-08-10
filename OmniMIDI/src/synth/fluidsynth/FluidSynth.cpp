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

#include "FluidSynth.hpp"

#ifdef _OFLUIDSYNTH_H

void OmniMIDI::FluidSynth::EventsThread() {
    // Spin while waiting for the stream to go online
    while (!AudioDrivers[0])
        Utils.MicroSleep(SLEEPVAL(1));

    for (size_t i = 0; i < AudioStreamSize; i++)
        fluid_synth_system_reset(AudioStreams[i]);

    while (IsSynthInitialized()) {
        if (!ProcessEvBuf())
            Utils.MicroSleep(SLEEPVAL(1));
    }
}

bool OmniMIDI::FluidSynth::ProcessEvBuf() {
    // SysEx
    int32_t len = 0;
    int32_t handled = 0;
    uint32_t sysev = 0;

    if (!AudioDrivers[0])
        return false;

    auto evtDword = ShortEvents->Read();

    if (!evtDword)
        return false;

    uint8_t status = MIDIUtils::GetStatus(evtDword);
    uint8_t command = MIDIUtils::GetCommand(status);
    uint8_t chan = MIDIUtils::GetChannel(status);

    uint8_t param1 = MIDIUtils::GetFirstParam(evtDword);
    uint8_t param2 = MIDIUtils::GetSecondParam(evtDword);

    fluid_synth_t *targetStream = _fluidConfig->ExperimentalMultiThreaded
                                      ? AudioStreams[chan]
                                      : AudioStreams[0];

    switch (command) {
    case NoteOn:
        // param1 is the key, param2 is the velocity
        fluid_synth_noteon(targetStream, chan, param1, param2);
        break;

    case NoteOff:
        // param1 is the key, ignore param2
        fluid_synth_noteoff(targetStream, chan, param1);
        break;

    case Aftertouch:
        fluid_synth_key_pressure(targetStream, chan, param1, param2);
        break;

    case CC:
        fluid_synth_cc(targetStream, chan, param1, param2);
        break;

    case PatchChange:
        fluid_synth_program_change(targetStream, chan, param1);
        break;

    case ChannelPressure:
        fluid_synth_channel_pressure(targetStream, chan, param1);
        break;

    case PitchBend:
        fluid_synth_pitch_bend(targetStream, chan,
                               MIDIUtils::MakeFullParam(param1, param2, 7));
        break;

    default:
        switch (status) {

        // Let's go!
        case SystemMessageStart:
            sysev = evtDword;

            Message("SysEx Begin: %x", sysev);
            fluid_synth_sysex(targetStream, (const char *)&sysev, 2, 0, &len,
                              &handled, 0);

            while (MIDIUtils::GetStatus(sysev) != SystemMessageEnd) {
                sysev = ShortEvents->Peek();

                if (MIDIUtils::GetStatus(sysev) != SystemMessageEnd) {
                    sysev = ShortEvents->Read();
                    Message("SysEx Ev: %x", sysev);
                    fluid_synth_sysex(targetStream, (const char *)&sysev, 3, 0,
                                      &len, &handled, 0);
                }
            }

            Message("SysEx End", sysev);
            break;

        case SystemReset:
            for (auto i = 0; i < 16; i++) {
                fluid_synth_all_notes_off(targetStream, i);
                fluid_synth_all_sounds_off(targetStream, i);
                fluid_synth_system_reset(targetStream);
            }
            break;

        case Unknown1:
        case Unknown2:
        case Unknown3:
        case Unknown4:
            return false;

        default:
            break;
        }

        break;
    }

    return true;
}

void OmniMIDI::FluidSynth::LoadSoundFonts() {
    // Free old SFs
    for (auto i = 0;
         i < std::count(SoundFontIDs.begin(), SoundFontIDs.end(), -1); i++) {
        for (size_t a = 0; a < AudioStreamSize; a++) {
            fluid_synth_sfunload(AudioStreams[a], SoundFontIDs[i], 0);
            fluid_synth_all_notes_off(AudioStreams[a], i);
            fluid_synth_all_sounds_off(AudioStreams[a], i);
            fluid_synth_system_reset(AudioStreams[a]);
        }
    }

    SoundFontIDs.clear();

    if ((_sfVec = _sfSystem.LoadList()) != nullptr) {
        std::vector<SoundFont> &dSFv = *_sfVec;

        if (dSFv.size() > 0) {
            for (size_t i = 0; i < dSFv.size(); i++) {
                if (!dSFv[i].enabled)
                    continue;

                for (size_t a = 0; a < AudioStreamSize; a++) {
                    SoundFontIDs.push_back(fluid_synth_sfload(
                        AudioStreams[a], (const char *)dSFv[i].path.c_str(),
                        1));
                }
            }
        }
    }
}

bool OmniMIDI::FluidSynth::LoadSynthModule() {
    if (!_fluidConfig) {
        auto ptr = (LibImport *)fLibImp;
        _fluidConfig = LoadSynthConfig<FluidSettings>();

        if (_fluidConfig == nullptr)
            return false;

        if (!FluiLib)
            FluiLib = new Lib(FLUIDLIB, FLUIDSFX, ErrLog, &ptr, fLibImpLen);

        if (!FluiLib->LoadLib())
            return false;

        if (!AllocateShortEvBuf(_fluidConfig->EvBufSize)) {
            Error("AllocateShortEvBuf failed.", true);
            return false;
        }

        _SinEvtThread = std::jthread(&FluidSynth::EventsThread, this);
    }

    return true;
}

bool OmniMIDI::FluidSynth::UnloadSynthModule() {
    if (!FluiLib || !FluiLib->IsOnline())
        return true;

    if (!AudioStreams[0] && !AudioDrivers[0]) {
        FreeShortEvBuf();
        FreeSynthConfig(_fluidConfig);

        delete_fluid_settings(fSet);
        fSet = nullptr;

        if (!FluiLib->UnloadLib()) {
            Fatal("FluiLib->UnloadLib FAILED!!!");
            return false;
        }

        return true;
    }

    Error("Call StopSynthModule() first!", true);
    return false;
}

bool OmniMIDI::FluidSynth::StartSynthModule() {
    if (!_fluidConfig)
        return false;

    fSet = new_fluid_settings();
    if (!fSet) {
        Error("new_fluid_settings failed to allocate memory for its settings!",
              true);
        return false;
    }

    if (_fluidConfig->ExperimentalMultiThreaded ||
        (_fluidConfig->ThreadsCount < 1 ||
         _fluidConfig->ThreadsCount > std::thread::hardware_concurrency()))
        _fluidConfig->ThreadsCount = 1;

    fluid_settings_setint(fSet, "synth.cpu-cores",
                          _fluidConfig->ExperimentalMultiThreaded
                              ? 1
                              : _fluidConfig->ThreadsCount);
    fluid_settings_setint(fSet, "audio.period-size", _fluidConfig->PeriodSize);
    fluid_settings_setint(fSet, "audio.periods", _fluidConfig->Periods);
    fluid_settings_setint(fSet, "synth.device-id", 16);
    fluid_settings_setint(fSet, "synth.min-note-length",
                          _fluidConfig->MinimumNoteLength);
    fluid_settings_setint(fSet, "synth.polyphony", _fluidConfig->VoiceLimit);
    fluid_settings_setint(fSet, "synth.verbose", 0);
    fluid_settings_setnum(fSet, "synth.sample-rate", _fluidConfig->SampleRate);
    fluid_settings_setnum(fSet, "synth.overflow.volume",
                          _fluidConfig->OverflowVolume);
    fluid_settings_setnum(fSet, "synth.overflow.percussion",
                          _fluidConfig->OverflowPercussion);
    fluid_settings_setnum(fSet, "synth.overflow.important",
                          _fluidConfig->OverflowImportant);
    fluid_settings_setnum(fSet, "synth.overflow.released",
                          _fluidConfig->OverflowReleased);
    fluid_settings_setstr(fSet, "audio.driver", _fluidConfig->Driver.c_str());
    fluid_settings_setstr(fSet, "audio.sample-format",
                          _fluidConfig->SampleFormat.c_str());
    fluid_settings_setstr(fSet, "synth.midi-bank-select", "xg");

    if (!_fluidConfig->ExperimentalMultiThreaded)
        AudioStreamSize = 1;

    for (size_t i = 0; i < AudioStreamSize; i++) {
        AudioStreams[i] = new_fluid_synth(fSet);

        if (!AudioStreams[i]) {
            Error("new_fluid_synth failed!", true);
            return false;
        }

        AudioDrivers[i] = new_fluid_audio_driver(fSet, AudioStreams[i]);
        if (!AudioDrivers[i]) {
            Error("new_fluid_audio_driver failed!", true);
            return false;
        }
    }

    StopDebugOutput();

    LoadSoundFonts();
    _sfSystem.RegisterCallback(this);

    Message("fSyn and fDrv are operational. FluidSynth is now working.");
    return true;
}

bool OmniMIDI::FluidSynth::StopSynthModule() {
    _sfSystem.RegisterCallback();
    _sfSystem.ClearList();

    for (size_t i = 0; i < AudioStreamSize; i++) {
        if (AudioDrivers[i]) {
            delete_fluid_audio_driver(AudioDrivers[i]);
            AudioDrivers[i] = nullptr;
        }

        if (AudioStreams[i]) {
            delete_fluid_synth(AudioStreams[i]);
            AudioStreams[i] = nullptr;
        }
    }

    StopDebugOutput();

    Message("fSyn and fDrv have been freed. FluidSynth is now asleep.");
    return true;
}

uint32_t OmniMIDI::FluidSynth::PlayLongEvent(uint8_t *ev, uint32_t size) {
    if (!FluiLib || !FluiLib->IsOnline())
        return 0;

    if (!IsSynthInitialized())
        return 0;

    return UPlayLongEvent(ev, size);
}

uint32_t OmniMIDI::FluidSynth::UPlayLongEvent(uint8_t *ev, uint32_t size) {
    int resp_len = 0;

    // Ignore 0xF0 and 0xF7
    fluid_synth_sysex(AudioStreams[0], (const char *)(ev + 1), size - 2, 0,
                      &resp_len, 0, 0);

    return resp_len;
}

#endif