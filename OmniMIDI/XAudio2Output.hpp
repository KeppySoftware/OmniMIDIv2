/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#ifndef _XAUDIO2_OUTPUT_H
#define _XAUDIO2_OUTPUT_H

#include "SoundOut.h"
#include "Utils.hpp"
#include "ErrSys.hpp"
#include <Windows.h>
#include <XAudio2.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#ifdef HAVE_KS_HEADERS
#include <ks.h>
#include <ksmedia.h>
#endif

typedef NTSTATUS(NTAPI* XA2C)(_Outptr_ IXAudio2** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor);

class XAudio2Output : public SoundOut
{
	class XAudio2DevNotifier : public IMMNotificationClient
	{
		volatile LONG registered;
		IMMDeviceEnumerator* pEnumerator;

		CRITICAL_SECTION lock;
		std::vector<XAudio2Output*> instances;

	public:
		XAudio2DevNotifier() : registered(0)
		{
			InitializeCriticalSection(&lock);
		}
		~XAudio2DevNotifier()
		{
			DeleteCriticalSection(&lock);
		}

		ULONG STDMETHODCALLTYPE AddRef()
		{
			return 1;
		}

		ULONG STDMETHODCALLTYPE Release()
		{
			return 1;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
		{
			if (IID_IUnknown == riid)
			{
				*ppvInterface = (IUnknown*)this;
			}
			else if (__uuidof(IMMNotificationClient) == riid)
			{
				*ppvInterface = (IMMNotificationClient*)this;
			}
			else
			{
				*ppvInterface = NULL;
				return E_NOINTERFACE;
			}
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
		{
			if (flow == eRender)
			{
				EnterCriticalSection(&lock);
				for (std::vector<XAudio2Output*>::iterator it = instances.begin(); it < instances.end(); ++it)
				{
					DeviceChanged(*it);
				}
				LeaveCriticalSection(&lock);
			}

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
		HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
		HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
		HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

		void Register(XAudio2Output* p_instance)
		{
			if (InterlockedIncrement(&registered) == 1)
			{
				pEnumerator = NULL;
				HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
				if (SUCCEEDED(hr))
				{
					pEnumerator->RegisterEndpointNotificationCallback(this);
					registered = true;
				}
			}

			EnterCriticalSection(&lock);
			instances.push_back(p_instance);
			LeaveCriticalSection(&lock);
		}

		void Unregister(XAudio2Output* p_instance)
		{
			if (InterlockedDecrement(&registered) == 0)
			{
				if (pEnumerator)
				{
					pEnumerator->UnregisterEndpointNotificationCallback(this);
					pEnumerator->Release();
					pEnumerator = NULL;
				}
				registered = false;
			}

			EnterCriticalSection(&lock);
			for (std::vector<XAudio2Output*>::iterator it = instances.begin(); it < instances.end(); ++it)
			{
				if (*it == p_instance)
				{
					instances.erase(it);
					break;
				}
			}
			LeaveCriticalSection(&lock);
		}

		void DeviceChanged(XAudio2Output* p_instance)
		{
			p_instance->OnDeviceChanged();
		}
	} devNotifier;

	class XAudio2BufNotify : public IXAudio2VoiceCallback
	{
	public:
		HANDLE hBufferEndEvent;

		XAudio2BufNotify() {
			hBufferEndEvent = NULL;
			hBufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		}

		~XAudio2BufNotify() {
			CloseHandle(hBufferEndEvent);
			hBufferEndEvent = NULL;
		}

		STDMETHOD_(void, OnBufferEnd) (void* pBufferContext) {
			SetEvent(hBufferEndEvent);
		}

		STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32 BytesRequired) {}
		STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
		STDMETHOD_(void, OnStreamEnd) () {}
		STDMETHOD_(void, OnBufferStart) (void* pBufferContext) {}
		STDMETHOD_(void, OnLoopEnd) (void* pBufferContext) {}
		STDMETHOD_(void, OnVoiceError) (void* pBufferContext, HRESULT Error) {};
	} bufNotifier;

private:
	volatile bool			devChanged = false;
	unsigned				sampleRate = 0, maxSamplesPerFrame = 0, samplesPerFrame = 0;
	unsigned short			bitDepth = 0;
	unsigned short			nCh = 0;
	unsigned short			nChks = 0;
	unsigned long long		bufReadHead = 0, bufWriteHead = 0;

	void*					audioBuf = 0;

	SOAudioFlags			flagsS = (SOAudioFlags)0;

	unsigned char*			sampleBuffer = 0;
	unsigned long long*		samplesInBuffer = 0;

	HMODULE					XALib = nullptr;
	OMShared::Funcs			Funcs;
	OMShared::SysPath		Utils;
	XA2C					xa2Create = nullptr;
	IXAudio2*				xaudDev = nullptr;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	IXAudio2SourceVoice*	sourceVoice = nullptr;
	XAUDIO2_VOICE_STATE		voiceState = { 0 };
	ErrorSystem::Logger		XAErr;

public:
	XAudio2Output();
	~XAudio2Output();
	SoundOutResult Init(HMODULE m_hModule, SOAudioFlags flags, unsigned sample_rate, unsigned spf, unsigned sr, unsigned chks);
	SoundOutResult Stop();
	SoundOutResult Update(void* buf, size_t len);
	void OnDeviceChanged();
};

#endif
#endif