
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
		unsigned int XASamplesPerFrame = 128;
		unsigned int XAChunksDivision = false;

		// WASAPI
		float WASAPIBuf = 32.0f;

		// ASIO
		unsigned int ASIOChunksDivision = 4;
		std::string ASIODevice = "None";
		std::string ASIOLCh = "0";
		std::string ASIORCh = "0";

		BASSSettings(ErrorSystem::Logger* PErr) : OMSettings(PErr) {}

		void RewriteSynthConfig() override {
			nlohmann::json DefConfig = {
				ConfGetVal(AsyncMode),
				ConfGetVal(ASIODevice),
				ConfGetVal(ASIOLCh),
				ConfGetVal(ASIORCh),
				ConfGetVal(StreamDirectFeed),
				ConfGetVal(FloatRendering),
				ConfGetVal(MonoRendering),
				ConfGetVal(XAChunksDivision),
				ConfGetVal(ASIOChunksDivision),
				ConfGetVal(OneThreadMode),
				ConfGetVal(ExperimentalMultiThreaded),
				ConfGetVal(FollowOverlaps),
				ConfGetVal(AudioEngine),
				ConfGetVal(SampleRate),
				ConfGetVal(EvBufSize),
				ConfGetVal(LoudMax),
				ConfGetVal(RenderTimeLimit),
				ConfGetVal(VoiceLimit),
				ConfGetVal(XASamplesPerFrame),
				ConfGetVal(WASAPIBuf)
			};

			if (AppendToConfig(DefConfig))
				WriteConfig();

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
				SynthSetVal(unsigned int, XASamplesPerFrame);
				SynthSetVal(unsigned int, VoiceLimit);
				SynthSetVal(unsigned int, XAChunksDivision);
				SynthSetVal(unsigned int, ASIOChunksDivision);

				if (!XASamplesPerFrame || XASamplesPerFrame < 16 || XASamplesPerFrame > 1024)
					XASamplesPerFrame = 128;

				if (XAChunksDivision > XASamplesPerFrame || !XAChunksDivision)
					XAChunksDivision = 4;

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
		HPLUGIN BFlaLib = 0;

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
			ImpFunc(BASS_SetConfig),
			ImpFunc(BASS_Stop),
			ImpFunc(BASS_StreamFree),

			ImpFunc(BASS_PluginLoad),
			ImpFunc(BASS_PluginFree),

			// BASSMIDI
			ImpFunc(BASS_MIDI_FontFree),
			ImpFunc(BASS_MIDI_FontInit),
			ImpFunc(BASS_MIDI_FontLoadEx),
			ImpFunc(BASS_MIDI_StreamCreate),
			ImpFunc(BASS_MIDI_StreamEvent),
			ImpFunc(BASS_MIDI_StreamEvents),
			ImpFunc(BASS_MIDI_StreamGetEvent),
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

#ifdef _WIN32
			// BASSVST
			ImpFunc(BASS_VST_ChannelSetDSP),
#endif

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

		unsigned char ASIOBuf[2048] = { 0 };
		unsigned int AudioStreams[ExperimentalAudioMultiplier] = { 0 };
		std::jthread _BASThread;

		SoundFontSystem SFSystem;
		std::vector<BASS_MIDI_FONTEX> SoundFonts;
		BASSSettings* Settings = nullptr;

		size_t AudioStreamSize = ExperimentalAudioMultiplier;

		// BASS system
		bool LoadFuncs();
		bool ClearFuncs();
		bool ProcessEvBuf();
		void ProcessEvBufChk();

		void AudioThread(unsigned int id);
		void EventsThread();
		void BASSThread();

		static unsigned long CALLBACK AudioProcesser(void*, unsigned long, BASSSynth*);
		static unsigned long CALLBACK AudioEvProcesser(void*, unsigned long, BASSSynth*);
		static unsigned long CALLBACK WasapiProc(void*, unsigned long, void*);
		static unsigned long CALLBACK WasapiEvProc(void*, unsigned long, void*);
		static unsigned long CALLBACK AsioProc(int, unsigned long, void*, unsigned long, void*);
		static unsigned long CALLBACK AsioEvProc(int, unsigned long, void*, unsigned long, void*);

	public:
		BASSSynth(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		void LoadSoundFonts();
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