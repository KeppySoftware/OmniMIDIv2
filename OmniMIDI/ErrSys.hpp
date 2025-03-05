/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _ERRSYS_H
#define _ERRSYS_H

#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef _MSC_VER
#define __func__ __FUNCTION__
#endif

#ifdef _WIN32
#include <tchar.h>
#include <Windows.h>
#include <Psapi.h>
#else
#define VERBOSE_LOG
#include <stdarg.h>
#endif

#ifdef _WIN32
#define MsgBox						MessageBoxA
#else
#define MsgBox(...)					0
#define MB_ICONWARNING				0
#define MB_ICONERROR				0
#define MB_SYSTEMMODAL				0
#define MB_OK						0
#endif

#define S2(x)						#x
#define S1(x)						S2(x)

#define NERROR(text, fatal, ...)	ErrLog->ThrowError(text, fatal, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define NERRORV(text, fatal, ...)	ErrLog->ThrowError(S1(text), fatal, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define FNERROR(text)				ErrLog->ThrowFatalError(text, __FILE__, __func__, __LINE__)

#define LOGI(text, ...)				ErrLog->Log(text, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#if defined(_DEBUG) || defined(VERBOSE_LOG)
#define LOG(text, ...)				ErrLog->Log(text, __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define LOGV(text, ...)				ErrLog->Log(S1(text), __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(...)
#define LOGV(...)
#endif

namespace ErrorSystem {
	class Logger {
	private:
		static const int BufSize = 2048;
		static const int SZBufSize = sizeof(char) * BufSize;

	public:
		void ShowError(const char* Message, const char* Title, bool IsSeriousError);
		void Log(const char* Message, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowFatalError(const char* Error, const char* File, const char* Func, const unsigned long Line);
	};
}

#endif