#include "BASSSynth.hpp"

bool OmniMIDI::BASSSynth::ProcessEvBuf() {
	if (!IsSynthInitialized())
		return false;

	auto pEvt = ShortEvents->ReadPtr();

	if (!pEvt)
		return false;

	unsigned int evtDword = *pEvt;
	unsigned int evt = MIDI_SYSTEM_DEFAULT;
	unsigned int ev = 0;
	unsigned char status = GetStatus(evtDword);
	unsigned char command = status & 0xF0;
	unsigned char chan = status & 0xF;
	unsigned char param1 = GetFirstParam(evtDword);
	unsigned char param2 = GetSecondParam(evtDword);

	HSTREAM targetStream = AudioStreams[0];

	size_t tgtChan = chan * Settings->ExpMTKeyboardDiv;
	size_t tgtChnk = param1 % Settings->ExpMTKeyboardDiv;
	size_t noteOnTgt = tgtChan + tgtChnk;

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
		// LOG("Aftertouch %u (%X, %X - %X)", ev, ev, param1, param2);
		break;
	case PatchChange:
		evt = MIDI_EVENT_PROGRAM;
		ev = param1;
		// LOG("PatchChange %u (%X)", ev, param1);
		break;
	case ChannelPressure:
		evt = MIDI_EVENT_CHANPRES;
		ev = param1;
		// LOG("ChannelPressure %u (%X)", ev, param1);
		break;
	case PitchBend:
		evt = MIDI_EVENT_PITCH;
		ev = param2 << 7 | param1;
		// LOG("PitchBend %u (%X, %X - %X)", ev, ev, param1, param2);
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

			if (Settings->ExperimentalMultiThreaded) {
				for (int i = 0; i < Settings->ExperimentalAudioMultiplier; i += Settings->ExpMTKeyboardDiv) {
					for (int j = 0; j < Settings->KeyboardChunk; j++) {
						targetStream = AudioStreams[i + j];
						BASS_MIDI_StreamEvent(targetStream, 0, MIDI_EVENT_SYSTEMEX, evt);
					}
				}
			}
			else BASS_MIDI_StreamEvent(targetStream, 0, MIDI_EVENT_SYSTEMEX, evt);

			return true;

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
		{
			unsigned char len = ((evtDword - 0xC0) & 0xE0) ? 3 : 2;

			if (Settings->ExperimentalMultiThreaded) {
				for (int i = 0; i < Settings->KeyboardChunk; i++) {
					targetStream = AudioStreams[(chan * Settings->ExpMTKeyboardDiv) + i];
					BASS_MIDI_StreamEvents(targetStream, BASS_MIDI_EVENTS_RAW, &evtDword, len);
				}
			}
			else BASS_MIDI_StreamEvents(targetStream, BASS_MIDI_EVENTS_RAW, &evtDword, len);

			return true;
		}
		}
	}

	if (Settings->ExperimentalMultiThreaded) {
		if (evt == MIDI_EVENT_NOTE || evt == MIDI_EVENT_KEYPRES) {
			targetStream = AudioStreams[noteOnTgt];
			return BASS_MIDI_StreamEvent(targetStream, chan, evt, ev);
		}
		else {
			for (int i = 0; i < Settings->KeyboardChunk; i++) {
				targetStream = AudioStreams[(chan * Settings->ExpMTKeyboardDiv) + i];
				BASS_MIDI_StreamEvent(targetStream, chan, evt, ev);
			}
		}
	}
	else BASS_MIDI_StreamEvent(targetStream, chan, evt, ev);

	return true;
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

	if (!BEfxLib->LoadLib())
		return false;

	switch (Settings->AudioEngine) {
#ifdef _WIN32
	case WASAPI:
		if (!BWasLib->LoadLib())
			return false;
		break;

	case ASIO:
		if (!BAsiLib->LoadLib())
			return false;
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

	if (!BEfxLib->UnloadLib())
		return false;

	if (!BMidLib->UnloadLib())
		return false;

	if (!BAudLib->UnloadLib())
		return false;

	return true;
}

void OmniMIDI::BASSSynth::AudioThread(unsigned int id) {
	switch (Settings->AudioEngine) {
	case Internal:
		while (IsSynthInitialized()) {
			if (Settings->OneThreadMode && !Settings->ExperimentalMultiThreaded)
				ProcessEvBufChk();

			BASS_ChannelUpdate(AudioStreams[id], 1);
			MiscFuncs.MicroSleep(-1);
		}
		break;

	default:
		break;
	}
}

