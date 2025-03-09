/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _COMMON_H
#define _COMMON_H

#define MAX_PATH_LONG	32767
#define RANGE(variable, minv, maxv) ((variable) >= minv && (value) <= maxv)

#ifdef _DEBUG
#define NAME "OmniMIDI (Debug)"
#define WNAME L"OmniMIDI (Debug)\0"
#else
#define NAME "OmniMIDI"
#define WNAME L"OmniMIDI\0"
#endif

#ifndef _WIN32
// For malloc and memcpy
#include <cstring>
#define MAX_PATH		MAX_PATH_LONG
#define Sleep			sleep
#endif

#if defined(_MSC_VER)
    //  Microsoft 
    #define EXPORT			__declspec(dllexport)
    #define IMPORT			__declspec(dllimport)
#elif defined(__GNUC__)
    //  GCC
    #define EXPORT			__attribute__((visibility("default")))
    #define IMPORT
	#define CONSTRUCTOR		__attribute__((constructor))
	#define DESTRUCTOR		__attribute__((destructor))
#else
    #define EXPORT
    #define IMPORT
    #pragma warning			Unknown platform
#endif

#endif