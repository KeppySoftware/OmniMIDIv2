/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "BASSSynth.hpp"

#ifdef _BASSSYNTH_H

void OmniMIDI::BASSSynth::ProcessEvBuf() {
	if (!IsSynthInitialized())
		return;

	auto pEvt = ShortEvents->ReadPtr();

	if (!pEvt)
		return;

	unsigned int evtDword = *pEvt;
	unsigned int evt = MIDI_SYSTEM_DEFAULT;
	unsigned int res = MIDI_SYSTEM_DEFAULT;
	unsigned int ev = 0;
	unsigned char len = ((evtDword - 0xC0) & 0xE0) ? 3 : 2;
	unsigned char status = GetStatus(evtDword);
	unsigned char command = status & 0xF0;
	unsigned char chan = status & 0xF;
	unsigned char param1 = GetFirstParam(evtDword);
	unsigned char param2 = GetSecondParam(evtDword);

	auto targetStream = AudioStreams[0];

	unsigned char tgtChan = 0;
	unsigned char tgtChnk = 0;
	unsigned int noteOnTgt = 0;

	if (Settings->ExperimentalAudioMultiplier) {
		tgtChan = chan * Settings->ExpMTKeyboardDiv;
		tgtChnk = param1 % Settings->ExpMTKeyboardDiv;
		noteOnTgt = tgtChan + tgtChnk;
	}

	switch (command) {
	case NoteOn:
		// param1 is the key, param2 is the velocity
		evt = MIDI_EVENT_NOTE;
		ev = param2 << 8 | param1;
		break;
	case NoteOff:
		// param1 is the key, ignore param2
		evt = MIDI_EVENT_NOTE;
		ev = param1;
		break;
	case Aftertouch:
		evt = MIDI_EVENT_KEYPRES;
		ev = param2 << 8 | param1;
		// Message("Aftertouch %u (%X, %X - %X)", ev, ev, param1, param2);
		break;
	case PatchChange:
		evt = MIDI_EVENT_PROGRAM;
		ev = param1;
		// Message("PatchChange %u (%X)", ev, param1);
		break;
	case ChannelPressure:
		evt = MIDI_EVENT_CHANPRES;
		ev = param1;
		// Message("ChannelPressure %u (%X)", ev, param1);
		break;
	case PitchBend:
		evt = MIDI_EVENT_PITCH;
		ev = param2 << 7 | param1;
		// Message("PitchBend %u (%X, %X - %X)", ev, ev, param1, param2);
		break;
	default:
		switch (status) {
		// Let's go!
		case SystemReset:
			evt = MIDI_EVENT_SYSTEMEX;
			// This is 0xFF, which is a system reset.
			switch (param1) {
			case 0x01:
				res = MIDI_SYSTEM_GS;
				break;

			case 0x02:
				res = MIDI_SYSTEM_GM1;
				break;

			case 0x03:
				res = MIDI_SYSTEM_GM2;
				break;

			case 0x04:
				res = MIDI_SYSTEM_XG;
				break;

			default:
				res = MIDI_SYSTEM_DEFAULT;
				break;
			}
			break;

		case MasterVolume:
			evt = MIDI_EVENT_MASTERVOL;
			ev = param1 * 128;
			break;

		case MasterKey:
			evt = MIDI_EVENT_MASTER_COARSETUNE;
			ev = param1;
			break;

		case MasterPan:
			evt = MIDI_EVENT_PAN;
			ev = param1;
			chan = 255;
			break;

		case RolandReverbMacro:
			evt = MIDI_EVENT_REVERB_MACRO;
			ev = param1;
			break;

		case RolandReverbTime:
			evt = MIDI_EVENT_REVERB_TIME;
			ev = param1;
			break;

		case RolandReverbLevel:
			evt = MIDI_EVENT_REVERB_LEVEL;
			ev = param1;
			break;

		case RolandReverbLoCutOff:
			evt = MIDI_EVENT_REVERB_LOCUTOFF;
			ev = 200 * (param1 + 1);
			break;

		case RolandReverbHiCutOff:
			evt = MIDI_EVENT_REVERB_HICUTOFF;
			ev = 3000 * (param1 + 1);
			break;

		case RolandChorusMacro:
			evt = MIDI_EVENT_CHORUS_MACRO;
			ev = param1;
			break;

		case RolandChorusDelay:
			evt = MIDI_EVENT_CHORUS_DELAY;
			ev = param1;
			break;

		case RolandChorusDepth:
			evt = MIDI_EVENT_CHORUS_DEPTH;
			ev = param1;
			break;

		case RolandChorusReverb:
			evt = MIDI_EVENT_CHORUS_REVERB;
			ev = param1;
			break;

		case RolandChorusFeedback:
			evt = MIDI_EVENT_CHORUS_FEEDBACK;
			ev = param1;
			break;

		default:
			evt = MIDI_EVENT_RAW;
			break;
		}
	}

	if (Settings->ExperimentalMultiThreaded) {
		switch (evt) {
			case MIDI_EVENT_NOTE:
			case MIDI_EVENT_KEYPRES:
				BASS_MIDI_StreamEvent(AudioStreams[noteOnTgt], chan, evt, ev);
				break;

			case MIDI_EVENT_SYSTEMEX:
				for (size_t i = 0; i < Settings->ExperimentalAudioMultiplier; i += Settings->ExpMTKeyboardDiv) {
					for (int j = 0; j < Settings->KeyboardChunk; j++) {
						BASS_MIDI_StreamEvent(AudioStreams[i + j], 0, evt | ExtraEvtFlags, res);
					}
				}
				break;

			case MIDI_EVENT_RAW:
				for (auto i = 0; i < Settings->KeyboardChunk; i++) {
					BASS_MIDI_StreamEvents(AudioStreams[tgtChan + i], BASS_MIDI_EVENTS_RAW | ExtraEvtFlags, &evtDword, len);
				}
				break;

			default:
				for (unsigned char i = 0; i < Settings->KeyboardChunk; i++) {
					BASS_MIDI_StreamEvent(AudioStreams[tgtChan + i], chan, evt, ev);
				}
				break;
		}
	}
	else {
		switch (evt) {
			case MIDI_EVENT_SYSTEMEX:
				BASS_MIDI_StreamEvent(targetStream, 0, evt, res);
				break;

			case MIDI_EVENT_RAW:
				BASS_MIDI_StreamEvents(targetStream, BASS_MIDI_EVENTS_RAW | ExtraEvtFlags, &evtDword, len);
				break;
			
			default:
				BASS_MIDI_StreamEvent(targetStream, chan, evt, ev);
				break;
		}
	}
}

