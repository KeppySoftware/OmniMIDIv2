/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

  The XSynth library is licensed under the LGPL 3.0.

*/

#ifndef _XSYNTH_H
#define _XSYNTH_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#define XSYNTH_IMP WINAPI
#else
#define XSYNTH_IMP
#endif

#define XSYNTH_VERSION 0x302

#define XSYNTH_AUDIO_EVENT_NOTEON 0
#define XSYNTH_AUDIO_EVENT_NOTEOFF 1
#define XSYNTH_AUDIO_EVENT_ALLNOTESOFF 2
#define XSYNTH_AUDIO_EVENT_ALLNOTESKILLED 3
#define XSYNTH_AUDIO_EVENT_RESETCONTROL 4
#define XSYNTH_AUDIO_EVENT_CONTROL 5
#define XSYNTH_AUDIO_EVENT_PROGRAMCHANGE 6
#define XSYNTH_AUDIO_EVENT_PITCH 7
#define XSYNTH_AUDIO_EVENT_FINETUNE 8
#define XSYNTH_AUDIO_EVENT_COARSETUNE 9

#define XSYNTH_CONFIG_SETLAYERS 0
#define XSYNTH_CONFIG_SETPERCUSSIONMODE 1

#define XSYNTH_AUDIO_CHANNELS_MONO 1
#define XSYNTH_AUDIO_CHANNELS_STEREO 2

#define XSYNTH_INTERPOLATION_NEAREST 0
#define XSYNTH_INTERPOLATION_LINEAR 1
#define XSYNTH_ENVELOPE_CURVE_LINEAR 0
#define XSYNTH_ENVELOPE_CURVE_EXPONENTIAL 1

typedef struct XSynth_StreamParams {
  uint32_t sample_rate;
  uint16_t audio_channels;
} XSynth_StreamParams;

typedef struct XSynth_ParallelismOptions {
  int32_t channel;
  int32_t key;
} XSynth_ParallelismOptions;

typedef struct XSynth_GroupOptions {
  struct XSynth_StreamParams stream_params;
  uint32_t channels;
  bool fade_out_killing;
  struct XSynth_ParallelismOptions parallelism;
} XSynth_GroupOptions;

typedef struct XSynth_ChannelGroup {
  void *group;
} XSynth_ChannelGroup;

typedef struct XSynth_Soundfont {
  void *soundfont;
} XSynth_Soundfont;

typedef struct XSynth_ByteRange {
  uint8_t start;
  uint8_t end;
} XSynth_ByteRange;

typedef struct XSynth_RealtimeConfig {
  uint32_t channels;
  int32_t multithreading;
  bool fade_out_killing;
  double render_window_ms;
  struct XSynth_ByteRange ignore_range;
} XSynth_RealtimeConfig;

typedef struct XSynth_RealtimeSynth {
  void *synth;
} XSynth_RealtimeSynth;

typedef struct XSynth_RealtimeStats {
  uint64_t voice_count;
  int64_t buffer;
  double render_time;
} XSynth_RealtimeStats;

typedef struct XSynth_EnvelopeOptions {
  uint8_t attack_curve;
  uint8_t decay_curve;
  uint8_t release_curve;
} XSynth_EnvelopeOptions;

typedef struct XSynth_SoundfontOptions {
  struct XSynth_StreamParams stream_params;
  int16_t bank;
  int16_t preset;
  struct XSynth_EnvelopeOptions vol_envelope_options;
  bool use_effects;
  uint16_t interpolator;
} XSynth_SoundfontOptions;

extern uint32_t (XSYNTH_IMP* XSynth_GetVersion)(void);

extern struct XSynth_StreamParams (XSYNTH_IMP* XSynth_GenDefault_StreamParams)(void);
extern struct XSynth_ParallelismOptions (XSYNTH_IMP* XSynth_GenDefault_ParallelismOptions)(void);
extern struct XSynth_GroupOptions (XSYNTH_IMP* XSynth_GenDefault_GroupOptions)(void);
extern struct XSynth_ChannelGroup (XSYNTH_IMP* XSynth_ChannelGroup_Create)(struct XSynth_GroupOptions options);

