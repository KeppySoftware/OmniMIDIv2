/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#ifdef _WIN32
#include <Windows.h>
#include <guiddef.h>
#include <ShlObj_core.h>
#include <strsafe.h>
#include <cassert>

#define loadLib				LoadLibraryA
#define loadLibW			LoadLibraryW
#define freeLib(x)			FreeLibrary((HMODULE)x)
#define getLib				GetModuleHandleA
#define getLibW				GetModuleHandleW
#define getAddr(x, y)		GetProcAddress((HMODULE)x, y)
#else
#include <dlfcn.h>
#include <unistd.h>

#define loadLib				dlopen
#define loadLibW			wdlopen
#define freeLib				dlclose
#define getLib				// Unavailable
#define getLibW				getLib
#define getAddr				dlsym
#endif

namespace OMShared {
	enum FIDs {
		System,
		UserFolder	
	};

	class SysPath {
	public:
		bool GetFolderPath(const FIDs FolderID, wchar_t* String, size_t StringLen);
	};

	class Funcs {
	private:
#ifdef _WIN32
		void* ntdll = nullptr;
		bool LL = false;
		unsigned int (WINAPI* pNtDelayExecution)(unsigned char, signed long long*) = nullptr;
		unsigned int (WINAPI* pNtQuerySystemTime)(signed long long*) = nullptr;
#endif

	public:
		Funcs() {
#ifdef _WIN32
			// There is no equivalent to this in Linux/macOS
			ntdll = getLib("ntdll");

			if (!ntdll) {
				LL = true;
				ntdll = loadLib("ntdll");
			}

			assert(ntdll != 0);
			if (!ntdll)
				return;

			auto v1 = (unsigned int (WINAPI*)(unsigned char, signed long long*))getAddr(ntdll, "NtDelayExecution");
			assert(v1 != 0);

			if (v1 == nullptr)
				return;

			pNtDelayExecution = v1;

			auto v2 = (unsigned int (WINAPI*)(signed long long*))getAddr(ntdll, "NtQuerySystemTime");
			assert(v2 != 0);

			if (v2 == nullptr)
				return;

			pNtQuerySystemTime = v2;
#endif
		}

		~Funcs() {
#ifdef _WIN32
			if (LL) {
				if (!freeLib(ntdll))
					exit(GetLastError());

				ntdll = nullptr;
			}
#endif
		}

		unsigned int uSleep(signed long long v) {
#ifdef _WIN32
			return pNtDelayExecution(0, &v);
#else
			return usleep((useconds_t)v);
#endif
		}

		unsigned int querySystemTime(signed long long* v) {
#ifdef _WIN32
			return pNtQuerySystemTime(v);
#else
			// todo
#endif
		}
	};
}

#endif