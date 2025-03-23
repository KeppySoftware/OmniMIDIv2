/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "Common.hpp"

#ifndef _SYNTHMAIN_H
#define _SYNTHMAIN_H

#pragma once

#include <cstdint>
#include <thread>
#include <vector>

// ERRORS
#define SYNTH_OK				0x00
#define SYNTH_NOTINIT			0x01
#define SYNTH_INITERR			0x02
#define SYNTH_LIBOFFLINE		0x03
#define SYNTH_INVALPARAM		0x04
#define SYNTH_INVALBUF			0x05

#define fv2fn(f)				(#f)
#define ImpFunc(f)				LibImport((void**)&(f), #f)

#define ConfGetVal(f)			{ #f, f }
#define JSONSetVal(t, f)		f = JsonData[#f].is_null() ? (f) : JsonData[#f].get<t>()
#define MainSetVal(t, f)		f = mainptr[#f].is_null() ? (f) : mainptr[#f].get<t>()
#define SynthSetVal(t, f)		f = synptr[#f].is_null() ? (f) : synptr[#f].get<t>()

#define SettingsManagerCase(choice, get, type, setting, var, size) \
	case choice: \
		if (size != sizeof(type)) return false; \
		if (get) *(type*)var = setting; \
		else setting = *(type*)var; \
		break;

#define SYNTHNAME_SZ		64
#define DUMMY_STR			"dummy"
#define EMPTYMODULE			0xDEADBEEF

#include "Common.hpp"
#include "ErrSys.hpp"
#include "Utils.hpp"
#include "EvBuf_t.hpp"
#include "KDMAPI.hpp"
#include "nlohmann/json.hpp"
#include <thread>
#include <fstream>
#include <string>
#include <filesystem>

using namespace OMShared;

namespace OmniMIDI {
	enum MIDIEventType {
		MasterVolume = 0x01,
		MasterTune = 0x02,
		MasterKey = 0x03,
		MasterPan = 0x04,

		RolandReverbTime = 0x05,
		RolandReverbDelay = 0x06,
		RolandReverbLoCutOff = 0x07,
		RolandReverbHiCutOff = 0x08,
		RolandReverbLevel = 0x09,
		RolandReverbMacro = 0x0A,

		RolandChorusDelay = 0x0B,
		RolandChorusDepth = 0x0C,
		RolandChorusRate = 0x0D,
		RolandChorusFeedback = 0x0E,
		RolandChorusLevel = 0x0F,
		RolandChorusReverb = 0x10,
		RolandChorusMacro = 0x11,

		RolandScaleTuning = 0x12,

		NoteOff = 0x80,
		NoteOn = 0x90,
		Aftertouch = 0xA0,
		CC = 0xB0,
		PatchChange = 0xC0,
		ChannelPressure = 0xD0,
		PitchBend = 0xE0,
		
		SystemMessageStart = 0xF0,
		SystemMessageEnd = 0xF7,

		MIDITCQF = 0xF1,
		SongPositionPointer = 0xF2,
		SongSelect = 0xF3,
		TuneRequest = 0xF6,
		TimingClock = 0xF8,
		Start = 0xFA,
		Continue = 0xFB,
		Stop = 0xFC,
		ActiveSensing = 0xFE,
		SystemReset = 0xFF,

		Unknown1 = 0xF4,
		Unknown2 = 0xF5,
		Unknown3 = 0xF9,
		Unknown4 = 0xFD
	};

	enum RolandMsg {
		Receive = 0x12,
		Send = 0x11,

		AddressMSB = 0xFF0000,
		Address = 0x00FF00,
		AddressLSB = 0x0000FF,
		PartMax = 0x1F,
		ChecksumDividend = 0x80,

		System = 0x000000,

		BlockDiscrim = 0xFF0000,
		BlockTypeDiscrim = 0xF00000,
		BlockIDDiscrim = 0x0F0000,

		PatchPartStart = 0x001000,
		BPort = 0x100000,

		DisplayData = 0x100000,
		DisplayDataBlockDiscrim = AddressLSB,
		ASCIIMode = 0x20,
		BitmapMode = 0x40,

		UserToneBank = 0x200000,
		UserDrumSet = 0x210000,
		UserEFX = 0x220000,
		UserPatchCommon = 0x230000,
		UserPatchPartB01a = 0x240000,
		UserPatchPartB01b = 0x250000,
		UserPatchPartB02a = 0x260000,
		UserPatchPartB02b = 0x270000,

		MIDISetup = 0x00007F,
		MIDIReset = MIDISetup | 0x400000,

		PatchCommonStart = 0x400000,
		PatchCommonA = PatchCommonStart,
		PatchPartA = PatchCommonA | PatchPartStart,
		PatchCommonB = PatchCommonStart | BPort,
		PatchPartB = PatchCommonB | PatchPartStart,

		PatchName = 0x0100,

		ReverbMacro = 0x0130,
		ReverbCharacter = 0x0131,
		ReverbPreLpf = 0x0132,
		ReverbLevel = 0x0133,
		ReverbTime = 0x0134,
		ReverbDelayFeedback = 0x0135,
		ReverbPredelayTime = 0x0137,

