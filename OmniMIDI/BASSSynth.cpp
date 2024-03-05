#include "BASSSynth.hpp"

void OmniMIDI::BASSSynth::AudioThread() {
	while (IsSynthInitialized()) {
		BASS_ChannelUpdate(AudioStream, 0);
		NTFuncs.uSleep(-1);
	}
}

void OmniMIDI::BASSSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (AudioStream == 0)
		NTFuncs.uSleep(-1);

	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			NTFuncs.uSleep(-1);
	}
}

bool OmniMIDI::BASSSynth::ProcessEvent(unsigned int tev) {
	/*

	For more info about how an event is structured, read this doc from Microsoft:
	https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutshortmsg

	-
	TL;DR is here though:
	Let's assume that we have an event coming through, of value 0x007F7F95.

	The high-order byte of the high word is ignored.

	The low-order of the high word contains the first part of the data, which, for
	a NoteOn event, is the key number (from 0 to 127).

	The high-order of the low word contains the second part of the data, which, for
	a NoteOn event, is the velocity of the key (again, from 0 to 127).

	The low-order byte of the low word contains two nibbles combined into one.
	The first nibble contains the type of event (1001 is 0x9, which is a NoteOn),
	while the second nibble contains the channel (0101 is 0x5, which is the 5th channel).

	So, in short, we can read it as follows:
	0x957F7F should press the 128th key on the keyboard at full velocity, on MIDI channel 5.

	But hey! We can also receive events with a long running status, which means there's no status byte!
	That event, if we consider the same example from before, will look like this: 0x00007F7F

	Such event will only work if the driver receives a status event, which will be stored in memory.
	So, in a sequence of data like this:
	..
	1. 0x00044A90 (NoteOn on key 75 at velocity 10)
	2. 0x00007F7F (... then on key 128 at velocity 127)
	3. 0x00006031 (... then on key 50 at velocity 96)
	4. 0x0000605F (... and finally on key 95 at velocity 96)
	..
	The same status from event 1 will be applied to all the status-less events from 2 and onwards.
	-

	INFO: ev will be recasted as char in some parts of the code, since those parts
	do not require the high-word part of the unsigned int.

	*/

	unsigned int sysev = 0;

	unsigned int evt = MIDI_SYSTEM_DEFAULT;
	unsigned int ev = 0;
	unsigned char status = GetStatus(tev);
	unsigned char cmd = GetCommand(tev);
	unsigned char ch = GetChannel(tev);
	unsigned char param1 = GetFirstParam(tev);
	unsigned char param2 = GetSecondParam(tev);

	unsigned int len = 3;

	switch (cmd) {
	case MIDI_NOTEON:
		// param1 is the key, param2 is the velocity
		evt = MIDI_EVENT_NOTE;
		ev = param2 << 8 | param1;
		break;
	case MIDI_NOTEOFF:
		// param1 is the key, ignore param2
		evt = MIDI_EVENT_NOTE;
		ev = param1;
		break;
	case MIDI_POLYAFTER:
		evt = MIDI_EVENT_KEYPRES;
		ev = param2 << 8 | param1;
		break;
	case MIDI_PROGCHAN:
		evt = MIDI_EVENT_PROGRAM;
		ev = param1;
		break;
	case MIDI_CHANAFTER:
		evt = MIDI_EVENT_CHANPRES;
		ev = param1;
		break;
	case MIDI_PITCHWHEEL:
		evt = MIDI_EVENT_PITCH;
		ev = param2 << 7 | param1;
		break;
	default:
		switch (status) {
			// Let's go!
		case MIDI_SYSEXBEG:
			sysev = tev << 8;

			LOG(SynErr, "SysEx Begin: %x", sysev);
			BASS_MIDI_StreamEvents(AudioStream, BASS_MIDI_EVENTS_RAW, &sysev, 2);

			while (GetStatus(sysev) != MIDI_SYSEXEND) {
				Events->Peek(&sysev);

				if (GetStatus(sysev) != MIDI_SYSEXEND) {
					Events->Pop(&sysev);
					LOG(SynErr, "SysEx Ev: %x", sysev);
					BASS_MIDI_StreamEvents(AudioStream, BASS_MIDI_EVENTS_RAW, &sysev, 3);
				}
			}

			LOG(SynErr, "SysEx End", sysev);
			return true;

		case MIDI_SYSRESET:
			// This is 0xFF, which is a system reset.
			BASS_MIDI_StreamEvent(AudioStream, 0, MIDI_EVENT_SYSTEMEX, MIDI_SYSTEM_DEFAULT);
			return true;

		default:
			// Some events do not have a specific counter part on BASSMIDI's side, so they have
			// to be directly fed to the library by using the BASS_MIDI_EVENTS_RAW flag.
			if (!(tev - 0x80 & 0xC0))
			{
				BASS_MIDI_StreamEvents(AudioStream, BASS_MIDI_EVENTS_RAW, &tev, 3);
				return true;
			}

			if (!((tev - 0xC0) & 0xE0)) len = 2;
			else if (cmd == 0xF0)
			{
				switch (GetChannel(tev))
				{
				case 0x3:
					// This is 0xF3, which is a system reset.
					len = 2;
					break;
				case 0xA:	// Start (CookedPlayer)
				case 0xB:	// Continue (CookedPlayer)
				case 0xC:	// Stop (CookedPlayer)
				case 0xE:	// Sensing
				default:
					// Not supported by OmniMIDI!
					return true;
				}
			}

			BASS_MIDI_StreamEvents(AudioStream, BASS_MIDI_EVENTS_RAW, &tev, len);
			return true;
		}
	}

	BASS_MIDI_StreamEvent(AudioStream, ch, evt, ev);
	return true;
}