void OmniMIDI::BASSSynth::EventsThread(unsigned int id) {
	size_t count = 0;

	LOG("_EvtThread[%d] idling, waiting for AudioStreams to come up.", id);

	// Spin while waiting for the stream to go online
	while (AudioStreams[0] == 0)
		MiscFuncs.MicroSleep(-1);

	LOG("_EvtThread[%d] spinned up.", id);

	while (IsSynthInitialized()) {
		if (!ProcessEvBuf())
			count++;

		if (count > 4096) {
			MiscFuncs.MicroSleep(-1);
			count = 0;
		}
	}
}

void OmniMIDI::BASSSynth::BASSThread() {
	unsigned int itv = 0;
	float buf = 0.0f;

	float rtr = 0.0f;
	float tv = 0.0f;

	while (AudioStreams[0] == 0)
		MiscFuncs.MicroSleep(-1);

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

		MiscFuncs.MicroSleep(-100000);
	}
}

bool OmniMIDI::BASSSynth::LoadSynthModule() {
	auto ptr = (LibImport*)LibImports;

	if (!Settings) {
		Settings = new BASSSettings(ErrLog);
		Settings->LoadSynthConfig();
	}

	if (!BAudLib)
		BAudLib = new Lib("bass", ErrLog, &ptr, LibImportsSize);

	if (!BMidLib)
		BMidLib = new Lib("bassmidi", ErrLog, &ptr, LibImportsSize);

	if (!BEfxLib)
		BEfxLib = new Lib("bass_fx", ErrLog, &ptr, LibImportsSize);

#if defined(_WIN32)
	if (!BWasLib)
		BWasLib = new Lib("basswasapi", ErrLog, &ptr, LibImportsSize);

	if (!BAsiLib)
		BAsiLib = new Lib("bassasio", ErrLog, &ptr, LibImportsSize);
#endif

	// LOG(SynErr, L"LoadBASSSynth called.");
	if (!LoadFuncs()) {
		NERROR("Something went wrong here!!!", true);
		return false;
	}
	else LOG("Libraries loaded.");

	if (!AllocateShortEvBuf(Settings->EvBufSize)) {
		NERROR("AllocateShortEvBuf failed.", true);
		return false;
	}
	else LOG("Buffers allocated. (EVBUF >> %u)", Settings->EvBufSize);

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

	NERROR("Something went wrong here!!!", true);
	return false;
}

