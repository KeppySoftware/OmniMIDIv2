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
typedef bool (*BASS_ASIO_Init)(int device, DWORD flags);
typedef bool (*BASS_ASIO_ControlPanel)();
typedef bool (*BASS_ASIO_Free)();

typedef int (*BASS_GetVersion)();
typedef int (*BASS_MIDI_GetVersion)();
typedef uint32_t (*XSynth_GetVersion)();
typedef char* (*fluid_version_str)();
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
    HMODULE hLib;
    BASS_GetVersion func1;
    BASS_MIDI_GetVersion func2;
    XSynth_GetVersion func3;
    fluid_version_str func4;

    hLib = LoadLibraryA("bass.dll");
    if (hLib == NULL) {
        v.bass = "N/A";
        goto bassmidi;
    }

    func1 = (BASS_GetVersion)GetProcAddress(hLib, "BASS_GetVersion");
    if (func1 == NULL) {
        FreeLibrary(hLib);
        v.bass = "N/A";
        goto bassmidi;
    }
    tmp = func1();
    v.bass = Utils::versionToString(tmp);

    FreeLibrary(hLib);
bassmidi:
    hLib = LoadLibraryA("bassmidi.dll");
    if (hLib == NULL) {
        v.bassmidi = "N/A";
        goto xsynth;
    }

    func2 = (BASS_MIDI_GetVersion)GetProcAddress(hLib, "BASS_MIDI_GetVersion");
    if (func2 == NULL) {
        FreeLibrary(hLib);
        v.bassmidi = "N/A";
        goto xsynth;
    }
    tmp = func2();
    v.bassmidi = Utils::versionToString(tmp);

    FreeLibrary(hLib);

xsynth:
    hLib = LoadLibraryA("xsynth.dll");
    if (hLib == NULL) {
        v.xsynth = "N/A";
        goto fluidsynth;
    }

    func3 = (XSynth_GetVersion)GetProcAddress(hLib, "XSynth_GetVersion");
    if (func3 == NULL) {
        FreeLibrary(hLib);
        v.xsynth = "N/A";
        goto fluidsynth;
    }
    tmp = func3();
    v.xsynth = Utils::versionToString(tmp << 8);

    FreeLibrary(hLib);

fluidsynth:
    hLib = LoadLibraryA("fluidsynth.dll");
    if (hLib == NULL) {
        v.fluidsynth = "N/A";
        goto finish;
    }

    func4 = (fluid_version_str)GetProcAddress(hLib, "fluid_version_str");
    if (func4 == NULL) {
        FreeLibrary(hLib);
        v.fluidsynth = "N/A";
        goto finish;
    }
    v.fluidsynth = std::string(func4());

    FreeLibrary(hLib);

finish:
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

    FreeLibrary(hLib);
    return out;
}

void Utils::openASIOConfig(std::string name) {
    HMODULE hLib = LoadLibraryA("bassasio.dll");
    if (hLib == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_Init func1 = (BASS_ASIO_Init)GetProcAddress(hLib, "BASS_ASIO_Init");
    if (func1 == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_ControlPanel func2 = (BASS_ASIO_ControlPanel)GetProcAddress(hLib, "BASS_ASIO_ControlPanel");
    if (func2 == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_GetDeviceInfo func3 = (BASS_ASIO_GetDeviceInfo)GetProcAddress(hLib, "BASS_ASIO_GetDeviceInfo");
    if (func3 == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    BASS_ASIO_Free func4 = (BASS_ASIO_Free)GetProcAddress(hLib, "BASS_ASIO_Free");
    if (func4 == NULL)
        throw std::runtime_error("Error loading BASSASIO library");

    int device;
    BASS_ASIO_DEVICEINFO info;
    for (int i = 0; func3(i, &info); i++) {
        if (name == std::string(info.name))
            break;
        ++device;
    }

    if (!func1(device, 0))
        throw std::runtime_error("Error initializing ASIO device");
    if (!func2())
        throw std::runtime_error("Error opening ASIO control panel");

    func4();
    FreeLibrary(hLib);
}
#endif
