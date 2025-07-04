/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _COMMON_H
#define _COMMON_H

#ifdef __VSCODE_IDE__
#define _NONFREE
#endif

#define MAX_PATH_LONG	            32767
#define RANGE(value, minv, maxv)    ((value) >= minv && (value) <= maxv)
#define MAKEVER(mj, mn, bd, rv)     (mj << 24) | (mn << 16) | (bd << 8) | rv

#ifdef _DEBUG
#define NAME                        "OmniMIDI (Debug)"
#define WNAME                       L"OmniMIDI (Debug)\0"
#define DEBUGCOMPILE                true
#else
#define NAME                        "OmniMIDI"
#define WNAME                       L"OmniMIDI\0"
#define DEBUGCOMPILE                false
#endif

#include <stdint.h>

#if !defined(_WIN32)
    // For malloc and memcpy
    #include <cstring>
    #define MAX_PATH		        4096
    #define SLEEPVAL(x)             x
    #define Sleep			        sleep
    #define SetTerminalTitle(x)     std::cout << "\033]0;" << x << "\007"
    #define WINAPI
#else
    #include <windows.h>
    #define SLEEPVAL(x)             -x
    #define SetTerminalTitle        SetConsoleTitleA
#endif

#if !defined(_M_IX86) && defined(__i386__)
    #define _M_IX86                 __i386__
#endif
#if !defined(_M_AMD64) && defined(__amd64__)
    #define _M_AMD64                __amd64__
#endif
#if !defined(_M_ARM) && defined(__arm__)
    #define _M_ARM                  __arm__
#endif
#if !defined(_M_ARM64) && defined(__aarch64__)
    #define _M_ARM64                __aarch64__
#endif

#if defined(_WIN32)
    //  Microsoft 
    #define EXPORT			        __declspec(dllexport)
    #define IMPORT			        __declspec(dllimport)
#elif defined(__gnu_linux__) || (defined(__FreeBSD_kernel__ ) && defined(__GLIBC__))
    //  GCC
    #define EXPORT			        __attribute__((visibility("default")))
    #define IMPORT
	#define CONSTRUCTOR		        __attribute__((constructor))
	#define DESTRUCTOR		        __attribute__((destructor))
#else
    #define EXPORT
    #define IMPORT
    #pragma warning			        Unknown compiler
#endif

#endif