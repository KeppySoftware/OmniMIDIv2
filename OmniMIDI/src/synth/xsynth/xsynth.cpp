/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "xsynth.h"

uint32_t (XSYNTH_IMP* XSynth_GetVersion)(void) = 0;

struct XSynth_StreamParams (XSYNTH_IMP* XSynth_GenDefault_StreamParams)(void) = 0;
struct XSynth_ParallelismOptions (XSYNTH_IMP* XSynth_GenDefault_ParallelismOptions)(void) = 0;
struct XSynth_GroupOptions (XSYNTH_IMP* XSynth_GenDefault_GroupOptions)(void) = 0;
struct XSynth_ChannelGroup (XSYNTH_IMP* XSynth_ChannelGroup_Create)(struct XSynth_GroupOptions options) = 0;
struct XSynth_EnvelopeOptions (XSYNTH_IMP* XSynth_GenDefault_EnvelopeOptions)(void) = 0;
struct XSynth_SoundfontOptions (XSYNTH_IMP* XSynth_GenDefault_SoundfontOptions)(void) = 0;

void (XSYNTH_IMP* XSynth_ChannelGroup_SendAudioEvent)(struct XSynth_ChannelGroup handle, uint32_t channel, uint16_t event, uint16_t params) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_SendAudioEventAll)(struct XSynth_ChannelGroup handle, uint16_t event, uint16_t params) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_SendConfigEvent)(struct XSynth_ChannelGroup handle, uint32_t channel, uint16_t event, uint32_t params) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_SendConfigEventAll)(struct XSynth_ChannelGroup handle, uint16_t event, uint32_t params) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_SetSoundfonts)(struct XSynth_ChannelGroup handle, const struct XSynth_Soundfont *sf_ids, uint64_t count) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_ClearSoundfonts)(struct XSynth_ChannelGroup handle) = 0;
void (XSYNTH_IMP* XSynth_ChannelGroup_ReadSamples)(struct XSynth_ChannelGroup handle, float *buffer, uint64_t length) = 0;
uint64_t (XSYNTH_IMP* XSynth_ChannelGroup_VoiceCount)(struct XSynth_ChannelGroup handle) = 0;
struct XSynth_StreamParams (XSYNTH_IMP* XSynth_ChannelGroup_GetStreamParams)(struct XSynth_ChannelGroup handle) = 0;
void (XSYNTH_IMP*XSynth_ChannelGroup_Drop)(struct XSynth_ChannelGroup handle) = 0;

struct XSynth_RealtimeConfig (XSYNTH_IMP* XSynth_GenDefault_RealtimeConfig)(void) = 0;
struct XSynth_RealtimeSynth (XSYNTH_IMP* XSynth_Realtime_Create)(struct XSynth_RealtimeConfig config) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SendAudioEvent)(struct XSynth_RealtimeSynth handle, uint32_t channel, uint16_t event, uint16_t params) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SendAudioEventAll)(struct XSynth_RealtimeSynth handle, uint16_t event, uint16_t params) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SendEventU32)(struct XSynth_RealtimeSynth handle, uint32_t event) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SendConfigEvent)(struct XSynth_RealtimeSynth handle, uint32_t channel, uint16_t event, uint32_t params) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SendConfigEventAll)(struct XSynth_RealtimeSynth handle, uint16_t event, uint32_t params) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SetBuffer)(struct XSynth_RealtimeSynth handle, double render_window_ms) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SetIgnoreRange)(struct XSynth_RealtimeSynth handle, struct XSynth_ByteRange ignore_range) = 0;
void (XSYNTH_IMP* XSynth_Realtime_SetSoundfonts)(struct XSynth_RealtimeSynth handle, const struct XSynth_Soundfont *sf_ids, uint64_t count) = 0;
void (XSYNTH_IMP* XSynth_Realtime_ClearSoundfonts)(struct XSynth_RealtimeSynth handle) = 0;
struct XSynth_StreamParams (XSYNTH_IMP* XSynth_Realtime_GetStreamParams)(struct XSynth_RealtimeSynth handle) = 0;
struct XSynth_RealtimeStats (XSYNTH_IMP* XSynth_Realtime_GetStats)(struct XSynth_RealtimeSynth handle) = 0;
void (XSYNTH_IMP* XSynth_Realtime_Reset)(struct XSynth_RealtimeSynth handle) = 0;
void (XSYNTH_IMP* XSynth_Realtime_Drop)(struct XSynth_RealtimeSynth handle) = 0;

struct XSynth_Soundfont (XSYNTH_IMP* XSynth_Soundfont_LoadNew)(const char *path, struct XSynth_SoundfontOptions options) = 0;
void (XSYNTH_IMP* XSynth_Soundfont_Remove)(struct XSynth_Soundfont handle) = 0;