void OmniMIDI::BASSSynth::ProcessEvBufChk() {
	do ProcessEvBuf();
	while (ShortEvents->NewEventsAvailable());
}

#if defined(_WIN32)
unsigned long CALLBACK OmniMIDI::BASSSynth::AudioProcesser(void* buffer, unsigned long length, BASSSynth* me) {
	auto buf = (unsigned char*)buffer;
	unsigned long len = 0;
	unsigned div = me->Settings->ASIOChunksDivision;
	auto chksize = length / div;

	for (unsigned long j = 0; j < div; j++) {
		len += BASS_ChannelGetData(me->AudioStreams[0], me->ASIOBuf, chksize);
		for (unsigned long k = 0; k < chksize; k++) {
			buf[(j * chksize) + k] = me->ASIOBuf[k];
		}
	}

	return len;
}

unsigned long CALLBACK OmniMIDI::BASSSynth::AudioEvProcesser(void* buffer, unsigned long length, BASSSynth* me) {
	me->ProcessEvBufChk();
	return AudioProcesser(buffer, length, me);
}

unsigned long CALLBACK OmniMIDI::BASSSynth::WasapiProc(void* buffer, unsigned long length, void* user) {
	return AudioProcesser(buffer, length, (BASSSynth*)user);
}

unsigned long CALLBACK OmniMIDI::BASSSynth::WasapiEvProc(void* buffer, unsigned long length, void* user) {
	return AudioEvProcesser(buffer, length, (BASSSynth*)user);
}

unsigned long CALLBACK OmniMIDI::BASSSynth::AsioProc(int, unsigned long, void* buffer, unsigned long length, void* user) {
	return AudioProcesser(buffer, length, (BASSSynth*)user);
}

unsigned long CALLBACK OmniMIDI::BASSSynth::AsioEvProc(int, unsigned long, void* buffer, unsigned long length, void* user) {
	return AudioEvProcesser(buffer, length, (BASSSynth*)user);
}
#endif

bool OmniMIDI::BASSSynth::LoadFuncs() {
	// Load required libs
	if (!BAudLib->LoadLib())
		return false;

	if (!BMidLib->LoadLib())
		return false;
	
	if (!BAudLib->IsSupported(BASS_GetVersion(), TGT_BASS) ||
		!BMidLib->IsSupported(BASS_MIDI_GetVersion(), TGT_BASSMIDI))
		return false;

	if (!BEfxLib->LoadLib()) {
		Error("Failed to load BASS_FX, LoudMax will not be available.", true);
		Settings->LoudMax = false;
	}

	switch (Settings->AudioEngine) {
#ifdef _WIN32
	case WASAPI:
		if (!BWasLib->LoadLib()){
			Error("Failed to load BASSWASAPI, defaulting to internal output.", true);
			Settings->AudioEngine = Internal;
		}
		break;

	case ASIO:
		if (!BAsiLib->LoadLib()){
			Error("Failed to load BASSASIO, defaulting to internal output.", true);
			Settings->AudioEngine = Internal;
		}
		break;
#endif

	default:
		break;
	}

	return true;
}

