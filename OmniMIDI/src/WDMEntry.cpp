/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#include "WDMEntry.hpp"

static ErrorSystem::Logger* ErrLog = nullptr;
static WinDriver::DriverCallback* fDriverCallback = nullptr;
static WinDriver::DriverComponent* DriverComponent = nullptr;
static WinDriver::DriverMask* DriverMask = nullptr;
static OmniMIDI::SynthHost* Host = nullptr;
static OMShared::Funcs Utils;
static signed long long TickStart = 0;

extern "C" {	
	bool InitializeWDM(HMODULE hModule);
	bool FreeWDM();

	EXPORT BOOL WINAPI DllMain(HMODULE hModule, DWORD ReasonForCall, LPVOID lpReserved) {
		switch (ReasonForCall)
		{
		case DLL_PROCESS_ATTACH:
			if (!hModule)
				return FALSE;

			return InitializeWDM(hModule);

		case DLL_PROCESS_DETACH:
			if (lpReserved == nullptr) {
				return FreeWDM();
			}

			break;

		default:
			break;
		}

		return TRUE;
	}

	bool InitializeWDM(HMODULE hModule) {
		if (!ErrLog) {
			ErrLog = new ErrorSystem::Logger;

#if defined(_DEBUG) || defined(VERBOSE_LOG)
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
		}

		if (!DriverComponent) {
			DriverComponent = new WinDriver::DriverComponent(ErrLog);

			if ((DriverComponent->SetLibraryHandle(hModule))) {
				if (!TickStart) {
					if (!(Utils.QuerySystemTime(&TickStart) == 0)) {
						Error("Failed to run NtQuerySystemTime.", true);
						delete DriverComponent;
						return false;
					}
				}

				if (!(DriverMask = new WinDriver::DriverMask(ErrLog))) {
					Error("Failed to allocate DriverMask.", true);
					return FALSE;
				}

				if (!(fDriverCallback = new WinDriver::DriverCallback(ErrLog))) {
					Error("Failed to allocate DriverCallback.", true);
					return false;
				}		
			}

			if (Host == nullptr) {
				if (!(Host = new OmniMIDI::SynthHost(fDriverCallback, hModule, ErrLog))) {
					Error("Failed to allocate SynthHost.", true);
					return false;
				}	
			}

			if (ErrLog && DriverComponent && Host) {
				Message("DriverComponent >> %08x", DriverComponent);
				Message("DriverMask >> %08x", DriverMask);
				Message("DriverCallback >> %08x", fDriverCallback);
				Message("SynthHost >> %08x", Host);
			}
		}

		return true;
	}

	bool FreeWDM() {
		if (DriverComponent) {
			delete Host;
			delete fDriverCallback;
			delete DriverMask;

			if (!DriverComponent->SetLibraryHandle())
				Fatal("What the hoy?");

			delete DriverComponent;
			DriverComponent = nullptr;
		}

		if (ErrLog) {
#if defined(_DEBUG) || defined(VERBOSE_LOG)
			FreeConsole();
#endif
			delete ErrLog;
			ErrLog = nullptr;
		}

		return true;
	}

	EXPORT LRESULT WINAPI DriverProc(DWORD DriverIdentifier, HDRVR DriverHandle, UINT Message, LONG Param1, LONG Param2) {
		bool v = false;

		switch (Message) {
		case DRV_OPEN:
			v = DriverComponent->SetDriverHandle(DriverHandle);
			Message("->SetDriverHandle(...) returned %d", v);
			return v;

		case DRV_CLOSE:
			v = DriverComponent->SetDriverHandle();
			Message("->SetDriverHandle() returned %d", v);
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

		unsigned long r = fDriverCallback->ProcessMessage(DriverIdentifier, DriverHandle, Message, Param1, Param2);
		Message("ProcessMessage returned %d", r);
		return r;
	}

	EXPORT MMRESULT WINAPI modMessage(UINT DeviceID, UINT Message, DWORD_PTR UserPointer, DWORD_PTR Param1, DWORD_PTR Param2) {
		switch (Message) {
		case MODM_DATA:
			Host->PlayShortEvent(Param1);
			return MMSYSERR_NOERROR;

		case MODM_LONGDATA:
		{
			MIDIHDR* mhdr = (MIDIHDR*)Param1;
			DWORD hdrLen = (DWORD)Param2;

			if (hdrLen < offsetof(MIDIHDR, dwOffset) ||
				!mhdr || !mhdr->lpData ||
				mhdr->dwBufferLength < mhdr->dwBytesRecorded)
			{
				Message("SysEx event 0x%08x is invalid. (lpData: 0x%08x, dwBL: %d, dwBR: %d, dwBR4: %d, cbSize: 0x%08x)",
					mhdr->lpData, mhdr->dwBufferLength, mhdr->dwBytesRecorded, mhdr->dwBytesRecorded % 4, Param1, Param2);
				return MMSYSERR_INVALPARAM;
			}

			if (!(mhdr->dwFlags & MHDR_PREPARED))
			{
				fDriverCallback->CallbackFunction(0, 0, 0xFEEDF00D);
				Message("Stream data 0x%08x has not been prepared.", Param1, Param2);
				return MIDIERR_UNPREPARED;
			}

			if (!(mhdr->dwFlags & MHDR_DONE))
			{
				if (mhdr->dwFlags & MHDR_INQUEUE) {
					Message("SysEx event 0x%08x is still in queue for StreamPlayer.", Param1, Param2);
					return MIDIERR_STILLPLAYING;
				}
			}

			auto ret = Host->PlayLongEvent(mhdr->lpData, mhdr->dwBufferLength);
			if (ret) {
				switch (ret) {
				case SYNTH_INVALPARAM:
					Message("Invalid SysEx event. (Buf = 0x%08x, SynthResult = %d, bufsize = %d)", Param1, ret, mhdr->dwBufferLength);
					return MMSYSERR_INVALPARAM;
				default:
					Message("No idea! 0x%08x", Param1);
					return MMSYSERR_ERROR;
				}
			}

			fDriverCallback->CallbackFunction(MOM_DONE, 0, (DWORD_PTR)mhdr);

			return MMSYSERR_NOERROR;
		}

		case MODM_STRMDATA:
		{
			MIDIHDR* mhdr = (MIDIHDR*)Param1;
			DWORD hdrLen = (DWORD)Param2;

			if (!Host->IsStreamPlayerAvailable())
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

			if (!Host->SpAddToQueue(mhdr))
				return MIDIERR_STILLPLAYING;

			return MMSYSERR_NOERROR;
		}

		case MODM_PREPARE:
		{
			MIDIHDR* mhdr = (MIDIHDR*)Param1;
			uint32_t mhdrSize = (uint32_t)Param2;

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
#ifdef WINXPMODE
				return MMSYSERR_NOMEM;
#else	
				if (!VirtualLock(mhdr->lpData, mhdr->dwBufferLength))
					return MMSYSERR_NOMEM;
#endif
			}

			mhdr->dwFlags |= MHDR_PREPARED;
			return MMSYSERR_NOERROR;
		}

		case MODM_UNPREPARE:
		{
			MIDIHDR* mhdr = (MIDIHDR*)Param1;
			uint32_t mhdrSize = (uint32_t)Param2;

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
#ifndef WINXPMODE
				if (!VirtualUnlock(mhdr->lpData, mhdr->dwBufferLength))
				{
					const uint32_t err = GetLastError();

					if (err != 0x9E) {
						exit(-1);
					}
				}
#endif
				
			}

			mhdr->dwFlags &= ~MHDR_PREPARED;
			return MMSYSERR_NOERROR;
		}

		case MODM_RESET:
			if (Host->IsStreamPlayerAvailable())
				Host->SpResetQueue();

			return (Host->Reset() == SYNTH_OK) ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;

		case MODM_OPEN:
		{
			LPMIDIOPENDESC midiOpenDesc = reinterpret_cast<LPMIDIOPENDESC>(Param1);
			DWORD callbackMode = (DWORD)Param2;

			if (Host->Start(callbackMode & MIDI_IO_COOKED)) {
				if (fDriverCallback->PrepareCallbackFunction(midiOpenDesc, callbackMode)) {
					Message("PrepareCallbackFunction done.");
				}

				Message("WinMM initialized.");
				return MMSYSERR_NOERROR;
			}

			Error("Failed to initialize synthesizer.", true);
			return MMSYSERR_NOMEM;
		}


		case MODM_CLOSE:
			if (Host->Stop()) {
				Message("WinMM freed.");
				return MMSYSERR_NOERROR;
			}

			Message("Failed to free synthesizer.");
			return MMSYSERR_ERROR;

		case MODM_GETNUMDEVS:
			Message("MODM_GETNUMDEVS");
			return Host->IsBlacklistedProcess() ? 0 : 1;

		case MODM_GETDEVCAPS:
			return DriverMask->GiveCaps(DeviceID, (PVOID)Param1, (DWORD)Param2);

		case MODM_PROPERTIES:
		{
			MIDIPROPTEMPO* mpTempo = (MIDIPROPTEMPO*)Param1;
			MIDIPROPTIMEDIV* mpTimeDiv = (MIDIPROPTIMEDIV*)Param1;
			DWORD mpFlags = (DWORD)Param2;

			if (!Host->IsStreamPlayerAvailable())
				return MIDIERR_NOTREADY;

			if (!(mpFlags & (MIDIPROP_GET | MIDIPROP_SET)))
				return MMSYSERR_INVALPARAM;

			if (mpFlags & MIDIPROP_TEMPO)
			{
				if (mpTempo->cbStruct != sizeof(MIDIPROPTEMPO))
					return MMSYSERR_INVALPARAM;

				if (mpFlags & MIDIPROP_SET)
					Host->SpSetTempo(mpTempo->dwTempo);
				else if (mpFlags & MIDIPROP_GET)
					mpTempo->dwTempo = Host->SpGetTempo();
			}
			else if (mpFlags & MIDIPROP_TIMEDIV) {
				if (mpTimeDiv->cbStruct != sizeof(MIDIPROPTIMEDIV))
					return MMSYSERR_INVALPARAM;

				if (mpFlags & MIDIPROP_SET)
					Host->SpSetTicksPerQN(mpTimeDiv->dwTimeDiv);
				else if (mpFlags & MIDIPROP_GET)
					mpTimeDiv->dwTimeDiv = Host->SpGetTicksPerQN();
			}
			else return MMSYSERR_INVALPARAM;

			return MMSYSERR_NOERROR;
		}

		case MODM_GETPOS:
		{
			if (!Host->IsStreamPlayerAvailable())
				return MIDIERR_NOTREADY;

			if (!Param1)
				return MMSYSERR_INVALPARAM;

			if (Param2 != sizeof(MMTIME))
				return MMSYSERR_INVALPARAM;

			Host->SpGetPosition((MMTIME*)Param1);

			return MMSYSERR_NOERROR;
		}

		case MODM_RESTART:
			if (!Host->IsStreamPlayerAvailable())
				return MMSYSERR_NOTENABLED;

			Host->SpStart();
			Host->PlayShortEvent(0xFF, 0x01, 0x01);

			return MMSYSERR_NOERROR;

		case MODM_PAUSE:
			if (!Host->IsStreamPlayerAvailable())
				return MMSYSERR_NOTENABLED;

			Host->SpStop();

			return MMSYSERR_NOERROR;

		case MODM_STOP:
			if (!Host->IsStreamPlayerAvailable())
				return MMSYSERR_NOTENABLED;

			if (Host->SpEmptyQueue()) {
				Host->PlayShortEvent(0xFF, 0x01, 0x01);
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

	EXPORT int32_t WINAPI IsKDMAPIAvailable() {
		return (int32_t)Host->IsKDMAPIAvailable();
	}

	EXPORT int32_t WINAPI InitializeKDMAPIStream() {
		Message("Called init function.");

		if (Host->Start()) {
			Message("KDMAPI initialized.");
			return 1;
		}

		Message("KDMAPI failed to initialize.");
		return 0;
	}

	EXPORT int32_t WINAPI TerminateKDMAPIStream() {
		Message("Called free function.");

		if (Host->Stop()) {
			Message("KDMAPI freed.");
			return 1;
		}

		Message("KDMAPI failed to free its resources.");
		return 0;
	}

	EXPORT void WINAPI ResetKDMAPIStream() {
		Host->PlayShortEvent(0xFF, 0x01, 0x01);
	}

	EXPORT void WINAPI SendDirectData(uint32_t ev) {
		Host->PlayShortEvent(ev);
	}

	EXPORT uint32_t WINAPI SendDirectDataNoBuf(uint32_t ev) {
		// Unsupported, forward to SendDirectData
		SendDirectData(ev);
		return 0;
	}

	EXPORT uint32_t WINAPI SendDirectLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		fDriverCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)IIMidiHdr, 0);
		return Host->PlayLongEvent(IIMidiHdr->lpData, IIMidiHdr->dwBytesRecorded) == SYNTH_OK ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;
	}

	EXPORT uint32_t WINAPI SendDirectLongDataNoBuf(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// Unsupported, forward to SendDirectLongData
		return SendDirectLongData(IIMidiHdr, IIMidiHdrSize);
	}

	EXPORT uint32_t WINAPI PrepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// not needed with KDMAPI
		return 0;
	}

	EXPORT uint32_t WINAPI UnprepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// not needed with KDMAPI
		return 0;
	}

	EXPORT int32_t WINAPI InitializeCallbackFeatures(HMIDI OMHM, DWORD_PTR OMCB, DWORD_PTR OMI, DWORD_PTR OMU, DWORD OMCM) {
		MIDIOPENDESC MidiP;

		MidiP.hMidi = OMHM;
		MidiP.dwCallback = OMCB;
		MidiP.dwInstance = OMI;

		if (!fDriverCallback->PrepareCallbackFunction(&MidiP, OMCM))
			return FALSE;

		fDriverCallback->CallbackFunction(MOM_OPEN, 0, 0);
		return TRUE;
	}

	EXPORT void WINAPI RunCallbackFunction(DWORD msg, DWORD_PTR p1, DWORD_PTR p2) {
		fDriverCallback->CallbackFunction(msg, p1, p2);
	}

	EXPORT int32_t WINAPI SendCustomEvent(uint32_t evt, uint32_t chan, uint32_t param) {
		return Host->TalkToSynthDirectly(evt, chan, param);
	}

	EXPORT int32_t WINAPI DriverSettings(uint32_t setting, uint32_t mode, void* value, uint32_t cbValue) {
		if (setting == KDMAPI_STREAM) {
			if (fDriverCallback->IsCallbackReady()) {
				if (!Host->SpInit()) {
					Host->Stop();
					return -1;
				}
			}
		}

		return Host->SettingsManager(setting, (bool)mode, value, (size_t)cbValue);
	}

	EXPORT int32_t WINAPI LoadCustomSoundFontsList(LPWSTR Directory) {
		return 1;
	}

	EXPORT uint64_t WINAPI timeGetTime64() {
		int64_t CurrentTime;
		Utils.QuerySystemTime(&CurrentTime);
		return (uint64_t)((CurrentTime-TickStart) / 10000.0);
	}

	EXPORT float WINAPI GetRenderingTime() {
		if (Host == nullptr)
			return 0.0f;

		return Host->GetRenderingTime();
	}

	EXPORT uint64_t WINAPI GetVoiceCount() {
		if (Host == nullptr)
			return 0;

		return Host->GetActiveVoices();
	}
}

#endif