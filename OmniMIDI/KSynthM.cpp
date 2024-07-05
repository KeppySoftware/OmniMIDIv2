/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "KSynthM.hpp"

bool OmniMIDI::KSynthM::ProcessEvBuf() {
	if (!Synth)
		return false;

	PSE tev = ShortEvents->PopItem();

	if (!tev)
		return false;

	unsigned char status = tev->status;
	unsigned char cmd = GetCommandChar(status);
	unsigned char ch = GetChannelChar(status);
	unsigned char param1 = tev->param1;
	unsigned char param2 = tev->param2;

	switch (cmd) {
	case NoteOn:
		// param1 is the key, param2 is the velocity
		ksynth_note_on(Synth, ch, param1, param2);
		break;
	case NoteOff:
		// param1 is the key, ignore param2
		ksynth_note_off(Synth, ch, param1);
		break;
	case CC:
		ksynth_cc(Synth, ch, param1, param2);
		break;
	default:
		switch (status) {
			// Let's go!
		case SystemReset:
			// This is 0xFF, which is a system reset.
			ksynth_note_off_all(Synth);
			LOG(SynErr, "SR!");
			break;
		default:
			return false;
		}
	}

	return true;
}

void OmniMIDI::KSynthM::ProcessEvBufChk() {
	do ProcessEvBuf();
	while (ShortEvents->NewEventsAvailable());
}

void OmniMIDI::KSynthM::AudioThread() {
	// If bufsize is 64, div will be 32, so the chunk will be 2
	unsigned div = Settings->XAChunksDivision;
	size_t arrsize = Settings->XAMaxSamplesPerFrame;
	size_t chksize = arrsize / div;
	float* buf = nullptr;
	float* cbuf = nullptr;

	buf = new float[arrsize];
	cbuf = new float[chksize];

	while (IsSynthInitialized()) {
		for (int i = 0; i < div; i++) {
			ProcessEvBufChk();

			ksynth_fill_buffer(Synth, cbuf, chksize);
			for (int j = 0; j < chksize; j++) {
				buf[(i * chksize) + j] = cbuf[j];
			}
		}

		WinAudioEngine->Update(buf, arrsize);
		MiscFuncs.uSleep(-1);
	}

	delete[] cbuf;
	delete[] buf;
}

void OmniMIDI::KSynthM::EventsThread() {
	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			MiscFuncs.uSleep(-1);
	}
}

void OmniMIDI::KSynthM::DataCheckThread() {
	const float smoothingFac = 0.0025f;
	float smoothedVal = 0.0f;

	while (IsSynthInitialized()) {
		smoothedVal = (1.0f - smoothingFac) * smoothedVal + smoothingFac * (Synth->rendering_time * 100);
		ActiveVoices = Synth->polyphony;
		RenderingTime = smoothedVal;
		MiscFuncs.uSleep(-1);
	}
}

bool OmniMIDI::KSynthM::LoadSynthModule() {
	auto ptr = (LibImport*)LibImports;

	if (!Settings) {
		Settings = new KSynthSettings;
		Settings->LoadSynthConfig();
	}

	if (!KLib)
		KLib = new Lib(L"ksynth", &ptr, LibImportsSize);

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (!KLib->LoadLib()) {
		NERROR(SynErr, "Something went wrong here!!!", true);
		return false;
	}

	if (!AllocateShortEvBuf(Settings->EvBufSize)) {
		NERROR(SynErr, "AllocateShortEvBuf failed.", true);
		return false;
	}

	return true;
}

bool OmniMIDI::KSynthM::UnloadSynthModule() {
	FreeShortEvBuf();

	if (!KLib)
		return true;

	if (!KLib->UnloadLib())
		return false;

	return true;
}

bool OmniMIDI::KSynthM::StartSynthModule() {
	if (!Settings)
		Settings = new KSynthSettings;

	Terminate = false;
	Synth = ksynth_new(Settings->SampleSet.c_str(), Settings->SampleRate, 2, Settings->VoiceLimit);

	if (Synth) {
		if (!WinAudioEngine)
			WinAudioEngine = (SoundOut*)CreateXAudio2Output();

		auto xar = WinAudioEngine->Init(nullptr, SOAudioFlags::FloatData | SOAudioFlags::StereoAudio, Settings->SampleRate, Settings->XAMaxSamplesPerFrame, Settings->XASamplesPerFrame, Settings->XAChunksDivision);
		if (xar != SoundOutResult::Success) {
			NERROR(SynErr, "WinAudioEngine->Init failed. %u", true, xar);

			StopSynthModule();
			return false;
		}

		ShortEvents = new EvBuf(Settings->EvBufSize);
		_AudThread[0] = std::jthread(&KSynthM::AudioThread, this);
		if (!_AudThread[0].joinable()) {
			NERROR(SynErr, "_AudThread failed. (ID: %x)", true, _AudThread[0].get_id());
			StopSynthModule();
			return false;
		}

		_DatThread = std::jthread(&KSynthM::DataCheckThread, this);
		if (!_DatThread.joinable()) {
			NERROR(SynErr, "_DatThread failed. (ID: %x)", true, _DatThread.get_id());
			StopSynthModule();
			return false;
		}

		_LogThread = std::jthread(&SynthModule::LogFunc, this);
		if (!_LogThread.joinable()) {
			NERROR(SynErr, "_LogThread failed. (ID: %x)", true, _LogThread.get_id());
			StopSynthModule();
			return false;
		}

		//_EvtThread = std::jthread(&KSynthM::EventsThread, this);
		// if (!_EvtThread.joinable()) {
		//	NERROR(SynErr, "_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
		//	return false;
		// }

		return true;
	}

	return false;
}

bool OmniMIDI::KSynthM::StopSynthModule() {
	Terminate = true;

	if (_DatThread.joinable())
		_DatThread.join();

	if (_AudThread[0].joinable())
		_AudThread[0].join();

	if (_LogThread.joinable())
		_LogThread.join();

	if (WinAudioEngine) {
		WinAudioEngine->Stop();
		delete WinAudioEngine;
		WinAudioEngine = nullptr;
	}

	if (Settings->IsDebugMode() && Settings->IsOwnConsole())
		Settings->CloseConsole();

	if (Synth) {
		ksynth_free(Synth);
		Synth = nullptr;
	}

	return true;
}

void OmniMIDI::KSynthM::PlayShortEvent(unsigned int ev) {
	if (!ShortEvents)
		return;

	UPlayShortEvent(ev);
}

void OmniMIDI::KSynthM::UPlayShortEvent(unsigned int ev) {
	ShortEvents->Push(ev);
}

OmniMIDI::SynthResult OmniMIDI::KSynthM::Reset() {
	if (Synth) {
		LOG(SynErr, "SR!");
		ksynth_note_off_all(Synth);
	}

	return Ok;
}