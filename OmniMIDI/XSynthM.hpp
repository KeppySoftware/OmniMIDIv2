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
		bool ThreadPool = false;
		bool FadeOutKilling = false;
		double RenderWindow = 5.0;
		uint64_t LayerCount = 2;

		XSynthSettings(ErrorSystem::Logger* PErr) : OMSettings(PErr) {
			LoadSynthConfig();
		}

		void RewriteSynthConfig() {
			CloseConfig();
			if (InitConfig(true, XSYNTH_STR, sizeof(XSYNTH_STR))) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(ThreadPool),
						ConfGetVal(FadeOutKilling),
						ConfGetVal(RenderWindow),
						ConfGetVal(LayerCount)
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
				SynthSetVal(bool, ThreadPool);
				SynthSetVal(bool, FadeOutKilling);
				SynthSetVal(double, RenderWindow);
				SynthSetVal(uint64_t, LayerCount);
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

		OMShared::Funcs MiscFuncs;
		std::vector<uint64_t> SoundFonts;
		XSynth_RealtimeConfig realtimeConf;
		XSynth_RealtimeStats realtimeStats;
		XSynth_StreamParams realtimeParams;
		std::jthread _XSyThread;

		LibImport xLibImp[14] = {
			// BASS
			ImpFunc(XSynth_GenDefault_RealtimeConfig),
			ImpFunc(XSynth_GenDefault_SoundfontOptions),
			ImpFunc(XSynth_Realtime_Drop),
			ImpFunc(XSynth_Realtime_GetStats),
			ImpFunc(XSynth_Realtime_GetStreamParams),
			ImpFunc(XSynth_Realtime_Init),
			ImpFunc(XSynth_Realtime_IsActive),
			ImpFunc(XSynth_Realtime_Reset),
			ImpFunc(XSynth_Realtime_SendEvent),
			ImpFunc(XSynth_Realtime_SetLayerCount),
			ImpFunc(XSynth_Realtime_SetSoundfonts),
			ImpFunc(XSynth_Soundfont_LoadNew),
			ImpFunc(XSynth_Soundfont_Remove),
			ImpFunc(XSynth_Soundfont_RemoveAll)
		};
		size_t xLibImpLen = sizeof(xLibImp) / sizeof(xLibImp[0]);

		XSynthSettings* Settings = nullptr;
		SoundFontSystem SFSystem;
		bool Running = false;

	public:
		XSynth(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return 48000; }
		bool IsSynthInitialized();
		int SynthID() { return 0x9AF3812A; }
		void LoadSoundFonts();
		void XSynthThread();

		// Event handling system
		void PlayShortEvent(unsigned int ev);
		void UPlayShortEvent(unsigned int ev);
		void PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2);
		void UPlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2);

		// Not supported in XSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif

#endif