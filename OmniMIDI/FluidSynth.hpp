/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _OFLUIDSYNTH_H
#define _OFLUIDSYNTH_H

#ifdef _WIN32
#include <Windows.h>
#endif

#include <fluidsynth.h>
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

#define FLUIDSYNTH_STR	"FluidSynth"

namespace OmniMIDI {
	class FluidSettings : public OMSettings {
	public:
		// Global settings
		unsigned int EvBufSize = 32768;
		unsigned int PeriodSize = 64;
		unsigned int Periods = 2;
		unsigned int ThreadsCount = 1;
		unsigned int MinimumNoteLength = 10;
		double OverflowVolume = 10000.0;
		double OverflowPercussion = 10000.0;
		double OverflowReleased = -10000.0;
		double OverflowImportant = 0.0;
		std::string Driver = "wasapi";
		std::string SampleFormat = "float";

		FluidSettings() {
			LoadSynthConfig();
		}

		void RewriteSynthConfig() {
			CloseConfig();
			if (InitConfig(true, FLUIDSYNTH_STR)) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(SampleRate),
						ConfGetVal(EvBufSize),
						ConfGetVal(VoiceLimit),
						ConfGetVal(PeriodSize),
						ConfGetVal(Periods),
						ConfGetVal(ThreadsCount),
						ConfGetVal(MinimumNoteLength),
						ConfGetVal(OverflowVolume),
						ConfGetVal(OverflowPercussion),
						ConfGetVal(OverflowReleased),
						ConfGetVal(OverflowImportant),
						ConfGetVal(Driver),
						ConfGetVal(SampleFormat)
					}
				};

				if (AppendToConfig(DefConfig))
					WriteConfig();
			}

			CloseConfig();
			InitConfig(false, FLUIDSYNTH_STR);
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, FLUIDSYNTH_STR)) {
				SynthSetVal(unsigned int, SampleRate);
				SynthSetVal(unsigned int, EvBufSize);
				SynthSetVal(unsigned int, VoiceLimit);
				SynthSetVal(unsigned int, PeriodSize);
				SynthSetVal(unsigned int, Periods);
				SynthSetVal(unsigned int, ThreadsCount);
				SynthSetVal(unsigned int, MinimumNoteLength);
				SynthSetVal(double, OverflowVolume);
				SynthSetVal(double, OverflowPercussion);
				SynthSetVal(double, OverflowReleased);
				SynthSetVal(double, OverflowImportant);
				SynthSetVal(std::string, Driver);
				SynthSetVal(std::string, SampleFormat);
				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class FluidSynth : public SynthModule {
	private:
		ErrorSystem::Logger SynErr;
		OMShared::Funcs MiscFuncs;

		Lib* FluiLib = nullptr;

		LibImport fLibImp[24] = {
			// BASS
			ImpFunc(new_fluid_synth),
			ImpFunc(new_fluid_settings),
			ImpFunc(delete_fluid_synth),
			ImpFunc(delete_fluid_settings),
			ImpFunc(fluid_synth_all_notes_off),
			ImpFunc(fluid_synth_all_sounds_off),
			ImpFunc(fluid_synth_noteoff),
			ImpFunc(fluid_synth_noteon),
			ImpFunc(fluid_synth_cc),
			ImpFunc(fluid_synth_channel_pressure),
			ImpFunc(fluid_synth_key_pressure),
			ImpFunc(fluid_synth_pitch_bend),
			ImpFunc(fluid_synth_pitch_wheel_sens),
			ImpFunc(fluid_synth_program_change),
			ImpFunc(fluid_synth_program_reset),
			ImpFunc(fluid_synth_program_select),
			ImpFunc(fluid_synth_sysex),
			ImpFunc(fluid_synth_system_reset),
			ImpFunc(fluid_synth_sfload),
			ImpFunc(fluid_settings_setint),
			ImpFunc(fluid_settings_setnum),
			ImpFunc(fluid_settings_setstr),
			ImpFunc(new_fluid_audio_driver),
			ImpFunc(delete_fluid_audio_driver)
		};
		size_t fLibImpLen = sizeof(fLibImp) / sizeof(fLibImp[0]);

		std::jthread _EvtThread;

		SoundFontSystem SFSystem;
		FluidSettings* Settings = nullptr;
		fluid_settings_t* fSet = nullptr;
		fluid_synth_t* fSyn = nullptr;
		fluid_audio_driver_t* fDrv = nullptr;
		std::vector<int> SoundFonts;

		void EventsThread();
		bool ProcessEvBuf();

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return Settings->SampleRate; }
		bool IsSynthInitialized() { return (fDrv != nullptr); }
		int SynthID() { return 0x6F704EC6; }

		// Event handling system
		void PlayShortEvent(unsigned int ev);
		void UPlayShortEvent(unsigned int ev);

		SynthResult PlayLongEvent(char* ev, unsigned int size);
		SynthResult UPlayLongEvent(char* ev, unsigned int size);

		// Not supported in FluidSynth
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif