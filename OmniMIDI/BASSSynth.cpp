#include "BASSSynth.hpp"

bool OmniMIDI::BASSSynth::ProcessEvBuf() {
	if (!AudioStreams[0])
		return false;

	auto tev = ShortEvents->Read();

	if (tev == 0)
		return false;

	unsigned int evt = MIDI_SYSTEM_DEFAULT;
	unsigned int ev = 0;
	unsigned char status = GetStatus(tev);
	unsigned char command = GetCommand(status);
	unsigned char chan = GetChannel(status);
	unsigned char param1 = GetFirstParam(tev);
	unsigned char param2 = GetSecondParam(tev);

	unsigned char len = 3;

	HSTREAM targetStream = AudioStreams[Settings->ExperimentalMultiThreaded ? chan : 0];

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
		// LOG(SynErr, "Aftertouch %u (%X, %X - %X)", ev, ev, param1, param2);
		break;
	case PatchChange:
		evt = MIDI_EVENT_PROGRAM;
		ev = param1;
		// LOG(SynErr, "PatchChange %u (%X)", ev, param1);
		break;
	case ChannelPressure:
		evt = MIDI_EVENT_CHANPRES;
		ev = param1;
		// LOG(SynErr, "ChannelPressure %u (%X)", ev, param1);
		break;
	case PitchBend:
		evt = MIDI_EVENT_PITCH;
		ev = param2 << 7 | param1;
		// LOG(SynErr, "PitchBend %u (%X, %X - %X)", ev, ev, param1, param2);
		break;
	default:
		switch (status) {
		// Let's go!
		case SystemReset:
			// This is 0xFF, which is a system reset.
			switch (param1) {
			case 0x01:
				evt = MIDI_SYSTEM_GS;
				break;

			case 0x02:
				evt = MIDI_SYSTEM_GM1;
				break;

			case 0x03:
				evt = MIDI_SYSTEM_GM2;
				break;

			case 0x04:
				evt = MIDI_SYSTEM_XG;
				break;

			default:
				evt = MIDI_SYSTEM_DEFAULT;
				break;
			}

			return BASS_MIDI_StreamEvent(targetStream, 0, MIDI_EVENT_SYSTEMEX, evt);

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
			if (!((tev - 0xC0) & 0xE0)) len = 2;
			BASS_MIDI_StreamEvents(targetStream, BASS_MIDI_EVENTS_RAW, &tev, len);
			return true;			
		}
	}

	if (chan == 255) {
		for (int i = 0; i < 16; i++) 
			BASS_MIDI_StreamEvent(targetStream, i, evt, ev);

		return true;
	}

	return BASS_MIDI_StreamEvent(targetStream, chan, evt, ev) ? true : false;
}

void OmniMIDI::BASSSynth::ProcessEvBufChk() {
	do ProcessEvBuf();
	while (ShortEvents->NewEventsAvailable());
}

