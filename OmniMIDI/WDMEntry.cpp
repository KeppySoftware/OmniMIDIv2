/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "WDMEntry.hpp"

typedef OmniMIDI::SynthModule* (*rInitModule)();
typedef void(*rStopModule)();

static ErrorSystem::Logger WDMErr;
static WinDriver::DriverCallback* fDriverCallback = nullptr;
static WinDriver::DriverComponent* DriverComponent = nullptr;
static WinDriver::DriverMask* DriverMask = nullptr;
static OmniMIDI::StreamPlayer* StreamPlayer = nullptr;
static OmniMIDI::WDMSettings* WDMSettings = nullptr;
static OmniMIDI::SynthModule* SynthModule = nullptr;
static char* lpDataBuf = new char[65536]();
static HMODULE extModule = nullptr;

static OMShared::Funcs MiscFuncs;
static signed long long TickStart = 0;

#ifdef _WIN64
#define WINAPI
#endif

int WINAPI DllMain(HMODULE hModule, DWORD ReasonForCall, LPVOID lpReserved)
{
	BOOL ret = FALSE;
	switch (ReasonForCall)
	{
	case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		if (AllocConsole()) {
			FILE* dummy;
			freopen_s(&dummy, "CONOUT$", "w", stdout);
			freopen_s(&dummy, "CONOUT$", "w", stderr);
			freopen_s(&dummy, "CONIN$", "r", stdin);
			std::cout.clear();
			std::clog.clear();
			std::cerr.clear();
			std::cin.clear();
		}
#endif

		if (!TickStart) {
			if (!(MiscFuncs.querySystemTime(&TickStart) == 0)) {
				LOG(WDMErr, "Failed to parse starting tick through NtQuerySystemTime! OmniMIDI will not load.");
				return FALSE;
			}
		}

		if (!DriverComponent) {
			DriverComponent = new WinDriver::DriverComponent;

			if ((ret = DriverComponent->SetLibraryHandle(hModule)) == true) {
				DriverMask = new WinDriver::DriverMask;
				fDriverCallback = new WinDriver::DriverCallback;
				WDMSettings = new OmniMIDI::WDMSettings;

				// Allocate a generic dummy synth for now
				SynthModule = new OmniMIDI::SynthModule;
			}
		}

		break;

	case DLL_PROCESS_DETACH:
		if (DriverComponent) {
#ifdef _DEBUG
			FreeConsole();
#endif

			delete SynthModule;
			delete fDriverCallback;
			delete DriverMask;
			if (WDMSettings) delete WDMSettings;

			ret = DriverComponent->UnsetLibraryHandle();

			delete DriverComponent;
			DriverComponent = nullptr;
		}
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return ret;
}

long WINAPI DriverProc(DWORD DriverIdentifier, HDRVR DriverHandle, UINT Message, LONG Param1, LONG Param2) {
	bool v = false;

	switch (Message) {
	case DRV_OPEN:
		v = DriverComponent->SetDriverHandle(DriverHandle);
		LOG(WDMErr, "->SetDriverHandle(...) returned %d", v);
		return v;

	case DRV_CLOSE:
		v = DriverComponent->UnsetDriverHandle();
		LOG(WDMErr, "->UnsetDriverHandle() returned %d", v);
		return v;

	case DRV_LOAD:
	case DRV_FREE:
		return DRVCNF_OK;

	case DRV_QUERYCONFIGURE:
		return DRVCNF_CANCEL;

	case DRV_ENABLE:
	case DRV_DISABLE:
		return DRVCNF_OK;

	case DRV_INSTALL:
		return DRVCNF_OK;

	case DRV_REMOVE:
		return DRVCNF_OK;
	}

	long r = DefDriverProc(DriverIdentifier, DriverHandle, Message, Param1, Param2);
	LOG(WDMErr, "DefDriverProc returned %d", r);
	return r;
}

void freeAudioRenderer() {
	if (extModule != nullptr) {
		if (SynthModule) {
			auto fM = (rStopModule)GetProcAddress(extModule, "freeModule");
			fM();
			LOG(WDMErr, "Called freeModule() from custom external synth module. ADDR:%x", extModule);
		}

		FreeLibrary(extModule);
		extModule = nullptr;
		SynthModule = nullptr;

		LOG(WDMErr, "Custom SynthModule freed.");
	}

	if (SynthModule != nullptr)
	{
		delete SynthModule;
		SynthModule = nullptr;
		LOG(WDMErr, "SynthModule freed.");
	}

	if (WDMSettings != nullptr)
	{
		delete WDMSettings;
		WDMSettings = nullptr;
		LOG(WDMErr, "WDMSettings freed.");
	}

	SynthModule = new OmniMIDI::SynthModule;
}

bool stopAudioRenderer() {
	if (StreamPlayer) {
		StreamPlayer->EmptyQueue();
		delete StreamPlayer;
		LOG(WDMErr, "StreamPlayer deleted.");
	}

	if (SynthModule->StopSynthModule()) {
		LOG(WDMErr, "::StopSynthModule() called.");

		if (SynthModule->UnloadSynthModule()) {
			LOG(WDMErr, "::UnloadSynthModule() called.");

			fDriverCallback->CallbackFunction(MOM_CLOSE, 0, 0);
			fDriverCallback->ClearCallbackFunction();
			LOG(WDMErr, "Callback system has been freed.");

			return true;
		}
	}

	return false;
}

bool startAudioRenderer() {
	if (SynthModule->LoadSynthModule()) {
		if (SynthModule->StartSynthModule()) {
			LOG(WDMErr, "Synth ID: %x", SynthModule->SynthID());
			return true;
		}
		else NERROR(WDMErr, "::StartSynthModule() failed! The driver will not output audio.", true);

		if (!SynthModule->StopSynthModule())
			FNERROR(WDMErr, "::StopSynthModule() failed!!!");
	}

	if (!SynthModule->UnloadSynthModule())
		FNERROR(WDMErr, "::UnloadSynthModule() failed!!!");

	freeAudioRenderer();
	return false;
}

bool getAudioRenderer() {
	bool good = true;

	freeAudioRenderer();

	WDMSettings = new OmniMIDI::WDMSettings;
	switch (WDMSettings->Renderer) {
	case EXTERNAL:
		extModule = LoadLibraryA(WDMSettings->CustomRenderer.c_str());

		if (extModule) {
			auto iM = (rInitModule)GetProcAddress(extModule, "initModule");

			if (iM) {
				SynthModule = iM();

				if (!SynthModule)
					good = false;
			}
			else good = false;
		}
		else good = false;
		break;
	case BASSMIDI:
#ifndef _M_ARM
		SynthModule = new OmniMIDI::BASSSynth;
#else
		NERROR(WDMErr, "BASSMIDI is not available on ARM Thumb-2.", true);
		good = false;
#endif
		break;
	case FLUIDSYNTH:
		SynthModule = new OmniMIDI::FluidSynth;
		break;

	case XSYNTH:
#ifdef _M_AMD64
		SynthModule = new OmniMIDI::XSynth;
#else
		NERROR(WDMErr, "XSynth is only available on AMD64 platforms.", true);
		good = false;
#endif
		break;

	case TINYSF:
	default:
		SynthModule = new OmniMIDI::TinySFSynth;
		break;
	}

	if (WDMSettings->Renderer == EXTERNAL && !good) 
		freeAudioRenderer();

	LOG(WDMErr, "Renderer: %d - Custom renderer: %s",
		WDMSettings->Renderer,
		WDMSettings->CustomRenderer.c_str());

	return good;
}

MMRESULT WINAPI modMessage(UINT DeviceID, UINT Message, DWORD_PTR UserPointer, DWORD_PTR Param1, DWORD_PTR Param2) {
	switch (Message) {
	case MODM_DATA:
		return (SynthModule->PlayShortEvent((DWORD)Param1) == SYNTH_OK) ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;

	case MODM_LONGDATA:
	{				
		MIDIHDR* mhdr = (MIDIHDR*)Param1;
		DWORD hdrLen = (DWORD)Param2;

		if (hdrLen < offsetof(MIDIHDR, dwOffset) ||
			!mhdr || !mhdr->lpData ||
			mhdr->dwBufferLength < mhdr->dwBytesRecorded)
		{
			LOG(WDMErr, "SysEx event 0x%08x is invalid. (lpData: 0x%08x, dwBL: %d, dwBR: %d, dwBR4: %d, cbSize: 0x%08x)", 
				*(mhdr->lpData), mhdr->dwBufferLength, mhdr->dwBytesRecorded, mhdr->dwBytesRecorded % 4, Param1, Param2);
			return MMSYSERR_INVALPARAM;
		}

		if (!(mhdr->dwFlags & MHDR_PREPARED))
		{
			fDriverCallback->CallbackFunction(0, 0, 0xFEEDF00D);
			LOG(WDMErr, "Stream data 0x%08x has not been prepared.", Param1, Param2);
			return MIDIERR_UNPREPARED;
		}

		if (!(mhdr->dwFlags & MHDR_DONE))
		{
			if (mhdr->dwFlags & MHDR_INQUEUE) {
				LOG(WDMErr, "SysEx event 0x%08x is still in queue for StreamPlayer.", Param1, Param2);
				return MIDIERR_STILLPLAYING;
			}	
		}

		auto ret = SynthModule->PlayLongEvent(mhdr->lpData, mhdr->dwBufferLength);
		if (ret) {
			switch (ret) {
			case SYNTH_INVALPARAM:
				LOG(WDMErr, "Invalid SysEx event. (Buf = 0x%08x, SynthResult = %d, bufsize = %d)", Param1, ret, mhdr->dwBufferLength);
				return MMSYSERR_INVALPARAM;
			default:
				LOG(WDMErr, "No idea! 0x%08x", Param1);
				return MMSYSERR_ERROR;
			}
		}

#if _DEBUG
		sprintf_s(lpDataBuf, 65536, "%02X", mhdr->lpData[0]);
		for (int i = 1; i < mhdr->dwBufferLength && i < 65536; i++) {
			sprintf_s(lpDataBuf + strlen(lpDataBuf), 65536, "%02X", mhdr->lpData[i]);
		}
		LOG(WDMErr, "SysEx event received! Data -> %s (Len %d - dwFlags 0x%04x)", lpDataBuf, mhdr->dwBufferLength, mhdr->dwFlags);
#endif

		fDriverCallback->CallbackFunction(MOM_DONE, 0, (DWORD_PTR)mhdr);

		return MMSYSERR_NOERROR;
	}

	case MODM_STRMDATA: 
	{
		MIDIHDR* mhdr = (MIDIHDR*)Param1;
		DWORD hdrLen = (DWORD)Param2;

		if (StreamPlayer == nullptr)
			return MIDIERR_NOTREADY;

		if (hdrLen < offsetof(MIDIHDR, dwOffset) ||
			!mhdr || !mhdr->lpData ||
			mhdr->dwBufferLength < mhdr->dwBytesRecorded)
		{
			return MMSYSERR_INVALPARAM;
		}

		if (!(mhdr->dwFlags & MHDR_PREPARED)) {
			fDriverCallback->CallbackFunction(0, 0, 0xFEEDF00D);
			return MIDIERR_UNPREPARED;
		}

		if (!(mhdr->dwFlags & MHDR_DONE))
		{
			if (mhdr->dwFlags & MHDR_INQUEUE)
				return MIDIERR_STILLPLAYING;
		}

		mhdr->dwFlags &= ~MHDR_DONE;
		mhdr->dwFlags |= MHDR_INQUEUE;
		mhdr->lpNext = 0;
		mhdr->dwOffset = 0;

		if (!StreamPlayer->AddToQueue(mhdr))
			return MIDIERR_STILLPLAYING;

		return MMSYSERR_NOERROR;
	}

	case MODM_PREPARE:
	{
		MIDIHDR* mhdr = (MIDIHDR*)Param1;
		unsigned int mhdrSize = (unsigned int)Param2;

		if (!mhdr)
			return MMSYSERR_INVALPARAM;

		if (mhdrSize < offsetof(MIDIHDR, dwOffset) ||
			!mhdr || !mhdr->lpData ||
			mhdr->dwBufferLength < mhdr->dwBytesRecorded)
		{
			return MMSYSERR_INVALPARAM;
		}

		if (!(mhdr->dwFlags & MHDR_PREPARED))
		{
			if (!VirtualLock(mhdr->lpData, mhdr->dwBufferLength))
				return MMSYSERR_NOMEM;
		}

		mhdr->dwFlags |= MHDR_PREPARED;
		return MMSYSERR_NOERROR;
	}

	case MODM_UNPREPARE:
	{
		MIDIHDR* mhdr = (MIDIHDR*)Param1;
		unsigned int mhdrSize = (unsigned int)Param2;

		if (!mhdr)
			return MMSYSERR_INVALPARAM;

		if (mhdrSize < offsetof(MIDIHDR, dwOffset) ||
			!mhdr || !mhdr->lpData ||
			mhdr->dwBufferLength < mhdr->dwBytesRecorded ||
			mhdr->dwBytesRecorded % 4)
		{
			return MMSYSERR_INVALPARAM;
		}

		if (!(mhdr->dwFlags & MHDR_INQUEUE))
		{
			if (!VirtualUnlock(mhdr->lpData, mhdr->dwBufferLength))
			{
				const unsigned int err = GetLastError();

				if (err != 0x9E) {
					throw std::runtime_error("Fatal error while unlocking buffer. Execution has been halted.");
				}
			}
		}

		mhdr->dwFlags &= ~MHDR_PREPARED;
		return MMSYSERR_NOERROR;
	}

	case MODM_RESET:
		if (StreamPlayer)
			StreamPlayer->ResetQueue();

		return (SynthModule->PlayShortEvent(0x0101FF) == SYNTH_OK) ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;

	case MODM_OPEN:
	{		
		LPMIDIOPENDESC midiOpenDesc = reinterpret_cast<LPMIDIOPENDESC>(Param1);
		DWORD callbackMode = (DWORD)Param2;

		freeAudioRenderer();
		if (getAudioRenderer()) {
			if (startAudioRenderer()) {
				if (fDriverCallback->PrepareCallbackFunction(midiOpenDesc, callbackMode)) {
					LOG(WDMErr, "PrepareCallbackFunction done.");

					if (callbackMode & MIDI_IO_COOKED) {
						StreamPlayer = new OmniMIDI::StreamPlayer(SynthModule, fDriverCallback);
						LOG(WDMErr, "StreamPlayer allocated.");

						if (!StreamPlayer)
						{
							freeAudioRenderer();
							return MMSYSERR_NOMEM;
						}

						LOG(WDMErr, "StreamPlayer address: %x", StreamPlayer);
					}
				}

				return MMSYSERR_NOERROR;
			}
		}

		NERROR(WDMErr, "Failed to initialize synthesizer.", true);
		return MMSYSERR_NOMEM;
	}


	case MODM_CLOSE:
		if (stopAudioRenderer()) {
			freeAudioRenderer();
			return MMSYSERR_NOERROR;
		}

		LOG(WDMErr, "Failed to free synthesizer.");
		return MMSYSERR_ERROR;

	case MODM_GETNUMDEVS:
		LOG(WDMErr, "MODM_GETNUMDEVS");
		return WDMSettings->IsBlacklistedProcess() ? 0 : 1;

	case MODM_GETDEVCAPS:
		return DriverMask->GiveCaps(DeviceID, (PVOID)Param1, (DWORD)Param2);

	case MODM_PROPERTIES:
	{
		MIDIPROPTEMPO* mpTempo = (MIDIPROPTEMPO*)Param1;
		MIDIPROPTIMEDIV* mpTimeDiv = (MIDIPROPTIMEDIV*)Param1;
		DWORD mpFlags = (DWORD)Param2;

		if (StreamPlayer == nullptr)
			return MIDIERR_NOTREADY;

		if (!(mpFlags & (MIDIPROP_GET | MIDIPROP_SET)))
			return MMSYSERR_INVALPARAM;

		if (mpFlags & MIDIPROP_TEMPO)
		{
			if (mpTempo->cbStruct != sizeof(MIDIPROPTEMPO))
				return MMSYSERR_INVALPARAM;

			if (mpFlags & MIDIPROP_SET)
				StreamPlayer->SetTempo(mpTempo->dwTempo);
			else if (mpFlags & MIDIPROP_GET)
				mpTempo->dwTempo = StreamPlayer->GetTempo();
		}
		else if (mpFlags & MIDIPROP_TIMEDIV) {
			if (mpTimeDiv->cbStruct != sizeof(MIDIPROPTIMEDIV))
				return MMSYSERR_INVALPARAM;

			if (mpFlags & MIDIPROP_SET)
				StreamPlayer->SetTicksPerQN(mpTimeDiv->dwTimeDiv);
			else if (mpFlags & MIDIPROP_GET)
				mpTimeDiv->dwTimeDiv = StreamPlayer->GetTicksPerQN();
		}
		else return MMSYSERR_INVALPARAM;

		return MMSYSERR_NOERROR;
	}

	case MODM_GETPOS:
	{
		if (StreamPlayer == nullptr)
			return MIDIERR_NOTREADY;

		if (!Param1)
			return MMSYSERR_INVALPARAM;

		if (Param2 != sizeof(MMTIME))
			return MMSYSERR_INVALPARAM;

		StreamPlayer->GetPosition((MMTIME*)Param1);

		return MMSYSERR_NOERROR;
	}

	case MODM_RESTART:
		if (StreamPlayer == nullptr)
			return MMSYSERR_NOTENABLED;

		StreamPlayer->Start();
		SynthModule->PlayShortEvent(0x0101FF);

		return MMSYSERR_NOERROR;

	case MODM_PAUSE:
		if (StreamPlayer == nullptr)
			return MMSYSERR_NOTENABLED;

		StreamPlayer->Stop();

		return MMSYSERR_NOERROR;

	case MODM_STOP:
		if (StreamPlayer == nullptr)
			return MMSYSERR_NOTENABLED;

		if (StreamPlayer->EmptyQueue()) {
			SynthModule->PlayShortEvent(0x0101FF);
			return MMSYSERR_NOERROR;
		}

		return MMSYSERR_NOERROR;

	case DRVM_INIT:
	case DRVM_EXIT:
	case DRVM_ENABLE:
	case DRVM_DISABLE:
	case MODM_CACHEPATCHES:
	case MODM_CACHEDRUMPATCHES:
	case DRV_QUERYDEVICEINTERFACESIZE:
	case DRV_QUERYDEVICEINTERFACE:
		return MMSYSERR_NOERROR;

	case MODM_SETVOLUME:
	case MODM_GETVOLUME:
	default:
		return MMSYSERR_NOTSUPPORTED;
	}
}

int WINAPI IsKDMAPIAvailable() {
	return (int)WDMSettings->KDMAPIEnabled;
}

int WINAPI InitializeKDMAPIStream() {
	freeAudioRenderer();
	if (getAudioRenderer()) {
		if (startAudioRenderer()) {
			return 1;
		}
	}

	return 0;
}

int WINAPI TerminateKDMAPIStream() {
	if (stopAudioRenderer()) {
		freeAudioRenderer();
		return 1;
	}

	LOG(WDMErr, "TerminateKDMAPIStream failed.");
	return 0;
}

void WINAPI ResetKDMAPIStream() {
	SynthModule->PlayShortEvent(0x010101FF);
}

void WINAPI SendDirectData(unsigned int ev) {
	SynthModule->PlayShortEvent(ev);
}

void WINAPI SendDirectDataNoBuf(unsigned int ev) {
	// Unsupported, forward to SendDirectData
	SendDirectData(ev);
}

unsigned int WINAPI SendDirectLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
	return modMessage(0, MODM_LONGDATA, 0, (DWORD_PTR)IIMidiHdr, (DWORD_PTR)IIMidiHdrSize);
}

