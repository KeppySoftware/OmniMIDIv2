/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _XSYNTH_H
#define _XSYNTH_H

extern void (*LoadSoundFont)(const char* path);
extern int (*StartModule)(double f);
extern int (*StopModule)();
extern void (*ResetModule)();
extern unsigned int (*SendData)(unsigned int ev);

#endif