
/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _BASSSYNTH_H
#define _BASSSYNTH_H

#include "ErrSys.hpp"
#include "EvBuf_t.hpp"
#include "SoundFontSystem.hpp"
#include "SynthMain.hpp"
#include <bass\bass.h>
#include <bass\bass_vst.h>
#include <bass\bassasio.h>
#include <bass\bassmidi.h>
#include <bass\basswasapi.h>
#include <thread>
#include <vector>

#ifdef _WIN32
#include "XAudio2Output.hpp"
#endif

namespace OmniMIDI {
	enum BASSEngine {
		Invalid = -1,
		Internal,
		WASAPI,
		XAudio2,
		ASIO,
		KS
	};

	class BASSSettings : public SynthSettings {
	private:
		ErrorSystem::Logger SetErr;

	public:
		// Global settings
		unsigned int EvBufSize = 32768;
		unsigned int AudioFrequency = 48000;
		unsigned int MaxVoices = 1000;
		unsigned int MaxCPU = 95;
		int AudioEngine = (int)WASAPI;
		bool LoudMax = false;

		// XAudio2
		int XABufSize = 88;
		int XASweepRate = 15;

		// WASAPI
		float WASAPIBuf = 32.0f;

		// ASIO
		std::string ASIODevice = "None";
		std::string ASIOLCh = "0";
		std::string ASIORCh = "0";