unsigned int WINAPI SendDirectLongDataNoBuf(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
	fDriverCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)IIMidiHdr, 0);
	return SynthModule->PlayLongEvent(IIMidiHdr->lpData, IIMidiHdr->dwBytesRecorded) == SYNTH_OK ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;
}

unsigned int WINAPI PrepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
	return modMessage(0, MODM_PREPARE, 0, (DWORD_PTR)IIMidiHdr, (DWORD_PTR)IIMidiHdrSize);
}

unsigned int WINAPI UnprepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
	return modMessage(0, MODM_UNPREPARE, 0, (DWORD_PTR)IIMidiHdr, (DWORD_PTR)IIMidiHdrSize);
}

int WINAPI InitializeCallbackFeatures(HMIDI OMHM, DWORD_PTR OMCB, DWORD_PTR OMI, DWORD_PTR OMU, DWORD OMCM) {
	MIDIOPENDESC MidiP;

	MidiP.hMidi = OMHM;
	MidiP.dwCallback = OMCB;
	MidiP.dwInstance = OMI;

	if (!fDriverCallback->PrepareCallbackFunction(&MidiP, OMCM))
		return FALSE;

	fDriverCallback->CallbackFunction(MOM_OPEN, 0, 0);
	return TRUE;
}

void WINAPI RunCallbackFunction(DWORD msg, DWORD_PTR p1, DWORD_PTR p2) {
	fDriverCallback->CallbackFunction(msg, p1, p2);
}

int WINAPI SendCustomEvent(unsigned int evt, unsigned int chan, unsigned int param) {
	return SynthModule->TalkToSynthDirectly(evt, chan, param);
}

int WINAPI DriverSettings(unsigned int setting, unsigned int mode, void* value, unsigned int cbValue) {
	if (setting == KDMAPI_STREAM) {
		if (fDriverCallback->IsCallbackReady()) {
			StreamPlayer = new OmniMIDI::StreamPlayer(SynthModule, fDriverCallback);
			LOG(WDMErr, "StreamPlayer allocated.");

			if (!StreamPlayer)
			{
				freeAudioRenderer();
				return -1;
			}

			LOG(WDMErr, "StreamPlayer address: %x", StreamPlayer);
		}
	}

	return SynthModule->SettingsManager(setting, (bool)mode, value, (size_t)cbValue);
}

unsigned long long WINAPI timeGetTime64() {
	signed long long CurrentTime;
	MiscFuncs.querySystemTime(&CurrentTime);
	return (unsigned long long)((CurrentTime)-TickStart) / 10000.0;
}