bool OmniMIDI::BASSSynth::StartSynthModule() {
	// If the audio stream is not 0, then stream is allocated already
	if (AudioStreams != nullptr)
		return true;

	// SF path
	OMShared::Funcs Utils;
	char OMPath[MAX_PATH] = { 0 };

	// BASS stream flags
	bool bInfoGood = false;
	bool noFreqChange = false;
	double devFreq = 0.0;
	unsigned int minBuf = 0;
	BASS_INFO defInfo = BASS_INFO();

#if defined(_WIN32)
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

	unsigned int deviceFlags = 
		(Settings->MonoRendering ? BASS_DEVICE_MONO : BASS_DEVICE_STEREO);

	unsigned int streamFlags = 
		(Settings->MonoRendering ? BASS_SAMPLE_MONO : 0) |
		(Settings->FloatRendering ? BASS_SAMPLE_FLOAT : 0) |
		(Settings->AsyncMode ? BASS_MIDI_ASYNC : 0) |
		(Settings->FollowOverlaps ? BASS_MIDI_NOTEOFF1 : 0);

	if ((Settings->AudioEngine < Internal || Settings->AudioEngine > BASSENGINE_COUNT) || Settings->AudioEngine == XAudio2)
		Settings->AudioEngine = Internal;

#ifdef _WIN32
	if (!(streamFlags & BASS_SAMPLE_FLOAT) && 
		Settings->AudioEngine == ASIO) {
		NERROR("The selected engine does not support integer audio.\nIt will render with floating point audio.", false);
		streamFlags |= BASS_SAMPLE_FLOAT;
	}
#endif

	if (Settings->ExperimentalMultiThreaded) {
		if (Settings->AudioEngine != Internal)
			Settings->AudioEngine = Internal;

		AudioStreamSize = Settings->ExperimentalAudioMultiplier;
		Settings->AsyncMode = true;
		EvtThreadsSize = Settings->ExpMTKeyboardDiv;

		LOG("Experimental multi BASS stream mode enabled. (CHA %d, CHK %d >> TOT %d)", Settings->ChannelDiv, Settings->ExpMTKeyboardDiv, Settings->ExperimentalAudioMultiplier);
	}
	else
	{
		AudioStreamSize = 1;
		EvtThreadsSize = 1;
	}

	AudioStreams = new unsigned int[AudioStreamSize] { 0 };
	_AudThread = new std::jthread[AudioStreamSize];

	switch (Settings->AudioEngine) {
	case Internal:
		deviceFlags |= BASS_DEVICE_DSOUND;

		BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, 1);
		if (BASS_Init(-1, Settings->SampleRate, BASS_DEVICE_STEREO, 0, nullptr)) {
			if ((bInfoGood = BASS_GetInfo(&defInfo))) {
				minBuf = defInfo.minbuf;
				LOG("minbuf %d", defInfo.minbuf);
			}

			BASS_Free();
		}

		BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);
		BASS_SetConfig(BASS_CONFIG_BUFFER, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

		BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, bInfoGood ? defInfo.minbuf : 0);
		BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, bInfoGood ? (defInfo.minbuf * 2) : 0);

		if (!BASS_Init(-1, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			NERROR("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		for (int i = 0; i < AudioStreamSize; i++) {
			AudioStreams[i] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
			if (AudioStreams[i] == 0) {
				NERROR("BASS_MIDI_StreamCreate failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			BASS_ChannelSetAttribute(AudioStreams[i], BASS_ATTRIB_BUFFER, 0);

			if (!BASS_ChannelPlay(AudioStreams[i], false)) {
				NERROR("BASS_ChannelPlay failed with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}

			_AudThread[i] = std::jthread(&BASSSynth::AudioThread, this, i);
			if (!_AudThread[i].joinable()) {
				NERROR("_AudThread failed. (ID: %x)", true, _AudThread[i].get_id());
				return false;
			}
			else LOG("_AudThread running! (ID: %x)", _AudThread[i].get_id());
		}

		LOG("bassDev %d >> devFreq %d", -1, Settings->SampleRate);
		break;

#if defined(_WIN32)
	case WASAPI:
	{
		auto proc = Settings->OneThreadMode ? &BASSSynth::WasapiEvProc : &BASSSynth::WasapiProc;

		streamFlags |= BASS_STREAM_DECODE;

		if (!BASS_Init(0, Settings->SampleRate, deviceFlags, 0, nullptr)) {
			NERROR("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		AudioStreams[0] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
		if (!AudioStreams[0]) {
			NERROR("BASS_MIDI_StreamCreate w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Init(-1, Settings->SampleRate, Settings->MonoRendering ? 1 : 2, ((Settings->AsyncMode) ? BASS_WASAPI_ASYNC : 0) | BASS_WASAPI_EVENT, Settings->WASAPIBuf / 1000.0f, 0, proc, this)) {
			NERROR("BASS_WASAPI_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!BASS_WASAPI_Start()) {
			NERROR("BASS_WASAPI_Start failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		LOG("wasapiDev %d >> devFreq %d", -1, Settings->SampleRate);
		break;
	}

	case ASIO:
	{
		auto proc = Settings->OneThreadMode ? &BASSSynth::AsioEvProc : &BASSSynth::AsioProc;
		streamFlags |= BASS_STREAM_DECODE;

		LOG("Available ASIO devices:");
		// Get the amount of ASIO devices available
		for (; BASS_ASIO_GetDeviceInfo(asioCount, &devInfo); asioCount++) {
			LOG("%s", devInfo.name);

			// Return the correct ID when found
			if (strcmp(dev, devInfo.name) == 0)
				asioDev = asioCount;
		}

		if (asioCount < 1) {
			NERROR("No ASIO devices available! Got 0 devices!!!", true);
			return false;
		}
		else LOG("Detected %d ASIO devices.", asioCount);

		if (!BASS_ASIO_Init(asioDev, BASS_ASIO_THREAD | BASS_ASIO_JOINORDER)) {
			NERROR("BASS_ASIO_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (BASS_ASIO_GetInfo(&asioInfo)) {
			LOG("ASIO device: %s (CurBuf %d, Min %d, Max %d, Gran %d, OutChans %d)",
				asioInfo.name, asioInfo.bufpref, asioInfo.bufmin, asioInfo.bufmax, asioInfo.bufgran, asioInfo.outputs);
		}

		if (!BASS_ASIO_SetRate(asioFreq)) {
			LOG("BASS_ASIO_SetRate failed, falling back to BASS_ASIO_ChannelSetRate... BASSERR: %d", BASS_ErrorGetCode());
			if (!BASS_ASIO_ChannelSetRate(0, 0, asioFreq)) {
				LOG("BASS_ASIO_ChannelSetRate failed as well, BASSERR: %d... This ASIO device does not want OmniMIDI to change its output frequency.", BASS_ErrorGetCode());
				noFreqChange = true;
			}
		}

		LOG("Available ASIO channels:");
		for (unsigned long curSrcCh = 0; curSrcCh < asioInfo.outputs; curSrcCh++) {
			BASS_ASIO_ChannelGetInfo(0, curSrcCh, &chInfo);

			if ((leftChID && rightChID) && (leftChID != -1 && rightChID != -1))
				break;

			LOG("%s", chInfo.name);

			// Return the correct ID when found
			if (strcmp(lCh, chInfo.name) == 0) {
				leftChID = curSrcCh;
				LOG("^^ This channel matches what the user requested for the left channel! (ID: %d)", leftChID);
			}
			else if (strcmp(rCh, chInfo.name) == 0) {
				rightChID = curSrcCh;
				LOG("^^ This channel matches what the user requested for the right channel! (ID: %d)", rightChID);
			}
		}

		if (leftChID == -1 && rightChID == -1) {
			leftChID = 0;
			rightChID = 1;
			LOG("No ASIO output channels found, defaulting to CH0 for left and CH1 for right.", leftChID, chInfo.name);
		}

		if (noFreqChange) {
			devFreq = BASS_ASIO_GetRate();
			LOG("BASS_ASIO_GetRate = %dHz", devFreq);
			if (devFreq != -1) {
				asioFreq = devFreq;
			}
			else {
				NERROR("BASS_ASIO_GetRate failed to get the device's frequency, with error 0x%x.", true, BASS_ErrorGetCode());
				return false;
			}
		}

		if (!asioFreq) {
			asioFreq = Settings->SampleRate;
			LOG("Nothing set the frequency for the ASIO device! Falling back to the value from the settings... (%dHz)", (int)asioFreq);
		}

		if (!BASS_Init(0, (unsigned long)asioFreq, deviceFlags, 0, nullptr)) {
			NERROR("BASS_Init failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}
		else LOG("BASS_Init returned true. (freq: %d, dFlags: %d)", (int)asioFreq, deviceFlags);

		AudioStreamSize = 1;
		AudioStreams[0] = BASS_MIDI_StreamCreate(16, streamFlags, Settings->SampleRate);
		if (!AudioStreams[0]) {
			NERROR("BASS_MIDI_StreamCreate[0] w/ BASS_STREAM_DECODE failed with error 0x%x.", true, BASS_ErrorGetCode());
			return false;
		}

		if (!noFreqChange) BASS_ASIO_SetRate(asioFreq);

		if (Settings->StreamDirectFeed) asioCheck = BASS_ASIO_ChannelEnableBASS(0, leftChID, AudioStreams[0], 0);
		else asioCheck = BASS_ASIO_ChannelEnable(0, leftChID, proc, this);

		if (asioCheck) {
			if (Settings->StreamDirectFeed) LOG("ASIO direct feed enabled.");
			else LOG("AsioProc allocated.");

			LOG("Channel %d set to %dHz and enabled.", leftChID, (int)asioFreq);

			if (Settings->MonoRendering) asioCheck = BASS_ASIO_ChannelEnableMirror(rightChID, 0, leftChID);
			else asioCheck = BASS_ASIO_ChannelJoin(0, rightChID, leftChID);

			if (asioCheck) {
				if (Settings->MonoRendering) LOG("Channel %d mirrored to %d for mono rendering. (F%d)", leftChID, rightChID, Settings->MonoRendering);
				else LOG("Channel %d joined to %d for stereo rendering. (F%d)", rightChID, leftChID, Settings->MonoRendering);

				if (!BASS_ASIO_Start(0, 0))
					NERROR("BASS_ASIO_Start failed with error 0x%x. If the code is zero, that means the device encountered an error and aborted the initialization process.", true, BASS_ErrorGetCode());

				else {
					BASS_ASIO_ChannelSetFormat(0, leftChID, BASS_ASIO_FORMAT_FLOAT | BASS_ASIO_FORMAT_DITHER);
					BASS_ASIO_ChannelSetRate(0, leftChID, asioFreq);

					LOG("asioDev %d >> chL %d, chR %d - asioFreq %d", asioDev, leftChID, rightChID, (int)asioFreq);
					break;
				}
			}
		}

		return false;
	}
#endif

	case Invalid:
	default:
		NERROR("Engine ID \"%d\" is not valid!", false, Settings->AudioEngine);
		return false;
	}

	// Sorry ARM users!

	if (Settings->FloatRendering && Settings->LoudMax) {
		BASS_BFX_COMPRESSOR2 compressor;
		LOG("Enabling compressor...");

		for (int i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
				audioLimiter = BASS_ChannelSetFX(AudioStreams[i], BASS_FX_BFX_COMPRESSOR2, 0);
			}
		}

		BASS_FXGetParameters(audioLimiter, &compressor);
		BASS_FXSetParameters(audioLimiter, &compressor);
		LOG("Got compressor...");
	}


	char* tmpUtils = new char[MAX_PATH] { 0 };
	if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, tmpUtils, sizeof(tmpUtils) * MAX_PATH)) {
#ifdef _WIN32
		snprintf(OMPath, sizeof(OMPath), "%s/OmniMIDI/SupportLibraries/bassflac", tmpUtils);
#else		
		snprintf(OMPath, sizeof(OMPath), "%s/OmniMIDI/SupportLibraries/libbassflac.so", tmpUtils);
#endif

		LOG("BFlaLib >> %s", OMPath);
		BFlaLib = BASS_PluginLoad(OMPath, BASS_UNICODE);

		if (!BFlaLib) LOG("No BASSFLAC, this could affect playback with FLAC based soundbanks.", BFlaLib);
		else LOG("BASSFLAC loaded. BFlaLib --> 0x%08x", BFlaLib);
	}
	delete[] tmpUtils;

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
	LOG("Stream settings loaded.");

	if (!Settings->OneThreadMode || Settings->ExperimentalMultiThreaded) {
		_EvtThread = std::jthread(&BASSSynth::EventsThread, this, 0);
		if (!_EvtThread.joinable()) {
			NERROR("_EvtThread failed. (ID: %x)", true, _EvtThread.get_id());
			return false;
		}
		else LOG("_EvtThread running! (ID: %x)", _EvtThread.get_id());
	}

	LoadSoundFonts();
	SFSystem.RegisterCallback(this);

	if (Settings->IsDebugMode()) {
		_BASThread = std::jthread(&BASSSynth::BASSThread, this);
		if (!_BASThread.joinable()) {
			NERROR("_BASThread failed. (ID: %x)", true, _BASThread.get_id());
			return false;
		}
		else LOG("_BASThread running! (ID: %x)", _BASThread.get_id());

		_LogThread = std::jthread(&BASSSynth::LogFunc, this);
		if (!_LogThread.joinable()) {
			NERROR("_LogThread failed. (ID: %x)", true, _LogThread.get_id());
			return false;
		}
		else LOG("_LogThread running! (ID: %x)", _LogThread.get_id());

		Settings->OpenConsole();
	}

#ifdef _WIN32
	if (Settings->ExperimentalMultiThreaded) {
#endif
		LOG("Experimental mode is enabled. Preloading samples to avoid crashes...");
		for (int i = 0; i < 128; i++) {
			for (int j = 127; j < 128; j++) {
				if (i < 0) i = 0;

				PlayShortEvent(NoteOn, i, j);
				PlayShortEvent(NoteOff, i, 0);

				MiscFuncs.MicroSleep(-100000);
			}
		}

		LOG("Samples are ready.");
#ifdef _WIN32
	}
#endif

	PlayShortEvent(SystemReset, 0x00, 0x00);
	PlayShortEvent(PitchBend, 0x00, 0x40);
	ShortEvents->ResetHeads();

	LOG("BASSMIDI ready.");
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
		SoundFontsVector = SFSystem.LoadList();

		if (SoundFontsVector != nullptr) {
			auto& dSFv = *SoundFontsVector;

			if (dSFv.size() > 0) {
				for (int i = 0; i < dSFv.size(); i++) {
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
					sf.font = BASS_MIDI_FontInit(MiscFuncs.GetUTF16((char*)sfPath), bmfiflags | BASS_UNICODE);
#else
					sf.font = BASS_MIDI_FontInit(sfPath, bmfiflags);
#endif
				

					if (sf.font != 0) {
						// Check if the soundfont loads, if it does then it's valid
						if (BASS_MIDI_FontLoadEx(sf.font, sf.spreset, sf.sbank, 0, 0)) {
							SoundFonts.push_back(sf);
							LOG("\"%s\" loaded!", sfPath);
						}


						else NERROR("Error 0x%x occurred while loading \"%s\"!", false, BASS_ErrorGetCode(), sfPath);
					}
					else NERROR("Error 0x%x occurred while initializing \"%s\"!", false, BASS_ErrorGetCode(), sfPath);
				}

				for (int i = 0; i < AudioStreamSize; i++) {
					if (AudioStreams[i] != 0) {
						if (!BASS_MIDI_StreamSetFonts(AudioStreams[i], &SoundFonts[0], (unsigned int)SoundFonts.size() | BASS_MIDI_FONT_EX))
							NERROR("BASS_MIDI_StreamSetFonts encountered error 0x%x!", false, BASS_ErrorGetCode());
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
		for (int i = 0; i < AudioStreamSize; i++) {
			if (AudioStreams[i] != 0) {
#ifdef _DEBUG
				LOG("Freeing BASS stream %x...", AudioStreams[i]);
#endif
				BASS_StreamFree(AudioStreams[i]);
				AudioStreams[i] = 0;
			}		
		}
		LOG("Streams wiped.");
	}
	
	if (_EvtThread.joinable()) {
		_EvtThread.join();
		LOG("_EvtThread freed.");
	}

	if (_AudThread != nullptr) {
		for (int i = 0; i < AudioStreamSize; i++) {
			if (_AudThread[i].joinable()) {
#ifdef _DEBUG
				LOG("Freeing _AudThread[%d]...", i);
#endif
				_AudThread[i].join();
			}
		}
		LOG("_AudThreads wiped.");
	}

	if (_LogThread.joinable()) {
		_LogThread.join();
		LOG("_LogThread freed.");
	}

	if (_BASThread.joinable()) {
		_BASThread.join();
		LOG("_BASThread freed.");
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

		LOG("BASSWASAPI freed.");
		break;

	case ASIO:
		BASS_ASIO_Stop();
		BASS_ASIO_Free();
		MiscFuncs.MicroSleep(-5000000);
		LOG("BASSASIO freed.");
		break;
#endif

	case Internal:
	default:
		break;
	}

	if (BFlaLib) {
		BASS_PluginFree(BFlaLib);
		LOG("BASSFLAC freed.");
	}

	BASS_Free();
	LOG("BASS freed.");

	if (AudioStreams != nullptr) {
		delete[] AudioStreams;
		AudioStreams = nullptr;
		LOG("Audio streams freed.");
	}

	if (_AudThread != nullptr) {
		delete[] _AudThread;
		_AudThread = nullptr;
		LOG("Audio threads freed.");
	}

	return true;
}

bool OmniMIDI::BASSSynth::SettingsManager(unsigned int setting, bool get, void* var, size_t size) {
	switch (setting) {

	case KDMAPI_MANAGE:
	{
		if (Settings || IsSynthInitialized()) {
			NERROR("You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
			return false;
		}

		LOG("KDMAPI REQUEST: The MIDI app wants to manage the settings.");
		Settings = new BASSSettings(ErrLog);

		break;
	}

	case KDMAPI_LEAVE:
	{
		if (Settings) {
			if (IsSynthInitialized()) {
				NERROR("You can not control the settings while the driver is open and running! Call TerminateKDMAPIStream() first then try again.", true);
				return false;
			}

			LOG("KDMAPI REQUEST: The MIDI app does not want to manage the settings anymore.");
			delete Settings;
			Settings = nullptr;
		}
		break;
	}

	case 0xFFFFE:
	case 0xFFFFF:
		// Old WinMMWRP code, ignore
		break;

	SettingsManagerCase(KDMAPI_AUDIOFREQ, get, unsigned int, Settings->SampleRate, var, size);
	SettingsManagerCase(KDMAPI_CURRENTENGINE, get, unsigned int, Settings->AudioEngine, var, size);
	SettingsManagerCase(KDMAPI_MAXVOICES, get, unsigned int, Settings->VoiceLimit, var, size);
	SettingsManagerCase(KDMAPI_MAXRENDERINGTIME, get, unsigned int, Settings->RenderTimeLimit, var, size);

	default:
		LOG("Unknown setting passed to SettingsManager. (VAL: 0x%x)", setting);
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

	return BASS_MIDI_StreamEvent(Settings->ExperimentalMultiThreaded ? AudioStreams[chan + (param % 16)] : AudioStreams[0], chan, evt, param) ? Ok : InvalidParameter;
}