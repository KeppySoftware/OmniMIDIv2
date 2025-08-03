/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

// Always first
#include "Common.hpp"

#ifdef _NONFREE

#ifndef _BASSSYNTH_H
#define _BASSSYNTH_H

#include "SynthModule.hpp"
#include <unordered_map>

#include "bass/bass.h"
#include "bass/bassmidi.h"
#include "bass/bass_fx.h"
#define	BASE_IMPORTS				43

#if defined(_WIN32)
#include "bass/bassasio.h"
#include "bass/basswasapi.h"
#define	ADD_IMPORTS					32
#define DEFAULT_ENGINE				WASAPI
#else
#define ADD_IMPORTS					0
#define DEFAULT_ENGINE				Internal
#endif

#define FINAL_IMPORTS				BASE_IMPORTS + ADD_IMPORTS

#define TGT_BASS					2 << 24 | 4 << 16 | 17 << 8 | 0
#define TGT_BASSMIDI				2 << 24 | 4 << 16 | 15 << 8 | 0

#define CCEvent(a)					evt = a; ev = param2;
#define CCEmptyEvent(a)				evt = a; ev = 0;

#define MIDI_EVENT_RAW				0xFFFF

#define BASSSYNTH_STR				"BASSSynth"

namespace OmniMIDI {
	enum BASSEngine {
		Invalid = -1,
		Internal,
		WASAPI,
		ASIO,
		BASSENGINE_COUNT = ASIO
	};

	class BASSSettings : public SettingsModule {
	public:
		// Global settings
		uint64_t EvBufSize = 32768;
		uint32_t RenderTimeLimit = 95;
		int32_t AudioAPI = (int)DEFAULT_ENGINE;

		bool FollowOverlaps = false;
		bool AudioLimiter = false;
		bool AsyncMode = true;
		bool FloatingPointAudio = true;
		bool MonoRendering = false;
		bool OneThreadMode = false;
		bool StreamDirectFeed = false;
		bool DisableEffects = false;

		// EXP
		const uint8_t MultiThreadChDiv = 16;
		uint8_t MultiThreadKbDiv = 4;
		uint8_t KeyboardChunk = 128 / MultiThreadKbDiv;

		bool MultiThreadMode = false;
		uint64_t MultiThreadMultiplier = MultiThreadChDiv * MultiThreadKbDiv;
		uint32_t AudioBufSize = 10;

#ifndef _WIN32
		uint32_t BufPeriod = 480;
#endif

#ifdef _WIN32
		// WASAPI
		float WASAPIBuf = 32.0f;

		// ASIO
		uint32_t ASIOChunksDivision = 4;
		std::string ASIODevice = "None";
		std::string ASIOLCh = "0";
		std::string ASIORCh = "0";
#endif

		BASSSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {}

