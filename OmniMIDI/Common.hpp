/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _COMMON_H
#define _COMMON_H

#define MAX_PATH_LONG	32767
#define RANGE(variable, minv, maxv) ((variable) >= minv && (value) <= maxv)

#ifndef _WIN32
// For malloc and memcpy
#include <cstring>
#define MAX_PATH		MAX_PATH_LONG
#define Sleep			sleep
#endif

#endif