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
				swprintf(DLLPath, MAX_PATH, L"%s\\OmniMIDI\\%s\0", SysDir, L"XAudio2_9redist.dll");

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

SoundOutResult XAudio2Output::Init(HMODULE m_hModule, SOAudioFlags flags, unsigned int strmSampleRate, unsigned spf, unsigned chks) {
#ifndef _M_ARM64
	if (!xa2Create)
		return Fail;
#endif

	Stop();

	WAVEFORMATEXTENSIBLE wfx = { 0 };
	HRESULT hr = 0;

	this->sampleRate = strmSampleRate;
	this->nCh = (flags & SOAudioFlags::MonoAudio) ? 1 : 2;
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
		wfx.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		break;
	default:
		wfx.Format.wFormatTag = WAVE_FORMAT_PCM;
		break;
	}

	audioBuf = new float[maxSamplesPerFrame]();
	LOG("audioBuf allocated with a size of %d", spf);

	wfx.Format.nChannels = nCh;
	wfx.Format.nSamplesPerSec = sampleRate;
	wfx.Format.wBitsPerSample = bitDepth * 8;
	wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
	wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
	wfx.Format.cbSize = 0;
	wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

	auto lat = (((double)maxSamplesPerFrame / (double)strmSampleRate) * 1000.0);
	LOG("SPF limit set to %dSPFs, with a query  value of %dSPFs. Buffer will be split in chunks of %d. Latency will be around %0.1fms.", maxSamplesPerFrame, samplesPerFrame, nChks, lat + 40.0);
	LOG("wfxStruct -> wFT: %d, nCh: %d, nSaPS: %d, nBlAlign: %d, nAvgByPS: %d, wBiPS: %d",
		wfx.Format.wFormatTag, wfx.Format.nChannels, wfx.Format.nSamplesPerSec, wfx.Format.nBlockAlign, wfx.Format.nAvgBytesPerSec, wfx.Format.wBitsPerSample);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

#ifndef _M_ARM64
	hr = xa2Create(&xaudDev, 0, XAUDIO2_DEFAULT_PROCESSOR);
#else
	hr = XAudio2Create(&xaudDev, 0, XAUDIO2_DEFAULT_PROCESSOR);
#endif

	if (FAILED(hr)) {
		NERROR("Error 0x%08x has occurred while opening the XAudio2 device.", true, hr);
		return StreamOpenFailed;
	}
	LOG("XA device created at 0x%08x.", true, xaudDev);

	hr = xaudDev->CreateMasteringVoice(
		&masterVoice,
		nCh,
		sampleRate,
		0,
		NULL,
		NULL,
		AudioCategory_Media);
	if (FAILED(hr)) {
		NERROR("Error 0x%08x has occurred while creating the mastering voice for the XAudio2 device.", true, hr);
		return MasteringVoiceFailed;
	}
	LOG("CreateMasteringVoice succeeded.", true, xaudDev);

	hr = xaudDev->CreateSourceVoice(&sourceVoice, (WAVEFORMATEX*)&wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &bufNotifier);
	if (FAILED(hr) || !sourceVoice) {
		NERROR("Error 0x%08x has occurred while creating the source voice for the XAudio2 device.", true, hr);
		return SourceVoiceFailed;
	}
	LOG("CreateSourceVoice succeeded.", true, xaudDev);

	hr = sourceVoice->Start(0);
	if (FAILED(hr)) {
		NERROR("Error 0x%08x has occurred while spinning up the XAudio2 device.", true, hr);
		return StartFailed;
	}
	LOG("Source voice started.", true, xaudDev);

	devChanged = false;
	bufReadHead = 0;
	bufWriteHead = 0;

	return Success;
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
		while (Init(nullptr, flagsS, sampleRate, samplesPerFrame, nChks))
			Funcs.uSleep(50000);

		devChanged = false;
	}

	for (;;) {
		sourceVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (voiceState.BuffersQueued < samplesPerFrame) break;
		else {
			const DWORD sleepms = (samplesPerFrame / nCh) * samplesPerFrame * 1000 / sampleRate;
			if (WaitForSingleObject(bufNotifier.hBufferEndEvent, sleepms) == WAIT_TIMEOUT)
			{
				devChanged = true;
				return Success;
			}
		}
	}

	audBuf.AudioBytes = len * bitDepth;
	audBuf.pAudioData = (BYTE*)buf;
	audBuf.pContext = this;

	bool success = sourceVoice->SubmitSourceBuffer(&audBuf) == S_OK;
	if (success)
		xaudDev->CommitChanges(XAUDIO2_COMMIT_NOW);

	audBuf = { 0 };
	return success ? Success : Fail;
}

void XAudio2Output::OnDeviceChanged() {
	devChanged = true;
}

SoundOut* CreateXAudio2Output()
{
	return new XAudio2Output;
}
#endif