		void RewriteSynthConfig() override {
			nlohmann::json DefConfig = {
				ConfGetVal(AsyncMode),
				ConfGetVal(StreamDirectFeed),
				ConfGetVal(FloatingPointAudio),
				ConfGetVal(MonoRendering),
				ConfGetVal(DisableEffects),

				ConfGetVal(MultiThreadMode),
				ConfGetVal(MultiThreadKbDiv),

				ConfGetVal(OneThreadMode),
				ConfGetVal(FollowOverlaps),
				ConfGetVal(AudioAPI),
				ConfGetVal(SampleRate),
				ConfGetVal(EvBufSize),
				ConfGetVal(AudioLimiter),
				ConfGetVal(RenderTimeLimit),
				ConfGetVal(VoiceLimit),
				ConfGetVal(AudioBufSize),

#if !defined(_WIN32)
				ConfGetVal(BufPeriod),
#endif

#if defined(_WIN32)
				ConfGetVal(ASIODevice),
				ConfGetVal(ASIOLCh),
				ConfGetVal(ASIORCh),
				ConfGetVal(ASIOChunksDivision),
				ConfGetVal(WASAPIBuf),
#endif
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
				SynthSetVal(bool, AudioLimiter);
				SynthSetVal(bool, OneThreadMode);
				SynthSetVal(bool, MultiThreadMode);
				SynthSetVal(char, MultiThreadKbDiv);
				SynthSetVal(bool, StreamDirectFeed);
				SynthSetVal(bool, FloatingPointAudio);
				SynthSetVal(bool, MonoRendering);
				SynthSetVal(bool, FollowOverlaps);
				SynthSetVal(bool, DisableEffects);
				SynthSetVal(int32_t, AudioAPI);
				SynthSetVal(uint32_t, SampleRate);
				SynthSetVal(uint32_t, RenderTimeLimit);
				SynthSetVal(uint32_t, VoiceLimit);
				SynthSetVal(uint32_t, AudioBufSize);
				SynthSetVal(size_t, EvBufSize);

#if !defined(_WIN32)
				SynthSetVal(uint32_t, BufPeriod);
#endif			

#if defined(_WIN32)
				SynthSetVal(std::string, ASIODevice);
				SynthSetVal(std::string, ASIOLCh);
				SynthSetVal(std::string, ASIORCh);
				SynthSetVal(uint32_t, ASIOChunksDivision);
				SynthSetVal(float, WASAPIBuf);
#endif

				if (SampleRate == 0 || SampleRate > 384000)
					SampleRate = 48000;

				if (VoiceLimit < 1 || VoiceLimit > 100000)
					VoiceLimit = 1024;

				if (AudioAPI < Internal || AudioAPI > BASSENGINE_COUNT)
					AudioAPI = DEFAULT_ENGINE;

				if (MultiThreadKbDiv > 128)
					MultiThreadKbDiv = 128;

				// Round to nearest power of 2, to avoid issues with pitch bends
				if ((bool)MultiThreadKbDiv && !(MultiThreadKbDiv & (MultiThreadKbDiv - 1))) {
					MultiThreadKbDiv = (uint8_t)pow(2, ceil(log(MultiThreadKbDiv)/log(2)));
				}

#if !defined(_WIN32)
				if (BufPeriod < 0 || BufPeriod > 4096)
					BufPeriod = 480;
#endif		
				
				KeyboardChunk = 128 / MultiThreadKbDiv;
				MultiThreadMultiplier = MultiThreadChDiv * MultiThreadKbDiv;

				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	struct BASSMTBufStruct {
		float* data = nullptr;
		bool filled = false;
	};

	class BASSMTBuf {
	private:
		BASSMTBufStruct* _bufPtr = nullptr;
		size_t _bufCount = 0;
		uint32_t _bufSize = 0;
		std::mutex audioBufferMutex;

	public:
		BASSMTBuf(size_t count, uint32_t size = 0) {
			_bufCount = count;
			_bufSize = size;

			if (_bufCount < 1)
				return;
			
			_bufPtr = new BASSMTBufStruct[count] { 0 };
			if (!_bufPtr)
				delete this;

			SetBufSize(_bufSize);
		}

		~BASSMTBuf() {
			SetBufSize(0);

			if (_bufPtr != nullptr)
				delete[] _bufPtr;

			_bufPtr = nullptr;
			_bufCount = 0;
		}

		void SetBufSize(uint32_t size) {
			_bufSize = size;

			for (size_t i = 0; i < _bufCount; i++) {
				if (_bufSize < 1) {
					if (_bufPtr[i].data != nullptr)
						delete[] _bufPtr[i].data;

					_bufPtr[i].data = nullptr;
					continue;
				}
	
				_bufPtr[i].data = new float[_bufSize] { 0.0f };
				if (!_bufPtr[i].data)
					delete this;
			}
		}

		BASSMTBufStruct* GetBufArray() { return _bufPtr; }
		BASSMTBufStruct* GetBufPtr(size_t id) { return &_bufPtr[id]; }
		size_t GetBufCount() { return _bufCount; }
		uint32_t GetBufSize() { return _bufSize; }
	};

	class BASSSynth : public SynthModule {
	private:
		Lib* BAudLib = nullptr;
		Lib* BMidLib = nullptr;
		Lib* BEfxLib = nullptr;

		Lib* BFlaLib = nullptr;
		uint32_t BFlaLibHandle = 0;

#ifdef _WIN32
		Lib* BWasLib = nullptr;
		Lib* BAsiLib = nullptr;

		unsigned char ASIOBuf[2048] = { 0 };
#endif

		std::unordered_map<int, std::string> BASSErrReason {
			{BASS_ERROR_UNKNOWN, "Unknown"},
			{BASS_ERROR_MEM, "Memory error"},
			{BASS_ERROR_FILEOPEN, "Can't open file"},
			{BASS_ERROR_DRIVER, "Can't find a free/valid output device"},
			{BASS_ERROR_HANDLE, "Invalid stream handle"},
			{BASS_ERROR_FORMAT, "Unsupported sample format for output device"},
			{BASS_ERROR_INIT, "Init has not been successfully called"},
			{BASS_ERROR_REINIT, "Output device needs to be reinitialized"},
			{BASS_ERROR_ALREADY, "Output device already initialized/paused"},
			{BASS_ERROR_ILLTYPE, "Illegal type"},
			{BASS_ERROR_ILLPARAM, "Illegal parameter"},
			{BASS_ERROR_DEVICE, "Illegal device"},
			{BASS_ERROR_FREQ, "Illegal sample rate"},
			{BASS_ERROR_NOHW, "No hardware voices available"},
			{BASS_ERROR_NOTAVAIL, "Requested data/action is not available"},
			{BASS_ERROR_DECODE, "The channel is/isn't a \"decoding channel\""},
			{BASS_ERROR_FILEFORM, "Unsupported file format"},
			{BASS_ERROR_VERSION, "Invalid BASS version"},
			{BASS_ERROR_CODEC, "Requested audio codec is not available"},
			{BASS_ERROR_BUSY, "Output device is busy"}
		};

		LibImport LibImports[FINAL_IMPORTS] = {
			// BASS
			ImpFunc(BASS_GetVersion),
			ImpFunc(BASS_ChannelFlags),
			ImpFunc(BASS_StreamCreate),
			ImpFunc(BASS_ChannelGetAttribute),
			ImpFunc(BASS_ChannelGetData),
			ImpFunc(BASS_ChannelSetPosition),
			ImpFunc(BASS_ChannelGetPosition),
			ImpFunc(BASS_ChannelSeconds2Bytes),
			ImpFunc(BASS_ChannelBytes2Seconds),
			ImpFunc(BASS_StreamPutData),
			ImpFunc(BASS_ChannelGetLevelEx),
			ImpFunc(BASS_ChannelIsActive),
			ImpFunc(BASS_ChannelPlay),
			ImpFunc(BASS_ChannelRemoveFX),
			ImpFunc(BASS_ChannelSetAttribute),
			ImpFunc(BASS_ChannelSetDevice),
			ImpFunc(BASS_ChannelSetFX),
			ImpFunc(BASS_ChannelStop),
			ImpFunc(BASS_ChannelUpdate),
			ImpFunc(BASS_ErrorGetCode),
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
			ImpFunc(BASS_MIDI_GetVersion),

			ImpFunc(BASS_FXGetParameters),
			ImpFunc(BASS_FXSetParameters),

#ifdef _WIN32
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
#endif
		};

		size_t LibImportsSize = sizeof(LibImports) / sizeof(LibImports[0]);

		HFX audioLimiter = 0;

		BASSMTBuf* _audBufs = nullptr;
		uint32_t* _audStreams = nullptr;

		uint32_t _extraEvtFlags = 0;
		std::jthread _BASThread;

		BASSSettings* _bassConfig = nullptr;
		SoundFontSystem _sfSystem;

		std::vector<BASS_MIDI_FONTEX> SoundFonts;

		size_t _streamsBegin = 0;
		size_t _streamsSize = 1;

		// BASS system
		bool LoadFuncs();
		bool ClearFuncs();
		const char* GetBASSError();
		void ProcessEvBuf();
		void ProcessEvBufChk();

		inline bool SIMDBufCpy(float* dst, size_t dstCount);
		void AudioThread(bool multiThreadedMode, uint32_t multiThreadedModed);
		void EventsThread();
		void BASSThread();

		static uint32_t CALLBACK MTProcesser(void *buffer, uint32_t length, BASSSynth* me);
		static DWORD CALLBACK BassProc(HSTREAM, void*, DWORD, void*);

#if defined(_WIN32)
		static DWORD CALLBACK AudioProcesser(void*, DWORD, BASSSynth*);
		static DWORD CALLBACK AudioEvProcesser(void*, DWORD, BASSSynth*);
		static DWORD CALLBACK WasapiProc(void*, DWORD, void*);
		static DWORD CALLBACK WasapiEvProc(void*, DWORD, void*);
		static DWORD CALLBACK AsioProc(int, DWORD, void*, DWORD, void*);
		static DWORD CALLBACK AsioEvProc(int, DWORD, void*, DWORD, void*);
#endif

	public:
		BASSSynth(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		void LoadSoundFonts() override;
		bool LoadSynthModule() override;
		bool UnloadSynthModule() override;
		bool StartSynthModule() override;
		bool StopSynthModule() override;
		bool SettingsManager(uint32_t setting, bool get, void* var, size_t size) override;
		uint32_t GetSampleRate() override { return _bassConfig != nullptr ? _bassConfig->SampleRate : 0; }
		bool IsSynthInitialized() override { return (_audStreams != nullptr && _audStreams[0] != 0); }
		uint32_t SynthID() override { return 0x1411BA55; }

		uint32_t PlayLongEvent(uint8_t* ev, uint32_t size) override;
		uint32_t UPlayLongEvent(uint8_t* ev, uint32_t size) override;

		SynthResult TalkToSynthDirectly(uint32_t evt, uint32_t chan, uint32_t param) override;
	};
}

#endif

#endif