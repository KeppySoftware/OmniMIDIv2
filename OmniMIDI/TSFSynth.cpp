/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#define TSF_IMPLEMENTATION
#include "TSFSynth.h"

void OmniMIDI::TinySFSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (!IsSynthInitialized())
		NTFuncs.uSleep(-1);

	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			NTFuncs.uSleep(-1);
	}
}

bool OmniMIDI::TinySFSynth::ProcessEvBuf() {
	unsigned int tev = 0;

	if (!Events->Pop(&tev) || !IsSynthInitialized())
		return false;

	if (CHKLRS(GETSTATUS(tev)) != 0) LastRunningStatus = GETSTATUS(tev);
	else tev = tev << 8 | LastRunningStatus;

	unsigned char status = GETSTATUS(tev);
	unsigned char cmd = GETCMD(tev);
	unsigned char ch = GETCHANNEL(tev);
	unsigned char param1 = GETFP(tev);
	unsigned char param2 = GETSP(tev);

	SDL_LockMutex(g_Mutex);

	switch (cmd) {
	case MIDI_NOTEON:
		tsf_channel_note_on(g_TinySoundFont, ch, param1, ((float)param2) / 127.0f);
		break;
	case MIDI_NOTEOFF:
		tsf_channel_note_off(g_TinySoundFont, ch, param1);
		break;
	case MIDI_PROGCHAN:
		tsf_channel_set_presetnumber(g_TinySoundFont, ch, param1, (ch == 9));
		break;
	case MIDI_CMC:
		tsf_channel_midi_control(g_TinySoundFont, ch, param1, param2);
		break;
	case MIDI_PITCHWHEEL:
		tsf_channel_set_pitchwheel(g_TinySoundFont, ch, param2 << 7 | param1);
		break;
	default:
		switch (status) {
		case MIDI_SYSRESET:
			for (int i = 0; i < 16; i++)
				tsf_channel_sounds_off_all(g_TinySoundFont, i);
		}
		// UNSUPPORTED!
		break;
	}

	SDL_UnlockMutex(g_Mutex);

	return true;
}

bool OmniMIDI::TinySFSynth::LoadSynthModule() {
	if (!Settings)
		Settings = new TinySFSettings;

	Events = new EvBuf(Settings->EvBufSize);
	_EvtThread = std::jthread(&TinySFSynth::EventsThread, this);

	return true;
}

bool OmniMIDI::TinySFSynth::UnloadSynthModule() {
	if (Events) {
		delete Events;
		Events = nullptr;
	}

	return true;
}

bool OmniMIDI::TinySFSynth::StartSynthModule() {
	if (Running)
		return true;

	if (!Settings)
		return false;

	// Define the desired audio output format we request
	SDL_AudioSpec OutputAudioSpec;
	SDL_AudioSpec FinalAudioSpec;
	memset(&OutputAudioSpec, 0, sizeof(OutputAudioSpec));
	memset(&FinalAudioSpec, 0, sizeof(FinalAudioSpec));
	OutputAudioSpec.freq = Settings->AudioFrequency;
	OutputAudioSpec.format = AUDIO_F32;
	OutputAudioSpec.channels = Settings->StereoRendering ? 2 : 1;
	OutputAudioSpec.samples = Settings->Samples;
	OutputAudioSpec.callback = TinySFSynth::cCallback;
	OutputAudioSpec.userdata = this;

	int sdlaudinit = SDL_AudioInit(TSF_NULL);
	// Initialize the audio system
	if (sdlaudinit < 0)
	{
		NERROR(SynErr, "SDL_AutoInit failed with code %d.", true, sdlaudinit);
		return false;
	}
	LOG(SynErr, "SDL_AudioInit returned 0.");

	std::vector<OmniMIDI::SoundFont>* SFv = SFSystem.LoadList();
	if (SFv != nullptr) {
		std::vector<SoundFont>& dSFv = *SFv;

		for (int i = 0; i < SFv->size(); i++) {
			g_TinySoundFont = tsf_load_filename(dSFv[i].path.c_str());
			if (g_TinySoundFont) {
				tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
				LOG(SynErr, "tsf_set_max_voices set to %d", Settings->MaxVoices);
				break;
			}
		}
	}

	if (!g_TinySoundFont) {
		LOG(SynErr, "No soundfont has been loaded, falling back to sine wave SoundFont...");
		g_TinySoundFont = tsf_load_memory(MinimalSoundFont, sizeof(MinimalSoundFont));
		if (!g_TinySoundFont)
			LOG(SynErr, "Could not load built-in SoundFont. You will not get any audio.");
	}

	tsf_set_output(g_TinySoundFont, Settings->StereoRendering ? TSF_STEREO_INTERLEAVED : TSF_MONO, OutputAudioSpec.freq, 0);
	tsf_set_max_voices(g_TinySoundFont, Settings->MaxVoices);
	LOG(SynErr, "tsf_set_max_voices set to %d", Settings->MaxVoices);

	g_Mutex = SDL_CreateMutex();

	// Request the desired audio output format
	if (SDL_OpenAudio(&OutputAudioSpec, &FinalAudioSpec) < 0)
	{
		NERROR(SynErr, "SDL_OpenAudio failed to open a target output device.", true);
		return false;
	}

	LOG(SynErr, "Op: freq %d (stereo: %d), ch %d, samp %d - Got: freq %d, ch %d, samp %d", 
		OutputAudioSpec.freq, Settings->StereoRendering, OutputAudioSpec.channels, OutputAudioSpec.samples,
		FinalAudioSpec.freq, FinalAudioSpec.channels, FinalAudioSpec.samples);

	SDL_PauseAudio(0);
	LOG(SynErr, "SDL audio stream is now playing.");

	Running = true;
	return true;
}

bool OmniMIDI::TinySFSynth::StopSynthModule() {
	if (IsSynthInitialized()) {
		SDL_PauseAudio(1);
		SDL_CloseAudio();
		SDL_DestroyMutex(g_Mutex);
		SDL_AudioQuit();
		tsf_close(g_TinySoundFont);
		g_TinySoundFont = nullptr;
		LOG(SynErr, "SDL stopped, tsf freed.");
	}

	return true;
}

SynthResult OmniMIDI::TinySFSynth::PlayShortEvent(unsigned int ev) {
	if (!Events || !IsSynthInitialized())
		return SYNTH_NOTINIT;

	return UPlayShortEvent(ev);
}

SynthResult OmniMIDI::TinySFSynth::UPlayShortEvent(unsigned int ev) {
	return Events->Push(ev) ? SYNTH_OK : SYNTH_INVALPARAM;
}

SynthResult OmniMIDI::TinySFSynth::PlayLongEvent(char* ev, unsigned int size) {
	return SYNTH_OK;
}

SynthResult OmniMIDI::TinySFSynth::UPlayLongEvent(char* ev, unsigned int size) {
	return SYNTH_OK;
}