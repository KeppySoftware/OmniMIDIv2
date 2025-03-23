/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "ErrSys.hpp"

std::mutex ErrorSystem::Logger::logMutex;

void ErrorSystem::Logger::ShowError(const char* Message, const char* Title, bool IsSeriousError) {
#if defined(_WIN32) && !defined(_M_ARM)
	MsgBox(NULL, Message, Title, IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
#else
	std::cout << Message << std::endl;
#endif
}

void ErrorSystem::Logger::Log(const char* Message, const char* File, const char* Func, const unsigned long Line, ...) {
	va_list vl;
	va_start(vl, Line);

	char* cBuf = new char[BufSize];
	char* tBuf = new char[BufSize];

	vsnprintf(tBuf, SZBufSize, Message, vl);
	dbgprintf;

	logMutex.lock();
	std::cout << cBuf << std::endl;
	logMutex.unlock();

	delete[] cBuf;
	delete[] tBuf;

	va_end(vl);
}

void ErrorSystem::Logger::ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...) {
	va_list vl;
	va_start(vl, Line);

	if (!Error) {
#if defined(_WIN32) && !defined(_M_ARM)
		int GLE = GetLastError();
		LPSTR GLEBuf = nullptr;

		size_t MsgBufSize = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, GLE,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&GLEBuf, 0, NULL);

		if (MsgBufSize != 0)
		{
			MsgBox(NULL, GLEBuf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
			LocalFree(GLEBuf);
		}
#else
		return;
#endif
	}
	else {
		char* tBuf = new char[BufSize];

		vsnprintf(tBuf, SZBufSize, Error, vl);

#if defined(_DEBUG) || defined(VERBOSE_LOG)
		char* Buf = new char[BufSize];
		char* cBuf = new char[BufSize];

		snprintf(Buf, BufSize, "An error has occured in the \"%s\" function!\n\nFile: %s - Line: %lu\n\nError:\n%s",
			Func, File, Line, tBuf);

		dbgprintf;

		logMutex.lock();
		std::cout << cBuf << std::endl;
		logMutex.unlock();
		
		delete[] cBuf;

		MsgBox(NULL, Buf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
		delete[] Buf;
#else
		MsgBox(NULL, tBuf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
#endif

		delete[] tBuf;
	}

	va_end(vl);
}

void ErrorSystem::Logger::ThrowFatalError(const char* Error, const char* File, const char* Func, const unsigned long Line) {
	char* Buf = new char[BufSize];

#if defined(_DEBUG) || defined(VERBOSE_LOG)
	snprintf(Buf, SZBufSize, "A fatal error has occured in the \"%s\" function, from which the driver can NOT recover! File: %s - Line: %lu - Error:%s",
		Func, File, Line, Error);
#else
	snprintf(Buf, BufSize, "An fatal error has occured in the \"%s\" function, from which the driver can NOT recover! Error: %s", 
		Func, Error);
#endif

	logMutex.lock();
	std::cout << Buf << std::endl;
	MsgBox(NULL, Buf, "OmniMIDI - FATAL ERROR", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
	logMutex.unlock();

	delete[] Buf;

#ifdef _DEBUG
	throw std::runtime_error(Error);
#else
	exit(-1);
#endif
}