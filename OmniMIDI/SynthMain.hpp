/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _SYNTHMAIN_H
#define _SYNTHMAIN_H

#pragma once

// Uncomment this if you want stats to be shown once the synth is closed
// #define _STATSDEV

// ERRORS
#define SYNTH_OK				0x00
#define SYNTH_NOTINIT			0x01
#define SYNTH_INITERR			0x02
#define SYNTH_LIBOFFLINE		0x03
#define SYNTH_INVALPARAM		0x04
#define SYNTH_INVALBUF			0x05

#define fv2fn(f)				(#f)
#define ImpFunc(f)				LibImport((void**)&f, #f)

#define ConfGetVal(f)			{ #f, f }
#define JSONSetVal(t, f)		f = JsonData[#f].is_null() ? f : JsonData[#f].get<t>()
#define MainSetVal(t, f)		f = mainptr[#f].is_null() ? f : mainptr[#f].get<t>()
#define SynthSetVal(t, f)		f = synptr[#f].is_null() ? f : synptr[#f].get<t>()

#define SettingsManagerCase(choice, get, type, setting, var, size) \
	case choice: \
		if (size != sizeof(type)) return false; \
		if (get) *(type*)var = setting; \
		else setting = *(type*)var; \
		break;

#define DUMMY_STR			"dummy"
#define EMPTYMODULE			0xDEADBEEF

#include "ErrSys.hpp"
#include "Utils.hpp"
#include "EvBuf_t.hpp"
#include "KDMAPI.hpp"
#include "nlohmann\json.hpp"
#include <thread>
#include <fstream>

namespace OmniMIDI {
	enum MIDIEventType {
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
		enum n {
			External = -1,
			BASSMIDI,
			FluidSynth,
			XSynth,
			TinySoundFont,
			ksynth,
			ShakraPipe
		};
	};

	class LibImport
	{
	protected:
		void** funcptr = nullptr;
		const char* funcname = nullptr;

	public:
		LibImport(void** pptr, const char* pfuncname) {
			funcptr = pptr;
			*(funcptr) = nullptr;
			funcname = pfuncname;
		}

		~LibImport() {
			*(funcptr) = nullptr;
		}

		void* GetPtr() { return *(funcptr); }
		const char* GetName() { return funcname; }
		bool LoadFailed() { return funcptr == nullptr || (funcptr != nullptr && *(funcptr) == nullptr); }

		bool SetPtr(void* lib = nullptr, const char* ptrname = nullptr) {
			void* ptr = nullptr;

			if (lib == nullptr && ptrname == nullptr)
			{
				if (funcptr)
					*(funcptr) = nullptr;

				return true;
			}

			ptr = (void*)getAddr(lib, ptrname);

			if (!ptr) {
				return false;
			}

			if (ptr != *(funcptr))
				*(funcptr) = ptr;

			return true;
		}
	};

	class Lib {
	protected:
		const wchar_t* Name;
		void* Library = nullptr;
		bool Initialized = false;
		bool LoadFailed = false;
		bool AppSelfHosted = false;
		ErrorSystem::Logger LibErr;

		LibImport* Funcs = nullptr;
		size_t FuncsCount = 0;

	public:
		void* Ptr() { return Library; }
		bool IsOnline() { return (Library != nullptr && Initialized && !LoadFailed); }

		Lib(const wchar_t* pName, LibImport** pFuncs, size_t pFuncsCount) {
			Name = pName;
			Funcs = *pFuncs;
			FuncsCount = pFuncsCount;
		}

		~Lib() {
			UnloadLib();
		}

