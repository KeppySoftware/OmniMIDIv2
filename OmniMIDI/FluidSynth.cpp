/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

// Not supported on ARM Thumb-2!

#include "FluidSynth.hpp"

void OmniMIDI::FluidSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (!fDrv)
		MiscFuncs.uSleep(-1);

	fluid_synth_system_reset(fSyn);
	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			MiscFuncs.uSleep(-1);
	}
}

bool OmniMIDI::FluidSynth::ProcessEvBuf() {
	// SysEx
	int len = 0;
	int handled = 0;
	unsigned int sysev = 0;
	unsigned int tgtev = 0;

	if (!fDrv)
		return false;

	ShortEvents->Pop(&tgtev);

	if (!tgtev)
		return false;

	tgtev = ApplyRunningStatus(tgtev);

	unsigned char st = GetStatus(tgtev);
	unsigned char cmd = GetCommand(st);
	unsigned char ch = GetChannel(tgtev);
	unsigned char param1 = GetFirstParam(tgtev);
	unsigned char param2 = GetSecondParam(tgtev);

	switch (cmd) {
	case NoteOn:
		// param1 is the key, param2 is the velocity
		fluid_synth_noteon(fSyn, ch, param1, param2);
		break;

	case NoteOff:
		// param1 is the key, ignore param2
		fluid_synth_noteoff(fSyn, ch, param1);
		break;

	case Aftertouch:
		fluid_synth_key_pressure(fSyn, ch, param1, param2);
		break;

	case CC:
		fluid_synth_cc(fSyn, ch, param1, param2);
		break;

	case PatchChange:
		fluid_synth_program_change(fSyn, ch, param1);
		break;

	case ChannelPressure:
		fluid_synth_channel_pressure(fSyn, ch, param1);
		break;

	case PitchBend:
		fluid_synth_pitch_bend(fSyn, ch, param2 << 7 | param1);
		break;

	default:
		switch (st) {

		// Let's go!
		case SystemMessageStart:
			sysev = tgtev << 8;

			LOG(SynErr, "SysEx Begin: %x", sysev);
			fluid_synth_sysex(fSyn, (const char*)&sysev, 2, 0, &len, &handled, 0);

			while (GetStatus(sysev) != SystemMessageEnd) {
				ShortEvents->Peek(&sysev);

				if (GetStatus(sysev) != SystemMessageEnd) {
					ShortEvents->Pop(&sysev);
					LOG(SynErr, "SysEx Ev: %x", sysev);
					fluid_synth_sysex(fSyn, (const char*)&sysev, 3, 0, &len, &handled, 0);
				}		
			}

			LOG(SynErr, "SysEx End", sysev);		
			break;

		case SystemReset:
			for (int i = 0; i < 16; i++)
			{
				fluid_synth_all_notes_off(fSyn, i);
				fluid_synth_all_sounds_off(fSyn, i);
				fluid_synth_system_reset(fSyn);
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

bool OmniMIDI::FluidSynth::LoadSynthModule() {
	if (!Settings) {
		auto ptr = (LibImport**)&fLibImp;

		if (!Settings) {
			Settings = new FluidSettings;
			Settings->LoadSynthConfig();
		}

		if (!FluiLib)
			FluiLib = new Lib(L"libfluidsynth-3", ptr, fLibImpLen);

		if (!FluiLib->LoadLib())
			return false;

		if (!AllocateShortEvBuf(Settings->EvBufSize)) {
			NERROR(SynErr, "AllocateShortEvBuf failed.", true);
			return false;
		}

		_EvtThread = std::jthread(&FluidSynth::EventsThread, this);
	}

	return true;
}

bool OmniMIDI::FluidSynth::UnloadSynthModule() {
	if (!FluiLib || !FluiLib->IsOnline())
		return true;

	if (!fSyn && !fDrv) {
		FreeShortEvBuf();

		if (Settings) {
			delete Settings;
			Settings = nullptr;
		}

		delete_fluid_settings(fSet);
		fSet = nullptr;

		if (!FluiLib->UnloadLib())
		{
			FNERROR(SynErr, "FluiLib->UnloadLib FAILED!!!");
			return false;
		}

		return true;
	}

	NERROR(SynErr, "Call StopSynthModule() first!", true);
	return false;
}

bool OmniMIDI::FluidSynth::StartSynthModule() {
	fSet = new_fluid_settings();
	if (!fSet)
		NERROR(SynErr, "new_fluid_settings failed to allocate memory for its settings!", true);

	if (fSet && Settings) {
		if (Settings->ThreadsCount < 1 || Settings->ThreadsCount > std::thread::hardware_concurrency())
			Settings->ThreadsCount = 1;

		fluid_settings_setint(fSet, "synth.cpu-cores", Settings->ThreadsCount);
		fluid_settings_setint(fSet, "audio.period-size", Settings->PeriodSize);
		fluid_settings_setint(fSet, "audio.periods", Settings->Periods);
		fluid_settings_setint(fSet, "synth.device-id", 16);
		fluid_settings_setint(fSet, "synth.min-note-length", Settings->MinimumNoteLength);
		fluid_settings_setint(fSet, "synth.polyphony", Settings->VoiceLimit);
		fluid_settings_setint(fSet, "synth.threadsafe-api", Settings->ThreadsCount > 1 ? 0 : 1);
		fluid_settings_setnum(fSet, "synth.sample-rate", Settings->SampleRate);
		fluid_settings_setnum(fSet, "synth.overflow.volume", Settings->OverflowVolume);
		fluid_settings_setnum(fSet, "synth.overflow.percussion", Settings->OverflowPercussion);
		fluid_settings_setnum(fSet, "synth.overflow.important", Settings->OverflowImportant);
		fluid_settings_setnum(fSet, "synth.overflow.released", Settings->OverflowReleased);
		fluid_settings_setstr(fSet, "audio.driver", Settings->Driver.c_str());
		fluid_settings_setstr(fSet, "audio.sample-format", Settings->SampleFormat.c_str());
		fluid_settings_setstr(fSet, "synth.midi-bank-select", "xg");

		fSyn = new_fluid_synth(fSet);
		if (!fSyn) {
			NERROR(SynErr, "new_fluid_synth failed!", true);
			goto err;
		}

		fDrv = new_fluid_audio_driver(fSet, fSyn);
		if (!fSyn) {
			NERROR(SynErr, "new_fluid_audio_driver failed!", true);
			goto err;
		}

		std::vector<OmniMIDI::SoundFont>* SFv = SFSystem.LoadList();
		if (SFv != nullptr) {
			std::vector<SoundFont>& dSFv = *SFv;

			for (int i = 0; i < SFv->size(); i++) {
				int sf = fluid_synth_sfload(fSyn, dSFv[i].path.c_str(), 1);

				// Check if the soundfont loads, if it does then it's valid
				if (sf)
					SoundFonts.push_back(sf);
				else NERROR(SynErr, "fluid_synth_sfload failed to load \"%s\"!", false, dSFv[i].path.c_str());
			}
		}

		LOG(SynErr, "fSyn and fDrv are operational. FluidSynth is now working.");
		return true;
	}

err:
	OmniMIDI::FluidSynth::StopSynthModule();
	return false;
}

bool OmniMIDI::FluidSynth::StopSynthModule() {
	SFSystem.ClearList();

	if (fDrv) {
		delete_fluid_audio_driver(fDrv);
		fDrv = nullptr;
	}

	if (fSyn) {
		delete_fluid_synth(fSyn);
		fSyn = nullptr;
	}

	LOG(SynErr, "fSyn and fDrv have been freed. FluidSynth is now asleep.");
	return true;
}

void OmniMIDI::FluidSynth::PlayShortEvent(unsigned int ev) {
	if (!ShortEvents || !IsSynthInitialized())
		return;

	UPlayShortEvent(ev);
}

void OmniMIDI::FluidSynth::UPlayShortEvent(unsigned int ev) {
	ShortEvents->Push(ev);
}

OmniMIDI::SynthResult OmniMIDI::FluidSynth::PlayLongEvent(char* ev, unsigned int size) {
	if (!FluiLib || !FluiLib->IsOnline())
		return LibrariesOffline;

	if (!IsSynthInitialized())
		return NotInitialized;

	// The size has to be between 1B and 64KB!
	if (size < 1 || size > 65536)
		return InvalidParameter;

	return UPlayLongEvent(ev, size);
}

OmniMIDI::SynthResult OmniMIDI::FluidSynth::UPlayLongEvent(char* ev, unsigned int size) {
	int handled = 0;

	fluid_synth_sysex(fSyn, ev, size, 0, 0, &handled, 0);

	return handled ? Ok : InvalidParameter;
}