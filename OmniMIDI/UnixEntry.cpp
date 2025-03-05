/*

    OmniMIDI v15+ (Rewrite) for UNIX-like OSes

    This file contains the required code to run the driver under UNIX-like OSes, like Linux (and possibly macOS)
    This file is useful only if you want to compile the driver under those OSes, it's not needed for Windows.

    Credits: https://github.com/Lurmog

*/

#ifndef _WIN32

#include <signal.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "SynthHost.hpp"

#define EXPORT __attribute__((__visibility__("default")))

// Global objects
static ErrorSystem::Logger* ErrLog = nullptr;
static OmniMIDI::SynthHost* Host = nullptr;

// Cleanup resources
void cleanup() {
    if (Host) {
        Host->Stop();
        delete Host;
        Host = nullptr;
    }

    if (ErrLog) {
        delete ErrLog;
        ErrLog = nullptr;
    }
}

void __attribute__((constructor)) _init(void) {
    try {
        // Initialize logger
        ErrLog = new ErrorSystem::Logger();
        Host = new OmniMIDI::SynthHost(ErrLog);

        LOG("_init succeeded! OmniMIDI loaded.");
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        cleanup();
    }
}

void __attribute__((destructor)) _fini(void) {
    LOG("_fini called. Terminating OmniMIDI...");
    cleanup();
}

extern "C" {
    int EXPORT IsKDMAPIAvailable() {
        return (int)Host->IsKDMAPIAvailable();
    }

    int EXPORT InitializeKDMAPIStream() {
        if (Host->Start()) {
            LOG("KDMAPI initialized.");
            return 1;
        }

        LOG("KDMAPI failed to initialize.");
        return 0;
    }

    int EXPORT TerminateKDMAPIStream() {
        if (Host->Stop()) {
            LOG("KDMAPI freed.");
            return 1;
        }

        LOG("KDMAPI failed to free its resources.");
        return 0;
    }

    void EXPORT ResetKDMAPIStream() {
        Host->PlayShortEvent(0xFF, 0x01, 0x01);
    }

    void EXPORT SendDirectData(unsigned int ev) {
        Host->PlayShortEvent(ev);
    }

    void EXPORT SendDirectDataNoBuf(unsigned int ev) {
        // Unsupported, forward to SendDirectData
        SendDirectData(ev);
    }

    unsigned int EXPORT SendDirectLongData(char* IIMidiHdr, unsigned int IIMidiHdrSize) {
        return Host->PlayLongEvent(IIMidiHdr, IIMidiHdrSize) == SYNTH_OK ? 0 : 11;
    }

    unsigned int EXPORT SendDirectLongDataNoBuf(char* IIMidiHdr, unsigned int IIMidiHdrSize) {
        // Unsupported, forward to SendDirectLongData
        return SendDirectLongData(IIMidiHdr, IIMidiHdrSize);
    }

    int EXPORT SendCustomEvent(unsigned int evt, unsigned int chan, unsigned int param) {
        return Host->TalkToSynthDirectly(evt, chan, param);
    }

    int EXPORT DriverSettings(unsigned int setting, unsigned int mode, void* value, unsigned int cbValue) {
        return 1;
    }

    int EXPORT LoadCustomSoundFontsList(char* Directory) {
        return DriverSettings(KDMAPI_SOUNDFONT, true, Directory, MAX_PATH_LONG);
    }

    float EXPORT GetRenderingTime() {
        if (Host == nullptr)
            return 0.0f;

        return Host->GetRenderingTime();
    }

    unsigned int EXPORT GetActiveVoices() {
        if (Host == nullptr)
            return 0;

        return Host->GetActiveVoices();
    }
}

#endif