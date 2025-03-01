#ifndef _WIN32

#include "SynthMain.hpp"
#include <alsa/asoundlib.h>
#include <signal.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "SynthHost.hpp"

// Global objects
static ErrorSystem::Logger* ErrLog = nullptr;
static std::atomic<bool> running{ false };
static OmniMIDI::SynthHost* Host = nullptr;

// ALSA MIDI sequencer
static snd_seq_t* seq = nullptr;
static int client_id = -1;
static int port_id = -1;
static std::thread midi_thread;

// Forward declarations
void cleanup();
void process_midi();

// Signal handler
void handle_signal(int sig) {
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
    running = false;
}

// Connect to available MIDI ports
void connect_to_midi_ports() {
    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    snd_seq_client_info_set_client(cinfo, -1);

    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);

        // Skip our own client and System client
        if (client == client_id || client == SND_SEQ_CLIENT_SYSTEM)
            continue;

        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);

        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);

            // Check if it's a readable MIDI port
            if ((caps & SND_SEQ_PORT_CAP_READ) &&
                (caps & SND_SEQ_PORT_CAP_SUBS_READ) &&
                !(caps & SND_SEQ_PORT_CAP_NO_EXPORT)) {

                int result = snd_seq_connect_from(seq, port_id,
                    snd_seq_port_info_get_client(pinfo),
                    snd_seq_port_info_get_port(pinfo));

                if (result >= 0) {
                    std::cout << "Connected to MIDI device: "
                        << snd_seq_client_info_get_name(cinfo) << ":"
                        << snd_seq_port_info_get_name(pinfo)
                        << std::endl;
                }
            }
        }
    }
}

// Initialize ALSA MIDI
bool init_alsa_midi() {
    int err;

    // Open sequencer
    if ((err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0)) < 0) {
        std::cerr << "Error opening ALSA sequencer: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set client name
    snd_seq_set_client_name(seq, "OmniMIDI");

    // Create port
    port_id = snd_seq_create_simple_port(seq, "MIDI Input",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION);

    if (port_id < 0) {
        std::cerr << "Error creating sequencer port: " << snd_strerror(port_id) << std::endl;
        return false;
    }

    client_id = snd_seq_client_id(seq);
    std::cout << "ALSA MIDI client created: " << client_id << ":" << port_id << std::endl;

    // Connect to system announce port to get notifications of new MIDI devices
    snd_seq_connect_from(seq, port_id, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);

    // Connect to available MIDI ports
    connect_to_midi_ports();

    return true;
}

// MIDI processing thread
void process_midi() {
    snd_seq_event_t* ev;
    int err;

    // Configure poll descriptors
    int npfd;
    struct pollfd* pfd;

    npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    pfd = (struct pollfd*)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN);

    while (running) {
        // Wait for events
        if (poll(pfd, npfd, 500) > 0) {
            do {
                if ((err = snd_seq_event_input(seq, &ev)) < 0) {
                    if (err != -EAGAIN) {
                        std::cerr << "ALSA MIDI event input error: " << snd_strerror(err) << std::endl;
                    }
                    break;
                }

                if (!Host) {
                    snd_seq_free_event(ev);
                    continue;
                }

                switch (ev->type) {
                case SND_SEQ_EVENT_NOTEON:
                    if (ev->data.note.velocity > 0) {
                        Host->PlayShortEvent(0x90 | ev->data.note.channel,
                            ev->data.note.note,
                            ev->data.note.velocity);
                    }
                    else {
                        // Note-on with velocity 0 is interpreted as note-off
                        Host->PlayShortEvent(0x80 | ev->data.note.channel,
                            ev->data.note.note,
                            64); // Default velocity for note-off
                    }
                    break;
                case SND_SEQ_EVENT_NOTEOFF:
                    Host->PlayShortEvent(0x80 | ev->data.note.channel,
                        ev->data.note.note,
                        ev->data.note.velocity);
                    break;
                case SND_SEQ_EVENT_CONTROLLER:
                    Host->PlayShortEvent(0xB0 | ev->data.control.channel,
                        ev->data.control.param,
                        ev->data.control.value);
                    break;
                case SND_SEQ_EVENT_PGMCHANGE:
                    Host->PlayShortEvent(0xC0 | ev->data.control.channel,
                        ev->data.control.value,
                        0);
                    break;
                case SND_SEQ_EVENT_PITCHBEND:
                {
                    int value = ev->data.control.value + 8192;
                    Host->PlayShortEvent(0xE0 | ev->data.control.channel,
                        value & 0x7F,
                        (value >> 7) & 0x7F);
                }
                break;
                case SND_SEQ_EVENT_PORT_START:
                    // New MIDI port available, try to connect to it
                {
                    snd_seq_addr_t sender;
                    sender.client = ev->data.addr.client;
                    sender.port = ev->data.addr.port;

                    // Skip system ports
                    if (sender.client != SND_SEQ_CLIENT_SYSTEM && sender.client != client_id) {
                        snd_seq_port_info_t* pinfo;
                        snd_seq_port_info_alloca(&pinfo);

                        if (snd_seq_get_any_port_info(seq, sender.client, sender.port, pinfo) >= 0) {
                            unsigned int cap = snd_seq_port_info_get_capability(pinfo);

                            // Only connect to readable ports
                            if ((cap & SND_SEQ_PORT_CAP_READ) &&
                                (cap & SND_SEQ_PORT_CAP_SUBS_READ) &&
                                !(cap & SND_SEQ_PORT_CAP_NO_EXPORT)) {

                                int result = snd_seq_connect_from(seq, port_id, sender.client, sender.port);
                                if (result >= 0) {
                                    std::cout << "Connected to new MIDI device: "
                                        << snd_seq_port_info_get_name(pinfo)
                                        << " (" << sender.client << ":" << sender.port << ")"
                                        << std::endl;
                                }
                            }
                        }
                    }
                }
                break;
                }

                snd_seq_free_event(ev);
            } while (snd_seq_event_input_pending(seq, 0) > 0);
        }
    }
}