		BASSSettings() {
			// When you initialize Settings(), load OM's own settings by default
			OMShared::SysPath Utils;
			wchar_t OMPath[MAX_PATH] = { 0 };

			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, OMPath, sizeof(OMPath))) {
				swprintf_s(OMPath, L"%s\\OmniMIDI\\settings.json\0", OMPath);
				LoadJSON(OMPath);
			}
		}

		void CreateJSON(wchar_t* Path) {
			std::fstream st;
			st.open(Path, std::fstream::out | std::ofstream::trunc);
			if (st.is_open()) {
				nlohmann::json defset = {
					{ "BASSSynth", {
						JSONGetVal(ASIODevice),
						JSONGetVal(ASIOLCh),
						JSONGetVal(ASIORCh),
						JSONGetVal(AudioEngine),
						JSONGetVal(AudioFrequency),
						JSONGetVal(EvBufSize),
						JSONGetVal(LoudMax),
						JSONGetVal(MaxCPU),
						JSONGetVal(MaxVoices),
						JSONGetVal(XABufSize),
						JSONGetVal(XASweepRate),
						JSONGetVal(WASAPIBuf)
					}}
				};

				std::string dump = defset.dump(1);
				st.write(dump.c_str(), dump.length());
				st.close();
			}
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadJSON(wchar_t* Path) {
			std::fstream st;
			st.open(Path, std::fstream::in);

			if (st.is_open()) {
				try {
					// Read the JSON data from there
					auto json = nlohmann::json::parse(st, nullptr, false, true);

					if (json != nullptr) {
						auto& JsonData = json["BASSSynth"];

						if (!(JsonData == nullptr)) {
							JSONSetVal(bool, LoudMax);
							JSONSetVal(float, WASAPIBuf);
							JSONSetVal(int, AudioEngine);
							JSONSetVal(std::string, ASIODevice);
							JSONSetVal(std::string, ASIOLCh);
							JSONSetVal(std::string, ASIORCh);
							JSONSetVal(unsigned int, AudioFrequency);
							JSONSetVal(unsigned int, EvBufSize);
							JSONSetVal(unsigned int, MaxCPU);
							JSONSetVal(unsigned int, XABufSize);
							JSONSetVal(unsigned int, XASweepRate);
							JSONSetVal(unsigned int, MaxVoices);
						}
					}
					else throw nlohmann::json::type_error::create(667, "json structure is not valid", nullptr);
				}
				catch (nlohmann::json::type_error ex) {
					st.close();
					LOG(SetErr, "The JSON is corrupted or malformed!nlohmann::json says: %s", ex.what());
					CreateJSON(Path);
					return;
				}
				st.close();
			}
		}
	};

	class BASSSynth : public SynthModule {
	private:
		ErrorSystem::Logger SynErr;
		OMShared::Funcs MiscFuncs;

		Lib* BAudLib = nullptr;
		Lib* BMidLib = nullptr;
		Lib* BWasLib = nullptr;
		Lib* BVstLib = nullptr;
		Lib* BAsiLib = nullptr;

		LibImport LibImports[66] = {
			// BASS
			ImpFunc(BASS_ChannelFlags),
			ImpFunc(BASS_ChannelGetAttribute),
			ImpFunc(BASS_ChannelGetData),
			ImpFunc(BASS_ChannelGetLevelEx),
			ImpFunc(BASS_ChannelIsActive),
			ImpFunc(BASS_ChannelPlay),
			ImpFunc(BASS_ChannelRemoveFX),
			ImpFunc(BASS_ChannelSeconds2Bytes),
			ImpFunc(BASS_ChannelSetAttribute),
			ImpFunc(BASS_ChannelSetDevice),
			ImpFunc(BASS_ChannelSetFX),
			ImpFunc(BASS_ChannelStop),
			ImpFunc(BASS_ChannelUpdate),
			ImpFunc(BASS_ErrorGetCode),
			ImpFunc(BASS_FXSetParameters),
			ImpFunc(BASS_Free),
			ImpFunc(BASS_GetDevice),
			ImpFunc(BASS_GetDeviceInfo),
			ImpFunc(BASS_GetInfo),
			ImpFunc(BASS_Init),
			ImpFunc(BASS_PluginFree),
			ImpFunc(BASS_SetConfig),
			ImpFunc(BASS_Stop),
			ImpFunc(BASS_StreamFree),

			// BASSMIDI
			ImpFunc(BASS_MIDI_FontFree),
			ImpFunc(BASS_MIDI_FontInit),
			ImpFunc(BASS_MIDI_FontLoad),
			ImpFunc(BASS_MIDI_StreamCreate),
			ImpFunc(BASS_MIDI_StreamEvent),
			ImpFunc(BASS_MIDI_StreamEvents),
			ImpFunc(BASS_MIDI_StreamGetEvent),
			ImpFunc(BASS_MIDI_StreamLoadSamples),
			ImpFunc(BASS_MIDI_StreamSetFonts),
			ImpFunc(BASS_MIDI_StreamGetChannel),

			// BASSWASAPI
			ImpFunc(BASS_WASAPI_Init),
			ImpFunc(BASS_WASAPI_Free),
			ImpFunc(BASS_WASAPI_IsStarted),
			ImpFunc(BASS_WASAPI_Start),
			ImpFunc(BASS_WASAPI_Stop),
			ImpFunc(BASS_WASAPI_GetDeviceInfo),
			ImpFunc(BASS_WASAPI_GetInfo),
			ImpFunc(BASS_WASAPI_GetDevice),
			ImpFunc(BASS_WASAPI_GetLevelEx),
			ImpFunc(BASS_WASAPI_SetNotify),

			// BASSVST
			ImpFunc(BASS_VST_ChannelSetDSP),

			// BASSASIO
			ImpFunc(BASS_ASIO_CheckRate),
			ImpFunc(BASS_ASIO_ChannelEnable),
			ImpFunc(BASS_ASIO_ChannelEnableBASS),
			ImpFunc(BASS_ASIO_ChannelEnableMirror),
			ImpFunc(BASS_ASIO_ChannelGetLevel),
			ImpFunc(BASS_ASIO_ChannelJoin),
			ImpFunc(BASS_ASIO_ChannelSetFormat),
			ImpFunc(BASS_ASIO_ChannelSetRate),
			ImpFunc(BASS_ASIO_ChannelGetInfo),
			ImpFunc(BASS_ASIO_ControlPanel),
			ImpFunc(BASS_ASIO_ErrorGetCode),
			ImpFunc(BASS_ASIO_Free),
			ImpFunc(BASS_ASIO_GetDevice),
			ImpFunc(BASS_ASIO_GetDeviceInfo),
			ImpFunc(BASS_ASIO_GetLatency),
			ImpFunc(BASS_ASIO_GetRate),
			ImpFunc(BASS_ASIO_Init),
			ImpFunc(BASS_ASIO_SetRate),
			ImpFunc(BASS_ASIO_Start),
			ImpFunc(BASS_ASIO_Stop),
			ImpFunc(BASS_ASIO_IsStarted)
		};
		size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

		std::jthread _AudThread;
		std::jthread _EvtThread;
		EvBuf* Events;

		unsigned int AudioStream = 0;

		SoundFontSystem SFSystem;
		std::vector<BASS_MIDI_FONTEX> SoundFonts;
		BASSSettings* Settings = nullptr;

#ifdef _WIN32
		XAudio2Output* XAEngine;
#endif

		bool RestartSynth = false;
		char LastRunningStatus = 0x0;

		// Threads func
		void AudioThread();
		void EventsThread();

		// BASS system
		bool LoadFuncs();
		bool ClearFuncs();
		void StreamSettings(bool restart);
		bool ProcessEvBuf();
		bool ProcessEvent(unsigned int ev);

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size);
		unsigned int GetSampleRate() { return Settings->AudioFrequency; }
		bool IsSynthInitialized() { return (AudioStream != 0); }
		int SynthID() { return 0x1411BA55; }

		// Event handling system
		SynthResult PlayShortEvent(unsigned int ev);
		SynthResult UPlayShortEvent(unsigned int ev);

		SynthResult PlayLongEvent(char* ev, unsigned int size);
		SynthResult UPlayLongEvent(char* ev, unsigned int size);

		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param);
	};
}

#endif