bool OmniMIDI::BASSSynth::ProcessEvBuf() {
	unsigned int tev = 0;

	if (!Events->Pop(&tev) || !AudioStream)
		return false;

	if (CheckRunningStatus(GetStatus(tev)) != 0) LastRunningStatus = GetStatus(tev);
	else tev = tev << 8 | LastRunningStatus;

	return ProcessEvent(tev);
}

bool OmniMIDI::BASSSynth::LoadFuncs() {
	// Load required libs
	if (!BAudLib->LoadLib())
		return false;

	if (!BMidLib->LoadLib())
		return false;

	switch (Settings->AudioEngine) {
	case WASAPI:
		if (!BWasLib->LoadLib())
			return false;
		break;

	case ASIO:
		if (!BAsiLib->LoadLib())
			return false;
		break;
	}

#if !defined(_M_ARM) && !defined(_M_ARM64)
	if (Settings->LoudMax)
	{
		if (!BVstLib->LoadLib())
			return false;
	}
#endif

	for (int i = 0; i < sizeof(BLibImports) / sizeof(BLibImports[0]); i++)
	{
		LoadPtr(BAudLib, BLibImports[i]);
		LoadPtr(BMidLib, BLibImports[i]);
		LoadPtr(BWasLib, BLibImports[i]);
		LoadPtr(BAsiLib, BLibImports[i]);
		LoadPtr(BVstLib, BLibImports[i]);

		throw;
	}

	return true;
}

bool OmniMIDI::BASSSynth::UnloadFuncs() {
	for (int i = 0; i < sizeof(BLibImports) / sizeof(BLibImports[0]); i++)
		ClearPtr(BLibImports[i]);

	if (!BAsiLib->UnloadLib())
		return false;

	if (!BWasLib->UnloadLib())
		return false;

	if (!BMidLib->UnloadLib())
		return false;

	if (!BAudLib->UnloadLib())
		return false;

	if (!BVstLib->UnloadLib())
		return false;

	return true;
}

void OmniMIDI::BASSSynth::StreamSettings(bool restart) {
	if (restart) {
		RestartSynth = true;

		if (StopSynthModule()) {
			if (StartSynthModule()) {
				return;
			}
		}

		NERROR(SynErr, "An error has occurred while restarting the synthesizer.", true);
		return;
	}

	BASS_ChannelSetAttribute(AudioStream, BASS_ATTRIB_MIDI_VOICES, Settings->MaxVoices);
	BASS_ChannelSetAttribute(AudioStream, BASS_ATTRIB_MIDI_CPU, Settings->MaxCPU);
	BASS_ChannelSetAttribute(AudioStream, BASS_ATTRIB_MIDI_EVENTBUF_ASYNC, 1 << 24);
}