		bool LoadLib(wchar_t* CustomPath = nullptr) {
			OMShared::SysPath Utils;

			char CName[MAX_PATH] = { 0 };
			wchar_t SysDir[MAX_PATH] = { 0 };
			wchar_t DLLPath[MAX_PATH] = { 0 };
			int swp = 0;

			if (Library == nullptr) {
#ifdef _WIN32
				if ((Library = getLibW(Name)) != nullptr)
				{
					// (TODO) Make it so we can load our own version of it
					// For now, just make the driver try and use that instead
					return (AppSelfHosted = true);
				}
				else {
#endif
					if (CustomPath != nullptr) {
						swp = swprintf(DLLPath, MAX_PATH, L"%s\\%s.dll\0", CustomPath, Name);
						assert(swp != -1);

						if (swp != -1) {
							wcstombs(CName, DLLPath, MAX_PATH);
							LOG(LibErr, "Will it work? %s", CName);
							Library = loadLibW(DLLPath);

							if (!Library)
								return false;
						}
						else return false;
					}
					else {
						swp = swprintf(DLLPath, MAX_PATH, L"%s.dll\0", Name);
						assert(swp != -1);

						if (swp != -1) {
							Library = loadLibW(DLLPath);

							if (!Library)
							{
								if (Utils.GetFolderPath(OMShared::FIDs::System, SysDir, sizeof(SysDir))) {
									swp = swprintf(DLLPath, MAX_PATH, L"%s\\OmniMIDI\\%s.dll\0", SysDir, Name);
									assert(swp != -1);
									if (swp != -1) {
										Library = loadLibW(DLLPath);
										assert(Library != 0);

										if (!Library) {
											wcstombs(CName, Name, MAX_PATH);
											NERROR(LibErr, "The required library \"%s\" could not be loaded or found. This is required for the synthesizer to work.", true, CName);
											return false;
										}

									}
									else return false;
								}
								else return false;						
							}
						}
						else return false;
						
					}
#ifdef _WIN32
				}
#endif
			}

			for (int i = 0; i < FuncsCount; i++)
			{
				if (Funcs[i].SetPtr(Library, Funcs[i].GetName()))
					LOG(LibErr, "%s --> 0x%08x", Funcs[i].GetName(), Funcs[i].GetPtr());
			}

			Initialized = true;
			return true;
		}

		bool UnloadLib() {
			if (Library != nullptr) {
				if (AppSelfHosted)
				{
					AppSelfHosted = false;
				}
				else {
					bool r = freeLib(Library);
					assert(r == true);
					if (!r) {
						throw;
					}
				}

				Library = nullptr;
			}

			LoadFailed = false;
			Initialized = false;
			return true;
		}
	};

	class OMSettings {
	protected:
		// When you initialize Settings(), load OM's own settings by default
		ErrorSystem::Logger SetErr;
		OMShared::SysPath Utils;
		std::fstream* JSONStream = nullptr;
		char* SynthName = nullptr;
		nlohmann::json mainptr = nullptr;
		nlohmann::json synptr = nullptr;
		bool OwnConsole = false;

		// Default values
		char Renderer = Synthesizers::BASSMIDI;
		bool KDMAPIEnabled = true;
		bool DebugMode = false;
		std::string CustomRenderer = "empty";

		wchar_t* SettingsPath = nullptr;

	private:
		nlohmann::json jsonptr = nullptr;

	public:
		// Basic settings
		unsigned int SampleRate = 48000;
		unsigned int VoiceLimit = 1024;

		virtual ~OMSettings() {
			CloseConfig();
		}

		virtual void LoadSynthConfig() {}
		virtual void RewriteSynthConfig() {}

		virtual char GetRenderer() { return Renderer; }
		virtual bool IsKDMAPIEnabled() { return KDMAPIEnabled; }
		virtual bool IsOwnConsole() { return OwnConsole; }
		virtual bool IsDebugMode() { return DebugMode; }
		virtual const char* GetCustomRenderer() { return CustomRenderer.c_str(); }