extern void (XSYNTH_IMP* XSynth_ChannelGroup_SendAudioEvent)(struct XSynth_ChannelGroup handle, uint32_t channel, uint16_t event, uint16_t params);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_SendAudioEventAll)(struct XSynth_ChannelGroup handle, uint16_t event, uint16_t params);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_SendConfigEvent)(struct XSynth_ChannelGroup handle, uint32_t channel, uint16_t event, uint32_t params);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_SendConfigEventAll)(struct XSynth_ChannelGroup handle, uint16_t event, uint32_t params);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_SetSoundfonts)(struct XSynth_ChannelGroup handle, const struct XSynth_Soundfont *sf_ids, uint64_t count);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_ClearSoundfonts)(struct XSynth_ChannelGroup handle);
extern void (XSYNTH_IMP* XSynth_ChannelGroup_ReadSamples)(struct XSynth_ChannelGroup handle, float *buffer, uint64_t length);
extern uint64_t (XSYNTH_IMP* XSynth_ChannelGroup_VoiceCount)(struct XSynth_ChannelGroup handle);
extern struct XSynth_StreamParams (XSYNTH_IMP* XSynth_ChannelGroup_GetStreamParams)(struct XSynth_ChannelGroup handle);
extern void (XSYNTH_IMP*XSynth_ChannelGroup_Drop)(struct XSynth_ChannelGroup handle);

extern struct XSynth_RealtimeConfig (XSYNTH_IMP* XSynth_GenDefault_RealtimeConfig)(void);
extern struct XSynth_RealtimeSynth (XSYNTH_IMP* XSynth_Realtime_Create)(struct XSynth_RealtimeConfig config);
extern void (XSYNTH_IMP* XSynth_Realtime_SendAudioEvent)(struct XSynth_RealtimeSynth handle, uint32_t channel, uint16_t event, uint16_t params);
extern void (XSYNTH_IMP* XSynth_Realtime_SendEventU32)(struct XSynth_RealtimeSynth handle, uint32_t event);
extern void (XSYNTH_IMP* XSynth_Realtime_SendAudioEventAll)(struct XSynth_RealtimeSynth handle, uint16_t event, uint16_t params);
extern void (XSYNTH_IMP* XSynth_Realtime_SendConfigEvent)(struct XSynth_RealtimeSynth handle, uint32_t channel, uint16_t event, uint32_t params);
extern void (XSYNTH_IMP* XSynth_Realtime_SendConfigEventAll)(struct XSynth_RealtimeSynth handle, uint16_t event, uint32_t params);
extern void (XSYNTH_IMP* XSynth_Realtime_SetBuffer)(struct XSynth_RealtimeSynth handle, double render_window_ms);
extern void (XSYNTH_IMP* XSynth_Realtime_SetIgnoreRange)(struct XSynth_RealtimeSynth handle, struct XSynth_ByteRange ignore_range);
extern void (XSYNTH_IMP* XSynth_Realtime_SetSoundfonts)(struct XSynth_RealtimeSynth handle, const struct XSynth_Soundfont *sf_ids, uint64_t count);
extern void (XSYNTH_IMP* XSynth_Realtime_ClearSoundfonts)(struct XSynth_RealtimeSynth handle);
extern struct XSynth_StreamParams (XSYNTH_IMP* XSynth_Realtime_GetStreamParams)(struct XSynth_RealtimeSynth handle);
extern struct XSynth_RealtimeStats (XSYNTH_IMP* XSynth_Realtime_GetStats)(struct XSynth_RealtimeSynth handle);
extern void (XSYNTH_IMP* XSynth_Realtime_Reset)(struct XSynth_RealtimeSynth handle);
extern void (XSYNTH_IMP* XSynth_Realtime_Drop)(struct XSynth_RealtimeSynth handle);

extern struct XSynth_EnvelopeOptions (XSYNTH_IMP* XSynth_GenDefault_EnvelopeOptions)(void);
extern struct XSynth_SoundfontOptions (XSYNTH_IMP* XSynth_GenDefault_SoundfontOptions)(void);
extern struct XSynth_Soundfont (XSYNTH_IMP* XSynth_Soundfont_LoadNew)(const char *path, struct XSynth_SoundfontOptions options);
extern void (XSYNTH_IMP* XSynth_Soundfont_Remove)(struct XSynth_Soundfont handle);

#endif