unsigned long CALLBACK OmniMIDI::BASSSynth::AudioProcesser(void* buffer, unsigned long length, BASSSynth* me) {
	auto buf = (unsigned char*)buffer;
	unsigned long len = 0;
	unsigned div = me->Settings->ASIOChunksDivision;
	auto chksize = length / div;

	for (int j = 0; j < div; j++) {
		len += BASS_ChannelGetData(me->AudioStreams[0], me->ASIOBuf, chksize);
		for (int k = 0; k < chksize; k++) {
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

#if defined(WIN32) && (!defined(_M_ARM) && !defined(_M_ARM64))
	if (Settings->LoudMax)
	{
		if (!BVstLib->LoadLib())
			return false;
	}
#endif

	return true;
}

bool OmniMIDI::BASSSynth::ClearFuncs() {
	if (!BAsiLib->UnloadLib())
		return false;

	if (!BWasLib->UnloadLib())
		return false;

	if (!BMidLib->UnloadLib())
		return false;

	if (!BAudLib->UnloadLib())
		return false;

#if defined(WIN32) && (!defined(_M_ARM) && !defined(_M_ARM64))
	if (!BVstLib->UnloadLib())
		return false;
#endif

	return true;
}

void OmniMIDI::BASSSynth::AudioThread(unsigned int id) {
	switch (Settings->AudioEngine) {
	case Internal:
		while (IsSynthInitialized()) {
			if (Settings->OneThreadMode && !Settings->ExperimentalMultiThreaded)
				ProcessEvBufChk();

			BASS_ChannelUpdate(AudioStreams[id], 0);
			MiscFuncs.uSleep(-1);
		}
		break;

	case XAudio2:
	{	
		unsigned div = Settings->XAChunksDivision;
		size_t arrsize = Settings->XASamplesPerFrame;
		size_t chksize = arrsize / div;

		size_t count = 128;
		auto buf = new float*[count] { 0 };
		auto cBuf = new float[chksize] { 0 };
		char pos = 0;

		for (int i = 0; i != count; i++)
			buf[i] = new float[arrsize];

		while (IsSynthInitialized()) {
			for (int i = 0; i < div; i++) {
				if (Settings->OneThreadMode)
					ProcessEvBufChk();

				BASS_ChannelGetData(AudioStreams[id], cBuf, BASS_DATA_FLOAT | (chksize * sizeof(float)));
				for (int k = 0; k < chksize; k++) {
					buf[pos][(i * chksize) + k] = cBuf[k];
				}
			}

			WinAudioEngine[id]->Update(buf[pos], arrsize);

			pos++;
			if (pos >= count)
				pos = 0;
		}

		while (WinAudioEngine[id]);

		for (int i = 0; i != count; i++)
			delete[] buf[i];

		delete[] buf;
		delete[] cBuf;
		break;
	}

	default:
		break;
	}
}

void OmniMIDI::BASSSynth::EventsThread() {
	// Spin while waiting for the stream to go online
	while (AudioStreams[0] == 0)
		MiscFuncs.uSleep(-1);

	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			MiscFuncs.uSleep(0);
	}
}

void OmniMIDI::BASSSynth::BASSThread() {
	unsigned int itv = 0;
	float buf = 0.0f;

	float rtr = 0.0f;
	float tv = 0.0f;

	while (AudioStreams[0] == 0)
		MiscFuncs.uSleep(-1);

	while (IsSynthInitialized()) {
		for (int i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
				BASS_ChannelGetAttribute(AudioStreams[i], BASS_ATTRIB_CPU, &buf);
				BASS_ChannelGetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_VOICES_ACTIVE, &tv);

				itv += (unsigned int)tv;
				rtr += buf;
			}
		}

		RenderingTime = rtr / AudioStreamSize;
		ActiveVoices = itv;
		itv = 0;
		rtr = 0.0f;

		if (IsSynthInitialized() && SFSystem.ListModified()) {
			LoadSoundFonts();
		}

		MiscFuncs.uSleep(-10000);
	}
}

