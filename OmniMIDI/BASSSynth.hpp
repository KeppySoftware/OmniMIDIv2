
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

#define BASSSYNTH_STR "BASSSynth"

namespace OmniMIDI {
	enum BASSEngine {
		Invalid = -1,
		Internal,
		WASAPI,
		XAudio2,
		ASIO,
		BASSENGINE_COUNT = ASIO
	};

	class BASSSettings : public OMSettings {
	public:
		// Global settings
		unsigned int EvBufSize = 32768;
		unsigned int RenderTimeLimit = 95;
		int AudioEngine = (int)WASAPI;

		bool FollowOverlaps = false;
		bool LoudMax = false;
		bool AsyncMode = true;
		bool FloatRendering = true;
		bool MonoRendering = false;
		bool OneThreadMode = false;
		bool ExperimentalMultiThreaded = false;
		bool StreamDirectFeed = false;

		// XAudio2
		unsigned int XAMaxSamplesPerFrame = 88;
		unsigned int XASamplesPerFrame = 15;
		unsigned int XAChunksDivision = false;

		// WASAPI
		float WASAPIBuf = 32.0f;

		// ASIO
		std::string ASIODevice = "None";
		std::string ASIOLCh = "0";
		std::string ASIORCh = "0";

		void RewriteSynthConfig() override {
			CloseConfig();
			if (InitConfig(true, BASSSYNTH_STR, sizeof(BASSSYNTH_STR))) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(AsyncMode),
						ConfGetVal(ASIODevice),
						ConfGetVal(ASIOLCh),
						ConfGetVal(ASIORCh),
						ConfGetVal(StreamDirectFeed),
						ConfGetVal(FloatRendering),
						ConfGetVal(MonoRendering),
						ConfGetVal(XAChunksDivision),
						ConfGetVal(OneThreadMode),
						ConfGetVal(ExperimentalMultiThreaded),
						ConfGetVal(FollowOverlaps),
						ConfGetVal(AudioEngine),
						ConfGetVal(SampleRate),
						ConfGetVal(EvBufSize),
						ConfGetVal(LoudMax),
						ConfGetVal(RenderTimeLimit),
						ConfGetVal(VoiceLimit),
						ConfGetVal(XAMaxSamplesPerFrame),
						ConfGetVal(XASamplesPerFrame),
						ConfGetVal(WASAPIBuf)
					}
				};

				if (AppendToConfig(DefConfig))
					WriteConfig();
			}

			CloseConfig();
			InitConfig(false, BASSSYNTH_STR, sizeof(BASSSYNTH_STR));
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() override {
			if (InitConfig(false, BASSSYNTH_STR, sizeof(BASSSYNTH_STR))) {
				SynthSetVal(bool, AsyncMode);
				SynthSetVal(bool, LoudMax);
				SynthSetVal(bool, OneThreadMode);
				SynthSetVal(bool, ExperimentalMultiThreaded);
				SynthSetVal(bool, StreamDirectFeed);
				SynthSetVal(bool, FloatRendering);
				SynthSetVal(bool, MonoRendering);
				SynthSetVal(bool, FollowOverlaps);
				SynthSetVal(float, WASAPIBuf);
				SynthSetVal(int, AudioEngine);
				SynthSetVal(std::string, ASIODevice);
				SynthSetVal(std::string, ASIOLCh);
				SynthSetVal(std::string, ASIORCh);
				SynthSetVal(unsigned int, SampleRate);
				SynthSetVal(unsigned int, EvBufSize);
				SynthSetVal(unsigned int, RenderTimeLimit);
				SynthSetVal(unsigned int, XAMaxSamplesPerFrame);
				SynthSetVal(unsigned int, XASamplesPerFrame);
				SynthSetVal(unsigned int, VoiceLimit);
				SynthSetVal(unsigned int, XAChunksDivision);

				if (!XAMaxSamplesPerFrame || XAMaxSamplesPerFrame < 16 || XAMaxSamplesPerFrame > 1024)
					XAMaxSamplesPerFrame = 96;

				if (XAChunksDivision > XAMaxSamplesPerFrame || !XAChunksDivision)
					XAChunksDivision = 4;

				if (!XASamplesPerFrame || XASamplesPerFrame < XAMaxSamplesPerFrame / XAChunksDivision || XASamplesPerFrame > XAMaxSamplesPerFrame / 2)
					XASamplesPerFrame = 24;

				if (SampleRate == 0 || SampleRate > 384000)
					SampleRate = 48000;

				if (VoiceLimit < 1 || VoiceLimit > 100000)
					VoiceLimit = 1024;

				if (AudioEngine < Internal || AudioEngine > BASSENGINE_COUNT)
					AudioEngine = WASAPI;

				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class BASSSynth : public SynthModule {
	private:
		Lib* BAudLib = nullptr;
		Lib* BMidLib = nullptr;
		Lib* BWasLib = nullptr;
		Lib* BVstLib = nullptr;
		Lib* BAsiLib = nullptr;

		LibImport LibImports[68] = {
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
			ImpFunc(BASS_Update),
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
			ImpFunc(BASS_ASIO_GetInfo),
			ImpFunc(BASS_ASIO_Init),
			ImpFunc(BASS_ASIO_SetRate),
			ImpFunc(BASS_ASIO_Start),
			ImpFunc(BASS_ASIO_Stop),
			ImpFunc(BASS_ASIO_IsStarted)
		};
		size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

		unsigned int AudioStreams[16] = { 0 };
		size_t AudioStreamSize = sizeof(AudioStreams) / sizeof(unsigned int);
		std::jthread _BASThread;

		SoundFontSystem SFSystem;
		std::vector<BASS_MIDI_FONTEX> SoundFonts;
		BASSSettings* Settings = nullptr;

#ifdef _WIN32
		SoundOut* WinAudioEngine;
#endif

		bool RestartSynth = false;

		// BASS system
		bool LoadFuncs();
		bool ClearFuncs();
		void StreamSettings(bool restart);
		void LoadSoundFonts();
		bool ProcessEvBuf();
		void ProcessEvBufChk();

		void AudioThread(HSTREAM hStream);
		void EventsThread();
		void BASSThread();

		static unsigned long CALLBACK AudioProcesser(void*, unsigned long, void*);
		static unsigned long CALLBACK AudioEvProcesser(void*, unsigned long, void*);
		static unsigned long CALLBACK AsioProc(int, unsigned long, void*, unsigned long, void*);
		static unsigned long CALLBACK AsioEvProc(int, unsigned long, void*, unsigned long, void*);

	public:
		bool LoadSynthModule();
		bool UnloadSynthModule();
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size);
		unsigned int GetSampleRate() { return Settings->SampleRate; }
		bool IsSynthInitialized() { return (AudioStreams[0] != 0); }
		int SynthID() { return 0x1411BA55; }

		unsigned int PlayLongEvent(char* ev, unsigned int size);
		unsigned int UPlayLongEvent(char* ev, unsigned int size);

		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param);
	};
}

#endif