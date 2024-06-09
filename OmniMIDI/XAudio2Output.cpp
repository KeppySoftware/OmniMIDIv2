/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#include "XAudio2Output.hpp"

XAudio2Output::XAudio2Output() {
#ifndef _M_ARM64
	wchar_t SysDir[MAX_PATH] = { 0 };
	wchar_t DLLPath[MAX_PATH] = { 0 };
	WIN32_FIND_DATA fd = { 0 };

	if (XALib == nullptr) {
		if (!(XALib = LoadLibraryA("XAudio2_9")) && !(XALib = LoadLibraryA("XAudio2_9redist"))) {
			if (Utils.GetFolderPath(OMShared::FIDs::System, SysDir, sizeof(SysDir))) {
				swprintf_s(DLLPath, MAX_PATH, L"%s\\OmniMIDI\\%s\0", SysDir, L"XAudio2_9redist.dll");

				// Found it?
				if (FindFirstFile(DLLPath, &fd) != INVALID_HANDLE_VALUE)
				{
					// Yes, load it
					XALib = LoadLibrary(DLLPath);

					// If it failed, return.
					if (!XALib) return;
				}
			}
			// Something went wrong, GetFolderPath failed
			else return;
		}

		xa2Create = (XA2C)GetProcAddress(XALib, "XAudio2Create");

		if (!xa2Create)
			return;
	}
#endif

	devNotifier.Register(this);
}

XAudio2Output::~XAudio2Output() {
	devNotifier.Unregister(this);

	Stop();

#ifndef _M_ARM64
	if (XALib != nullptr) {
		FreeLibrary(XALib);
		XALib = nullptr;
		xa2Create = nullptr;
	}
#endif
}

SoundOutResult XAudio2Output::Init(HMODULE m_hModule, SOAudioFlags flags, unsigned strmSampleRate, unsigned mspf, unsigned spf, unsigned chks) {
#ifndef _M_ARM64
	if (!xa2Create)
		return Fail;
#endif

	Stop();

	WAVEFORMATEX wfx = { 0 };
	HRESULT hr = 0;

	this->sampleRate = strmSampleRate;
	this->nCh = (flags & SOAudioFlags::MonoAudio) ? 1 : 2;
	this->maxSamplesPerFrame = mspf;
	this->samplesPerFrame = spf;
	this->nChks = chks;

	if (flags & SOAudioFlags::ShortData)
		this->bitDepth = 1;
	else if (flags & SOAudioFlags::TwentyFourData)
		this->bitDepth = 3;
	else if (flags & SOAudioFlags::FloatData)
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

	audioBuf = new float[mspf]();
	LOG(XAErr, "audioBuf allocated with a size of %d", mspf);

	wfx.nChannels = nCh;
	wfx.nSamplesPerSec = sampleRate;
	wfx.wBitsPerSample = bitDepth * 8;
	wfx.nBlockAlign = nCh * bitDepth;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	auto lat = (((double)mspf / (double)strmSampleRate) * 1000.0) * (double)samplesPerFrame;
	LOG(XAErr, "Max samples per frame set to %dSPFs, with an average samples per frame value of %d. Buffer will be split in chunks of %d. Latency will be %0.1fms.", maxSamplesPerFrame, samplesPerFrame, nChks, lat + 40.0);
	LOG(XAErr, "wfxStruct -> wFT: %d, nCh: %d, nSaPS: %d, nBlAlign: %d, nAvgByPS: %d, wBiPS: %d",
		wfx.wFormatTag, wfx.nChannels, wfx.nSamplesPerSec, wfx.nBlockAlign, wfx.nAvgBytesPerSec, wfx.wBitsPerSample);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

#ifndef _M_ARM64
	hr = xa2Create(&xaudDev, 0, XAUDIO2_DEFAULT_PROCESSOR);
#else
	hr = XAudio2Create(&xaudDev, 0, XAUDIO2_DEFAULT_PROCESSOR);
#endif

	if (FAILED(hr)) {
		NERROR(XAErr, "Error 0x%08x has occurred while opening the XAudio2 device.", true, hr);
		return StreamOpenFailed;
	}
	LOG(XAErr, "XA device created at 0x%08x.", true, xaudDev);

	hr = xaudDev->CreateMasteringVoice(
		&masterVoice,
		nCh,
		sampleRate,
		0,
		NULL,
		NULL,
		AudioCategory_Media);
	if (FAILED(hr)) {
		NERROR(XAErr, "Error 0x%08x has occurred while creating the mastering voice for the XAudio2 device.", true, hr);
		return MasteringVoiceFailed;
	}
	LOG(XAErr, "CreateMasteringVoice succeeded.", true, xaudDev);

	hr = xaudDev->CreateSourceVoice(&sourceVoice, &wfx, 0, 4.0f, &bufNotifier);
	if (FAILED(hr)) {
		NERROR(XAErr, "Error 0x%08x has occurred while creating the source voice for the XAudio2 device.", true, hr);
		return SourceVoiceFailed;
	}
	LOG(XAErr, "CreateSourceVoice succeeded.", true, xaudDev);

	hr = sourceVoice->Start(0);
	if (FAILED(hr)) {
		NERROR(XAErr, "Error 0x%08x has occurred while spinning up the XAudio2 device.", true, hr);
		return StartFailed;
	}
	LOG(XAErr, "Source voice started.", true, xaudDev);

	devChanged = false;
	bufReadHead = 0;
	bufWriteHead = 0;

	sampleBuffer = new unsigned char[maxSamplesPerFrame * samplesPerFrame * bitDepth]();
	samplesInBuffer = new unsigned long long[samplesPerFrame]();

	if (sampleBuffer && samplesInBuffer) {
		LOG(XAErr, "Allocated buffers.", true, xaudDev);
		return Success;
	}

	NERROR(XAErr, "One of the buffers returned zero. sampleBuffer: %d | samplesInBuffer: %d", true, sampleBuffer != nullptr, samplesInBuffer != nullptr);
	Stop();
}

