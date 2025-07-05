#include "utils.h"

#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shlobj.h>

typedef bool (*InitializeKDMAPIStream)();
typedef bool (*TerminateKDMAPIStream)();

typedef struct {
    const char *name;
    const char *driver;
} BASS_ASIO_DEVICEINFO;
typedef int (*BASS_ASIO_GetDeviceInfo)(unsigned int, BASS_ASIO_DEVICEINFO *info);
#endif

#define MAX_STRING 8192

fs::path Utils::GetConfigFolder() {
    fs::path path;
#if defined(__linux__)
    char *env;
    env = std::getenv("XDG_CONFIG_HOME");
    if (env == NULL) {
        env = std::getenv("HOME");
        path = env;
        path /= ".config";
    } else {
        path = env;
    }
    path /= "OmniMIDI";
    return path;
#elif defined(_WIN32)
    char *p = (char *)malloc(MAX_STRING);
    SHGetFolderPathA(nullptr, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, p);
    path = p;
    path /= "OmniMIDI";
    free(p);
    return path;
#endif
}

void Utils::InitializeKdmapi() {
#if defined(__linux__)
    void* handle;
    bool (*InitializeKDMAPIStream)();
    bool (*TerminateKDMAPIStream)();

    handle = dlopen("libOmniMIDI.so", RTLD_LAZY);
    if (!handle)
        throw std::runtime_error("Error loading OmniMIDI library");

    *(void**)(&InitializeKDMAPIStream) = dlsym(handle, "InitializeKDMAPIStream");
    if (!InitializeKDMAPIStream) {
        dlclose(handle);
        throw std::runtime_error("Error loading OmniMIDI library");
    }

    *(void**)(&TerminateKDMAPIStream) = dlsym(handle, "TerminateKDMAPIStream");
    if (!TerminateKDMAPIStream) {
        dlclose(handle);
        throw std::runtime_error("Error loading OmniMIDI library");
    }

    if (!InitializeKDMAPIStream()) {
        dlclose(handle);
        throw std::runtime_error("Error initializing OmniMIDI");
    }
    if (!TerminateKDMAPIStream()) {
        dlclose(handle);
        throw std::runtime_error("Error initializing OmniMIDI");
    }

    dlclose(handle);
#elif defined(_WIN32)
    HMODULE hLib = LoadLibraryA("OmniMIDI.dll");
    if (hLib == NULL)
        throw std::runtime_error("Error loading OmniMIDI library");

    InitializeKDMAPIStream func1 = (InitializeKDMAPIStream)GetProcAddress(hLib, "InitializeKDMAPIStream");
    if (func1 == NULL)
        throw std::runtime_error("Error loading OmniMIDI library");
    TerminateKDMAPIStream func2 = (TerminateKDMAPIStream)GetProcAddress(hLib, "TerminateKDMAPIStream");
    if (func2 == NULL)
        throw std::runtime_error("Error loading OmniMIDI library");

    if (!func1()) {
        FreeLibrary(hLib);
        throw std::runtime_error("Error initializing OmniMIDI");
    }
    if (!func2()) {
        FreeLibrary(hLib);
        throw std::runtime_error("Error initializing OmniMIDI");
    }

    FreeLibrary(hLib);
#endif
}

std::string Utils::versionToString(int ver) {
    int revision = ver & 0xFF;
    int build = (ver >> 8) & 0xFF;
    int minor = (ver >> 16) & 0xFF;
    int major = (ver >> 24) & 0xFF;

    return std::to_string(major) + "."
         + std::to_string(minor) + "."
         + std::to_string(build)
         + (revision != 0 ? " Rev." + std::to_string(revision) : "");
}

SynthVersions Utils::getSynthVersions() {
    SynthVersions v;
    int tmp;

#if defined(__linux__)
    void* handle;
    int (*BASS_GetVersion)();
    int (*BASS_MIDI_GetVersion)();
    uint32_t (*XSynth_GetVersion)();
    char* (*fluid_version_str)();

    handle = dlopen("libbass.so", RTLD_LAZY);
    if (!handle) {
        v.bass = "N/A";
        goto bassmidi;
    }

    *(void**)(&BASS_GetVersion) = dlsym(handle, "BASS_GetVersion");
    if (!BASS_GetVersion) {
        dlclose(handle);
        v.bass = "N/A";
        goto bassmidi;
    }

    tmp = BASS_GetVersion();
    v.bass = Utils::versionToString(tmp);

    dlclose(handle);

bassmidi:
    handle = dlopen("libbassmidi.so", RTLD_LAZY);
    if (!handle) {
        v.bassmidi = "N/A";
        goto xsynth;
    }

    *(void**)(&BASS_MIDI_GetVersion) = dlsym(handle, "BASS_MIDI_GetVersion");
    if (!BASS_MIDI_GetVersion) {
        dlclose(handle);
        v.bassmidi = "N/A";
        goto xsynth;
    }

    tmp = BASS_MIDI_GetVersion();
    v.bassmidi = Utils::versionToString(tmp);

    dlclose(handle);

xsynth:
    handle = dlopen("libxsynth.so", RTLD_LAZY);
    if (!handle) {
        v.xsynth = "N/A";
        goto fluid;
    }

    *(void**)(&XSynth_GetVersion) = dlsym(handle, "XSynth_GetVersion");
    if (!XSynth_GetVersion) {
        dlclose(handle);
        v.xsynth = "N/A";
        goto fluid;
    }

    tmp = (int)XSynth_GetVersion();
    v.xsynth = Utils::versionToString(tmp << 8);

    dlclose(handle);

fluid:
    handle = dlopen("libfluidsynth.so", RTLD_LAZY);
    if (!handle) {
        v.fluidsynth = "N/A";
        goto finish;
    }

    *(void**)(&fluid_version_str) = dlsym(handle, "fluid_version_str");
    if (!fluid_version_str) {
        dlclose(handle);
        v.fluidsynth = "N/A";
        goto finish;
    }

    v.fluidsynth = std::string(fluid_version_str());

    dlclose(handle);
finish:
#elif defined(_WIN32)
#endif
    return v;
}

#ifdef _WIN32
std::vector<std::string> Utils::getASIODevices() {
    std::vector<std::string> out;

    HMODULE hLib = LoadLibraryA("bassasio.dll");
    if (hLib == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_GetDeviceInfo func = (BASS_ASIO_GetDeviceInfo)GetProcAddress(hLib, "BASS_ASIO_GetDeviceInfo");
    if (func == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_DEVICEINFO info;
    for (int i = 0; func(i, &info); i++) {
        out.push_back(std::string(info.name));
    }

    return out;
}

void Utils::openASIOConfig(std::string name) {
    // ToDo
}
#endif
