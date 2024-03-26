/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _BASSXA_H
#define _BASSXA_H

#include "bass/bass.h"
#include "ErrSys.hpp"
#include <XAudio2.h>

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

	unsigned int*			bassStrmPtr = 0;
	unsigned int			bassFlagsS = 0;

	unsigned char*			sampleBuffer = 0;
	unsigned long long*		samplesInBuffer = 0;

	IXAudio2*				xaudDev = nullptr;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	IXAudio2SourceVoice*	sourceVoice = nullptr;
	XAUDIO2_VOICE_STATE		voiceState;
	XAUDIO2_BUFFER*			xaudioBuf;
	ErrorSystem::Logger		XAErr;

public:
	enum XAResult {
		Success,
		StreamOpenFailed,
		MasteringVoiceFailed,
		SourceVoiceFailed,
		StartFailed,
		NoBASSPointer
	};

	~XAudio2Output();
	XAResult Init(unsigned sample_rate, unsigned int bassflags, unsigned max_samples_per_frame, unsigned num_frames, unsigned int* basspointer);
	XAResult Stop();
	XAResult Update();
};

#endif