bool OmniMIDI::BASSSynth::LoadSynthModule() {
	auto ptr = (LibImport*)LibImports;

	if (!Settings) {
		Settings = new BASSSettings;
		Settings->LoadSynthConfig();
	}

	if (!BAudLib)
		BAudLib = new Lib(L"BASS", &ptr, LibImportsSize);

	if (!BMidLib)
		BMidLib = new Lib(L"BASSMIDI", &ptr, LibImportsSize);

	if (!BWasLib)
		BWasLib = new Lib(L"BASSWASAPI", &ptr, LibImportsSize);

	if (!BAsiLib)
		BAsiLib = new Lib(L"BASSASIO", &ptr, LibImportsSize);

	if (!BVstLib)
		BVstLib = new Lib(L"BASS_VST", &ptr, LibImportsSize);

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (!LoadFuncs()) {
		NERROR(SynErr, "Something went wrong here!!!", true);
		return false;
	}

	if (!AllocateShortEvBuf(Settings->EvBufSize)) {
		NERROR(SynErr, "AllocateShortEvBuf failed.", true);
		return false;
	}

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

	NERROR(SynErr, "Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
	// SF path
	OMShared::SysPath Utils;
	wchar_t OMPath[MAX_PATH] = { 0 };

	// BASS stream flags
	bool bInfoGood = false;
	bool noFreqChange = false;
	double devFreq = 0.0;
	double asioFreq = (double)Settings->SampleRate;
	unsigned int leftChID = -1, rightChID = -1;
	const char* lCh = Settings->ASIOLCh.c_str();
	const char* rCh = Settings->ASIORCh.c_str();
	const char* dev = Settings->ASIODevice.c_str();
	BASS_INFO defInfo = BASS_INFO();
	BASS_ASIO_INFO asioInfo = BASS_ASIO_INFO();
	BASS_ASIO_DEVICEINFO devInfo = BASS_ASIO_DEVICEINFO();
	BASS_ASIO_CHANNELINFO chInfo = BASS_ASIO_CHANNELINFO();

	bool asioCheck = false;
	unsigned int asioCount = 0, asioDev = 0;

	unsigned int deviceFlags = 
		(Settings->MonoRendering ? BASS_DEVICE_MONO : BASS_DEVICE_STEREO);

	unsigned int streamFlags = 
		(Settings->MonoRendering ? BASS_SAMPLE_MONO : 0) |
		(Settings->FloatRendering ? BASS_SAMPLE_FLOAT : 0) |
		(Settings->AsyncMode ? BASS_MIDI_ASYNC : 0) |
		(Settings->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0);

	// If the audio stream is not 0, then stream is allocated already
	if (AudioStreams[0])
		return true;

	if (!(streamFlags & BASS_SAMPLE_FLOAT) && (Settings->AudioEngine == XAudio2 || Settings->AudioEngine == ASIO)) {
		NERROR(SynErr, "The selected engine does not support integer audio.\nIt will render with floating point audio.", false);
		streamFlags |= BASS_SAMPLE_FLOAT;
	}

	if (Settings->ExperimentalMultiThreaded) {
		LOG(SynErr, "Experimental multi BASS stream mode enabled.");

		AudioStreamSize = sizeof(AudioStreams) / sizeof(unsigned int);
	}
	else AudioStreamSize = 1;

	switch (Settings->AudioEngine) {
	case Internal:
		deviceFlags |= BASS_DEVICE_DSOUND;

		BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, 1);
		if (BASS_Init(-1, Settings->SampleRate, BASS_DEVICE_STEREO, 0, nullptr)) {
			if ((bInfoGood = BASS_GetInfo(&defInfo)))
				LOG(SynErr, "minbuf %d", defInfo.minbuf);

			BASS_Free();
		}

		BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);
		BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

		BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, bInfoGood ? defInfo.minbuf : 0);
		BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, bInfoGood ? (defInfo.minbuf * 2) : 0);

		if (!BASS_Init(-1, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		for (int i = 0; i < AudioStreamSize; i++) {
			AudioStreams[i] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
			if (AudioStreams[i] == 0) {
				NERROR(SynErr, "BASS_MIDI_StreamCreate failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_BUFFER, 0);

			if (!BASS_ChannelPlay(AudioStreams[i], false)) {
				NERROR(SynErr, "BASS_ChannelPlay failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			_AudThread[i] = std::jthread(&BASSSynth::AudioThread, this, i);
			if (!_AudThread[i].joinable()) {
				NERROR(SynErr, "_AudThread failed. (ID: %x)", true, _AudThread[i].get_id());
				return false;
			}
		}

		LOG(SynErr, "bassDev %d >> devFreq %d", -1, Settings->SampleRate);
		break;

	case WASAPI:
	{
		if (Settings->ExperimentalMultiThreaded) {
			NERROR(SynErr, "Experimental multi-threaded rendering is not supported by WASAPI.", true);
			return false;
		}

		auto proc = Settings->OneThreadMode ? &BASSSynth::WasapiEvProc : &BASSSynth::WasapiProc;

		streamFlags |= BASS_STREAM_DECODE;

		if (!BASS_Init(0, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		AudioStreams[0] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
		if (!AudioStreams[0]) {
			NERROR(SynErr, "BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Init(-1, Settings->SampleRate, Settings->MonoRendering ? 1 : 2, ((Settings->AsyncMode) ? BASS_WASAPI_ASYNC : 0) | BASS_WASAPI_EVENT, Settings->WASAPIBuf / 1000.0f, 0, proc, this)) {
			NERROR(SynErr, "BASS_WASAPI_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Start()) {
			NERROR(SynErr, "BASS_WASAPI_Start failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		LOG(SynErr, "wasapiDev %d >> devFreq %d", -1, Settings->SampleRate);
		break;
	}

#ifdef _WIN32
	case XAudio2:
	{
		SOAudioFlags xaFlags = (Settings->MonoRendering ? SOAudioFlags::MonoAudio : SOAudioFlags::StereoAudio) | SOAudioFlags::FloatData;
		streamFlags |= BASS_STREAM_DECODE;

		if (!BASS_Init(0, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		for (int i = 0; i < AudioStreamSize; i++) {
			WinAudioEngine[i] = CreateXAudio2Output();

			auto xar = WinAudioEngine[i]->Init(m_hModule, xaFlags, Settings->SampleRate, Settings->XASamplesPerFrame, Settings->XAChunksDivision);
			if (xar != SoundOutResult::Success) {
				NERROR(SynErr, "WinAudioEngine[%d]->Init failed. %u", true, i, xar);
				return false;
			}
			LOG(SynErr, "WinAudioEngine[%d]->Init succeeded.", i);

			AudioStreams[i] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
			if (!AudioStreams[i]) {
				NERROR(SynErr, "BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			_AudThread[i] = std::jthread(&BASSSynth::AudioThread, this, i);
			if (!_AudThread[i].joinable()) {
				NERROR(SynErr, "_AudThread[i] failed. (ID: %x)", true, i, _AudThread[i].get_id());
				return false;
			}
		}


		break;
	}
#endif

	case ASIO:
	{
		if (Settings->ExperimentalMultiThreaded) {
			NERROR(SynErr, "Experimental multi-threaded rendering is not supported by ASIO.", true);
			return false;
		}

		auto proc = Settings->OneThreadMode ? &BASSSynth::AsioEvProc : &BASSSynth::AsioProc;
		streamFlags |= BASS_STREAM_DECODE;

		LOG(SynErr, "Available ASIO devices:");
		// Get the amount of ASIO devices available
		for (; BASS_ASIO_GetDeviceInfo(asioCount, &devInfo); asioCount++) {
			LOG(SynErr, "%s", devInfo.name);

			// Return the correct ID when found
			if (strcmp(dev, devInfo.name) == 0)
				asioDev = asioCount;
		}

		if (asioCount < 1) {
			NERROR(SynErr, "No ASIO devices available! Got 0 devices!!!", true);
			return false;
		}
		else LOG(SynErr, "Detected %d ASIO devices.", asioCount);

		if (!BASS_ASIO_Init(asioDev, BASS_ASIO_THREAD | BASS_ASIO_JOINORDER)) {
			NERROR(SynErr, "BASS_ASIO_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (BASS_ASIO_GetInfo(&asioInfo)) {
			LOG(SynErr, "ASIO device: %s (CurBuf %d, Min %d, Max %d, Gran %d, OutChans %d)",
				asioInfo.name, asioInfo.bufpref, asioInfo.bufmin, asioInfo.bufmax, asioInfo.bufgran, asioInfo.outputs);
		}

		if (!BASS_ASIO_SetRate(asioFreq)) {
			LOG(SynErr, "BASS_ASIO_SetRate failed, falling back to BASS_ASIO_ChannelSetRate... BASSERR: %d", BASS_ErrorGetCode());
			if (!BASS_ASIO_ChannelSetRate(0, 0, asioFreq)) {
				LOG(SynErr, "BASS_ASIO_ChannelSetRate failed as well, BASSERR: %d... This ASIO device does not want OmniMIDI to change its output frequency.", BASS_ErrorGetCode());
				noFreqChange = true;
			}
		}

		LOG(SynErr, "Available ASIO channels:");
		for (int curSrcCh = 0; curSrcCh < asioInfo.outputs; curSrcCh++) {
			BASS_ASIO_ChannelGetInfo(0, curSrcCh, &chInfo);

			if ((leftChID && rightChID) && (leftChID != -1 && rightChID != -1))
				break;

			LOG(SynErr, "%s", chInfo.name);

			// Return the correct ID when found
			if (strcmp(lCh, chInfo.name) == 0) {
				leftChID = curSrcCh;
				LOG(SynErr, "^^ This channel matches what the user requested for the left channel! (ID: %d)", leftChID);
			}
			else if (strcmp(rCh, chInfo.name) == 0) {
				rightChID = curSrcCh;
				LOG(SynErr, "^^ This channel matches what the user requested for the right channel! (ID: %d)", rightChID);
			}
		}

		if (leftChID == -1 && rightChID == -1) {
			leftChID = 0;
			rightChID = 1;
			LOG(SynErr, "No ASIO output channels found, defaulting to CH0 for left and CH1 for right.", leftChID, chInfo.name);
		}

		if (noFreqChange) {
			devFreq = BASS_ASIO_GetRate();
			LOG(SynErr, "BASS_ASIO_GetRate = %dHz", devFreq);
			if (devFreq != -1) {
				asioFreq = devFreq;
			}
			else {
				NERROR(SynErr, "BASS_ASIO_GetRate failed to get the device's frequency, with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}
		}

		if (!asioFreq) {
			asioFreq = Settings->SampleRate;
			LOG(SynErr, "Nothing set the frequency for the ASIO device! Falling back to the value from the settings... (%dHz)", (int)asioFreq);
		}

		if (!BASS_Init(0, asioFreq, deviceFlags, 0, nullptr)) {
			NERROR(SynErr, "BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}
		else LOG(SynErr, "BASS_Init returned true. (freq: %d, dFlags: %d)", (int)asioFreq, deviceFlags);

		AudioStreamSize = 1;
		for (int i = 0; i < AudioStreamSize; i++) {
			AudioStreams[i] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
			if (!AudioStreams[i]) {
				NERROR(SynErr, "BASS_MIDI_StreamCreate[%d] w/ BASS_STREAM_DECODE failed with error 0x%x.", true, i, BASS_ErrorGetCode());
				return false;
			}
		}

		if (!noFreqChange) BASS_ASIO_SetRate(asioFreq);

		if (Settings->StreamDirectFeed) asioCheck = BASS_ASIO_ChannelEnableBASS(0, leftChID, AudioStreams[0], 0);
		else asioCheck = BASS_ASIO_ChannelEnable(0, leftChID, proc, this);

		if (asioCheck) {
			if (Settings->StreamDirectFeed) LOG(SynErr, "ASIO direct feed enabled.");
			else LOG(SynErr, "AsioProc allocated.");

			LOG(SynErr, "Channel %d set to %dHz and enabled.", leftChID, (int)asioFreq);

			if (Settings->MonoRendering) asioCheck = BASS_ASIO_ChannelEnableMirror(rightChID, 0, leftChID);
			else asioCheck = BASS_ASIO_ChannelJoin(0, rightChID, leftChID);

			if (asioCheck) {
				if (Settings->MonoRendering) LOG(SynErr, "Channel %d mirrored to %d for mono rendering. (F%d)", leftChID, rightChID, Settings->MonoRendering);
				else LOG(SynErr, "Channel %d joined to %d for stereo rendering. (F%d)", rightChID, leftChID, Settings->MonoRendering);

				if (!BASS_ASIO_Start(0, 0))
					NERROR(SynErr, "BASS_ASIO_Start failed with error 0x%x. If the code is zero, that means the device encountered an error and aborted the initialization process.", true, BASS_ErrorGetCode());

				else {
					BASS_ASIO_ChannelSetFormat(0, leftChID, BASS_ASIO_FORMAT_FLOAT | BASS_ASIO_FORMAT_DITHER);
					BASS_ASIO_ChannelSetRate(0, leftChID, asioFreq);

					LOG(SynErr, "asioDev %d >> chL %d, chR %d - asioFreq %d", asioDev, leftChID, rightChID, (int)asioFreq);
					break;
				}
			}
		}

		return false;
	}

	case Invalid:
	default:
		NERROR(SynErr, "Engine ID \"%d\" is not valid!", false, Settings->AudioEngine);
		return false;
	}

	// Sorry ARM users!
#ifdef _WIN32 
#if defined(_M_AMD64) || defined(_M_IX86)
	if (Settings->FloatRendering) {
		if (Settings->LoudMax && Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath)))
		{
#if defined(_M_AMD64)
			swprintf_s(OMPath, L"%s\\OmniMIDI\\LoudMax\\LoudMax64.dll", OMPath);
#elif defined(_M_IX86)
			swprintf_s(OMPath, L"%s\\OmniMIDI\\LoudMax\\LoudMax32.dll", OMPath);
#endif

			if (GetFileAttributesW(OMPath) != INVALID_FILE_ATTRIBUTES) {
				for (int i = 0; i < AudioStreamSize; i++) {
					if (AudioStreams[i] != 0) {
						BASS_VST_ChannelSetDSP(AudioStreams[i], OMPath, BASS_UNICODE, 1);
					}
				}
			}
		}
	}
#endif
#endif

	for (int i = 0; i < AudioStreamSize; i++) {
		if (AudioStreams[i] != 0) {
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_SRC, 0);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_KILL, 1);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_VOICES, (float)Settings->VoiceLimit);
			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_CPU, (float)Settings->RenderTimeLimit);
			
			if (Settings->AsyncMode)
				BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_MIDI_EVENTBUF_ASYNC, 1 << 24);
		}
	}
	LOG(SynErr, "Stream settings loaded.");

	LoadSoundFonts();
	LOG(SynErr, "SoundFonts loaded.");

	if (!Settings->OneThreadMode || Settings->ExperimentalMultiThreaded) {
		_EvtThread = std::jthread(&BASSSynth::EventsThread, this);
		if (!_EvtThread.joinable()) {
			NERROR(SynErr, "_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
			return false;
		}
	}

	if (Settings->IsDebugMode()) {
		_BASThread = std::jthread(&BASSSynth::BASSThread, this);
		if (!_BASThread.joinable()) {
			NERROR(SynErr, "_BASThread failed. (ID: %x)", true, _BASThread.get_id());
			return false;
		}

		_LogThread = std::jthread(&SynthModule::LogFunc, this);
		if (!_LogThread.joinable()) {
			NERROR(SynErr, "_LogThread failed. (ID: %x)", true, _LogThread.get_id());
			return false;
		}

		Settings->OpenConsole();
	}
	
	PlayShortEvent(SystemReset, 0x00, 0x00);
	PlayShortEvent(PitchBend, 0x00, 0x40);

	LOG(SynErr, "BASSMIDI ready.");
	return true;
}

void OmniMIDI::BASSSynth::LoadSoundFonts() {
	if (SoundFonts.size() > 0)
	{
		for (int i = 0; i < SoundFonts.size(); i++)
		{
			if (SoundFonts[i].font)
				BASS_MIDI_FontFree(SoundFonts[i].font);
		}
	}
	SoundFonts.clear();

	if (SFSystem.ClearList()) {
		int retry = 0;
		std::vector<OmniMIDI::SoundFont>* SFv = nullptr;

		while (SFv == nullptr || retry++ != 30)
			SFv = SFSystem.LoadList();

		if (SFv != nullptr) {
			auto& dSFv = *SFv;

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

			for (int i = 0; i < AudioStreamSize; i++) {
				if (AudioStreams[i] != 0) {
					BASS_MIDI_StreamSetFonts(AudioStreams[i], &SoundFonts[0], (unsigned int)SoundFonts.size() | BASS_MIDI_FONT_EX);
				}
			}
		}
	}
}

bool OmniMIDI::BASSSynth::StopSynthModule() {
	if (AudioStreams[0]) {
		for (int i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
				BASS_StreamFree(AudioStreams[i]);
				AudioStreams[i] = 0;
			}		
		}

		SFSystem.ClearList();
	}

	if (_LogThread.joinable())
		_LogThread.join();

	if (_BASThread.joinable())
		_BASThread.join();

	if (Settings->IsDebugMode() && Settings->IsOwnConsole())
		Settings->CloseConsole();

	switch (Settings->AudioEngine) {
	case WASAPI:
		BASS_WASAPI_Stop(true);
		BASS_WASAPI_Free();

		// why do I have to do it myself
		BASS_WASAPI_SetNotify(nullptr, nullptr);
		break;

	case XAudio2:
		for (int i = 0; i < AudioStreamSize; i++) {
			if (WinAudioEngine[i]) {
				WinAudioEngine[i]->Stop();
				delete WinAudioEngine[i];
				WinAudioEngine[i] = nullptr;
			}
		}

		break;

	case ASIO:
		BASS_ASIO_Stop();
		BASS_ASIO_Free();
		MiscFuncs.uSleep(-5000000);
		break;

	case Internal:
	default:
		break;
	}

	BASS_Free();

	for (int i = 0; i < AudioStreamSize; i++) {
		if (_AudThread[i].joinable()) {
			_AudThread[i].join();
		}
	}

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

	case 0xFFFFF:
		// Old WinMMWRP code, ignore
		break;

	SettingsManagerCase(KDMAPI_AUDIOFREQ, get, unsigned int, Settings->SampleRate, var, size);
	SettingsManagerCase(KDMAPI_CURRENTENGINE, get, unsigned int, Settings->AudioEngine, var, size);
	SettingsManagerCase(KDMAPI_MAXVOICES, get, unsigned int, Settings->VoiceLimit, var, size);
	SettingsManagerCase(KDMAPI_MAXRENDERINGTIME, get, unsigned int, Settings->RenderTimeLimit, var, size);

	default:
		LOG(SynErr, "Unknown setting passed to SettingsManager. (VAL: 0x%x)", setting);
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

	for (int si = 0; si < AudioStreamSize; si++) {
		unsigned int tmp = BASS_MIDI_StreamEvents(AudioStreams[si], BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_ASYNC | BASS_MIDI_EVENTS_NORSTATUS, ev, size);
		if (tmp == -1)
			break;

		data += tmp;
	}

	return data;
}

OmniMIDI::SynthResult OmniMIDI::BASSSynth::TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) {
	if (!BMidLib || !BMidLib->IsOnline())
		return LibrariesOffline;

	return BASS_MIDI_StreamEvent(Settings->ExperimentalMultiThreaded ? AudioStreams[chan] : AudioStreams[0], chan, evt, param) ? Ok : InvalidParameter;
}