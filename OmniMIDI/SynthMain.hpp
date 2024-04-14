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
#define SYNTH_OK			0x00
#define SYNTH_NOTINIT		0x01
#define SYNTH_INITERR		0x02
#define SYNTH_LIBOFFLINE	0x03
#define SYNTH_INVALPARAM	0x04
#define SYNTH_INVALBUF		0x05

// Renderers
#define EXTERNAL			-1
#define BASSMIDI			0
#define FLUIDSYNTH			1
#define XSYNTH				2
#define TINYSF				3
#define KSYNTH				4

#define fv2fn(f)			(#f)
#define ImpFunc(f)			LibImport((void**)&f, #f)

#define JSONGetVal(f)		{ #f, f }
#define JSONSetVal(t, f)	f = JsonData[#f].is_null() ? f : JsonData[#f].get<t>()

#define SettingsManagerCase(choice, get, type, setting, var, size) \
	case choice: \
		if (size != sizeof(type)) return false; \
		if (get) *(type*)var = setting; \
		else setting = *(type*)var; \
		break;

#define EMPTYMODULE			0xDEADBEEF

#include "ErrSys.hpp"
#include "Utils.hpp"
#include "EvBuf_t.hpp"
#include "KDMAPI.hpp"
#include "nlohmann\json.hpp"
#include <thread>

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

		bool SetPtr(void* lib = nullptr, const char* ptrname = nullptr){
			void* ptr = (void*)-1;

			if (lib == nullptr && ptrname == nullptr)
			{
				if (funcptr)
					*(funcptr) = nullptr;

				return true;
			}

			ptr = (void*)getAddr(lib, ptrname);

			if (!ptr)
				return false;

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
		ErrorSystem::Logger SetErr;

	public:
		virtual ~OMSettings() {}
	};

	class SynthModule {
	protected:
		OMShared::Funcs MiscFuncs;
		ErrorSystem::Logger SynErr;

		std::jthread _AudThread;
		std::jthread _EvtThread;
		std::jthread _LogThread;

		unsigned char LRS = 0;
		EvBuf* Events;

	public:
		constexpr unsigned int ApplyRunningStatus(unsigned int ev) {
			unsigned int tev = GetStatus(ev);

			if ((tev & 0x80) != 0) LRS = tev;
			else {
				unsigned int pev = ev;
				pev = ev << 8 | LRS;
				return pev;
			}

			return ev;
		}
		constexpr unsigned int GetStatus(unsigned int ev) { return (ev & 0xFF); }
		constexpr unsigned int GetCommand(unsigned int ev) { return (ev & 0xF0); }
		constexpr unsigned int GetChannel(unsigned int ev) { return (ev & 0xF); }
		constexpr unsigned int GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned int GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

		virtual ~SynthModule() {}
		virtual bool LoadSynthModule() { return true; }
		virtual bool UnloadSynthModule() { return true; }
		virtual bool StartSynthModule() { return true; }
		virtual bool StopSynthModule() { return true; }
		virtual bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		virtual unsigned int GetSampleRate() { return 44100; }
		virtual bool IsSynthInitialized() { return true; }
		virtual int SynthID() { return EMPTYMODULE; }

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