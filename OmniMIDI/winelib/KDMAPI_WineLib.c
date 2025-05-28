#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <windows.h>
#include <math.h>
#include <windef.h> /* Part of the Wine header files */
#include <winuser.h>

#define NAME                "KDMAPI WINELIB"
#define WNAME               L"KDMAPI WINELIB\0"
#define MAXPNAMELEN         32

#define MODM_INIT           101
#define MODM_GETNUMDEVS     1
#define MODM_GETDEVCAPS     2
#define MODM_OPEN           3
#define MODM_CLOSE          4
#define MODM_PREPARE        5
#define MODM_UNPREPARE      6
#define MODM_DATA           7
#define MODM_LONGDATA       8
#define MODM_RESET          9
#define MODM_GETVOLUME      10
#define MODM_SETVOLUME      11
#define MODM_CACHEPATCHES       12
#define MODM_CACHEDRUMPATCHES   13

#define MIDI_IO_PACKED	0x00000000L			// Legacy mode, used by most MIDI apps
#define MIDI_IO_COOKED	0x00000002L			// Stream mode, used by some old MIDI apps (Such as GZDoom)

#define MANUFACTURERID  0xFFFF
#define PRODUCTID       0xFFFF
#define SYNTHTECHN      MOD_SWSYNTH
#define SYNTHSUPPORT    MIDICAPS_VOLUME | MIDICAPS_STREAM

static void *kdmapi_handle = NULL;

typedef VOID(CALLBACK* WMMC)(HMIDIOUT, DWORD, DWORD_PTR, DWORD_PTR, DWORD_PTR);
static DWORD dwCallbackMode = CALLBACK_NULL | MIDI_IO_PACKED;
static DWORD_PTR dwCallback = 0;
static DWORD_PTR dwInstance = 0;
static HMIDI dwHMI = NULL;

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

    kdmapi_handle = dlopen("./libOmniMIDI.so", RTLD_NOW);
    if (!kdmapi_handle) {
        MessageBoxA(NULL, "libOmniMIDI.so load failed.", "KDMAPI WINELIB", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
        return FALSE;
    }

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
    switch (dwMsg) {
        case MODM_DATA:
            lnk_SendDirectData(p1);
            return MMSYSERR_NOERROR;
        case MODM_LONGDATA:
            return lnk_SendDirectLongData((PVOID)p1, p2);
        case MODM_OPEN:
            return lnk_InitializeKDMAPIStream() ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
        case MODM_CLOSE:
            return lnk_TerminateKDMAPIStream() ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
        case MODM_GETNUMDEVS:
            return 1;
        case MODM_GETDEVCAPS:
            {
                MIDIOUTCAPSA CapsA;
                MIDIOUTCAPSW CapsW;
                MIDIOUTCAPS2A Caps2A;
                MIDIOUTCAPS2W Caps2W;

                DWORD dummy = 0;

                PVOID CapsPointer = (PVOID)p1;
                DWORD CapsSize = (DWORD)p2;

                // Why would this happen? Stupid MIDI app dev smh
                if (CapsPointer == NULL || CapsSize == 0)
                {
                    return MMSYSERR_INVALPARAM;
                }

                // I have to support all this s**t or else it won't work in some apps smh
                switch (CapsSize) {
                case (sizeof(MIDIOUTCAPSA)):
                    strncpy(CapsA.szPname, NAME, MAXPNAMELEN);
                    CapsA.dwSupport = SYNTHSUPPORT;
                    CapsA.wChannelMask = 0xFFFF;
                    CapsA.wMid = MANUFACTURERID;
                    CapsA.wPid = PRODUCTID;
                    CapsA.wTechnology = SYNTHTECHN;
                    CapsA.wNotes = 65535;
                    CapsA.wVoices = 65535;
                    CapsA.vDriverVersion = MAKEWORD(6, 2);
                    memcpy((LPMIDIOUTCAPSA)CapsPointer, &CapsA, fmin(CapsSize, sizeof(CapsA)));
                    break;

                case (sizeof(MIDIOUTCAPSW)):
                    wcsncpy(CapsW.szPname, WNAME, MAXPNAMELEN);
                    CapsW.dwSupport = SYNTHSUPPORT;
                    CapsW.wChannelMask = 0xFFFF;
                    CapsW.wMid = MANUFACTURERID;
                    CapsW.wPid = PRODUCTID;
                    CapsW.wTechnology = SYNTHTECHN;
                    CapsW.wNotes = 65535;
                    CapsW.wVoices = 65535;
                    CapsW.vDriverVersion = MAKEWORD(6, 2);
                    memcpy((LPMIDIOUTCAPSW)CapsPointer, &CapsW, fmin(CapsSize, sizeof(CapsW)));
                    break;

                case (sizeof(MIDIOUTCAPS2A)):
                    strncpy(Caps2A.szPname, NAME, MAXPNAMELEN);
                    Caps2A.dwSupport = SYNTHSUPPORT;
                    Caps2A.wChannelMask = 0xFFFF;
                    Caps2A.wMid = MANUFACTURERID;
                    Caps2A.wPid = PRODUCTID;
                    Caps2A.wTechnology = SYNTHTECHN;
                    Caps2A.wNotes = 65535;
                    Caps2A.wVoices = 65535;
                    Caps2A.vDriverVersion = MAKEWORD(6, 2);
                    memcpy((LPMIDIOUTCAPS2A)CapsPointer, &Caps2A, fmin(CapsSize, sizeof(Caps2A)));
                    break;

                case (sizeof(MIDIOUTCAPS2W)):
                    wcsncpy(Caps2W.szPname, WNAME, MAXPNAMELEN);
                    Caps2W.dwSupport = SYNTHSUPPORT;
                    Caps2W.wChannelMask = 0xFFFF;
                    Caps2W.wMid = MANUFACTURERID;
                    Caps2W.wPid = PRODUCTID;
                    Caps2W.wTechnology = SYNTHTECHN;
                    Caps2W.wNotes = 65535;
                    Caps2W.wVoices = 65535;
                    Caps2W.vDriverVersion = MAKEWORD(6, 2);
                    memcpy((LPMIDIOUTCAPS2W)CapsPointer, &Caps2W, fmin(CapsSize, sizeof(Caps2W)));
                    break;

                default:
                    // ???????
                    return MMSYSERR_INVALPARAM;

                }

                return MMSYSERR_NOERROR;
            }
        case MODM_SETVOLUME:
		case MODM_GETVOLUME:
            return MMSYSERR_NOTSUPPORTED;
		default:
			return MMSYSERR_NOERROR;
    }
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