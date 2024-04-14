/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _KSYNTHM_H
#define _KSYNTHM_H

#ifdef _WIN32
#include <Windows.h>
#endif
#include <thread>
#include <atomic>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <codecvt>
#include <locale>
#include <future>
#include <ksynth.h>
#include "EvBuf_t.hpp"
#include "SynthMain.hpp"
#include "SoundFontSystem.hpp"
#include "SynthMain.hpp"
#include "XAudio2Output.hpp"

namespace OmniMIDI {
	class KSynthSettings : public OMSettings {
	public:
		unsigned int EvBufSize = 32768;
		unsigned int SampleRate = 48000;
		unsigned int MaxVoices = 500;

		// XAudio2
		unsigned int XABufSize = 128;
		unsigned int XASweepRate = 32;
		bool ShowStats = false;

		std::string SampleSet = "sample.ksmp";

		KSynthSettings() {
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
					{ "XSynth", {
						JSONGetVal(EvBufSize),
						JSONGetVal(SampleRate),
						JSONGetVal(MaxVoices),
						JSONGetVal(XABufSize),
						JSONGetVal(XASweepRate),
						JSONGetVal(SampleSet),
						JSONGetVal(ShowStats)
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
						auto& JsonData = json["KSynth"];

						if (!(JsonData == nullptr)) {
							JSONSetVal(unsigned int, EvBufSize);
							JSONSetVal(unsigned int, SampleRate);
							JSONSetVal(unsigned int, MaxVoices);
							JSONSetVal(unsigned int, XABufSize);
							JSONSetVal(unsigned int, XASweepRate);
							JSONSetVal(bool, ShowStats);
							JSONSetVal(std::string, SampleSet);
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

	class KSynthM : public SynthModule {
	private:
		XAudio2Output* XAEngine;
		KSynth* Synth = nullptr;
		KSynthSettings* Settings = nullptr;
		bool OwnConsole = false;
		bool Terminate = false;

		Lib* KLib = nullptr;
		LibImport LibImports[7] = {
			// BASS
			ImpFunc(ksynth_new),
			ImpFunc(ksynth_note_on),
			ImpFunc(ksynth_note_off),
			ImpFunc(ksynth_note_off_all),
			ImpFunc(ksynth_generate_buffer),
			ImpFunc(ksynth_buffer_free),
			ImpFunc(ksynth_free)
		};
		size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

		void AudioThread();
		void EventsThread();
		void LogThread();
		bool ProcessEvBuf();

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return 48000; }
		bool IsSynthInitialized() { return 0; }
		int SynthID() { return 0xFEFEFEFE; }

		// Event handling system
		void PlayShortEvent(unsigned int ev);
		void UPlayShortEvent(unsigned int ev);

		SynthResult PlayLongEvent(char* ev, unsigned int size) { return Ok; }
		SynthResult UPlayLongEvent(char* ev, unsigned int size) { return Ok; }

		SynthResult Reset();

		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif