/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _XSYNTHM_H
#define _XSYNTHM_H

// Not supported on ARM Thumb-2!
#ifndef _M_ARM

#ifdef _WIN32
#include <windows.h>
#endif

#include "inc/xsynth.h"
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

#define XSYNTH_STR "XSynth"

namespace OmniMIDI {
	class XSynthSettings : public SettingsModule {
	public:
		// Global settings
		bool ThreadPool = false;
		bool FadeOutKilling = false;
		double RenderWindow = 5.0;
		uint64_t LayerCount = 2;

		XSynthSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {
			LoadSynthConfig();
		}

		void RewriteSynthConfig() {
			nlohmann::json DefConfig = {
					ConfGetVal(ThreadPool),
					ConfGetVal(FadeOutKilling),
					ConfGetVal(RenderWindow),
					ConfGetVal(LayerCount)
			};

			if (AppendToConfig(DefConfig))
				WriteConfig();

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

		void XSynthThread();

	public:
		XSynth(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		bool LoadSynthModule() override;
		bool UnloadSynthModule() override;
		bool StartSynthModule() override;
		bool StopSynthModule() override;
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) override { return false; }
		unsigned int GetSampleRate() override { return 48000; }
		bool IsSynthInitialized() override;
		int SynthID() override { return 0x9AF3812A; }
		void LoadSoundFonts() override;

		// Event handling system
		void PlayShortEvent(unsigned int ev) override;
		void UPlayShortEvent(unsigned int ev) override;
		void PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) override;
		void UPlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) override;

		// Not supported in XSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) override { return Ok; }
	};
}

#endif

#endif