bool OmniMIDI::BASSSynth::ClearFuncs() {
#if defined(WIN32) && (!defined(_M_ARM) && !defined(_M_ARM64))
	if (!BWasLib->UnloadLib())
		return false;

	if (!BAsiLib->UnloadLib())
		return false;
#endif

	if (!BFlaLib->UnloadLib())
		return false;

	if (!BEfxLib->UnloadLib())
		return false;

	if (!BMidLib->UnloadLib())
		return false;

	if (!BAudLib->UnloadLib())
		return false;

	return true;
}

void OmniMIDI::BASSSynth::AudioThread(unsigned int streamId) {
	int sleepRate = -1;
	unsigned int updRate = 1;

#ifndef _WIN32
	// Linux/macOS don't, so let's do the math ourselves
	sleepRate = (int)(((double)Settings->BufPeriod / (double)Settings->SampleRate) * 10000000.0);
	updRate = 0;
#endif

	switch (Settings->AudioEngine) {
		case Internal:
			break;

		default:
			return;
	}

	while (IsSynthInitialized()) {
		if (Settings->OneThreadMode && !Settings->ExperimentalMultiThreaded)
			ProcessEvBufChk();

		BASS_ChannelUpdate(AudioStreams[streamId], updRate);
		Utils.MicroSleep(sleepRate);
	}
}

void OmniMIDI::BASSSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (AudioStreams[0] == 0)
		Utils.MicroSleep(SLEEPVAL(1));

	Message("_EvtThread spinned up.");

	while (IsSynthInitialized()) {
		ProcessEvBuf();

		if (!ShortEvents->NewEventsAvailable())
			Utils.MicroSleep(SLEEPVAL(1));
	}
}

void OmniMIDI::BASSSynth::BASSThread() {
	unsigned int itv = 0;
	float buf = 0.0f;

	float rtr = 0.0f;
	float tv = 0.0f;

	while (AudioStreams[0] == 0)
		Utils.MicroSleep(SLEEPVAL(1));

	while (IsSynthInitialized()) {
		for (size_t i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams != nullptr && AudioStreams[i] != 0) {
				BASS_ChannelGetAttribute(AudioStreams[i], BASS_ATTRIB_CPU, &buf);
				BASS_ChannelGetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_VOICES_ACTIVE, &tv);

				itv += (unsigned int)tv;

				if (buf > rtr)
					rtr = buf;
			}
		}

		RenderingTime = rtr /* / AudioStreamSize */;
		ActiveVoices = itv;
		itv = 0;
		rtr = 0.0f;

		Utils.MicroSleep(SLEEPVAL(100000));
	}
}

bool OmniMIDI::BASSSynth::LoadSynthModule() {
	auto ptr = (LibImport*)LibImports;

	if (!Settings) {
		Settings = new BASSSettings(ErrLog);
		Settings->LoadSynthConfig();
	}

	if (!BAudLib)
		BAudLib = new Lib("bass", nullptr, ErrLog, &ptr, LibImportsSize);

	if (!BMidLib)
		BMidLib = new Lib("bassmidi", nullptr, ErrLog, &ptr, LibImportsSize);

	if (!BFlaLib)
		BFlaLib = new Lib("bassflac", nullptr, ErrLog);
		
	if (!BEfxLib)
		BEfxLib = new Lib("bass_fx", nullptr, ErrLog, &ptr, LibImportsSize);

#if defined(_WIN32)
	if (!BWasLib)
		BWasLib = new Lib("basswasapi", nullptr, ErrLog, &ptr, LibImportsSize);

	if (!BAsiLib)
		BAsiLib = new Lib("bassasio", nullptr, ErrLog, &ptr, LibImportsSize);
#endif

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (!LoadFuncs()) {
		Error("Something went wrong while importing the libraries' functions!!!", true);
		return false;
	}
	else Message("Libraries loaded.");

	if (!AllocateShortEvBuf(Settings->EvBufSize)) {
		Error("AllocateShortEvBuf failed.", true);
		return false;
	}
	else Message("Short events buffer allocated for %llu events.", Settings->EvBufSize);

	return true;
}

