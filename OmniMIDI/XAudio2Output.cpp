/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#include "XAudio2Output.hpp"

XAudio2Output::~XAudio2Output() {
	Stop();
}

XAResult XAudio2Output::Init(XAFlags flags, unsigned strmSampleRate, unsigned spf, unsigned sr) {
	WAVEFORMATEX wfx = { 0 };
	HRESULT hr = 0;

	this->sampleRate = strmSampleRate;
	this->nCh = (flags & MonoAudio) ? 1 : 2;
	this->samplesPerFrame = spf;
	this->sweepRate = sr;

	if (flags & ShortData)
		this->bitDepth = 1;
	else if (flags & TwentyFourData)
		this->bitDepth = 3;
	else if (flags & FloatData)
		this->bitDepth = 4;
	else
		this->bitDepth = 2;

	flagsS = flags;

	switch (bitDepth) {
	case 4:
		wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		break;
	default:
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		break;
	}

	audioBuf = new float[spf]();
	LOG(XAErr, "audioBuf allocated with a size of %d", spf);

	wfx.nChannels = nCh;
	wfx.nSamplesPerSec = sampleRate;
	wfx.wBitsPerSample = bitDepth * 8;
	wfx.nBlockAlign = nCh * bitDepth;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	auto lat = (((double)spf / (double)strmSampleRate) * 1000.0) * (double)sweepRate;
	LOG(XAErr, "Buffer size set to %d, with sweep rate value of %d. Latency will be %0.1fms.", spf, sr, lat + 40.0);
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
	if (FAILED(hr)) 
		return MasteringVoiceFailed;

	hr = xaudDev->CreateSourceVoice(&sourceVoice, &wfx, 0, 1.0f);
	if (FAILED(hr)) 
		return SourceVoiceFailed;

	hr = sourceVoice->Start(0);
	if (FAILED(hr))
		return StartFailed;

	devChanged = false;
	restartCount = 0;
	bufWriteHead = 0;

	sampleBuffer = new unsigned char[samplesPerFrame * sweepRate * bitDepth]();
	samplesInBuffer = new unsigned long long[sweepRate]();
	xaudioBuf = new XAUDIO2_BUFFER();

	return Success;
}

XAResult XAudio2Output::Stop() {
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

XAResult XAudio2Output::Update(unsigned int basspointer) {
	auto dl = BASS_ChannelGetData(basspointer, audioBuf, ((this->bitDepth == 4) ? BASS_DATA_FLOAT : 0) + samplesPerFrame * sizeof(float));
	
	if (dl != -1)
		return Update(audioBuf, dl / sizeof(float));

	return Fail;
}

XAResult XAudio2Output::Update(void* buf, size_t len) {
	for (;;) {
		sourceVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (voiceState.BuffersQueued < sweepRate) break;
	}

	if (buf == nullptr || len < 1)
		return Success;

	samplesInBuffer[bufWriteHead] = len / nCh;
	unsigned num_bytes = len * bitDepth;

	xaudioBuf->AudioBytes = num_bytes;
	xaudioBuf->pAudioData = sampleBuffer + samplesPerFrame * bufWriteHead * bitDepth;
	xaudioBuf->pContext = this;
	bufWriteHead = (bufWriteHead + 1) % sweepRate;

	memcpy((void*)xaudioBuf->pAudioData, buf, num_bytes);

	if (sourceVoice->SubmitSourceBuffer(xaudioBuf) == S_OK)
	{
		xaudDev->CommitChanges(XAUDIO2_COMMIT_NOW);
		return Success;
	}

	return Fail;
}

#endif