SoundOutResult XAudio2Output::Stop() {
	if (sourceVoice) {
		sourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
		sourceVoice->FlushSourceBuffers();
		sourceVoice->DestroyVoice();
		sourceVoice = nullptr;
	}

	if (masterVoice) {
		masterVoice->DestroyVoice();
		masterVoice = nullptr;
	}

	if (xaudDev) {
		xaudDev->StopEngine();
		xaudDev->Release();
		xaudDev = nullptr;
	}

	if (sampleBuffer) {
		delete[] sampleBuffer;
		sampleBuffer = nullptr;
	}

	if (samplesInBuffer) {
		delete[] samplesInBuffer;
		samplesInBuffer = nullptr;
	}

	CoUninitialize();

	return Success;
}

SoundOutResult XAudio2Output::Update(void* buf, size_t len) {
#ifndef _M_ARM64
	if (!xa2Create)
		return Fail;
#endif

	if (buf == nullptr || len < 1)
		return Success;

	if (devChanged)
	{
		while (Init(nullptr, flagsS, sampleRate, maxSamplesPerFrame, samplesPerFrame, nChks))
			Funcs.uSleep(50000);

		devChanged = false;
	}

	for (;;) {
		sourceVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (voiceState.BuffersQueued < samplesPerFrame) break;
		else {
			const DWORD sleepms = (maxSamplesPerFrame / nCh) * samplesPerFrame * 1000 / sampleRate;
			if (WaitForSingleObject(bufNotifier.hBufferEndEvent, sleepms) == WAIT_TIMEOUT)
			{
				devChanged = true;
				return Success;
			}
		}
	}

	samplesInBuffer[bufWriteHead] = len / nCh;
	size_t num_bytes = len * bitDepth;

	XAUDIO2_BUFFER xaudioBuf = { 0 };
	xaudioBuf.AudioBytes = num_bytes;
	xaudioBuf.pAudioData = sampleBuffer + maxSamplesPerFrame * bufWriteHead * bitDepth;
	xaudioBuf.pContext = this;
	bufWriteHead = (bufWriteHead + 1) % samplesPerFrame;

	memcpy((void*)xaudioBuf.pAudioData, buf, num_bytes);

	if (sourceVoice->SubmitSourceBuffer(&xaudioBuf) == S_OK)
	{
		xaudDev->CommitChanges(XAUDIO2_COMMIT_NOW);
		return Success;
	}

	return Fail;
}

void XAudio2Output::OnDeviceChanged() {
	devChanged = true;
}

SoundOut* CreateXAudio2Output()
{
	return new XAudio2Output;
}
#endif