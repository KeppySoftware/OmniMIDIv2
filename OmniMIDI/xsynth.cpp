/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include <xsynth.h>

void (*LoadSoundFont)(const char* path);
int (*StartModule)(double f);
int (*StopModule)();
void (*ResetModule)();
unsigned int (*SendData)(unsigned int ev);