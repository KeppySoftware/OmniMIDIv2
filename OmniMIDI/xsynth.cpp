/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "inc/xsynth.h"

uint32_t(_cdecl* XSynth_GetVersion)(void) = 0;
struct XSynth_StreamParams(*XSynth_GenDefault_StreamParams)(void) = 0;
struct XSynth_RealtimeConfig(*XSynth_GenDefault_RealtimeConfig)(void) = 0;
void(_cdecl* XSynth_Realtime_Init)(struct XSynth_RealtimeConfig config) = 0;
void(_cdecl* XSynth_Realtime_SendEvent)(uint32_t channel, uint16_t event, uint16_t params) = 0;
bool(_cdecl* XSynth_Realtime_IsActive)(void) = 0;
struct XSynth_StreamParams(_cdecl* XSynth_Realtime_GetStreamParams)(void) = 0;
struct XSynth_RealtimeStats(_cdecl* XSynth_Realtime_GetStats)(void) = 0;
void(_cdecl* XSynth_Realtime_SetLayerCount)(uint64_t layers) = 0;
void(_cdecl* XSynth_Realtime_SetSoundfonts)(const uint64_t* sf_ids, uint64_t count) = 0;
void(_cdecl* XSynth_Realtime_Reset)(void) = 0;
void(_cdecl* XSynth_Realtime_Drop)(void) = 0;
struct XSynth_SoundfontOptions(_cdecl* XSynth_GenDefault_SoundfontOptions)(void) = 0;
uint64_t(_cdecl* XSynth_Soundfont_LoadNew)(const char* path, struct XSynth_SoundfontOptions options) = 0;
void(_cdecl* XSynth_Soundfont_RemoveAll)(void) = 0;
void(_cdecl* XSynth_Soundfont_Remove)(uint64_t id) = 0;