		ChorusMacro = 0x0138,
		ChorusPreLpf = 0x0139,
		ChorusLevel = 0x013A,
		ChorusFeedback = 0x013B,
		ChorusDelay = 0x013C,
		ChorusRate = 0x013D,
		ChorusDepth = 0x013E,
		ChorusSendLevelToReverb = 0x013F,
		ChorusSendLevelToDelay = 0x0140,

		DelayMacro = 0x0150,
		DelayPreLpf = 0x0151,
		DelayTimeCenter = 0x0152,
		DelayTimeRatioLeft = 0x0153,
		DelayTimeRatioRight = 0x0154,
		DelayLevelCenter = 0x0155,
		DelayLevelLeft = 0x0156,
		DelayLevelRight = 0x0157,
		DelayLevel = 0x0158,
		DelayFeedback = 0x0159,
		DelaySendLevelToReverb = 0x0160,

		EQLowFreq = 0x0200,
		EQLowGain = 0x0201,
		EQHighFreq = 0x0202,
		EQHighGain = 0x0203
	};

	enum SynthResult {
		Unknown = -1,
		Ok,
		NotInitialized,
		InitializationError,
		LibrariesOffline,
		InvalidParameter,
		InvalidBuffer,
		NotSupported
	};

	class Synthesizers {
	public:
		enum engineID {
			External = -1,
			BASSMIDI,
			FluidSynth,
			XSynth,
			ShakraPipe
		};
	};

	class SoundFont {
#ifdef _WIN32
	private:
		wchar_t* wPtr = nullptr;
#endif

	public:
		std::string path;
		
		bool enabled = true;
		bool xgdrums = false;
		bool linattmod = false;
		bool lindecvol = false;
		bool minfx = true;
		bool nolimits = true;
		bool norampin = false;

		int sbank = -1;
		int spreset = -1;
		int dbank = 0;
		int dbanklsb = 0;
		int dpreset = -1;

		nlohmann::json GetExampleList() {
			auto obj = nlohmann::json::object({});
			obj["SoundFonts"][0] =
			{
					ConfGetVal(path),
					ConfGetVal(enabled),
					ConfGetVal(xgdrums),
					ConfGetVal(linattmod),
					ConfGetVal(lindecvol),
					ConfGetVal(minfx),
					ConfGetVal(nolimits),
					ConfGetVal(norampin),
					ConfGetVal(sbank),
					ConfGetVal(spreset),
					ConfGetVal(dbank),
					ConfGetVal(dpreset),
					ConfGetVal(dbanklsb),
			};

			return obj;
		}
	};

	class SettingsModule {
	protected:
		// When you initialize Settings(), load OM's own settings by default
		ErrorSystem::Logger* ErrLog = nullptr;
		OMShared::Funcs Utils;
		char* SynthName = nullptr;
		bool OwnConsole = false;

		// JSON
		std::fstream* JSONStream = nullptr;
		nlohmann::json jsonptr = nullptr;
		nlohmann::json mainptr = nullptr;
		nlohmann::json synptr = nullptr;

		// Default values
		char Renderer = Synthesizers::BASSMIDI;
		bool KDMAPIEnabled = true;
		bool DebugMode = false;
		std::string CustomRenderer = "empty";

		bool IgnoreVelocityRange = false;
		unsigned char VelocityMin = 0;
		unsigned char VelocityMax = 127;

		bool TransposeNote = false;
		unsigned char TransposeValue = 60;

		char* SettingsPath = nullptr;

	public:
		// Basic settings
		unsigned int SampleRate = 48000;
		unsigned int VoiceLimit = 1024;

		SettingsModule(ErrorSystem::Logger* PErr) {
			JSONStream = new std::fstream;
			ErrLog = PErr;
		}

		virtual ~SettingsModule() {
			CloseConfig();
			delete JSONStream;
			JSONStream = nullptr;
		}

		virtual void LoadSynthConfig() {}
		virtual void RewriteSynthConfig() {}

		virtual const char GetRenderer() { return Renderer; }
		virtual const bool IsKDMAPIEnabled() { return KDMAPIEnabled; }
		virtual const bool IsOwnConsole() { return OwnConsole; }
		virtual const bool IsDebugMode() { return DebugMode; }
		virtual const char* GetCustomRenderer() { return CustomRenderer.c_str(); }

		virtual bool InitConfig(bool write = false, const char* pSynthName = nullptr, size_t pSynthName_sz = 0);
		virtual bool AppendToConfig(nlohmann::json &content);
		virtual bool WriteConfig();
		virtual bool ReloadConfig();
		virtual void CloseConfig();

		virtual void OpenConsole();
		virtual void CloseConsole();

		virtual bool IsConfigOpen() { return JSONStream && JSONStream->is_open(); }
		virtual bool IsSynthConfigValid() { return synptr != nullptr; }
		virtual char* GetConfigPath() { return SettingsPath; }
	};

	class SynthModule {
	protected:
		SettingsModule* Settings = nullptr;
		std::vector<OmniMIDI::SoundFont>* SoundFontsVector = nullptr;
		OMShared::Funcs Utils;
		ErrorSystem::Logger* ErrLog = nullptr;