bool OmniMIDI::BASSSynth::UnloadSynthModule() {
	// LOG(SynErr, L"UnloadBASSSynth called.");
	if (ClearFuncs()) {
		FreeShortEvBuf();

		SoundFonts.clear();

		if (Settings) {
			delete Settings;
			Settings = nullptr;
		}

		return true;
	}

	Error("Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
	// If the audio stream is not 0, then stream is allocated already
	if (AudioStreams != nullptr)
		return true;

	// BASS stream flags
	bool bInfoGood = false;
	unsigned int uMinbuf = 10;
	BASS_INFO defInfo = BASS_INFO();

#if defined(_WIN32)
	bool noFreqChange = false;
	double devFreq = 0.0;
	BASS_ASIO_INFO asioInfo = BASS_ASIO_INFO();
	BASS_ASIO_DEVICEINFO devInfo = BASS_ASIO_DEVICEINFO();
	BASS_ASIO_CHANNELINFO chInfo = BASS_ASIO_CHANNELINFO();

	bool asioCheck = false;
	unsigned int asioCount = 0, asioDev = 0;

	double asioFreq = (double)Settings->SampleRate;
	unsigned int leftChID = -1, rightChID = -1;
	const char* lCh = Settings->ASIOLCh.c_str();
	const char* rCh = Settings->ASIORCh.c_str();
	const char* dev = Settings->ASIODevice.c_str();
#endif

	if (Settings->ExperimentalMultiThreaded) {
		if (Settings->AudioEngine != Internal)
			Settings->AudioEngine = Internal;

		AudioStreamSize = Settings->ExperimentalAudioMultiplier;
		Settings->AsyncMode = true;
		EvtThreadsSize = Settings->ExpMTKeyboardDiv;

		Message("Experimental multi BASS stream mode enabled. (CHA %d, CHK %d >> TOT %d)", Settings->ChannelDiv, Settings->ExpMTKeyboardDiv, Settings->ExperimentalAudioMultiplier);
	}
	else
	{
		AudioStreamSize = 1;
		EvtThreadsSize = 1;
	}

	unsigned int deviceFlags = 
		(Settings->MonoRendering ? BASS_DEVICE_MONO : BASS_DEVICE_STEREO);

	unsigned int streamFlags = 
		(Settings->MonoRendering ? BASS_SAMPLE_MONO : 0) |
		(Settings->FloatRendering ? BASS_SAMPLE_FLOAT : 0) |
		(Settings->AsyncMode ? BASS_MIDI_ASYNC : 0) |
		(Settings->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0);

	if (Settings->AudioEngine < Internal || Settings->AudioEngine > BASSENGINE_COUNT)
		Settings->AudioEngine = Internal;

#ifdef _WIN32
	if (!(streamFlags & BASS_SAMPLE_FLOAT) && 
		Settings->AudioEngine == ASIO) {
		Error("The selected engine does not support integer audio.\nIt will render with floating point audio.", false);
		streamFlags |= BASS_SAMPLE_FLOAT;
	}
#endif

	if (Settings->AsyncMode)
		ExtraEvtFlags |= BASS_MIDI_EVENTS_ASYNC;

	AudioStreams = new unsigned int[AudioStreamSize] { 0 };
	_AudThread = new std::jthread[AudioStreamSize];

	switch (Settings->AudioEngine) {
	case Internal:	
#if defined(_WIN32)
		deviceFlags |= BASS_DEVICE_DSOUND;
#endif

		BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, 1);
		if (BASS_Init(-1, Settings->SampleRate, BASS_DEVICE_STEREO, 0, nullptr)) {
			if ((bInfoGood = BASS_GetInfo(&defInfo))) {
				Message("minbuf %d", defInfo.minbuf);
				uMinbuf = defInfo.minbuf;
			}

			BASS_Free();
		}
		else uMinbuf = 10;

		if (Settings->AudioBuf < uMinbuf)
			Settings->AudioBuf = uMinbuf;

		BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);
		BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
		
		if (!BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0))
			Error("BASS_CONFIG_UPDATEPERIOD failed", true);
		if (!BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0))
			Error("BASS_CONFIG_UPDATETHREADS failed", true);

#if !defined(_WIN32)
		// Only Linux and macOS can do this
		BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, Settings->BufPeriod * -1);
#else
		BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, Settings->AudioBuf);