// Cleanup resources
void cleanup() {
    if (running) {
        running = false;

        if (midi_thread.joinable()) {
            midi_thread.join();
        }
    }

    if (seq) {
        snd_seq_close(seq);
        seq = nullptr;
    }

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

void _start()
{
    return;
}

// Library initialization
void __attribute__((constructor)) _init(void) {
    // Install signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::cout << "Initializing OmniMIDI for Linux..." << std::endl;
    try {
        // Initialize logger
        ErrLog = new ErrorSystem::Logger();
        std::cout << "ERR LOG INIT" << std::endl;

        Host = new OmniMIDI::SynthHost(ErrLog);
        std::cout << "SYNHOST INIT" << std::endl;

        if (Host->Start()) {
            std::cout << "Synthesizer initialized successfully." << std::endl;

            // Initialize ALSA MIDI
            if (init_alsa_midi()) {
                running = true;

                // Start MIDI processing thread
                midi_thread = std::thread(process_midi);

                std::cout << "OmniMIDI initialized successfully." << std::endl;
            }
            else {
                std::cerr << "Failed to initialize ALSA MIDI." << std::endl;
                cleanup();
            }
        }
        else {
            std::cerr << "Failed to start synthesizer." << std::endl;
            cleanup();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        cleanup();
    }
}

// Library cleanup
void __attribute__((destructor)) _fini(void) {
    std::cout << "Shutting down OmniMIDI..." << std::endl;
    cleanup();
    std::cout << "OmniMIDI shutdown complete." << std::endl;
}

#define EXPORT __attribute__((visibility("default")))

EXPORT int IsKDMAPIAvailable() {
    return (int)Host->IsKDMAPIAvailable();
}

EXPORT int InitializeKDMAPIStream() {
    if (Host->Start()) {
        LOG("KDMAPI initialized.");
        return 1;
    }

    LOG("KDMAPI failed to initialize.");
    return 0;
}

EXPORT int TerminateKDMAPIStream() {
    if (Host->Stop()) {
        LOG("KDMAPI freed.");
        return 1;
    }

    LOG("KDMAPI failed to free its resources.");
    return 0;
}

EXPORT void ResetKDMAPIStream() {
    Host->PlayShortEvent(0xFF, 0x01, 0x01);
}

EXPORT void SendDirectData(unsigned int ev) {
    Host->PlayShortEvent(ev);
}

EXPORT void SendDirectDataNoBuf(unsigned int ev) {
    // Unsupported, forward to SendDirectData
    SendDirectData(ev);
}

EXPORT int SendCustomEvent(unsigned int evt, unsigned int chan, unsigned int param) {
    return Host->TalkToSynthDirectly(evt, chan, param);
}

EXPORT int DriverSettings(unsigned int setting, unsigned int mode, void* value, unsigned int cbValue) {
    return Host->SettingsManager(setting, (bool)mode, value, (size_t)cbValue);
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

#endif