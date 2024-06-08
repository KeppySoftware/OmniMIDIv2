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

#define KSYNTH_STR "KSynth"

namespace OmniMIDI {
	class KSynthSettings : public OMSettings {
	public:
		// Global settings
		unsigned int EvBufSize = 32768;
		unsigned int SampleRate = 48000;
		unsigned int VoiceLimit = 500;

		// XAudio2
		unsigned int XAMaxSamplesPerFrame = 88;
		unsigned int XASamplesPerFrame = 15;
		unsigned int XAChunksDivision = 1;

		std::string SampleSet = "sample.ksmp";

		void RewriteSynthConfig() {
			CloseConfig();
			if (InitConfig(true, KSYNTH_STR, sizeof(KSYNTH_STR))) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(EvBufSize),
						ConfGetVal(SampleRate),
						ConfGetVal(VoiceLimit),
						ConfGetVal(XAMaxSamplesPerFrame),
						ConfGetVal(XASamplesPerFrame),
						ConfGetVal(SampleSet)
					}
				};

				if (AppendToConfig(DefConfig))
					WriteConfig();
			}

			CloseConfig();
			InitConfig(false, KSYNTH_STR, sizeof(KSYNTH_STR));
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, KSYNTH_STR, sizeof(KSYNTH_STR))) {
				SynthSetVal(unsigned int, EvBufSize);
				SynthSetVal(unsigned int, SampleRate);
				SynthSetVal(unsigned int, VoiceLimit);
				SynthSetVal(unsigned int, XAMaxSamplesPerFrame);
				SynthSetVal(unsigned int, XASamplesPerFrame);
				SynthSetVal(std::string, SampleSet);

				if (!XAMaxSamplesPerFrame || XAMaxSamplesPerFrame < 32 || XAMaxSamplesPerFrame > 512)
					XAMaxSamplesPerFrame = 88;

				if (!XASamplesPerFrame || XASamplesPerFrame < XAMaxSamplesPerFrame / 8 || XASamplesPerFrame > XAMaxSamplesPerFrame / 2)
					XASamplesPerFrame = 15;

				if (XAChunksDivision > 4 || !XAChunksDivision)
					XAChunksDivision = 1;

				if (SampleRate == 0 || SampleRate > 384000)
					SampleRate = 48000;

				if (VoiceLimit < 1 || VoiceLimit > 100000)
					VoiceLimit = 1024;

				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class KSynthM : public SynthModule {
	private:
		SoundOut* WinAudioEngine = nullptr;
		KSynth* Synth = nullptr;
		KSynthSettings* Settings = nullptr;
		bool Terminate = false;

		Lib* KLib = nullptr;
		LibImport LibImports[7] = {
			// BASS
			ImpFunc(ksynth_new),
			ImpFunc(ksynth_cc),
			ImpFunc(ksynth_note_on),
			ImpFunc(ksynth_note_off),
			ImpFunc(ksynth_note_off_all),
			ImpFunc(ksynth_fill_buffer),
			ImpFunc(ksynth_free)
		};
		size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

		void AudioThread();
		void EventsThread();
		void LogThread();
		bool ProcessEvBuf();
		void ProcessEvBufChk();

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return 48000; }
		bool IsSynthInitialized() { return Synth != nullptr; }
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