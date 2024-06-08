/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _XSYNTHM_H
#define _XSYNTHM_H

// Not supported on ARM Thumb-2!
#ifndef _M_ARM

#ifdef _WIN32
#include <Windows.h>
#endif

#include <xsynth.h>
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
#include "EvBuf_t.hpp"
#include "SynthMain.hpp"
#include "SoundFontSystem.hpp"

#define XSYNTH_STR "XSynth"

namespace OmniMIDI {
	class XSynthSettings : public OMSettings {
	public:
		// Global settings
		double BufSize = 5.0;

		XSynthSettings() {
			LoadSynthConfig();
		}

		void RewriteSynthConfig() {
			CloseConfig();
			if (InitConfig(true, XSYNTH_STR, sizeof(XSYNTH_STR))) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(BufSize)
					}
				};

				if (AppendToConfig(DefConfig))
					WriteConfig();
			}

			CloseConfig();
			InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR));
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR))) {
				SynthSetVal(double, BufSize);
				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class XSynth : public SynthModule {
	private:
		Lib* XLib = nullptr;

		LibImport xLibImp[5] = {
			// BASS
			ImpFunc(StartModule),
			ImpFunc(StopModule),
			ImpFunc(SendData),
			ImpFunc(LoadSoundFont),
			ImpFunc(ResetModule)
		};
		size_t xLibImpLen = sizeof(xLibImp) / sizeof(xLibImp[0]);

		SoundFontSystem SFSystem;
		XSynthSettings* Settings = nullptr;
		bool Running = false;

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return 48000; }
		bool IsSynthInitialized() { return 0; }
		int SynthID() { return 0x9AF3812A; }

		// Event handling system
		void PlayShortEvent(unsigned int ev);
		void UPlayShortEvent(unsigned int ev);

		SynthResult PlayLongEvent(char* ev, unsigned int size);
		SynthResult UPlayLongEvent(char* ev, unsigned int size);

		// Not supported in XSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif

#endif