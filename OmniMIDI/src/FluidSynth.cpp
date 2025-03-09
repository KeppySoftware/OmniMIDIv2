/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

// Not supported on ARM Thumb-2!

#include "FluidSynth.hpp"

void OmniMIDI::FluidSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (!AudioDrivers[0])
		MiscFuncs.MicroSleep(-1);

	for (int i = 0; i < AudioStreamSize; i++)
		fluid_synth_system_reset(AudioStreams[i]);

	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			MiscFuncs.MicroSleep(-1);
	}
}

bool OmniMIDI::FluidSynth::ProcessEvBuf() {
	// SysEx
	int len = 0;
	int handled = 0;
	unsigned int sysev = 0;

	if (!AudioDrivers[0])
		return false;

	auto tev = ShortEvents->Read();

	if (!tev)
		return false;

	unsigned char status = GetStatus(tev);
	unsigned char cmd = GetCommand(status);
	unsigned char ch = GetChannel(status);
	unsigned char param1 = GetFirstParam(tev);
	unsigned char param2 = GetSecondParam(tev);

	fluid_synth_t* targetStream = Settings->ExperimentalMultiThreaded ? AudioStreams[ch] : AudioStreams[0];

	switch (cmd) {
	case NoteOn:
		// param1 is the key, param2 is the velocity
		fluid_synth_noteon(targetStream, ch, param1, param2);
		break;

	case NoteOff:
		// param1 is the key, ignore param2
		fluid_synth_noteoff(targetStream, ch, param1);
		break;

	case Aftertouch:
		fluid_synth_key_pressure(targetStream, ch, param1, param2);
		break;

	case CC:
		fluid_synth_cc(targetStream, ch, param1, param2);
		break;

	case PatchChange:
		fluid_synth_program_change(targetStream, ch, param1);
		break;

	case ChannelPressure:
		fluid_synth_channel_pressure(targetStream, ch, param1);
		break;

	case PitchBend:
		fluid_synth_pitch_bend(targetStream, ch, param2 << 7 | param1);
		break;

	default:
		switch (status) {

		// Let's go!
		case SystemMessageStart:
			sysev = tev;

			LOG("SysEx Begin: %x", sysev);
			fluid_synth_sysex(targetStream, (const char*)&sysev, 2, 0, &len, &handled, 0);

			while (GetStatus(sysev) != SystemMessageEnd) {
				sysev = ShortEvents->Peek();

				if (GetStatus(sysev) != SystemMessageEnd) {
					sysev = ShortEvents->Read();
					LOG("SysEx Ev: %x", sysev);
					fluid_synth_sysex(targetStream, (const char*)&sysev, 3, 0, &len, &handled, 0);
				}		
			}

			LOG("SysEx End", sysev);		
			break;

		case SystemReset:
			for (int i = 0; i < 16; i++)
			{
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

bool OmniMIDI::FluidSynth::LoadSynthModule() {
	if (!Settings) {
		auto ptr = (LibImport*)fLibImp;

		if (!Settings) {
			Settings = new FluidSettings(ErrLog);
			Settings->LoadSynthConfig();
		}

		if (!FluiLib)
			FluiLib = new Lib(FLUIDLIB, FLUIDSFX, ErrLog, &ptr, fLibImpLen);

		if (!FluiLib->LoadLib())
			return false;

		if (!AllocateShortEvBuf(Settings->EvBufSize)) {
			NERROR("AllocateShortEvBuf failed.", true);
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

		if (Settings) {
			delete Settings;
			Settings = nullptr;
		}

		delete_fluid_settings(fSet);
		fSet = nullptr;

		if (!FluiLib->UnloadLib())
		{
			FNERROR("FluiLib->UnloadLib FAILED!!!");
			return false;
		}

		return true;
	}

	NERROR("Call StopSynthModule() first!", true);
	return false;
}

void OmniMIDI::FluidSynth::LoadSoundFonts() {
	// Free old SFs
	for (int i = 0; i < std::count(SoundFontIDs.begin(), SoundFontIDs.end(), -1); i++) {
		for (int a = 0; a < AudioStreamSize; a++) {
			fluid_synth_sfunload(AudioStreams[a], SoundFontIDs[i], 0);
			fluid_synth_all_notes_off(AudioStreams[a], i);
			fluid_synth_all_sounds_off(AudioStreams[a], i);
			fluid_synth_system_reset(AudioStreams[a]);
		}
	}

	SoundFontIDs.clear();

	if ((SoundFontsVector = SFSystem.LoadList()) != nullptr) {
		std::vector<SoundFont>& dSFv = *SoundFontsVector;

		if (dSFv.size() > 0) {
			for (int i = 0; i < dSFv.size(); i++) {
				for (int a = 0; a < AudioStreamSize; a++) {
					SoundFontIDs.push_back(fluid_synth_sfload(AudioStreams[a], (const char*)dSFv[i].path.c_str(), 1));
				}
			}
		}	
	}
}

bool OmniMIDI::FluidSynth::StartSynthModule() {
	if (!Settings)
		return false;

	fSet = new_fluid_settings();
	if (!fSet) {
		NERROR("new_fluid_settings failed to allocate memory for its settings!", true);
		return false;
	}

	if (Settings->ExperimentalMultiThreaded || (Settings->ThreadsCount < 1 || Settings->ThreadsCount > std::thread::hardware_concurrency()))
		Settings->ThreadsCount = 1;

	fluid_settings_setint(fSet, "synth.cpu-cores", Settings->ExperimentalMultiThreaded ? 1 : Settings->ThreadsCount);
	fluid_settings_setint(fSet, "audio.period-size", Settings->PeriodSize);
	fluid_settings_setint(fSet, "audio.periods", Settings->Periods);
	fluid_settings_setint(fSet, "synth.device-id", 16);
	fluid_settings_setint(fSet, "synth.min-note-length", Settings->MinimumNoteLength);
	fluid_settings_setint(fSet, "synth.polyphony", Settings->VoiceLimit);
	fluid_settings_setint(fSet, "synth.verbose", 0);
	fluid_settings_setnum(fSet, "synth.sample-rate", Settings->SampleRate);
	fluid_settings_setnum(fSet, "synth.overflow.volume", Settings->OverflowVolume);
	fluid_settings_setnum(fSet, "synth.overflow.percussion", Settings->OverflowPercussion);
	fluid_settings_setnum(fSet, "synth.overflow.important", Settings->OverflowImportant);
	fluid_settings_setnum(fSet, "synth.overflow.released", Settings->OverflowReleased);
	fluid_settings_setstr(fSet, "audio.driver", Settings->Driver.c_str());
	fluid_settings_setstr(fSet, "audio.sample-format", Settings->SampleFormat.c_str());
	fluid_settings_setstr(fSet, "synth.midi-bank-select", "xg");

	if (!Settings->ExperimentalMultiThreaded)
		AudioStreamSize = 1;

	for (int i = 0; i < AudioStreamSize; i++) {
		AudioStreams[i] = new_fluid_synth(fSet);

		if (!AudioStreams[i]) {
			NERROR("new_fluid_synth failed!", true);
			return false;
		}

		AudioDrivers[i] = new_fluid_audio_driver(fSet, AudioStreams[i]);
		if (!AudioDrivers[i]) {
			NERROR("new_fluid_audio_driver failed!", true);
			return false;
		}
	}

	LoadSoundFonts();
	SFSystem.RegisterCallback(this);

	LOG("fSyn and fDrv are operational. FluidSynth is now working.");
	return true;
}

bool OmniMIDI::FluidSynth::StopSynthModule() {
	SFSystem.RegisterCallback();
	SFSystem.ClearList();

	for (int i = 0; i < AudioStreamSize; i++) {
		if (AudioDrivers[i]) {
			delete_fluid_audio_driver(AudioDrivers[i]);
			AudioDrivers[i] = nullptr;
		}

		if (AudioStreams[i]) {
			delete_fluid_synth(AudioStreams[i]);
			AudioStreams[i] = nullptr;
		}
	}

	LOG("fSyn and fDrv have been freed. FluidSynth is now asleep.");
	return true;
}

unsigned int OmniMIDI::FluidSynth::PlayLongEvent(char* ev, unsigned int size) {
	if (!FluiLib || !FluiLib->IsOnline())
		return 0;

	if (!IsSynthInitialized())
		return 0;

	return UPlayLongEvent(ev, size);
}

unsigned int OmniMIDI::FluidSynth::UPlayLongEvent(char* ev, unsigned int size) {
	int resp_len = 0;

	// Ignore 0xF0 and 0xF7
	fluid_synth_sysex(AudioStreams[0], ev + 1, size - 2, 0, &resp_len, 0, 0);

	return resp_len;
}