bool OmniMIDI::BASSSynth::LoadSynthModule() {
	if (!Settings)
		Settings = new BASSSettings;

	if (!BAudLib)
		BAudLib = new Lib(L"BASS");

	if (!BMidLib)
		BMidLib = new Lib(L"BASSMIDI");

	if (!BWasLib)
		BWasLib = new Lib(L"BASSWASAPI");

	if (!BAsiLib)
		BAsiLib = new Lib(L"BASSASIO");

	if (!BVstLib)
		BVstLib = new Lib(L"BASS_VST");

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (LoadFuncs()) {
		Events = new EvBuf(Settings->EvBufSize);
		_EvtThread = std::jthread(&BASSSynth::EventsThread, this);
		// LOG(SynErr, L"LoadFuncs succeeded.");
		// TODO
		return true;
	}

	NERROR(SynErr, "Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::BASSSynth::UnloadSynthModule() {
	// LOG(SynErr, L"UnloadBASSSynth called.");
	if (UnloadFuncs()) {
		delete Events;
		Events = nullptr;

		SoundFonts.clear();

		delete Settings;
		Settings = nullptr;
		return true;
	}

	NERROR(SynErr, "Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
	// SF path
	OMShared::SysPath Utils;
	wchar_t OMPath[MAX_PATH] = { 0 };

	// BASS stream flags
	const char* dev = Settings->ASIODevice.c_str();
	BASS_ASIO_DEVICEINFO devinfo = BASS_ASIO_DEVICEINFO();

	bool nofreqchange = false;
	double devfreq = 0.0;
	unsigned int asiofreq = (double)Settings->AudioFrequency;
	unsigned int lchv = 0, rchv = 1;
	const char* lch = Settings->ASIOLCh.c_str();
	const char* rch = Settings->ASIORCh.c_str();
	BASS_ASIO_CHANNELINFO chinfo = BASS_ASIO_CHANNELINFO();

	unsigned int ASIOCount = 0, ASIODev = 0, ASIOCh = 0;
	unsigned int StreamFlags = BASS_SAMPLE_FLOAT | BASS_MIDI_ASYNC;

	// If the audio stream is not 0, then stream is allocated already
	if (AudioStream)
		return true;

	switch (Settings->AudioEngine) {
	case BASS_INTERNAL:
		BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

		if (!BASS_Init(-1, Settings->AudioFrequency, BASS_DEVICE_STEREO, nullptr, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		AudioStream = BASS_MIDI_StreamCreate(16, StreamFlags, 0);
		if (!AudioStream) {
			NERROR(SynErr, "BASS_MIDI_StreamCreate failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		BASS_ChannelSetAttribute(AudioStream, BASS_ATTRIB_BUFFER, 0);

		if (!BASS_ChannelPlay(AudioStream, false)) {
			NERROR(SynErr, "BASS_ChannelPlay failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		_AudThread = std::jthread(&BASSSynth::AudioThread, this);
		if (!_AudThread.joinable()) {
			NERROR(SynErr, "_AudThread failed. (ID: %x)", true, _AudThread.get_id());
			Fail = true;
			return false;
		}

		break;

	case WASAPI:
		StreamFlags |= BASS_STREAM_DECODE;

		if (!BASS_Init(0, Settings->AudioFrequency, BASS_DEVICE_STEREO, nullptr, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		AudioStream = BASS_MIDI_StreamCreate(16, StreamFlags, 0);
		if (!AudioStream) {
			NERROR(SynErr, "BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		if (!BASS_WASAPI_Init(-1, 0, 0, BASS_WASAPI_ASYNC | BASS_WASAPI_EVENT | BASS_WASAPI_SAMPLES, Settings->WASAPIBuf, 0, WASAPIPROC_BASS, (void*)AudioStream)) {
			NERROR(SynErr, "BASS_WASAPI_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		if (!BASS_WASAPI_Start()) {
			NERROR(SynErr, "BASS_WASAPI_Start failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		break;

	case ASIO:
		StreamFlags |= BASS_STREAM_DECODE;

		// Get the amount of ASIO devices available
		for (; BASS_ASIO_GetDeviceInfo(ASIOCount, &devinfo); ASIOCount++) {
			// Return the correct ID when found
			if (strcmp(dev, devinfo.name) == 0) {
				LOG(SynErr, "Found target device: %s (ID %d)", dev, ASIOCount);
				ASIODev = ASIOCount;
			}
		}

		if (ASIOCount < 1) {
			NERROR(SynErr, "No ASIO devices available! Got 0 devices!!!", true);
			Fail = true;
			return false;
		}
		else LOG(SynErr, "Detected %d ASIO devices.", ASIOCount);

		if (!BASS_ASIO_Init(ASIODev, BASS_ASIO_THREAD | BASS_ASIO_JOINORDER)) {
			NERROR(SynErr, "BASS_ASIO_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		if (BASS_ASIO_CheckRate(asiofreq)) {
			if (!BASS_ASIO_SetRate(asiofreq)) {
				LOG(SynErr, "BASS_ASIO_SetRate failed, falling back to BASS_ASIO_ChannelSetRate... BASSERR: %d", BASS_ErrorGetCode());
				if (!BASS_ASIO_ChannelSetRate(0, 0, asiofreq)) {
					LOG(SynErr, "BASS_ASIO_ChannelSetRate failed as well, BASSERR: %d... This ASIO device does not want OmniMIDI to change its output frequency.", BASS_ErrorGetCode());
					nofreqchange = true;
				}
			}
		}

		for (; BASS_ASIO_ChannelGetInfo(0, ASIOCh, &chinfo); ASIOCh++) {
			// Return the correct ID when found
			if (strcmp(lch, chinfo.name) == 0) {
				lchv = ASIOCh;
				LOG(SynErr, "lchv = %d", lchv);
			}
			else if (strcmp(rch, chinfo.name) == 0) {
				rchv = ASIOCh;
				LOG(SynErr, "rchv = %d", lchv);
			}
		}

		if (nofreqchange) {
			devfreq = BASS_ASIO_GetRate();
			LOG(SynErr, "BASS_ASIO_GetRate = %dHz", devfreq);
			if (devfreq != -1) {
				asiofreq = devfreq;
			}
			else {
				NERROR(SynErr, "BASS_ASIO_GetRate failed to get the device's frequency, with error 0x%x.", true, BASS_ErrorGetCode());
				Fail = true;
				return false;
			}
		}

		if (!BASS_Init(0, asiofreq, BASS_DEVICE_STEREO, nullptr, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		AudioStream = BASS_MIDI_StreamCreate(16, StreamFlags, 0);
		if (!AudioStream) {
			NERROR(SynErr, "BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			Fail = true;
			return false;
		}

		if (!nofreqchange) BASS_ASIO_SetRate(asiofreq);

		if (BASS_ASIO_ChannelEnable(0, lchv, nullptr, nullptr)) {
			LOG(SynErr, "BASS_ASIO_ChannelEnable called on channel %d.", lchv);

			BASS_ASIO_ChannelSetFormat(0, lchv, BASS_ASIO_FORMAT_FLOAT);
			BASS_ASIO_ChannelSetRate(0, lchv, asiofreq);
			BASS_ASIO_ChannelEnableBASS(0, lchv, AudioStream, false);
			LOG(SynErr, "Channel %d set to %dHz and enabled.", lchv, asiofreq);

			if (BASS_ASIO_ChannelEnable(0, rchv, nullptr, nullptr)) {
				LOG(SynErr, "BASS_ASIO_ChannelEnable called on channel %d.", rchv);
				BASS_ASIO_ChannelJoin(0, rchv, lchv);
				LOG(SynErr, "Channel %d joined to %d.", rchv, lchv);

				if (!BASS_ASIO_Start(0, 0)) {
					NERROR(SynErr, "BASS_ASIO_Start failed with error 0x%x. If the code is zero, that means the device encountered an error and aborted the initialization process.", true, BASS_ErrorGetCode());
				}
				else break;
			}
		}

		Fail = true;
		return false;

	case INVALID_ENGINE:
	default:
		NERROR(SynErr, "Engine ID \"%d\" is not valid!", false, Settings->AudioEngine);
		Fail = true;
		return false;
	}

	std::vector<OmniMIDI::SoundFont>* SFv = SFSystem.LoadList();
	if (SFv != nullptr) {
		std::vector<SoundFont>& dSFv = *SFv;

		for (int i = 0; i < SFv->size(); i++) {
			unsigned int bmfiflags = 0;
			BASS_MIDI_FONTEX sf = BASS_MIDI_FONTEX();

			sf.spreset = dSFv[i].spreset;
			sf.sbank = dSFv[i].sbank;
			sf.dpreset = dSFv[i].dpreset;
			sf.dbank = dSFv[i].dbank;
			sf.dbanklsb = dSFv[i].dbanklsb;

			if (dSFv[i].xgdrums) bmfiflags |= BASS_MIDI_FONT_XGDRUMS;
			if (dSFv[i].linattmod) bmfiflags |= BASS_MIDI_FONT_LINATTMOD;
			if (dSFv[i].lindecvol) bmfiflags |= BASS_MIDI_FONT_LINDECVOL;
			if (dSFv[i].minfx) bmfiflags |= BASS_MIDI_FONT_MINFX;
			if (dSFv[i].nolimits) bmfiflags |= BASS_MIDI_FONT_NOLIMITS;
			if (dSFv[i].norampin) bmfiflags |= BASS_MIDI_FONT_NORAMPIN;

			sf.font = BASS_MIDI_FontInit(dSFv[i].path.c_str(), bmfiflags | BASS_MIDI_FONT_MMAP);

			// Check if the soundfont loads, if it does then it's valid
			if (BASS_MIDI_FontLoad(sf.font, sf.spreset, sf.sbank))
				SoundFonts.push_back(sf);
			else NERROR(SynErr, "Error 0x%x occurred while loading \"%s\"!", false, BASS_ErrorGetCode(), dSFv[i].path.c_str());
		}

		BASS_MIDI_StreamSetFonts(AudioStream, &SoundFonts[0], (unsigned int)SoundFonts.size() | BASS_MIDI_FONT_EX);
	}

	// Sorry ARM users!
#if defined(_M_AMD64) || defined(_M_IX86)
	if (Settings->LoudMax && Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath)))
	{
#if defined(_M_AMD64)
		swprintf_s(OMPath, L"%s\\OmniMIDI\\LoudMax\\LoudMax64.dll", OMPath);
#elif defined(_M_IX86)
		swprintf_s(OMPath, L"%s\\OmniMIDI\\LoudMax\\LoudMax32.dll", OMPath);
#endif

		if (GetFileAttributesW(OMPath) != INVALID_FILE_ATTRIBUTES)
			BASS_VST_ChannelSetDSP(AudioStream, OMPath, BASS_UNICODE, 1);
	}
#endif

	StreamSettings(false);
	return true;
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
	switch (Settings->AudioEngine) {
	case WASAPI:
		BASS_WASAPI_Stop(true);
		BASS_WASAPI_Free();

		// why do I have to do it myself
		BASS_WASAPI_SetNotify(nullptr, nullptr);
		break;

	case ASIO:
		BASS_ASIO_Stop();
		BASS_ASIO_Free();
		NTFuncs.uSleep(-500000);
		break;

	case BASS_INTERNAL:
	default:
		break;
	}

	if (AudioStream) {
		SFSystem.ClearList();

		BASS_StreamFree(AudioStream);
		AudioStream = 0;
	}

	BASS_Free();

	Fail = false;
	return true;
}

bool OmniMIDI::BASSSynth::SettingsManager(unsigned int setting, bool get, void* var, size_t size) {

	switch (setting) {

	case KDMAPI_MANAGE:
	{
		if (Settings || IsSynthInitialized()) {
			NERROR(SynErr, "You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
			return false;
		}

		LOG(SynErr, "KDMAPI REQUEST: The MIDI app wants to manage the settings.");
		Settings = new BASSSettings;

		break;
	}

	case KDMAPI_LEAVE:
	{
		if (Settings) {
			if (IsSynthInitialized()) {
				NERROR(SynErr, "You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
				return false;
			}

			LOG(SynErr, "KDMAPI REQUEST: The MIDI app does not want to manage the settings anymore.");
			delete Settings;
			Settings = nullptr;
		}
		break;
	}

	SettingsManagerCase(KDMAPI_AUDIOFREQ, get, unsigned int, Settings->AudioFrequency, var, size);
	SettingsManagerCase(KDMAPI_CURRENTENGINE, get, unsigned int, Settings->AudioEngine, var, size);
	SettingsManagerCase(KDMAPI_MAXVOICES, get, unsigned int, Settings->MaxVoices, var, size);
	SettingsManagerCase(KDMAPI_MAXRENDERINGTIME, get, unsigned int, Settings->MaxCPU, var, size);

	default:
		LOG(SynErr, "Unknown setting passed to SettingsManager.");
		return false;
	}

	return true;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::PlayShortEvent(unsigned int ev) {
	if (!Events)
		return NotInitialized;

	return UPlayShortEvent(ev);
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::UPlayShortEvent(unsigned int ev) {
	return Events->Push(ev) ? Ok : InvalidParameter;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::PlayLongEvent(char* ev, unsigned int size) {
	if (!BMidLib || !BMidLib->IsOnline())
		return LibrariesOffline;

	// The size has to be between 1B and 64KB!
	if (size < 1 || size > 65536)
		return InvalidParameter;

	return UPlayLongEvent(ev, size);
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::UPlayLongEvent(char* ev, unsigned int size) {
	unsigned int r = BASS_MIDI_StreamEvents(AudioStream, BASS_MIDI_EVENTS_RAW, ev, size);
	return (r != -1) ? Ok : InvalidBuffer;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) {
	if (!BMidLib || !BMidLib->IsOnline())
		return LibrariesOffline;

	if (!evt && !chan)
		return ProcessEvent(param) ? Ok : InvalidParameter;

	return BASS_MIDI_StreamEvent(AudioStream, chan, evt, param) ? Ok : InvalidParameter;
}