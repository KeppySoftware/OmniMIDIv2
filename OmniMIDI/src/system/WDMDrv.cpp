/*
* 
	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#include "WDMDrv.hpp"

bool WinDriver::DriverMask::ChangeSettings(short NMID, short NPID, short NT, short NS) {
	// TODO: Check if the values contain valid technologies/support flags

	this->ManufacturerID = NMID;
	this->ProductID = NPID;
	this->Technology = NT;
	this->Support = NS;

	return true;
}

unsigned long WinDriver::DriverMask::GiveCaps(UINT DeviceIdentifier, PVOID CapsPointer, DWORD CapsSize) {
	MIDIOUTCAPSA CapsA;
	MIDIOUTCAPSW CapsW;
	MIDIOUTCAPS2A Caps2A;
	MIDIOUTCAPS2W Caps2W;

	// Why would this happen? Stupid MIDI app dev smh
	if (CapsPointer == nullptr)
	{
		Error("A null pointer has been passed to the function. The driver can't share its info with the application.", false);
		return MMSYSERR_INVALPARAM;
	}

	if (CapsSize == 0) {
		Error("CapsSize has a value of 0, how is the driver supposed to determine the subtype of the struct?", false);
		return MMSYSERR_INVALPARAM;
	}

	// I have to support all this s**t or else it won't work in some apps smh
	switch (CapsSize) {
	case (sizeof(MIDIOUTCAPSA)):
		strncpy_s(CapsA.szPname, NAME, MAXPNAMELEN);
		CapsA.dwSupport = this->Support;
		CapsA.wChannelMask = 0xFFFF;
		CapsA.wMid = this->ManufacturerID;
		CapsA.wPid = this->ProductID;
		CapsA.wTechnology = this->Technology;
		CapsA.wNotes = 65535;
		CapsA.wVoices = 65535;
		CapsA.vDriverVersion = MAKEWORD(6, 2);
		memcpy((LPMIDIOUTCAPSA)CapsPointer, &CapsA, fmin(CapsSize, sizeof(CapsA)));
		Message("MIDIOUTCAPSA given to app.");
		break;

	case (sizeof(MIDIOUTCAPSW)):
		wcsncpy_s(CapsW.szPname, WNAME, MAXPNAMELEN);
		CapsW.dwSupport = this->Support;
		CapsW.wChannelMask = 0xFFFF;
		CapsW.wMid = this->ManufacturerID;
		CapsW.wPid = this->ProductID;
		CapsW.wTechnology = this->Technology;
		CapsW.wNotes = 65535;
		CapsW.wVoices = 65535;
		CapsW.vDriverVersion = MAKEWORD(6, 2);
		memcpy((LPMIDIOUTCAPSW)CapsPointer, &CapsW, fmin(CapsSize, sizeof(CapsW)));
		Message("MIDIOUTCAPSW given to app.");
		break;

	case (sizeof(MIDIOUTCAPS2A)):
		strncpy_s(Caps2A.szPname, NAME, MAXPNAMELEN);
		Caps2A.dwSupport = this->Support;
		Caps2A.wChannelMask = 0xFFFF;
		Caps2A.wMid = this->ManufacturerID;
		Caps2A.wPid = this->ProductID;
		Caps2A.wTechnology = this->Technology;
		Caps2A.wNotes = 65535;
		Caps2A.wVoices = 65535;
		Caps2A.vDriverVersion = MAKEWORD(6, 2);
		memcpy((LPMIDIOUTCAPS2A)CapsPointer, &Caps2A, fmin(CapsSize, sizeof(Caps2A)));
		Message("MIDIOUTCAPS2A given to app.");
		break;

	case (sizeof(MIDIOUTCAPS2W)):
		wcsncpy_s(Caps2W.szPname, WNAME, MAXPNAMELEN);
		Caps2W.dwSupport = this->Support;
		Caps2W.wChannelMask = 0xFFFF;
		Caps2W.wMid = this->ManufacturerID;
		Caps2W.wPid = this->ProductID;
		Caps2W.wTechnology = this->Technology;
		Caps2W.wNotes = 65535;
		Caps2W.wVoices = 65535;
		Caps2W.vDriverVersion = MAKEWORD(6, 2);
		memcpy((LPMIDIOUTCAPS2W)CapsPointer, &Caps2W, fmin(CapsSize, sizeof(Caps2W)));
		Message("MIDIOUTCAPS2W given to app.");
		break;

	default:
		// ???????
		Error("Size passed to CapsSize does not match any valid MIDIOUTCAPS struct.", false);
		return MMSYSERR_INVALPARAM;

	}

	return MMSYSERR_NOERROR;
}

WinDriver::DriverCallback::DriverCallback(ErrorSystem::Logger* PErr) {
	ErrLog = PErr;

	auto winmmHandle = LibFuncs::GetLibraryAddress("winmm");
	if (winmmHandle) {
		pDrvProc = (drvDefDriverProc)LibFuncs::GetFuncAddress(winmmHandle, "DefDriverProc");
	}
}

bool WinDriver::DriverCallback::IsCallbackReady() {
	return (pCallback != nullptr);
}

bool WinDriver::DriverCallback::PrepareCallbackFunction(MIDIOPENDESC* OpInfStruct, DWORD CallbackMode) {
	unsigned int cMode = CallbackMode & CALLBACK_TYPEMASK;

	if (pCallback) {
		Error("A callback has already been allocated!", false);
		return false;
	}

	// Check if the app wants the driver to do callbacks
	if (cMode != CALLBACK_NULL) {
		pCallback = new Callback{
			.Handle = (HMIDIOUT)OpInfStruct->hMidi,
			.Mode = cMode,
			.funcPtr = OpInfStruct->dwCallback != 0 ? reinterpret_cast<midiOutProc>(OpInfStruct->dwCallback) : nullptr,
			.Instance = (cMode != CALLBACK_WINDOW && cMode != CALLBACK_THREAD) ? OpInfStruct->dwInstance : 0
		};

		CallbackFunction(MOM_OPEN, 0, 0);

		Message(".Handle -> %x - .Mode -> %x - .Ptr -> %x - .Instance -> %x", pCallback->Handle, pCallback->Mode, *pCallback->funcPtr, pCallback->Instance);
	}

	return true;
}

bool WinDriver::DriverCallback::ClearCallbackFunction() {
	// Clear the callback
	if (pCallback)
	{
		delete pCallback;
		pCallback = nullptr;
	}

	return true;
}

void WinDriver::DriverCallback::CallbackFunction(DWORD Message, DWORD_PTR Arg1, DWORD_PTR Arg2) {
	if (!pCallback)
		return;

	switch (pCallback->Mode & CALLBACK_TYPEMASK) {

	case CALLBACK_FUNCTION:	// Use a custom function to notify the app
		// Use the app's custom function to send the callback
		if (*pCallback->funcPtr)
			pCallback->funcPtr(pCallback->Handle, Message, pCallback->Instance, Arg1, Arg2);
		break;

	case CALLBACK_EVENT:	// Set an event to notify the app
		SetEvent((HANDLE)(*pCallback->funcPtr));
		break;

	case CALLBACK_TASK:		// Send a message to a thread to notify the app
		PostThreadMessage((DWORD_PTR)(*pCallback->funcPtr), Message, Arg1, Arg2);
		break;

	case CALLBACK_WINDOW:	// Send a message to the app's main window
		PostMessage((HWND)(*pCallback->funcPtr), Message, Arg1, Arg2);
		break;

	default:				// stub
		break;

	}
}

LRESULT WinDriver::DriverCallback::ProcessMessage(DWORD_PTR dwDI, HDRVR hdrvr, UINT uMsg, LPARAM lP1, LPARAM lP2) {
	if (pDrvProc)
		return pDrvProc(dwDI, hdrvr, uMsg, lP1, lP2);

	return MMSYSERR_NOERROR;
}

bool WinDriver::DriverComponent::SetDriverHandle(HDRVR handle) {
	// The app tried to initialize the driver with no pointer?
	if (handle == nullptr) {
		Message("DrvHandle emptied.");
		this->DrvHandle = nullptr;
		return true;
	}

	// Check if we have the same pointer in memory
	if (this->DrvHandle != handle)
		this->DrvHandle = handle;

	// All good, save the pointer to a local variable and return true
	this->DrvHandle = handle;
	Message("DrvHandle stored! Addr: 0x%08x", this->DrvHandle);
	return true;
}

bool WinDriver::DriverComponent::SetLibraryHandle(HMODULE mod) {
	// The app tried to initialize the driver with no pointer?
	if (mod == nullptr) {
		Message("LibHandle emptied.");
		this->LibHandle = nullptr;
		return true;
	}

	// We already have the same pointer in memory.
	if (this->LibHandle != mod)
		this->LibHandle = mod;

	// All good, save the pointer to a local variable and return true
	this->LibHandle = mod;
	Message("LibHandle stored! Addr: 0x%08x", this->LibHandle);
	return true;
}

bool WinDriver::DriverComponent::OpenDriver(MIDIOPENDESC* OpInfStruct, DWORD CallbackMode, DWORD_PTR CookedPlayerAddress) {
	if (OpInfStruct->hMidi == nullptr) {
		Error("No valid HMIDI pointer has been specified.", false);
		return false;
	}

	// Check if the app wants a cooked player
	if (CallbackMode & 0x00000002L) {
		if (CookedPlayerAddress == 0) {
			Error("No memory address has been specified for the MIDI_IO_COOKED player.", false);
			return false;
		}
		// stub
	}

	// Everything is hunky-dory, proceed
	return true;
}

bool WinDriver::DriverComponent::CloseDriver() {
	// stub
	return true;
}

#endif