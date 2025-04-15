/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#include "Common.hpp"

#include <cstring>
#include <fstream>
#include <cassert>
#include "ErrSys.hpp"
#include <format>

#ifdef _WIN32
#include <windows.h>
#include <guiddef.h>
#include <strsafe.h>
#include <cassert>

#include <shlobj.h>

#define loadLib				LoadLibraryA
#define getError			GetLastError
#define freeLib(x)			FreeLibrary((HMODULE)x)
#define getLib				GetModuleHandleA
#define getAddr(x, y)		GetProcAddress((HMODULE)x, y)
#define TYPE				%s
#define TYPENO				TYPE
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/time.h>

#define loadLib(l)			dlopen(l, RTLD_NOW)
#define getError()			0
#define freeLib				dlclose
#define getLib()			0
#define getLibErr()			dlerror
#define getAddr				dlsym
#define TYPE				lib%s.so
#define TYPENO				lib%s
#endif

#define STREXP(X)			#X
#define LIBEXP(X)			STREXP(X)

namespace OMShared {
	enum FIDs {
		CurrentDirectory,
		UserFolder,
		PluginFolder,
		LibGeneric,
		LibLocal,
		Libi386,
		LibAMD64,
		LibAArch64
	};

	struct LibVersion {
		unsigned char Build = 0;
		unsigned char Major = 0;
		unsigned char Minor = 0;
		unsigned char Rev = 0;

		LibVersion(unsigned int raw) {
			Build = (raw >> 24) & 0xFF;
			Major = (raw >> 16) & 0xFF;
			Minor = (raw >> 8) & 0xFF;
			Rev = raw & 0xFF;
		}

		unsigned int GetHiWord() { return (Build << 8 | Major) & 0xFFFF; }
		unsigned int GetLoWord() { return (Minor << 8 | Rev) & 0xFFFF; }
		unsigned int GetRaw() { return GetHiWord() << 16 | GetLoWord(); }
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

			if (lib == nullptr)
				return false;

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
		const char* Name;
		const char* Suffix;
		void* Library = nullptr;
		bool Initialized = false;
		bool LoadFailed = false;
		bool AppSelfHosted = false;
		ErrorSystem::Logger* ErrLog;

		LibImport* Funcs = nullptr;
		size_t FuncsCount = 0;

		bool IteratePath(char* outPath, OMShared::FIDs fid);

	public:
		void* Ptr() { return Library; }
		bool IsOnline() { return (Library != nullptr && Initialized && !LoadFailed); }

		Lib(const char* pName, const char* Suffix, ErrorSystem::Logger* PErr, LibImport** pFuncs = nullptr, size_t pFuncsCount = 0);
		~Lib();

		bool GetLibPath(char* outPath = nullptr);
		bool LoadLib(char* CustomPath = nullptr);
		bool UnloadLib();
		bool IsSupported(unsigned int loaded, unsigned int minimum);
	};

	class Funcs {
#ifdef _WIN32
	private:
		void* ntdll = nullptr;
		bool LL = false;
		unsigned int (WINAPI* pNtDelayExecution)(unsigned char, signed long long*) = nullptr;
		unsigned int (WINAPI* pNtQuerySystemTime)(signed long long*) = nullptr;
		ErrorSystem::Logger* ErrLog;
#endif

	public:
		Funcs();
		Funcs(ErrorSystem::Logger* PErr);
		~Funcs();

		void MicroSleep(signed long long v);
		unsigned int QuerySystemTime(signed long long* v);

#ifdef _WIN32
		wchar_t* GetUTF16(char* utf8);
#endif

		bool GetFolderPath(const FIDs fID, char* folderPath, size_t szFolderPath);
		bool DoesFileExist(std::string filePath);
	};
}

#endif