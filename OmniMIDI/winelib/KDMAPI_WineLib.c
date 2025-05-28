#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <windows.h>
#include <windef.h> /* Part of the Wine header files */
#include <winuser.h>

#define MIDI_IO_PACKED	0x00000000L			// Legacy mode, used by most MIDI apps
#define MIDI_IO_COOKED	0x00000002L			// Stream mode, used by some old MIDI apps (Such as GZDoom)

static void *kdmapi_handle = NULL;

typedef VOID(CALLBACK* WMMC)(HMIDIOUT, DWORD, DWORD_PTR, DWORD_PTR, DWORD_PTR);
static DWORD dwCallbackMode = CALLBACK_NULL | MIDI_IO_PACKED;
static DWORD_PTR dwCallback = 0;
static DWORD_PTR dwInstance = 0;
static HMIDI dwHMI = NULL;

static uint32_t (*lnk_modMessage)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) = NULL;

static uint32_t (*lnk_ReturnKDMAPIVer)() = NULL;
static int32_t (*lnk_IsKDMAPIAvailable)() = NULL;
static int32_t (*lnk_InitializeKDMAPIStream)() = NULL;
static int32_t (*lnk_TerminateKDMAPIStream)() = NULL;
static void (*lnk_ResetKDMAPIStream)() = NULL;
static void (*lnk_SendDirectData)(uint32_t) = NULL;
static uint32_t (*lnk_SendDirectLongData)(void*, uint32_t) = NULL;
static uint32_t (*lnk_PrepareLongData)(LPMIDIHDR, UINT) = NULL;
static uint32_t (*lnk_UnprepareLongData)(LPMIDIHDR, UINT) = NULL;
static int32_t (*lnk_SendCustomEvent)(uint32_t, uint32_t, uint32_t) = NULL;
static int32_t (*lnk_DriverSettings)(uint32_t, uint32_t, void*, uint32_t) = NULL;
static int32_t (*lnk_LoadCustomSoundFontsList)(char*) = NULL;
static float (*lnk_GetRenderingTime)() = NULL;
static uint64_t (*lnk_GetVoiceCount)() = NULL;

