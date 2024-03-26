/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.

*/

#include "ErrSys.hpp"

void ErrorSystem::Logger::Log(const char* Message, const char* File, const char* Func, const unsigned long Line, ...) {
#if defined(_WIN32) && !defined(_M_ARM) && defined(_DEBUG)
	va_list vl;
	va_start(vl, Line);

	char* cBuf = new char[SZBufSize];
	char* tBuf = new char[SZBufSize];
	
	vsprintf_s(tBuf, SZBufSize, Message, vl);
	sprintf_s(cBuf, SZBufSize, "[%s -> %s, L%d] >> %s", Func, File, Line, tBuf);
	std::cout << cBuf << std::endl;

	delete[] cBuf;
	delete[] tBuf;

	va_end(vl);
#endif
}

void ErrorSystem::Logger::ThrowError(const char* Error, bool IsSeriousError, const char* File, const char* Func, const unsigned long Line, ...) {
#if defined(_WIN32) && !defined(_M_ARM)
	va_list vl;
	va_start(vl, Line);

	int GLE = GetLastError();
	char* Buf = nullptr;
	char* tBuf = nullptr;
	char* cBuf = nullptr;
	LPSTR GLEBuf = nullptr;

	if (!Error) {
		size_t MsgBufSize = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, GLE,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&GLEBuf, 0, NULL);

		if (MsgBufSize != 0)
		{
			MessageBoxA(NULL, GLEBuf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
			LocalFree(GLEBuf);
		}
	}
	else {
		tBuf = new char[SZBufSize];
		Buf = new char[SZBufSize];
		cBuf = new char[SZBufSize];

		vsprintf_s(tBuf, SZBufSize, Error, vl);
		MessageBoxA(NULL, tBuf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);

#ifdef _DEBUG
		sprintf_s(Buf, BufSize, "An error has occured in the \"%s\" function! File: %s - Line: %d - Error: %s", 
			Func, File, Line, tBuf);
#else
		sprintf_s(Buf, BufSize, "An error has occured in the \"%s\" function! Error: %s", 
			Func, tBuf);
#endif
		sprintf_s(cBuf, SZBufSize, "[%s -> %s, L%d] >> %s", Func, File, Line, tBuf);
		std::async([&cBuf]() { std::cout << cBuf << std::endl; });
		delete[] cBuf;
		delete[] tBuf;

		MessageBoxA(NULL, Buf, "OmniMIDI - ERROR", IsSeriousError ? MB_ICONERROR : MB_ICONWARNING | MB_OK | MB_SYSTEMMODAL);
		delete[] Buf;
	}

	va_end(vl);
#endif
}

void ErrorSystem::Logger::ThrowFatalError(const char* Error, const char* File, const char* Func, const unsigned long Line) {
#if defined(_WIN32) && !defined(_M_ARM)
	char* Buf = new char[SZBufSize];

#ifdef _DEBUG
	sprintf_s(Buf, BufSize, "A fatal error has occured in the \"%s\" function, from which the driver can NOT recover! File: %s - Line: %s - Error:%s",
		Func, File, Line, Error);

#else
	sprintf_s(Buf, BufSize, "An fatal error has occured in the \"%s\" function, from which the driver can NOT recover! Error: %s", 
		Func, Error);
#endif

	MessageBoxA(NULL, Buf, "OmniMIDI - FATAL ERROR", MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
	std::async([&Buf]() { std::cout << Buf << std::endl; });
	delete[] Buf;

	throw std::runtime_error(Error);
#endif
}