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

//
#define MIDI_NOTEOFF		0x80
#define MIDI_NOTEON			0x90
#define MIDI_POLYAFTER		0xA0
#define MIDI_CMC			0xB0
#define MIDI_PROGCHAN		0xC0
#define MIDI_CHANAFTER		0xD0
#define MIDI_PITCHWHEEL		0xE0

// TBD
#define MIDI_SYSEXBEG		0xF0
#define MIDI_SYSEXEND		0xF7
#define MIDI_COOKPLPLAY		0xFA
#define MIDI_COOKPLCONT		0xFB
#define MIDI_COOKPLSTOP		0xFC
#define MIDI_SENSING		0xFE
#define MIDI_SYSRESET		0xFF

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

#define fv2fn(f)			(#f)
#define ImpFunc(f)			LibImport((void**)&##f, #f)

#define JSONGetVal(f)		{ #f, f }
#define JSONSetVal(t, f)	f = JsonData[#f].is_null() ? f : JsonData[#f].get<t>()

#define SettingsManagerCase(choice, get, type, setting, var, size) \
	case choice: \
		if (size != sizeof(type)) return false; \
		if (get) *(type*)var = setting; \
		else setting = *(type*)var; \
		break;

#define LoadPtr(lib, func) \
		if (lib->IsOnline()) \
			if (func.SetLib(lib->Ptr())) \
				if (func.SetPtr(func.GetName())) \
					continue;

#define ClearPtr(func) \
		if (func.SetLib()) \
			if (func.SetPtr()) \
				continue;

#define EMPTYMODULE			0xDEADBEEF

#include "ErrSys.hpp"
#include "Utils.hpp"
#include "KDMAPI.hpp"
#include <nlohmann\json.hpp>

namespace OmniMIDI {
	enum SynthResult {
		Unknown = -1,
		Ok,
		NotInitialized,
		InitializationError,
		LibrariesOffline,
		InvalidParameter,
		InvalidBuffer
	};

	class LibImport
	{
	private:
		void* lib = nullptr;
		void** funcptr = nullptr;
		const char* funcname = nullptr;

	public:
		LibImport(void** pptr, const char* pfuncname) {
			funcptr = pptr;
			funcname = pfuncname;
		}

		~LibImport() {
			*(funcptr) = nullptr;
		}

		void SetName(const char* pfuncname) { funcname = pfuncname; }
		const char* GetName() { return funcname; }

		bool SetLib(void* tlib = nullptr) {
			if (lib != nullptr && tlib != nullptr && lib != tlib)
				return false;

			if (tlib != nullptr)
			{
				lib = tlib;
				return true;
			}

			lib = nullptr;
			return true;
		}

		bool SetPtr(const char* ptrname = nullptr){
			void* ptr = (void*)-1;

			if (lib == nullptr || ptrname == nullptr)
			{
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

		void* GetPtr() { return *(funcptr); }
	};

	class Lib {
	private:
		const wchar_t* Name;
		void* Library = nullptr;
		bool Initialized = false;
		bool LoadFailed = false;
		bool AppSelfHosted = false;
		ErrorSystem::Logger LibErr;

	public:
		void* Ptr() { return Library; }
		bool IsOnline() { return (Library != nullptr && Initialized && !LoadFailed); }

		Lib(const wchar_t* pName) {
			Name = pName;
		}

		bool LoadLib(wchar_t* CustomPath = nullptr) {
			OMShared::SysPath Utils;

			char CName[MAX_PATH] = { 0 };
			wchar_t SysDir[MAX_PATH] = { 0 };
			wchar_t DLLPath[MAX_PATH] = { 0 };
			int swp = 0;

			if (Library == nullptr) {
				if ((Library = GetModuleHandle(Name)) != nullptr)
				{
					// (TODO) Make it so we can load our own version of it
					// For now, just make the driver try and use that instead
					return (AppSelfHosted = true);
				}
				else {
					if (CustomPath != nullptr) {
						swp = swprintf_s(DLLPath, MAX_PATH, L"%s\\%s.dll\0", CustomPath, Name);
						assert(swp != -1);

						if (swp != -1) {
							Library = loadLibW(DLLPath);

							if (!Library)
								return false;
						}
						else return false;
					}
					else {
						if (Utils.GetFolderPath(OMShared::FIDs::System, SysDir, sizeof(SysDir))) {
							swp = swprintf_s(DLLPath, MAX_PATH, L"%s.dll\0", Name);
							assert(swp != -1);

							if (swp != -1) {
								Library = loadLibW(DLLPath);

								if (!Library)
								{
									swp = swprintf_s(DLLPath, MAX_PATH, L"%s\\OmniMIDI\\%s.dll\0", SysDir, Name);
									assert(swp != -1);
									if (swp != -1) {
										Library = loadLibW(DLLPath);
										assert(Library != 0);

										if (!Library) {
											wcstombs_s(nullptr, CName, Name, MAX_PATH);
											NERROR(LibErr, "The required library \"%s\" could not be loaded or found. This is required for the synthesizer to work.", true, CName);
											return false;
										}
											
									}
									else return false;
								}
							}
							else return false;
						}
						else return false;
					}
				}
			}

			Initialized = true;
			return true;
		}

		bool UnloadLib() {
			if (Library != nullptr) {
				Initialized = false;
				LoadFailed = false;

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

			return true;
		}
	};

	class SynthSettings {
	public:
		SynthSettings() {}
	};

	class SynthModule {
	public:
		constexpr unsigned int CheckRunningStatus(unsigned int ev) { return (ev & 0x80); }
		constexpr unsigned int GetStatus(unsigned int ev) { return (ev & 0xFF); }
		constexpr unsigned int GetCommand(unsigned int ev) { return (ev & 0xF0); }
		constexpr unsigned int GetChannel(unsigned int ev) { return (ev & 0xF); }
		constexpr unsigned int GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned int GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

		virtual ~SynthModule() {}
		virtual bool LoadSynthModule() { return true; }
		virtual bool UnloadSynthModule() { return true; }
		virtual bool StartSynthModule() { return false; }
		virtual bool StopSynthModule() { return false; }
		virtual bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return true; }
		virtual unsigned int GetSampleRate() { return 0; }
		virtual bool IsSynthInitialized() { return false; }
		virtual int SynthID() { return EMPTYMODULE; }

		// Event handling system
		virtual SynthResult PlayShortEvent(unsigned int ev) { return NotInitialized; }
		virtual SynthResult UPlayShortEvent(unsigned int ev) { return NotInitialized; }

		virtual SynthResult PlayLongEvent(char* ev, unsigned int size) { return NotInitialized; }
		virtual SynthResult UPlayLongEvent(char* ev, unsigned int size) { return NotInitialized; }

		virtual SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return NotInitialized; }
	};
}

#endif