		bool InitConfig(bool write = false, const char* pSynthName = nullptr, size_t pSynthName_sz = 64) {
			if (JSONStream && JSONStream->is_open())
				return true;

			if (SynthName == nullptr && pSynthName != nullptr) {
				if (strcmp(pSynthName, DUMMY_STR)) {
					if (pSynthName_sz > 64)
						pSynthName_sz = 64;

					SynthName = new char[pSynthName_sz];
					if (strcpy(SynthName, pSynthName) != SynthName) {
						NERROR(SetErr, "An error has occurred while parsing SynthName!", true);
						CloseConfig();
						return false;
					}
				}
			}
			else {
				NERROR(SetErr, "InitConfig called with no SynthName specified!", true);
				CloseConfig();
				return false;
			}

			SettingsPath = new wchar_t[MAX_PATH];
			if (Utils.GetFolderPath(OMShared::FIDs::UserFolder, SettingsPath, sizeof(SettingsPath))) {
				swprintf(SettingsPath, L"%s\\OmniMIDI\\settings.json\0", SettingsPath);

				if (!JSONStream) JSONStream = new std::fstream;
				JSONStream->open(SettingsPath, write ? (std::fstream::out | std::fstream::trunc) : std::fstream::in);

				if (!JSONStream->is_open()) {
					NERROR(SetErr, "An error has occurred while opening the config file!", true);
					CloseConfig();
					return false;
				}

				jsonptr = nlohmann::json::parse(*JSONStream, nullptr, false, true);
				
				mainptr = jsonptr["OmniMIDI"];
				if (mainptr == nullptr) {
					NERROR(SetErr, "An error has occurred while parsing the settings for OmniMIDI!", true);
					CloseConfig();
					return false;
				}

				MainSetVal(int, Renderer);
				MainSetVal(bool, DebugMode);
				MainSetVal(bool, KDMAPIEnabled);
				MainSetVal(std::string, CustomRenderer);

				if (SynthName) {
					synptr = mainptr["SynthModules"][SynthName];
					if (synptr == nullptr) {
						NERROR(SetErr, "An error has occurred while parsing the settings for the chosen synth module!", true, SynthName);
						CloseConfig();
						return false;
					}
				}

				return true;
			}
			else NERROR(SetErr, "An error has occurred while parsing the user profile path!", true);

			return false;
		}

		bool AppendToConfig(nlohmann::json content) {
			nlohmann::json tmp = nullptr;

			if (content == nullptr)
				return false;

			tmp = jsonptr["OmniMIDI"]["SynthModules"][SynthName];
			if (tmp != nullptr) {
				jsonptr["OmniMIDI"]["SynthModules"].erase(SynthName);
			}

			tmp.clear();
			tmp = {
				{ SynthName, {
					content
				}}
			};

			jsonptr["OmniMIDI"]["SynthModules"].push_back(tmp);
			mainptr = jsonptr["OmniMIDI"];
			synptr = mainptr["SynthModules"][SynthName];
			return true;
		}

		bool WriteConfig() {
			if (!JSONStream || !JSONStream->is_open())
				return false;

			std::string dump = mainptr.dump(1);
			JSONStream->write(dump.c_str(), dump.length());
			JSONStream->close();

			return true;
		}

		bool ReloadConfig() {
			JSONStream->close();
			return InitConfig();
		}

		bool IsConfigOpen() { return JSONStream && JSONStream->is_open(); }

		bool IsSynthConfigValid() { return synptr != nullptr; }

		wchar_t* GetConfigPath() { return SettingsPath; }

		void CloseConfig() {
			if (!jsonptr.is_null()) {
				jsonptr.clear();
			}

			if (!mainptr.is_null()) {
				mainptr.clear();
			}

			if (!synptr.is_null()) {
				synptr.clear();
			}

			if (JSONStream) {
				if (JSONStream->is_open())
					JSONStream->close();

				delete JSONStream;
				JSONStream = nullptr;
			}

			if (SettingsPath) {
				delete SettingsPath;
				SettingsPath = nullptr;
			}

			if (SynthName) {
				delete SynthName;
				SynthName = nullptr;
			}
		}

