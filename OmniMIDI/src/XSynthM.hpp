/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _XSYNTHM_H
#define _XSYNTHM_H

// First
#include "Common.hpp"

// Not supported on ARM Thumb-2!
#ifndef _M_ARM

#include "SynthMain.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <vector>
#include "xsynth.h"

#define XSYNTH_STR "XSynth"

namespace OmniMIDI {
	class XSynthSettings : public SettingsModule {
	public:
		// Global settings
		bool FadeOutKilling = false;
		double RenderWindow = 5.0;
		uint64_t LayerCount = 2;
		int32_t ThreadsCount = 1;

		XSynthSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {}

		void RewriteSynthConfig() {
			nlohmann::json DefConfig = {
					ConfGetVal(FadeOutKilling),
					ConfGetVal(RenderWindow),
					ConfGetVal(ThreadsCount),
					ConfGetVal(LayerCount),
			};

			if (AppendToConfig(DefConfig))
				WriteConfig();

			CloseConfig();
			InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR));
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, XSYNTH_STR, sizeof(XSYNTH_STR))) {
				SynthSetVal(bool, FadeOutKilling);
				SynthSetVal(double, RenderWindow);
				SynthSetVal(int32_t, ThreadsCount);
				SynthSetVal(uint64_t, LayerCount);
			
				if (!RANGE(ThreadsCount, -1, std::thread::hardware_concurrency()))
					ThreadsCount = -1;

				if (!RANGE(LayerCount, 1, UINT16_MAX))
					LayerCount = 4;

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
		std::vector<XSynth_Soundfont> SoundFonts;
		XSynth_RealtimeSynth realtimeSynth;
		XSynth_RealtimeConfig realtimeConf;
		XSynth_RealtimeStats realtimeStats;
		std::jthread _XSyThread;

		LibImport xLibImp[12] = {
			ImpFunc(XSynth_GenDefault_RealtimeConfig),
			ImpFunc(XSynth_GenDefault_SoundfontOptions),
			ImpFunc(XSynth_Realtime_Drop),
			ImpFunc(XSynth_Realtime_GetStats),
			ImpFunc(XSynth_Realtime_GetStreamParams),
			ImpFunc(XSynth_Realtime_Create),
			ImpFunc(XSynth_Realtime_Reset),
			ImpFunc(XSynth_Realtime_SendEventU32),
			ImpFunc(XSynth_Realtime_SendConfigEventAll),
			ImpFunc(XSynth_Realtime_SetSoundfonts),
			ImpFunc(XSynth_Realtime_ClearSoundfonts),
			ImpFunc(XSynth_Soundfont_LoadNew)
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

		// Not supported in XSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) override { return Ok; }
	};
}

#endif

#endif