#endif
		BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, bInfoGood ? (Settings->AudioBuf * 2) : 0);

		if (!BASS_Init(-1, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			Error("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}
		else Message("bassDev %d >> devFreq %d", -1, Settings->SampleRate);

		for (size_t i = 0; i < AudioStreamSize; i++) {
			AudioStreams[i] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
			if (AudioStreams[i] == 0) {
				Error("BASS_MIDI_StreamCreate failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			_AudThread[i] = std::jthread(&BASSSynth::AudioThread, this, i);
			if (!_AudThread[i].joinable()) {
				Error("_AudThread[%d] failed. (ID: %x)", true, i, _AudThread[i].get_id());
				return false;
			}
			else Message("_AudThread[%d] running! (ID: %x)", i, _AudThread[i].get_id());

			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_BUFFER, 0);

			if (!BASS_ChannelPlay(AudioStreams[i], false)) {
				Error("BASS_ChannelPlay failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}
		}

		if (AudioStreamSize > 1) Message("Audio streams ready. _AudThreads count: %d", AudioStreamSize);
		else Message("Audio stream ready.");

		break;

#if defined(_WIN32)
	case WASAPI:
	{
		auto proc = Settings->OneThreadMode ? &BASSSynth::WasapiEvProc : &BASSSynth::WasapiProc;
		streamFlags |= BASS_STREAM_DECODE;

		if (!BASS_Init(0, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			Error("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		AudioStreams[0] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
		if (!AudioStreams[0]) {
			Error("BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Init(-1, Settings->SampleRate, Settings->MonoRendering ? 1 : 2, ((Settings->AsyncMode) ? BASS_WASAPI_ASYNC : 0) | BASS_WASAPI_EVENT, Settings->WASAPIBuf / 1000.0f, 0, proc, this)) {
			Error("BASS_WASAPI_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Start()) {
			Error("BASS_WASAPI_Start failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		Message("wasapiDev %d >> devFreq %d", -1, Settings->SampleRate);
		break;
	}

	case ASIO:
	{
		auto proc = Settings->OneThreadMode ? &BASSSynth::AsioEvProc : &BASSSynth::AsioProc;
		streamFlags |= BASS_STREAM_DECODE;

		Message("Available ASIO devices:");
		// Get the amount of ASIO devices available
		for (; BASS_ASIO_GetDeviceInfo(asioCount, &devInfo); asioCount++) {
			Message("%s", devInfo.name);

			// Return the correct ID when found
			if (strcmp(dev, devInfo.name) == 0)
				asioDev = asioCount;
		}

		if (asioCount < 1) {
			Error("No ASIO devices available! Got 0 devices!!!", true);
			return false;
		}
		else Message("Detected %d ASIO devices.", asioCount);

		if (!BASS_ASIO_Init(asioDev, BASS_ASIO_THREAD | BASS_ASIO_JOINORDER)) {
			Error("BASS_ASIO_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (BASS_ASIO_GetInfo(&asioInfo)) {
			Message("ASIO device: %s (CurBuf %d, Min %d, Max %d, Gran %d, OutChans %d)",
				asioInfo.name, asioInfo.bufpref, asioInfo.bufmin, asioInfo.bufmax, asioInfo.bufgran, asioInfo.outputs);
		}

		if (!BASS_ASIO_SetRate(asioFreq)) {
			Message("BASS_ASIO_SetRate failed, falling back to BASS_ASIO_ChannelSetRate... BASSERR: %d", BASS_ErrorGetCode());
			if (!BASS_ASIO_ChannelSetRate(0, 0, asioFreq)) {
				Message("BASS_ASIO_ChannelSetRate failed as well, BASSERR: %d... This ASIO device does not want OmniMIDI to change its output frequency.", BASS_ErrorGetCode());
				noFreqChange = true;
			}
		}

		Message("Available ASIO channels:");
		for (unsigned long curSrcCh = 0; curSrcCh < asioInfo.outputs; curSrcCh++) {
			BASS_ASIO_ChannelGetInfo(0, curSrcCh, &chInfo);

			Message("Checking ASIO dev %s...", chInfo.name);

			// Return the correct ID when found
			if (strcmp(lCh, chInfo.name) == 0) {
				leftChID = curSrcCh;
				Message("%s >> This channel matches what the user requested for the left channel! (ID: %d)", chInfo.name, leftChID);
			}

			if (strcmp(rCh, chInfo.name) == 0) {
				rightChID = curSrcCh;
				Message("%s >> This channel matches what the user requested for the right channel! (ID: %d)",  chInfo.name, rightChID);
			}

			if (leftChID != (unsigned int)-1 && rightChID != (unsigned int)-1)
				break;
		}

		if (leftChID == (unsigned int)-1 && rightChID == (unsigned int)-1) {
			leftChID = 0;
			rightChID = 1;
			Message("No ASIO output channels found, defaulting to CH%d for left and CH%d for right.", leftChID, rightChID);
		}

		if (noFreqChange) {
			devFreq = BASS_ASIO_GetRate();
			Message("BASS_ASIO_GetRate >> %dHz", devFreq);
			if (devFreq != -1) {
				asioFreq = devFreq;
			}
			else {
				Error("BASS_ASIO_GetRate failed to get the device's frequency, with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}
		}

		if (!asioFreq) {
			asioFreq = Settings->SampleRate;
			Message("Nothing set the frequency for the ASIO device! Falling back to the value from the settings... (%dHz)", (int)asioFreq);
		}

		if (!BASS_Init(0, (unsigned long)asioFreq, deviceFlags, 0, nullptr)) {
			Error("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}
		else Message("BASS_Init returned true. (freq: %d, dFlags: %d)", (int)asioFreq, deviceFlags);

		AudioStreams[0] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
		if (!AudioStreams[0]) {
			Error("BASS_MIDI_StreamCreate[0] w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!noFreqChange) BASS_ASIO_SetRate(asioFreq);

		if (Settings->StreamDirectFeed) asioCheck = BASS_ASIO_ChannelEnableBASS(0, leftChID, AudioStreams[0], 0);
		else asioCheck = BASS_ASIO_ChannelEnable(0, leftChID, proc, this);

		if (asioCheck) {
			if (Settings->StreamDirectFeed) Message("ASIO direct feed enabled.");
			else Message("AsioProc allocated.");

			Message("Channel %d set to %dHz and enabled.", leftChID, (int)asioFreq);

			if (Settings->MonoRendering) asioCheck = BASS_ASIO_ChannelEnableMirror(rightChID, 0, leftChID);
			else asioCheck = BASS_ASIO_ChannelJoin(0, rightChID, leftChID);

			if (asioCheck) {
				if (Settings->MonoRendering) Message("Channel %d mirrored to %d for mono rendering. (F%d)", leftChID, rightChID, Settings->MonoRendering);
				else Message("Channel %d joined to %d for stereo rendering. (F%d)", rightChID, leftChID, Settings->MonoRendering);

				if (!BASS_ASIO_Start(0, 0))
					Error("BASS_ASIO_Start failed with error 0x%x. If the code is zero, that means the device encountered an error and aborted the initialization process.", true, BASS_ErrorGetCode());

				else {
					BASS_ASIO_ChannelSetFormat(0, leftChID, BASS_ASIO_FORMAT_FLOAT | BASS_ASIO_FORMAT_DITHER);
					BASS_ASIO_ChannelSetRate(0, leftChID, asioFreq);

					Message("asioDev %d >> chL %d, chR %d - asioFreq %d", asioDev, leftChID, rightChID, (int)asioFreq);
					break;
				}
			}
		}

		return false;
	}
#endif

	case Invalid:
	default:
		Error("Engine ID \"%d\" is not valid!", false, Settings->AudioEngine);
		return false;
	}

	if (Settings->FloatRendering && Settings->LoudMax) {
		BASS_BFX_COMPRESSOR2 compressor;

		for (size_t i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
				audioLimiter = BASS_ChannelSetFX(AudioStreams[i], BASS_FX_BFX_COMPRESSOR2, 0);
			}
		}

		BASS_FXGetParameters(audioLimiter, &compressor);
		BASS_FXSetParameters(audioLimiter, &compressor);
		Message("LoudMax enabled.");
	}

	char* tmpUtils = new char[MAX_PATH_LONG] { 0 };
	if (BFlaLib->GetLibPath(tmpUtils)) {
#if defined(_WIN32)
		wchar_t* szPath = Utils.GetUTF16(tmpUtils);
		if (szPath != nullptr) {
			BFlaLibHandle = BASS_PluginLoad(szPath, BASS_UNICODE);
			delete[] szPath;
		}
#else
		BFlaLibHandle = BASS_PluginLoad(tmpUtils, 0);
#endif
	}
	delete[] tmpUtils;

	if (BFlaLibHandle) Message("BASSFLAC loaded. BFlaLib --> 0x%08x", BFlaLibHandle);		
	else Message("No BASSFLAC, this could affect playback with FLAC based soundbanks.", BFlaLibHandle);

	for (size_t i = 0; i < AudioStreamSize; i++) {
		if (AudioStreams[i] != 0) {
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_SRC, 0);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_KILL, 1);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_VOICES, (float)Settings->VoiceLimit);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_CPU, (float)Settings->RenderTimeLimit);
			
			if (Settings->AsyncMode) {
				auto fsize = Settings->EvBufSize * 4;
				if (BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_EVENTBUF_ASYNC, fsize)) {
					Message("AudioStream[%d] >> Async buffer enabled for %llu events.", fsize);
				}
				else Error("AudioStream[%d] >> Failed to set async buffer size!", true, fsize);
			}
		}
	}
	Message("Stream settings loaded.");

	if (!Settings->OneThreadMode || Settings->ExperimentalMultiThreaded) {
		_EvtThread = std::jthread(&BASSSynth::EventsThread, this);
		if (!_EvtThread.joinable()) {
			Error("_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
			return false;
		}
		else Message("_EvtThread running! (ID: %x)", _EvtThread.get_id());
	}

	LoadSoundFonts();
	SFSystem.RegisterCallback(this);

	_BASThread = std::jthread(&BASSSynth::BASSThread, this);
	if (!_BASThread.joinable()) {
		Error("_BASThread failed. (ID: %x)", true, _BASThread.get_id());
		return false;
	}
	else Message("_BASThread running! (ID: %x)", _BASThread.get_id());

	if (Settings->IsDebugMode()) {
		_LogThread = std::jthread(&SynthModule::LogFunc, this);
		if (!_LogThread.joinable()) {
			Error("_LogThread failed. (ID: %x)", true, _LogThread.get_id());
			return false;
		}
		else Message("_LogThread running! (ID: %x)", _LogThread.get_id());

		Settings->OpenConsole();
	}

	if (Settings->ExperimentalMultiThreaded) {
		Message("Experimental mode is enabled. Preloading samples to avoid crashes...");
		for (int i = 0; i < 128; i++) {
			for (int j = 127; j < 128; j++) {
				if (i < 0) i = 0;

				PlayShortEvent(NoteOn, i, j);
				PlayShortEvent(NoteOff, i, 0);

				Utils.MicroSleep(SLEEPVAL(100000));
			}
		}

		Message("Samples are ready.");
	}

	PlayShortEvent(SystemReset, 0x00, 0x00);
	PlayShortEvent(PitchBend, 0x00, 0x40);
	ShortEvents->ResetHeads();

	Message("BASSMIDI ready.");
	return true;
}

void OmniMIDI::BASSSynth::LoadSoundFonts() {
	if (SoundFonts.size() > 0)
	{
		for (size_t i = 0; i < SoundFonts.size(); i++)
		{
			if (SoundFonts[i].font)
				BASS_MIDI_FontFree(SoundFonts[i].font);
		}
	}
	SoundFonts.clear();

	if (SFSystem.ClearList()) {
		SoundFontsVector = SFSystem.LoadList();

		if (SoundFontsVector != nullptr) {
			auto& dSFv = *SoundFontsVector;

			if (dSFv.size() > 0) {
				for (size_t i = 0; i < dSFv.size(); i++) {
					const char* sfPath = dSFv[i].path.c_str();

					if (!dSFv[i].enabled)
						continue;

					unsigned int bmfiflags = 0;
					BASS_MIDI_FONTEX sf = BASS_MIDI_FONTEX();

					sf.spreset = dSFv[i].spreset;
					sf.sbank = dSFv[i].sbank;
					sf.dpreset = dSFv[i].dpreset;
					sf.dbank = dSFv[i].dbank;
					sf.dbanklsb = dSFv[i].dbanklsb;

					bmfiflags |= dSFv[i].xgdrums ? BASS_MIDI_FONT_XGDRUMS : 0;
					bmfiflags |= dSFv[i].linattmod ? BASS_MIDI_FONT_LINATTMOD : 0;
					bmfiflags |= dSFv[i].lindecvol ? BASS_MIDI_FONT_LINDECVOL : 0;
					bmfiflags |= dSFv[i].minfx ? BASS_MIDI_FONT_MINFX : 0;
					bmfiflags |= dSFv[i].nolimits ? BASS_MIDI_FONT_SBLIMITS : BASS_MIDI_FONT_NOSBLIMITS;
					bmfiflags |= dSFv[i].norampin ? BASS_MIDI_FONT_NORAMPIN : 0;

#ifdef _WIN32
					wchar_t* szPath = Utils.GetUTF16((char*)sfPath);
					if (szPath != nullptr) {
						sf.font = BASS_MIDI_FontInit(szPath, bmfiflags | BASS_UNICODE);
						delete[] szPath;
					}
#else
					sf.font = BASS_MIDI_FontInit(sfPath, bmfiflags);
#endif	

					if (sf.font != 0) {
						// Check if the soundfont loads, if it does then it's valid
						if (BASS_MIDI_FontLoadEx(sf.font, sf.spreset, sf.sbank, 0, 0)) {
							SoundFonts.push_back(sf);
							Message("\"%s\" loaded!", sfPath);
						}


						else Error("Error 0x%x occurred while loading \"%s\"!", false, BASS_ErrorGetCode(), sfPath);
					}
					else Error("Error 0x%x occurred while initializing \"%s\"!", false, BASS_ErrorGetCode(), sfPath);
				}

				for (size_t i = 0; i < AudioStreamSize; i++) {
					if (AudioStreams[i] != 0) {
						if (!BASS_MIDI_StreamSetFonts(AudioStreams[i], &SoundFonts[0], (unsigned int)SoundFonts.size() | BASS_MIDI_FONT_EX))
							Error("BASS_MIDI_StreamSetFonts encountered error 0x%x!", false, BASS_ErrorGetCode());
					}
				}
			}
		}
	}
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
	SFSystem.ClearList();
	SFSystem.RegisterCallback();

	if (AudioStreams != nullptr) {
		for (size_t i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
#ifdef _DEBUG
				Message("Freeing BASS stream %x...", AudioStreams[i]);
#endif
				BASS_StreamFree(AudioStreams[i]);
				AudioStreams[i] = 0;
			}		
		}
		Message("Streams wiped.");
	}
	
	if (_EvtThread.joinable()) {
		_EvtThread.join();
		Message("_EvtThread freed.");
	}

	if (_AudThread != nullptr) {
		for (size_t i = 0; i < AudioStreamSize; i++) {
			if (_AudThread[i].joinable()) {
#ifdef _DEBUG
				Message("Freeing _AudThread[%d]...", i);
#endif
				_AudThread[i].join();
			}
		}
		Message("_AudThreads wiped.");
	}

	if (_LogThread.joinable()) {
		_LogThread.join();
		Message("_LogThread freed.");
	}

	if (_BASThread.joinable()) {
		_BASThread.join();
		Message("_BASThread freed.");
	}

	if (Settings->IsDebugMode() && Settings->IsOwnConsole())
		Settings->CloseConsole();

	switch (Settings->AudioEngine) {
#if defined(_WIN32)
	case WASAPI:
		BASS_WASAPI_Stop(true);
		BASS_WASAPI_Free();

		// why do I have to do it myself
		BASS_WASAPI_SetNotify(nullptr, nullptr);

		Message("BASSWASAPI freed.");
		break;

	case ASIO:
		BASS_ASIO_Stop();
		BASS_ASIO_Free();
		Utils.MicroSleep(-5000000);
		Message("BASSASIO freed.");
		break;
#endif

	case Internal:
	default:
		break;
	}

	if (BFlaLibHandle) {
		BASS_PluginFree(BFlaLibHandle);
		Message("BASSFLAC freed.");
	}

	BASS_Free();
	Message("BASS freed.");

	if (AudioStreams != nullptr) {
		delete[] AudioStreams;
		AudioStreams = nullptr;
		Message("Audio streams freed.");
	}

	if (_AudThread != nullptr) {
		delete[] _AudThread;
		_AudThread = nullptr;
		Message("Audio threads freed.");
	}

	return true;
}

bool OmniMIDI::BASSSynth::SettingsManager(unsigned int setting, bool get, void* var, size_t size) {
	switch (setting) {

	case KDMAPI_MANAGE:
	{
		if (Settings || IsSynthInitialized()) {
			Error("You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
			return false;
		}

		Message("KDMAPI REQUEST: The MIDI app wants to manage the settings.");
		Settings = new BASSSettings(ErrLog);

		break;
	}

	case KDMAPI_LEAVE:
	{
		if (Settings) {
			if (IsSynthInitialized()) {
				Error("You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
				return false;
			}

			Message("KDMAPI REQUEST: The MIDI app does not want to manage the settings anymore.");
			delete Settings;
			Settings = nullptr;
		}
		break;
	}

	case WINMMWRP_ON:
	case WINMMWRP_OFF:
		// Old WinMMWRP code, ignore
		break;

	SettingsManagerCase(KDMAPI_AUDIOFREQ, get, unsigned int, Settings->SampleRate, var, size);
	SettingsManagerCase(KDMAPI_CURRENTENGINE, get, unsigned int, Settings->AudioEngine, var, size);
	SettingsManagerCase(KDMAPI_MAXVOICES, get, unsigned int, Settings->VoiceLimit, var, size);
	SettingsManagerCase(KDMAPI_MAXRENDERINGTIME, get, unsigned int, Settings->RenderTimeLimit, var, size);

	default:
		Message("Unknown setting passed to SettingsManager. (VAL: 0x%x)", setting);
		return false;
	}

	return true;
}

unsigned int OmniMIDI::BASSSynth::PlayLongEvent(char* ev, unsigned int size) {
	if (!BMidLib || !BMidLib->IsOnline())
		return 0;

	// The size has to be more than 1B!
	if (size < 1)
		return 0;

	return UPlayLongEvent(ev, size);
}

unsigned int OmniMIDI::BASSSynth::UPlayLongEvent(char* ev, unsigned int size) {
	unsigned int data = 0;

	if (Settings->ExperimentalMultiThreaded)
		return size;

	for (size_t si = 0; si < AudioStreamSize; si++) {
		unsigned int tmp = BASS_MIDI_StreamEvents(AudioStreams[si], BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_ASYNC | BASS_MIDI_EVENTS_NORSTATUS, ev, size);
		if (tmp == (unsigned int)-1)
			break;

		data += tmp;
	}

	return data;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) {
	if (!BMidLib || !BMidLib->IsOnline())
		return LibrariesOffline;

	return BASS_MIDI_StreamEvent(Settings->ExperimentalMultiThreaded ? AudioStreams[chan + (param % 16)] : AudioStreams[0], chan, evt, param) ? Ok : InvalidParameter;
}

#endif