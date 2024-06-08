/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "KSynthM.hpp"

bool OmniMIDI::KSynthM::ProcessEvBuf() {
	unsigned int ev = 0;

	if (!Synth)
		return false;

	ShortEvents->Pop(&ev);

	if (!ev)
		return false;

	ev = ApplyRunningStatus(ev);

	unsigned char status = GetStatus(ev);
	unsigned char command = GetCommand(status);
	unsigned char chan = GetChannel(ev);
	unsigned char param1 = GetFirstParam(ev);
	unsigned char param2 = GetSecondParam(ev);

	switch (command) {
	case NoteOn:
		// param1 is the key, param2 is the velocity
		ksynth_note_on(Synth, chan, param1, param2);
		break;
	case NoteOff:
		// param1 is the key, ignore param2
		ksynth_note_off(Synth, chan, param1);	
		break;
	case CC:
		ksynth_cc(Synth, chan, param1, param2);
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
	unsigned div = Settings->XAMaxSamplesPerFrame / (Settings->XAMaxSamplesPerFrame / 2);
	unsigned int arrsize = Settings->XAMaxSamplesPerFrame;
	unsigned int chksize = arrsize / div;
	float* buf = new float[arrsize];
	float* cbuf = new float[chksize];

	if (chksize < 2)
		chksize = 2;

	while (!Terminate) {
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
	while (!Terminate) {
		if (!ProcessEvBuf())
			MiscFuncs.uSleep(-1);
	}
}

void OmniMIDI::KSynthM::LogThread() {
	const float smoothingFac = 0.0025f;
	float smoothedVal = 0.0f;
	char* Buf = new char[192];

	while (!Terminate) {
		smoothedVal = (1.0f - smoothingFac) * smoothedVal + smoothingFac * (Synth->rendering_time * 100);
		sprintf_s(Buf, 192, "RT > %06.2f%% - POLY: %d (Ev RH%05d WH%05d)", smoothedVal, Synth->polyphony, ShortEvents->GetReadHeadPos(), ShortEvents->GetWriteHeadPos());
		SetConsoleTitleA(Buf);

		MiscFuncs.uSleep(-1);
	}

	delete[] Buf;
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
		_AudThread = std::jthread(&KSynthM::AudioThread, this);
		if (!_AudThread.joinable()) {
			NERROR(SynErr, "_AudThread failed. (ID: %x)", true, _AudThread.get_id());
			StopSynthModule();
			return false;
		}

		_LogThread = std::jthread(&KSynthM::LogThread, this);
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

		if (Settings->IsDebugMode()) {
			if (AllocConsole()) {
				OwnConsole = true;
				FILE* dummy;
				freopen_s(&dummy, "CONOUT$", "w", stdout);
				freopen_s(&dummy, "CONOUT$", "w", stderr);
				freopen_s(&dummy, "CONIN$", "r", stdin);
				std::cout.clear();
				std::clog.clear();
				std::cerr.clear();
				std::cin.clear();
			}
		}

		return true;
	}

	return false;
}

bool OmniMIDI::KSynthM::StopSynthModule() {
	Terminate = true;

	if (_AudThread.joinable())
		_AudThread.join();

	if (_LogThread.joinable())
		_LogThread.join();

	if (WinAudioEngine) {
		WinAudioEngine->Stop();
		delete WinAudioEngine;
		WinAudioEngine = nullptr;
	}

	if (Settings->IsDebugMode() && OwnConsole) {
		FreeConsole();
		OwnConsole = false;
	}

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