static BOOL load_kdmapi() {
    if (kdmapi_handle != NULL) return TRUE;

    MessageBoxA(NULL, "loading.", "!!!", MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);

    kdmapi_handle = dlopen("./libOmniMIDI.so", RTLD_NOW);
    if (!kdmapi_handle) {
        MessageBoxA(NULL, "fail.", "!!!", MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
        return FALSE;
    }

    lnk_modMessage = dlsym(kdmapi_handle, "modMessage");

    lnk_ReturnKDMAPIVer = dlsym(kdmapi_handle, "ReturnKDMAPIVer");
    lnk_IsKDMAPIAvailable = dlsym(kdmapi_handle, "IsKDMAPIAvailable");
    lnk_InitializeKDMAPIStream = dlsym(kdmapi_handle, "InitializeKDMAPIStream");
    lnk_TerminateKDMAPIStream = dlsym(kdmapi_handle, "TerminateKDMAPIStream");
    lnk_ResetKDMAPIStream = dlsym(kdmapi_handle, "ResetKDMAPIStream");
    lnk_SendDirectData = dlsym(kdmapi_handle, "SendDirectData");
    lnk_SendDirectLongData = dlsym(kdmapi_handle, "SendDirectLongData");
    lnk_PrepareLongData = dlsym(kdmapi_handle, "PrepareLongData");
    lnk_UnprepareLongData = dlsym(kdmapi_handle, "UnprepareLongData");
    lnk_SendCustomEvent = dlsym(kdmapi_handle, "SendCustomEvent");
    lnk_DriverSettings = dlsym(kdmapi_handle, "DriverSettings");
    lnk_LoadCustomSoundFontsList = dlsym(kdmapi_handle, "LoadCustomSoundFontsList");
    lnk_GetRenderingTime = dlsym(kdmapi_handle, "GetRenderingTime");
    lnk_GetVoiceCount = dlsym(kdmapi_handle, "GetVoiceCount");

    return TRUE;
}

static void DoCallback(DWORD msg, DWORD_PTR p1, DWORD_PTR p2) {
	switch (dwCallbackMode & CALLBACK_TYPEMASK) {
	case CALLBACK_FUNCTION:
	{
		(*(WMMC)dwCallback)((HMIDIOUT)dwHMI, msg, dwInstance, p1, p2);
		break;
	}
	case CALLBACK_EVENT:
	{
		SetEvent((HANDLE)dwCallback);
		break;
	}
	case CALLBACK_THREAD:
	{
		PostThreadMessageW((DWORD)dwCallback, msg, p1, p2);
		break;
	}
	case CALLBACK_WINDOW:
	{
		PostMessageW((HWND)dwCallback, msg, p1, p2);
		break;
	}
	default:
		break;
	}
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason, LPVOID lpReserved)
{
    switch (ul_reason)
    {
    case DLL_PROCESS_ATTACH:
        if (!load_kdmapi())
            return FALSE;

    default:
        break;
    }

	return TRUE;
}

uint32_t WINAPI proxy_modMessage(UINT uDID, UINT dwMsg, DWORD_PTR usrPtr, DWORD_PTR p1, DWORD_PTR p2) {
    return lnk_modMessage(uDID, dwMsg, usrPtr, p1, p2);
}

uint32_t WINAPI proxy_ReturnKDMAPIVer() {
    return lnk_ReturnKDMAPIVer();
}

int32_t WINAPI proxy_IsKDMAPIAvailable() {
    return lnk_IsKDMAPIAvailable();
}

int32_t WINAPI proxy_InitializeKDMAPIStream() {
    return lnk_InitializeKDMAPIStream();
}

int32_t WINAPI proxy_TerminateKDMAPIStream() {
    return lnk_TerminateKDMAPIStream();
}

void WINAPI proxy_ResetKDMAPIStream() {
    lnk_ResetKDMAPIStream();
}

int32_t WINAPI proxy_InitializeCallbackFeatures(HMIDI hmo, DWORD_PTR cb, DWORD_PTR inst, DWORD_PTR usr, DWORD cbmode) {
    const BOOL check = ((cbmode != 0) && (!cb && !inst));

    if ((cbmode & CALLBACK_TYPEMASK) == CALLBACK_WINDOW) 
	{
		if (cb && !IsWindow((HWND)cb))
		{
			return FALSE;
		}
	}

    // Not supported in Linux
    if (cbmode & MIDI_IO_COOKED)
        return FALSE;

    dwHMI = hmo;
    dwCallback = (DWORD_PTR)(check ? 0 : cb);
	dwInstance = (DWORD_PTR)(check ? 0 : inst);
	dwCallbackMode = (DWORD)(check ? 0 : cbmode);

    return TRUE;
}

void WINAPI proxy_RunCallbackFunction(DWORD msg, DWORD_PTR p1, DWORD_PTR p2) {
    DoCallback(msg, p1, p2);
}

void WINAPI proxy_SendDirectData(uint32_t ev) {
    lnk_SendDirectData(ev);
}

uint32_t WINAPI proxy_SendDirectLongData(void* IIMidiHdr, uint32_t IIMidiHdrSize) {
    // TODO, LINUX TO WIN32
    return 0;
}

uint32_t WINAPI proxy_PrepareLongData(void* IIMidiHdr, uint32_t IIMidiHdrSize) {
    // TODO, LINUX TO WIN32
    return 0;
}

uint32_t WINAPI proxy_UnprepareLongData(void* IIMidiHdr, uint32_t IIMidiHdrSize) {
    // TODO, LINUX TO WIN32
    return 0;
}

int32_t WINAPI proxy_SendCustomEvent(uint32_t evt, uint32_t chan, uint32_t param) {
    return lnk_SendCustomEvent(evt, chan, param);
}

int32_t WINAPI proxy_DriverSettings(uint32_t setting, uint32_t mode, void* value, uint32_t cbValue) {
    return lnk_DriverSettings(setting, mode, value, cbValue);
}

int32_t WINAPI proxy_LoadCustomSoundFontsList(char* Directory) {
    return lnk_LoadCustomSoundFontsList(Directory);
}

float WINAPI proxy_GetRenderingTime() {
    if (!lnk_GetRenderingTime)
        return 0.0f;

    return lnk_GetRenderingTime();
}

uint64_t WINAPI proxy_GetVoiceCount() {
    if (!lnk_GetVoiceCount)
        return 0;

    return lnk_GetVoiceCount();
}