		std::jthread* _AudThread = nullptr;
		std::jthread _EvtThread;
		std::jthread _SinEvtThread;
		std::jthread _LogThread;

#ifdef _WIN32
		HMODULE m_hModule = NULL;
#endif

		unsigned long long ActiveVoices = 0;
		float RenderingTime = 0.0f;

		BEvBuf* ShortEvents = new BaseEvBuf_t;
		BEvBuf* LongEvents = new BaseEvBuf_t;

	public:
		constexpr unsigned char GetStatus(unsigned int ev) { return (ev & 0xFF); }
		constexpr unsigned char GetCommand(unsigned char status) { return (status & 0xF0); }
		constexpr unsigned char GetChannel(unsigned char status) { return (status & 0xF); }
		constexpr unsigned char GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned char GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

		SynthModule() { ErrLog = nullptr; }
		SynthModule(ErrorSystem::Logger* PErr) { ErrLog = PErr; }
		virtual ~SynthModule() { }
		virtual bool LoadSynthModule() { return true; }
		virtual bool UnloadSynthModule() { return true; }
		virtual bool StartSynthModule() { return true; }
		virtual bool StopSynthModule() { return true; }
		virtual void LoadSoundFonts() { return; }
		virtual bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		virtual unsigned int GetSampleRate() { return 44100; }
		virtual bool IsSynthInitialized() { return true; }
		virtual unsigned int SynthID() { return EMPTYMODULE; }
		virtual unsigned long long GetActiveVoices() { return ActiveVoices; }
		virtual float GetRenderingTime() { return RenderingTime; }

#ifdef _WIN32
		virtual void SetInstance(HMODULE hModule) { m_hModule = hModule; }
#endif

		virtual void LogFunc() {
			const char Templ[] = "R%06.2f%% >> P%llu (Ev%08zu/%08zu)";
			char* Buf = new char[96];

			while (!IsSynthInitialized())
				Utils.MicroSleep(SLEEPVAL(1));

			while (IsSynthInitialized()) {
				sprintf(Buf, Templ, GetRenderingTime(), GetActiveVoices(), ShortEvents->GetReadHeadPos(), ShortEvents->GetWriteHeadPos());
				SetTerminalTitle(Buf);
				
				Utils.MicroSleep(SLEEPVAL(1));
			}

			sprintf(Buf, Templ, 0.0f, (unsigned long long)0, (size_t)0, (size_t)0);
			SetTerminalTitle(Buf);

			delete[] Buf;
		}

		virtual void FreeEvBuf(BEvBuf* target) {
			if (target) {
				auto tEvents = new BEvBuf;
				auto oEvents = target;

				target = tEvents;

				delete oEvents;
			}
		}

		virtual BEvBuf* AllocateShortEvBuf(size_t size) {
			if (ShortEvents) {
				auto tEvents = new EvBuf(size);
				auto oEvents = ShortEvents;

				ShortEvents = tEvents;

				delete oEvents;
			}

			// Double check
			return ShortEvents;
		}

		virtual BEvBuf* AllocateLongEvBuf(size_t size) {
			if (LongEvents) {
				auto tEvents = new LEvBuf(size);
				auto oEvents = LongEvents;

				LongEvents = tEvents;

				delete oEvents;
			}

			// Double check
			return LongEvents;
		}

		virtual void FreeShortEvBuf() { FreeEvBuf(ShortEvents); }
		virtual void FreeLongEvBuf() { FreeEvBuf(LongEvents); }

		// Event handling system
		virtual void PlayShortEvent(unsigned int ev) {
			if (!ShortEvents)
				return;

			UPlayShortEvent(ev);
		}
		virtual void PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) {
			if (!ShortEvents)
				return;

			UPlayShortEvent(status, param1, param2);
		}
		virtual void UPlayShortEvent(unsigned int ev) { ShortEvents->Write(ev); }
		virtual void UPlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) { ShortEvents->Write(status, param1, param2); }

		virtual unsigned int PlayLongEvent(char* ev, unsigned int size) { return 0; }
		virtual unsigned int UPlayLongEvent(char* ev, unsigned int size) { return 0; }

		virtual SynthResult Reset(char type = 0) { PlayShortEvent(0xFF, type, 0); return Ok; }

		virtual SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};

	class SoundFontSystem {
	private:
		ErrorSystem::Logger* ErrLog = nullptr;
		OMShared::Funcs Utils;
		char* ListPath = nullptr;
		std::filesystem::file_time_type ListLastEdit;
		std::vector<SoundFont> SoundFonts;
		OmniMIDI::SynthModule* SynthModule = nullptr;
		std::jthread _SFThread;
		bool StayAwake = false;
		void SoundFontThread();

	public:
		SoundFontSystem() { ErrLog = nullptr; }
		SoundFontSystem(ErrorSystem::Logger* PErr) { ErrLog = PErr; }
		bool RegisterCallback(OmniMIDI::SynthModule* ptr = nullptr);
		std::vector<SoundFont>* LoadList(std::string list = "");
		bool ClearList();
	};
}

#endif