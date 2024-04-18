/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#ifndef _XAUDIO2_OUTPUT_H
#define _XAUDIO2_OUTPUT_H

#include "bass/bass.h"
#include "ErrSys.hpp"
#include <XAudio2.h>
#include <mmsystem.h>

enum XAResult {
	Fail = -1,
	Success,
	StreamOpenFailed,
	MasteringVoiceFailed,
	SourceVoiceFailed,
	StartFailed,
	NoBASSPointer
};

enum XAFlags {
	Default = 0,
	ShortData = 1 << 0,
	TwentyFourData = 1 << 2,
	FloatData = 1 << 3,
	MonoAudio = 1 << 4,
	StereoAudio = 1 << 5
};
DEFINE_ENUM_FLAG_OPERATORS(XAFlags);

class XAudio2Output
{
private:
	volatile bool			devChanged = 0;
	unsigned				restartCount = 0;
	unsigned				sampleRate = 0, samplesPerFrame = 0, sweepRate = 0;
	unsigned short			bitDepth = 0;
	unsigned short			nCh = 0;
	unsigned long long		bufWriteHead = 0;

	void*					audioBuf = 0;

	XAFlags					flagsS = (XAFlags)0;

	unsigned char*			sampleBuffer = 0;
	unsigned long long*		samplesInBuffer = 0;

	IXAudio2*				xaudDev = nullptr;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	IXAudio2SourceVoice*	sourceVoice = nullptr;
	XAUDIO2_VOICE_STATE		voiceState = { 0 };
	XAUDIO2_BUFFER*			xaudioBuf = nullptr;
	ErrorSystem::Logger		XAErr;

public:
	~XAudio2Output();
	XAResult Init(XAFlags flags, unsigned sample_rate, unsigned spf, unsigned sr);
	XAResult Stop();
	XAResult Update(void* buf, size_t len);

};

#endif
#endif