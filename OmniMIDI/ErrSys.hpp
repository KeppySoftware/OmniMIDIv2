/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _ERRSYS_H
#define _ERRSYS_H

#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <tchar.h>
#include <Windows.h>
#endif

#ifdef _WIN32
#define MsgBox					MessageBoxA
#else
#define MsgBox
#define MB_ICONERROR			0
#define MB_SYSTEMMODAL			0
#define MB_OK					0
#endif

#define S2(x)					#x
#define S1(x)					S2(x)

#define NERROR(x, y, z, ...)	x.ThrowError(y, z, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define FNERROR(x, y)			x.ThrowFatalError(y, __FILE__, __func__, __LINE__)

#define LOGI(x, y, ...)			x.Log(y, __FILE__, __func__, __LINE__, __VA_ARGS__)
#if _DEBUG
#define LOG(x, y, ...)			x.Log(y, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LOGV(x, y, ...)			x.Log(S1(y), __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define LOG(x, y, ...)
#define LOGV(x, y, ...)
#endif

namespace ErrorSystem {
	class Logger {
	private:
		static const int BufSize = 2048;
		static const int SZBufSize = sizeof(char) * BufSize;

	public:
		void Log(const char* Error, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowFatalError(const char* Error, const char* File, const char* Func, const unsigned long Line);
	};
}

#endif