		virtual void OpenConsole() {
			IsDebugMode();

#if defined _WIN32 && !defined _DEBUG
			if (IsDebugMode()) {
				if (AllocConsole()) {
					OwnConsole = true;
					FILE* dummy;
					freopen_s(&dummy, "CONOUT$", "w", stdout);
					freopen_s(&dummy, "CONOUT$", "w", stderr);
					freopen_s(&dummy, "CONIN$", "r", stdin);
					std::cout.clear();
					std::clog.clear();
					std::cerr.clear();
					std::cin.clear();
				}
			}
#endif
		}

		virtual void CloseConsole() {
#if defined _WIN32 && !defined _DEBUG
			if (IsDebugMode() && OwnConsole) {
				FreeConsole();
				OwnConsole = false;
			}
#endif
		}

	};

	class SynthModule {
	protected:
		OMSettings* Settings;
		OMShared::Funcs MiscFuncs;
		ErrorSystem::Logger SynErr;

		std::jthread _AudThread[16];
		std::jthread _EvtThread;
		std::jthread _LogThread;

#ifdef _WIN32
		HMODULE m_hModule;
#endif

		unsigned int ActiveVoices = 0;
		float RenderingTime = 0.0f;

		BEvBuf* ShortEvents = new BaseEvBuf_t;
		BEvBuf* LongEvents = new BaseEvBuf_t;

	public:
		constexpr unsigned int GetStatus(unsigned int ev) { return (ev & 0xFF); }
		constexpr unsigned int GetCommand(unsigned int ev) { return (ev & 0xF0); }
		constexpr unsigned int GetChannel(unsigned int ev) { return (ev & 0xF); }
		constexpr unsigned int GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned int GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

		constexpr unsigned int GetStatusChar(unsigned char ev) { return (ev & 0xFF); }
		constexpr unsigned int GetCommandChar(unsigned char ev) { return (ev & 0xF0); }
		constexpr unsigned int GetChannelChar(unsigned char ev) { return (ev & 0xF); }

		virtual ~SynthModule() { }
		virtual bool LoadSynthModule() { return true; }
		virtual bool UnloadSynthModule() { return true; }
		virtual bool StartSynthModule() { return true; }
		virtual bool StopSynthModule() { return true; }
		virtual bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		virtual unsigned int GetSampleRate() { return 44100; }
		virtual bool IsSynthInitialized() { return true; }
		virtual int SynthID() { return EMPTYMODULE; }
		virtual unsigned int GetActiveVoices() { return ActiveVoices; }
		virtual float GetRenderingTime() { return RenderingTime; }

#ifdef _WIN32
		virtual void SetInstance(HMODULE hModule) { m_hModule = hModule; }
#endif

		virtual void LogFunc() {
			const char Templ[] = "RT > %06.2f%% - POLY: %d (Ev RH%08zu WH%08zu)";
			char* Buf = new char[96];

			while (!IsSynthInitialized())
				MiscFuncs.uSleep(-1);

			while (IsSynthInitialized()) {
				sprintf(Buf, Templ, GetRenderingTime(), GetActiveVoices(), ShortEvents->GetReadHeadPos(), ShortEvents->GetWriteHeadPos());

#ifdef _WIN32
				SetConsoleTitleA(Buf);
#else
				std::cout << "\033]0;" << Buf << "\007";
#endif

				MiscFuncs.uSleep(-1);
			}

			sprintf(Buf, Templ, 0.0f, 0, (size_t)0, (size_t)0);
			SetConsoleTitleA(Buf);
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
		virtual void PlayShortEvent(unsigned int ev) { return; }
		virtual void UPlayShortEvent(unsigned int ev) { return; }

		virtual SynthResult PlayLongEvent(char* ev, unsigned int size) { return Ok; }
		virtual SynthResult UPlayLongEvent(char* ev, unsigned int size) { return Ok; }

		virtual SynthResult Reset() { PlayShortEvent(0x0101FF); return Ok; }

		virtual SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Ok; }
	};
}

#endif