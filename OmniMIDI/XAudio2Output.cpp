/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "XAudio2Output.hpp"

XAudio2Output::~XAudio2Output() {
	Stop();
}

XAudio2Output::XAResult XAudio2Output::Init(unsigned strmSampleRate, unsigned int bassFlags, unsigned spf, unsigned sr, unsigned int* basspointer = nullptr) {
	WAVEFORMATEX wfx;
	HRESULT hr = 0;

	if (!basspointer && !bassStrmPtr)
		return NoBASSPointer;

	this->sampleRate = strmSampleRate;
	this->nCh = (bassFlags & BASS_SAMPLE_MONO) ? 1 : 2;
	this->samplesPerFrame = spf;
	this->sweepRate = sr;

	if (bassFlags & BASS_SAMPLE_8BITS)
		this->bitDepth = 1;
	else if (bassFlags & BASS_SAMPLE_FLOAT)
		this->bitDepth = 4;
	else
		this->bitDepth = 2;

	if (basspointer)
		bassStrmPtr = basspointer;

	bassFlagsS = bassFlags;
	int buflen = BASS_ChannelSeconds2Bytes(*bassStrmPtr, 0.016) / bitDepth;

	switch (bitDepth) {
	case 1:
	case 2:
		wfx.wFormatTag = WAVE_FORMAT_ADPCM;
		audioBuf = new int[buflen]();
		LOG(XAErr, "basspointer 0x%08x -> audioBuf (int) allocated with a size of %d", *bassStrmPtr, buflen);
		break;
	case 4:
	default:
		wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		audioBuf = new float[buflen]();
		LOG(XAErr, "basspointer 0x%08x -> audioBuf (float) allocated with a size of %d", *bassStrmPtr, buflen);
		break;
	}

	wfx.nChannels = nCh;
	wfx.nSamplesPerSec = sampleRate;
	wfx.nBlockAlign = bitDepth * nCh;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.wBitsPerSample = bitDepth * 8;
	wfx.cbSize = 0;

	LOG(XAErr, "SPF set to %d with a sweep rate of %d.", spf, sr);
	LOG(XAErr, "wfxStruct -> wFT: %d, nCh: %d, nSaPS: %d, nBlAlign: %d, nAvgByPS: %d, wBiPS: %d",
		wfx.wFormatTag, wfx.nChannels, wfx.nSamplesPerSec, wfx.nBlockAlign, wfx.nAvgBytesPerSec, wfx.wBitsPerSample);

	hr = XAudio2Create(&xaudDev, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) return StreamOpenFailed;

	hr = xaudDev->CreateMasteringVoice(
		&masterVoice,
		nCh,
		sampleRate,
		0,
		NULL,
		NULL,
		AudioCategory_Media);
	if (FAILED(hr)) return MasteringVoiceFailed;

	hr = xaudDev->CreateSourceVoice(&sourceVoice, &wfx, 0, 1.0f);
	if (FAILED(hr)) return SourceVoiceFailed;

	hr = sourceVoice->Start(0);
	if (FAILED(hr)) return StartFailed;

	devChanged = false;
	restartCount = 0;
	bufWriteHead = 0;

	sampleBuffer = new unsigned char[samplesPerFrame * sweepRate * bitDepth]();
	samplesInBuffer = new unsigned long long[sweepRate]();
	xaudioBuf = new XAUDIO2_BUFFER();

	return Success;
}

XAudio2Output::XAResult XAudio2Output::Stop() {
	if (sourceVoice) {
		sourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
		sourceVoice->DestroyVoice();
		sourceVoice = nullptr;
	}

	if (masterVoice) {
		masterVoice->DestroyVoice();
		masterVoice = nullptr;
	}

	if (xaudDev) {
		xaudDev->Release();
		xaudDev = nullptr;
	}

	if (sampleBuffer)
	{
		delete[] sampleBuffer;
		sampleBuffer = nullptr;
	}

	if (samplesInBuffer) {
		delete[] samplesInBuffer;
		samplesInBuffer = nullptr;
	}

	if (xaudioBuf) {
		delete xaudioBuf;
		xaudioBuf = nullptr;
	}

	return Success;
}

XAudio2Output::XAResult XAudio2Output::Update() {
	if (devChanged)
	{
		Stop();
		restartCount = 5;
		devChanged = false;
		return Success;
	}

	if (restartCount)
	{
		if (!--restartCount)
		{
			auto err = Init(sampleRate, bassFlagsS, samplesPerFrame, sweepRate);
			if (err)
			{
				restartCount = 60 * 5;
				return err;
			}
		}
		else return Success;
	}

	for (;;) {
		sourceVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (voiceState.BuffersQueued < sweepRate)
			break;
	}

	auto voidSize = (this->bitDepth == 4) ? sizeof(float) : sizeof(int);
	auto bufs = ((this->bitDepth == 4) ? BASS_DATA_FLOAT : BASS_DATA_FIXED) + samplesPerFrame * voidSize;
	auto data = BASS_ChannelGetData(*bassStrmPtr, audioBuf, bufs);
	auto dataSize = data / voidSize;

	if (data == -1)
		return Success;

	samplesInBuffer[bufWriteHead] = dataSize / nCh;
	unsigned num_bytes = dataSize * bitDepth;

	xaudioBuf->AudioBytes = num_bytes;
	xaudioBuf->pAudioData = sampleBuffer + samplesPerFrame * bufWriteHead * bitDepth;
	xaudioBuf->pContext = this;
	bufWriteHead = (bufWriteHead + 1) % sweepRate;

	memcpy((void*)xaudioBuf->pAudioData, audioBuf, num_bytes);

	if (sourceVoice->SubmitSourceBuffer(xaudioBuf) == S_OK)
	{
		xaudDev->CommitChanges(XAUDIO2_COMMIT_NOW);
		return Success;
	}

	Stop();
	restartCount = 60 * 5;

	return Success;
}