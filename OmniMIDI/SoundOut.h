#ifndef _SOUNDOUT_H_
#define _SOUNDOUT_H_

#include <Windows.h>

enum SoundOutResult {
	Fail = -1,
	Success,
	StreamOpenFailed,
	MasteringVoiceFailed,
	SourceVoiceFailed,
	StartFailed,
	NoBASSPointer
};

enum SOAudioFlags {
	Default = 0,
	ShortData = 1 << 0,
	TwentyFourData = 1 << 2,
	FloatData = 1 << 3,
	MonoAudio = 1 << 4,
	StereoAudio = 1 << 5
};
DEFINE_ENUM_FLAG_OPERATORS(SOAudioFlags);

class SoundOutVirtualWindow
{
	HWND m_hWnd = nullptr;

public:
	SoundOutVirtualWindow(HMODULE hModule) {
		static const char* class_name = "OMV2_HWND";
		WNDCLASSEXA wx = {};
		wx.cbSize = sizeof(WNDCLASSEX);
		wx.lpfnWndProc = DefWindowProcA;        // function which will handle messages
		wx.hInstance = hModule;
		wx.lpszClassName = class_name;
		if (RegisterClassExA(&wx)) {
			m_hWnd = CreateWindowExA(0, class_name, "om dummy", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
		}
	}

	~SoundOutVirtualWindow()
	{
		if (IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
	}

	HWND GetHWND() const { return m_hWnd; }
};

class SoundOut
{
public:
	SoundOutVirtualWindow* MsgWnd = nullptr;

	virtual ~SoundOut() {}
	virtual SoundOutResult Init(HMODULE m_hModule, SOAudioFlags flags, unsigned int sample_rate, unsigned spf, unsigned chks) = 0;
	virtual SoundOutResult Stop() = 0;
	virtual SoundOutResult Update(void* buf, size_t len) = 0;
};

SoundOut* CreateXAudio2Output();

#endif