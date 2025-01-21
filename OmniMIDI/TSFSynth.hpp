/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _TSFSYNTH_H
#define _TSFSYNTH_H

#ifdef _M_ARM
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#include "inc/tsf/minisdl_audio.h"
#include "inc/tsf.h"
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

#define TINYSF_STR "TinySFSynth"

namespace OmniMIDI {
	class TinySFSettings : public OMSettings {
	public:
		// Global settings
		bool StereoRendering = true;
		unsigned int EvBufSize = 32768;
		unsigned int Samples = 4096;

		TinySFSettings(ErrorSystem::Logger* PErr) : OMSettings(PErr) {
			LoadSynthConfig();
		}

		void RewriteSynthConfig() {
			nlohmann::json DefConfig = {
					ConfGetVal(StereoRendering),
					ConfGetVal(EvBufSize),
					ConfGetVal(SampleRate),
					ConfGetVal(Samples),
					ConfGetVal(VoiceLimit)
			};

			if (AppendToConfig(DefConfig))
				WriteConfig();

			CloseConfig();
			InitConfig(false, TINYSF_STR);
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, TINYSF_STR, sizeof(TINYSF_STR))) {
				SynthSetVal(bool, StereoRendering);
				SynthSetVal(unsigned int, EvBufSize);
				SynthSetVal(unsigned int, SampleRate);
				SynthSetVal(unsigned int, Samples);
				SynthSetVal(unsigned int, VoiceLimit);
				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class TinySFSynth : public SynthModule {
	private:
		OMShared::Funcs MiscFuncs;

		SDL_AudioSpec OutputAudioSpec = { 0 };

		// Holds the global instance pointer
		tsf* g_TinySoundFont = nullptr;

		// A Mutex so we don't call note_on/note_off while rendering audio samples
		SDL_mutex* g_Mutex = nullptr;

		SoundFontSystem SFSystem;
		TinySFSettings* Settings = nullptr;
		bool Running = false;

		const static constexpr unsigned char MinimalSoundFont[] =
		{
			#define TEN0 0,0,0,0,0,0,0,0,0,0
			'R','I','F','F',220,1,0,0,'s','f','b','k',
			'L','I','S','T',88,1,0,0,'p','d','t','a',
			'p','h','d','r',76,TEN0,TEN0,TEN0,TEN0,0,0,0,0,TEN0,0,0,0,0,0,0,0,255,0,255,0,1,TEN0,0,0,0,
			'p','b','a','g',8,0,0,0,0,0,0,0,1,0,0,0,'p','m','o','d',10,TEN0,0,0,0,'p','g','e','n',8,0,0,0,41,0,0,0,0,0,0,0,
			'i','n','s','t',44,TEN0,TEN0,0,0,0,0,0,0,0,0,TEN0,0,0,0,0,0,0,0,1,0,
			'i','b','a','g',8,0,0,0,0,0,0,0,2,0,0,0,'i','m','o','d',10,TEN0,0,0,0,
			'i','g','e','n',12,0,0,0,54,0,1,0,53,0,0,0,0,0,0,0,
			's','h','d','r',92,TEN0,TEN0,0,0,0,0,0,0,0,50,0,0,0,0,0,0,0,49,0,0,0,34,86,0,0,60,0,0,0,1,TEN0,TEN0,TEN0,TEN0,0,0,0,0,0,0,0,
			'L','I','S','T',112,0,0,0,'s','d','t','a','s','m','p','l',100,0,0,0,86,0,119,3,31,7,147,10,43,14,169,17,58,21,189,24,73,28,204,31,73,35,249,38,46,42,71,46,250,48,150,53,242,55,126,60,151,63,108,66,126,72,207,
				70,86,83,100,72,74,100,163,39,241,163,59,175,59,179,9,179,134,187,6,186,2,194,5,194,15,200,6,202,96,206,159,209,35,213,213,216,45,220,221,223,76,227,221,230,91,234,242,237,105,241,8,245,118,248,32,252
		};

		static void cCallback(void* data, Uint8* stream, int len) {
			static_cast<TinySFSynth*>(data)->Callback(reinterpret_cast<float*>(stream), len);
		}

		void Callback(float* stream, int len) {
			// Render the audio samples in float format
			int SampleCount = (len / ((Settings->StereoRendering ? 2 : 1) * sizeof(float))); //2 output channels
			SDL_LockMutex(g_Mutex); //get exclusive lock
			tsf_render_float(g_TinySoundFont, stream, SampleCount, 0);
			SDL_UnlockMutex(g_Mutex);
		}

		void EventsThread();
		bool ProcessEvBuf();

	public:
		TinySFSynth(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		void LoadSoundFonts();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return Settings->SampleRate; }
		bool IsSynthInitialized() { return (g_TinySoundFont != nullptr); }
		int SynthID() { return 0xA0A0A0A0; }

		unsigned int PlayLongEvent(char* ev, unsigned int size);
		unsigned int UPlayLongEvent(char* ev, unsigned int size);

		// Not supported in XSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif