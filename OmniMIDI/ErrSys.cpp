/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "ErrSys.hpp"

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
	snprintf(cBuf, SZBufSize, "[%s -> %s, L%d] >> %s", Func, File, Line, tBuf);
	std::cout << cBuf << std::endl;

	delete[] cBuf;
	delete[] tBuf;

	va_end(vl);
}

void ErrorSystem::Logger::ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...) {
	va_list vl;
	va_start(vl, Line);

	char* Buf = nullptr;
	char* tBuf = nullptr;
	char* cBuf = nullptr;

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
		tBuf = new char[BufSize];
		Buf = new char[BufSize];
		cBuf = new char[BufSize];

		vsnprintf(tBuf, SZBufSize, Error, vl);

#if defined(_DEBUG) || defined(WIN7VERBOSE)
		snprintf(Buf, BufSize, "An error has occured in the \"%s\" function! File: %s - Line: %d - Error: %s",
			Func, File, Line, tBuf);

		snprintf(cBuf, SZBufSize, "[%s -> %s, L%d] >> %s", Func, File, Line, tBuf);
		std::cout << cBuf << std::endl;
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

#if defined(_DEBUG) || defined(WIN7VERBOSE)
	snprintf(Buf, SZBufSize, "A fatal error has occured in the \"%s\" function, from which the driver can NOT recover! File: %s - Line: %s - Error:%s",
		Func, File, Line, Error);
#else
	snprintf(Buf, BufSize, "An fatal error has occured in the \"%s\" function, from which the driver can NOT recover! Error: %s", 
		Func, Error);
#endif

	MsgBox(NULL, Buf, "OmniMIDI - FATAL ERROR", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
	std::async([&Buf]() { std::cout << Buf << std::endl; });
	delete[] Buf;

	throw std::runtime_error(Error);
}