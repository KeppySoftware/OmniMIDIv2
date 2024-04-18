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

	Events->Pop(&ev);

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
			break;
		default:
			return false;
		}
	}

	return true;
}

void OmniMIDI::KSynthM::ProcessEvBufChk() {
	do ProcessEvBuf();
	while (Events->NewEventsAvailable());
}

void OmniMIDI::KSynthM::AudioThread() {
	unsigned div = Settings->XABufSize / (Settings->XABufSize / 2);
	size_t arrsize = Settings->XABufSize;
	size_t chksize = arrsize / div;
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

		XAEngine->Update(buf, arrsize);

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
	char* Buf = new char[4096];

	while (!Terminate) {
		sprintf_s(Buf, 4096, "RT > %06.2f%% - POLY: %d (Ev RH%05d WH%05d)", Synth->rendering_time * 100, Synth->polyphony, Events->GetReadHeadPos(), Events->GetWriteHeadPos());
		SetConsoleTitleA(Buf);

		MiscFuncs.uSleep(-1);
	}

	delete[] Buf;
}

bool OmniMIDI::KSynthM::LoadSynthModule() {
	auto ptr = (LibImport*)LibImports;

	if (!Settings)
		Settings = new KSynthSettings;

	if (!KLib)
		KLib = new Lib(L"ksynth", &ptr, LibImportsSize);

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (KLib->LoadLib()) {
		return true;
	}

	NERROR(SynErr, "Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::KSynthM::UnloadSynthModule() {
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
	Synth = ksynth_new(Settings->SampleSet.c_str(), Settings->SampleRate, 2, Settings->MaxVoices);

	if (Synth) {
		XAEngine = new XAudio2Output;

		auto xar = XAEngine->Init(XAFlags::FloatData | XAFlags::StereoAudio, Settings->SampleRate, Settings->XABufSize, Settings->XASweepRate);
		if (xar != XAResult::Success) {
			NERROR(SynErr, "XAEngine->Init failed. %u", true, xar);

			XAEngine->Stop();
			delete XAEngine;
	
			return false;
		}

		Events = new EvBuf(Settings->EvBufSize);
		_AudThread = std::jthread(&KSynthM::AudioThread, this);
		if (!_AudThread.joinable()) {
			NERROR(SynErr, "_AudThread failed. (ID: %x)", true, _AudThread.get_id());
			return false;
		}

		_LogThread = std::jthread(&KSynthM::LogThread, this);
		if (!_LogThread.joinable()) {
			NERROR(SynErr, "_LogThread failed. (ID: %x)", true, _LogThread.get_id());
			return false;
		}

		//_EvtThread = std::jthread(&KSynthM::EventsThread, this);
		// if (!_EvtThread.joinable()) {
		//	NERROR(SynErr, "_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
		//	return false;
		// }

		if (Settings->ShowStats) {
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

	if (Settings->ShowStats && OwnConsole) {
		FreeConsole();
		OwnConsole = false;
	}

	ksynth_free(Synth);
	Synth = nullptr;

	return true;
}

void OmniMIDI::KSynthM::PlayShortEvent(unsigned int ev) {
	if (!Events)
		return;

	UPlayShortEvent(ev);
}

void OmniMIDI::KSynthM::UPlayShortEvent(unsigned int ev) {
	Events->Push(ev);
}

OmniMIDI::SynthResult OmniMIDI::KSynthM::Reset() {
	if (Synth)
		ksynth_note_off_all(Synth);

	return Ok;
}