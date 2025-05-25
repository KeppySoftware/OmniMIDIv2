#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <windows.h>
#include <windef.h> /* Part of the Wine header files */
#include <winuser.h>

static void *kdmapi_handle = NULL;

static uint32_t (*lnk_modMessage)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) = NULL;

static int32_t (*lnk_IsKDMAPIAvailable)() = NULL;
static int32_t (*lnk_InitializeKDMAPIStream)() = NULL;
static int32_t (*lnk_TerminateKDMAPIStream)() = NULL;
static void (*lnk_ResetKDMAPIStream)() = NULL;
static void (*lnk_SendDirectData)(uint32_t) = NULL;
static uint32_t (*lnk_SendDirectLongData)(void*, uint32_t) = NULL;
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
    lnk_IsKDMAPIAvailable = dlsym(kdmapi_handle, "IsKDMAPIAvailable");
    lnk_InitializeKDMAPIStream = dlsym(kdmapi_handle, "InitializeKDMAPIStream");
    lnk_TerminateKDMAPIStream = dlsym(kdmapi_handle, "TerminateKDMAPIStream");
    lnk_ResetKDMAPIStream = dlsym(kdmapi_handle, "ResetKDMAPIStream");
    lnk_SendDirectData = dlsym(kdmapi_handle, "SendDirectData");
    lnk_SendDirectLongData = dlsym(kdmapi_handle, "SendDirectLongData");
    lnk_SendCustomEvent = dlsym(kdmapi_handle, "SendCustomEvent");
    lnk_DriverSettings = dlsym(kdmapi_handle, "DriverSettings");
    lnk_LoadCustomSoundFontsList = dlsym(kdmapi_handle, "LoadCustomSoundFontsList");
    lnk_GetRenderingTime = dlsym(kdmapi_handle, "GetRenderingTime");
    lnk_GetVoiceCount = dlsym(kdmapi_handle, "GetVoiceCount");

    return TRUE;
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

void WINAPI proxy_SendDirectData(uint32_t ev) {
    lnk_SendDirectData(ev);
}

uint32_t WINAPI proxy_SendDirectLongData(void* IIMidiHdr, uint32_t IIMidiHdrSize) {
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
    return lnk_GetRenderingTime();
}

uint64_t WINAPI proxy_GetVoiceCount() {
    return lnk_GetVoiceCount();
}