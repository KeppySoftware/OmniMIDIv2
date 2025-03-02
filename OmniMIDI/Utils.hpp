/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <guiddef.h>
#include <strsafe.h>
#include <cassert>

#include <ShlObj.h>

#define loadLib				LoadLibraryA
#define getError			GetLastError
#define freeLib(x)			FreeLibrary((HMODULE)x)
#define getLib				GetModuleHandleA
#define getAddr(x, y)		GetProcAddress((HMODULE)x, y)
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
#endif

namespace OMShared {
	enum FIDs {
		CurrentDirectory,
		System,
		UserFolder
	};

	class Funcs {
#ifdef _WIN32
	private:
		void* ntdll = nullptr;
		bool LL = false;
		unsigned int (WINAPI* pNtDelayExecution)(unsigned char, signed long long*) = nullptr;
		unsigned int (WINAPI* pNtQuerySystemTime)(signed long long*) = nullptr;
#endif

	public:
		Funcs();
		~Funcs();

		unsigned int MicroSleep(signed long long v);
		unsigned int QuerySystemTime(signed long long* v);

#ifdef _WIN32
		wchar_t* GetUTF16(char* utf8);
#endif

		bool GetFolderPath(const FIDs fID, char* folderPath, size_t szFolderPath);
		bool DoesFileExist(std::string filePath);
	};
}

#endif