/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#ifdef _WIN64
#define EXPORT	__declspec(dllexport) 
#define APICALL
#else
#define EXPORT
#define APICALL __stdcall
#endif

#include "WDMEntry.hpp"

static ErrorSystem::Logger* ErrLog = nullptr;
static WinDriver::DriverCallback* fDriverCallback = nullptr;
static WinDriver::DriverComponent* DriverComponent = nullptr;
static WinDriver::DriverMask* DriverMask = nullptr;
static OmniMIDI::SynthHost* Host = nullptr;
static OMShared::Funcs MiscFuncs;
static signed long long TickStart = 0;

extern "C" {
	EXPORT int APICALL DllMain(HMODULE hModule, DWORD ReasonForCall, LPVOID lpReserved) {
		BOOL ret = FALSE;
		switch (ReasonForCall)
		{
		case DLL_PROCESS_ATTACH:
			if (!ErrLog) 
				ErrLog = new ErrorSystem::Logger;

			if (!DriverComponent) {
				DriverComponent = new WinDriver::DriverComponent(ErrLog);

				if ((ret = DriverComponent->SetLibraryHandle(hModule)) == true) {
					if (!TickStart) {
						if (!(MiscFuncs.querySystemTime(&TickStart) == 0)) {
							delete DriverComponent;
							return FALSE;
						}
					}

#if defined(_DEBUG) || defined(WIN7VERBOSE)
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

					DriverMask = new WinDriver::DriverMask(ErrLog);
					fDriverCallback = new WinDriver::DriverCallback(ErrLog);

					// Allocate a generic dummy synth for now
					Host = new OmniMIDI::SynthHost(fDriverCallback, hModule, ErrLog);
				}
			}

			break;

		case DLL_PROCESS_DETACH:
			if (DriverComponent) {
#if defined(_DEBUG) || defined(WIN7VERBOSE)
				FreeConsole();
#endif

				delete Host;
				delete fDriverCallback;
				delete DriverMask;

				ret = DriverComponent->UnsetLibraryHandle();

				delete DriverComponent;
				DriverComponent = nullptr;
			}

			if (ErrLog) {
				delete ErrLog;
				ErrLog = nullptr;
			}

			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
		}

		return ret;
	}

	EXPORT long APICALL DriverProc(DWORD DriverIdentifier, HDRVR DriverHandle, UINT Message, LONG Param1, LONG Param2) {
		bool v = false;

		switch (Message) {
		case DRV_OPEN:
			v = DriverComponent->SetDriverHandle(DriverHandle);
			LOG("->SetDriverHandle(...) returned %d", v);
			return v;

		case DRV_CLOSE:
			v = DriverComponent->UnsetDriverHandle();
			LOG("->UnsetDriverHandle() returned %d", v);
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
		LOG("DefDriverProc returned %d", r);
		return r;
	}

	EXPORT MMRESULT APICALL modMessage(UINT DeviceID, UINT Message, DWORD_PTR UserPointer, DWORD_PTR Param1, DWORD_PTR Param2) {
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
				LOG("SysEx event 0x%08x is invalid. (lpData: 0x%08x, dwBL: %d, dwBR: %d, dwBR4: %d, cbSize: 0x%08x)",
					mhdr->lpData, mhdr->dwBufferLength, mhdr->dwBytesRecorded, mhdr->dwBytesRecorded % 4, Param1, Param2);
				return MMSYSERR_INVALPARAM;
			}

			if (!(mhdr->dwFlags & MHDR_PREPARED))
			{
				fDriverCallback->CallbackFunction(0, 0, 0xFEEDF00D);
				LOG("Stream data 0x%08x has not been prepared.", Param1, Param2);
				return MIDIERR_UNPREPARED;
			}

			if (!(mhdr->dwFlags & MHDR_DONE))
			{
				if (mhdr->dwFlags & MHDR_INQUEUE) {
					LOG("SysEx event 0x%08x is still in queue for StreamPlayer.", Param1, Param2);
					return MIDIERR_STILLPLAYING;
				}
			}

			auto ret = Host->PlayLongEvent(mhdr->lpData, mhdr->dwBufferLength);
			if (ret) {
				switch (ret) {
				case SYNTH_INVALPARAM:
					LOG("Invalid SysEx event. (Buf = 0x%08x, SynthResult = %d, bufsize = %d)", Param1, ret, mhdr->dwBufferLength);
					return MMSYSERR_INVALPARAM;
				default:
					LOG("No idea! 0x%08x", Param1);
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
#ifndef WINXPMODE
				if (!VirtualUnlock(mhdr->lpData, mhdr->dwBufferLength))
				{
					const unsigned int err = GetLastError();

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
					LOG("PrepareCallbackFunction done.");
				}

				LOG("WinMM initialized.");
				return MMSYSERR_NOERROR;
			}

			NERROR("Failed to initialize synthesizer.", true);
			return MMSYSERR_NOMEM;
		}


		case MODM_CLOSE:
			if (Host->Stop()) {
				LOG("WinMM freed.");
				return MMSYSERR_NOERROR;
			}

			LOG("Failed to free synthesizer.");
			return MMSYSERR_ERROR;

		case MODM_GETNUMDEVS:
			LOG("MODM_GETNUMDEVS");
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

	EXPORT int APICALL IsKDMAPIAvailable() {
		return (int)Host->IsKDMAPIAvailable();
	}

	EXPORT int APICALL InitializeKDMAPIStream() {
		if (Host->Start()) {
			LOG("KDMAPI initialized.");
			return 1;
		}

		LOG("KDMAPI failed to initialize.");
		return 0;
	}

	EXPORT int APICALL TerminateKDMAPIStream() {
		if (Host->Stop()) {
			LOG("KDMAPI freed.");
			return 1;
		}

		LOG("KDMAPI failed to free its resources.");
		return 0;
	}

	EXPORT void APICALL ResetKDMAPIStream() {
		Host->PlayShortEvent(0xFF, 0x01, 0x01);
	}

	EXPORT void APICALL SendDirectData(unsigned int ev) {
		Host->PlayShortEvent(ev);
	}

	EXPORT void APICALL SendDirectDataNoBuf(unsigned int ev) {
		// Unsupported, forward to SendDirectData
		SendDirectData(ev);
	}

	EXPORT unsigned int APICALL SendDirectLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		fDriverCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)IIMidiHdr, 0);
		return Host->PlayLongEvent(IIMidiHdr->lpData, IIMidiHdr->dwBytesRecorded) == SYNTH_OK ? MMSYSERR_NOERROR : MMSYSERR_INVALPARAM;
	}

	EXPORT unsigned int APICALL SendDirectLongDataNoBuf(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// Unsupported, forward to SendDirectLongData
		return SendDirectLongData(IIMidiHdr, IIMidiHdrSize);
	}

	EXPORT unsigned int APICALL PrepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// not needed with KDMAPI
		return 0;
	}

	EXPORT unsigned int APICALL UnprepareLongData(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize) {
		// not needed with KDMAPI
		return 0;
	}

	EXPORT int APICALL InitializeCallbackFeatures(HMIDI OMHM, DWORD_PTR OMCB, DWORD_PTR OMI, DWORD_PTR OMU, DWORD OMCM) {
		MIDIOPENDESC MidiP;

		MidiP.hMidi = OMHM;
		MidiP.dwCallback = OMCB;
		MidiP.dwInstance = OMI;

		if (!fDriverCallback->PrepareCallbackFunction(&MidiP, OMCM))
			return FALSE;

		fDriverCallback->CallbackFunction(MOM_OPEN, 0, 0);
		return TRUE;
	}

	EXPORT void APICALL RunCallbackFunction(DWORD msg, DWORD_PTR p1, DWORD_PTR p2) {
		fDriverCallback->CallbackFunction(msg, p1, p2);
	}

	EXPORT int APICALL SendCustomEvent(unsigned int evt, unsigned int chan, unsigned int param) {
		return Host->TalkToSynthDirectly(evt, chan, param);
	}

	EXPORT int APICALL DriverSettings(unsigned int setting, unsigned int mode, void* value, unsigned int cbValue) {
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

	EXPORT unsigned long long WINAPI timeGetTime64() {
		signed long long CurrentTime;
		MiscFuncs.querySystemTime(&CurrentTime);
		return (unsigned long long)((CurrentTime)-TickStart) / 10000.0;
	}

	EXPORT float GetRenderingTime() {
		if (Host == nullptr)
			return 0.0f;

		return Host->GetRenderingTime();
	}

	EXPORT unsigned int GetActiveVoices() {
		if (Host == nullptr)
			return 0;

		return Host->GetActiveVoices();
	}

#ifdef WIN32
	// Internal benchmark tools for myself!!!

	EXPORT void WINAPI BufferThroughput(HWND hwnd, HINSTANCE hinst, LPWSTR pszCmdLine, int nCmdShow) {
#ifndef _DEBUG
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
		else exit(0);
#endif

		volatile bool stop = true;
		volatile bool waitThread = false;

		std::vector<size_t> mesS, mesR, mesW;
		size_t avgS = 0, avgR = 0, avgW = 0;
		std::atomic<size_t> senderBufProc = 0, receiverBufProc = 0, waitingBufProc = 0;

		OMShared::Funcs d;
		OmniMIDI::EvBuf* buffer = new OmniMIDI::EvBuf(0x7FFFFF);
		int count = 0;
		int interval = 5;

		std::jthread sender;
		std::jthread receiver;
		std::jthread checker;
		std::stop_source ssource;

		sender = std::jthread([&, stoken = ssource.get_token()]() {
			while (stop) d.uSleep(-1);

			while (!stoken.stop_requested()) {
				while (waitThread);

				buffer->Write(0x10, 0x10, 0x10);
				senderBufProc++;
			}
			});

		receiver = std::jthread([&, stoken = ssource.get_token()]() {
			while (stop) d.uSleep(-1);

			while (!stoken.stop_requested()) {
				while (waitThread);

				if (buffer->Read())
					receiverBufProc++;
				else
					waitingBufProc++;
			}
			});

		checker = std::jthread([&, stoken = ssource.get_token()]() {
			while (stop) d.uSleep(-1);

			while (!stoken.stop_requested()) {
				d.uSleep(-10000000);
				waitThread = true;

				auto s = (size_t)senderBufProc;
				auto r = (size_t)receiverBufProc;
				auto w = (size_t)waitingBufProc;
				mesS.push_back(s);
				mesR.push_back(r);
				mesW.push_back(w);

				LOG("sent %llu - received %llu (lost %llu)", mesS[count] * 3, mesR[count] * 3, mesW[count] * 3);
				count++;

				senderBufProc = 0;
				receiverBufProc = 0;
				waitingBufProc = 0;

				waitThread = false;
			}
			});

		MessageBoxA(NULL, "Press OK to start benchmark", "OMB", MB_OK | MB_SYSTEMMODAL);

		stop = false;
		Sleep(interval * 1000);
		ssource.request_stop();
		sender.join();
		receiver.join();
		checker.join();

		for (int i = 0; i < mesS.size(); i++) {
			avgS += mesS[i];
			avgR += mesR[i];
			avgW += mesW[i];
		}

		auto totalS = avgS;
		auto totalR = avgR;
		auto totalW = avgW;
		auto potential = totalR + totalW;
		avgS = avgS / mesS.size();
		avgR = avgR / mesS.size();
		avgW = avgW / mesW.size();
		auto avgPotential = avgR + avgW;

		int perc = 10000 - (totalW * 10000 + potential / 2) / potential;

		char* Text = new char[256];

		sprintf(Text, "Sent %llu ev/sec, and processed %llu ev/sec (%llu/%llu ?%llu). The theoretical maximum could be %llu ev/sec. (Loss percentage is %.2f%%)", avgS * 3, avgR * 3, totalS * 3, totalR * 3, (long long)(totalR - totalS) * 3, avgPotential, 100.0f - (perc / 100.0f));
		LOG(Text);
		MessageBoxA(NULL, Text, "OMB", MB_OK | MB_SYSTEMMODAL);
	}


#endif
}

#endif