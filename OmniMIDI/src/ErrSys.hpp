/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _ERRSYS_H
#define _ERRSYS_H

#include <future>
#include <iostream>
#include <string>
#include <string_view>
#include <mutex>

#if !defined(_MSC_VER)
#define __func__ __FUNCTION__
#endif

#if defined(_WIN32)
#include <tchar.h>
#include <windows.h>
#include <psapi.h>
#else
#include <stdarg.h>
#endif

#if defined(_WIN32)
#define MsgBox								MessageBoxA
#else
#define MsgBox(...)
#define MB_ICONWARNING						0
#define MB_ICONERROR						0
#define MB_SYSTEMMODAL						0
#define MB_OK								0
#endif

#if defined(_DEBUG) || defined(VERBOSE_LOG)
#include <stdexcept>
#define	__SRCFILE							__FILE__
#define	__FUNCNAME							__func__
#define	__LINENUMBER						__LINE__
#define dbgprintf							snprintf(cBuf, SZBufSize, "[%s -> %s, L%lu] >> %s", Func, File, Line, tBuf);
#else
#define	__SRCFILE							nullptr
#define	__FUNCNAME							nullptr
#define	__LINENUMBER						0
#define dbgprintf							snprintf(cBuf, SZBufSize, "[] >> %s", tBuf);
#endif

#define Error(text, fatal, ...)				ErrLog->ThrowError(text, fatal, __SRCFILE, __FUNCNAME, __LINENUMBER, ##__VA_ARGS__)
#define Fatal(text)							ErrLog->ThrowFatalError(text, __SRCFILE, __FUNCNAME, __LINENUMBER)
#define Message(text, ...)					ErrLog->Log(text, __SRCFILE, __FUNCNAME, __LINENUMBER, ##__VA_ARGS__)

namespace ErrorSystem {
	class Logger {
	private:
		static const int BufSize = 2048;
		static const int SZBufSize = sizeof(char) * BufSize;
		static std::mutex logMutex;

	public:
		void ShowError(const char* Message, const char* Title, bool IsSeriousError);
		void Log(const char* Message, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...);
		void ThrowFatalError(const char* Error, const char* File, const char* Func, const unsigned long Line);
	};
}

#endif