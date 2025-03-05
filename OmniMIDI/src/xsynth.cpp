/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "xsynth.h"

uint32_t(XSYNTH_IMP* XSynth_GetVersion)(void) = 0;
struct XSynth_StreamParams(*XSynth_GenDefault_StreamParams)(void) = 0;
struct XSynth_RealtimeConfig(*XSynth_GenDefault_RealtimeConfig)(void) = 0;
void(XSYNTH_IMP* XSynth_Realtime_Init)(struct XSynth_RealtimeConfig config) = 0;
void(XSYNTH_IMP* XSynth_Realtime_SendEvent)(uint32_t channel, uint16_t event, uint16_t params) = 0;
bool(XSYNTH_IMP* XSynth_Realtime_IsActive)(void) = 0;
struct XSynth_StreamParams(XSYNTH_IMP* XSynth_Realtime_GetStreamParams)(void) = 0;
struct XSynth_RealtimeStats(XSYNTH_IMP* XSynth_Realtime_GetStats)(void) = 0;
void(XSYNTH_IMP* XSynth_Realtime_SetLayerCount)(uint64_t layers) = 0;
void(XSYNTH_IMP* XSynth_Realtime_SetSoundfonts)(const uint64_t* sf_ids, uint64_t count) = 0;
void(XSYNTH_IMP* XSynth_Realtime_Reset)(void) = 0;
void(XSYNTH_IMP* XSynth_Realtime_Drop)(void) = 0;
struct XSynth_SoundfontOptions(XSYNTH_IMP* XSynth_GenDefault_SoundfontOptions)(void) = 0;
uint64_t(XSYNTH_IMP* XSynth_Soundfont_LoadNew)(const char* path, struct XSynth_SoundfontOptions options) = 0;
void(XSYNTH_IMP* XSynth_Soundfont_RemoveAll)(void) = 0;
void(XSYNTH_IMP* XSynth_Soundfont_Remove)(uint64_t id) = 0;