/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _WINSYNTHPIPE_H
#define _WINSYNTHPIPE_H

#ifdef _WIN32

#include "ErrSys.hpp"
#include "EvBuf_t.hpp"
#include "SynthMain.hpp"
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#define WSP_STR "WinSynthPipe"

namespace OmniMIDI {
	class WSPSettings : public OMSettings {
	public:
		// Global settings
		unsigned int EvBufSize = 32768;

		WSPSettings(ErrorSystem::Logger* PErr) : OMSettings(PErr) {}

		void RewriteSynthConfig() {
			CloseConfig();
			if (InitConfig(true, WSP_STR, sizeof(WSP_STR))) {
				nlohmann::json DefConfig = {
					{
						ConfGetVal(EvBufSize)
					}
				};

				if (AppendToConfig(DefConfig))
					WriteConfig();
			}

			CloseConfig();
			InitConfig(false, WSP_STR, sizeof(WSP_STR));
		}

		// Here you can load your own JSON, it will be tied to ChangeSetting()
		void LoadSynthConfig() {
			if (InitConfig(false, WSP_STR, sizeof(WSP_STR))) {
				SynthSetVal(unsigned int, EvBufSize);
				return;
			}

			if (IsConfigOpen() && !IsSynthConfigValid()) {
				RewriteSynthConfig();
			}
		}
	};

	class ShakraPipe : public SynthModule {
	private:
		// Pipe names
		const wchar_t* FileMappingTemplate = L"Local\\Shakra%sBuf%s";

		// R/W heads
		WSPSettings* Settings = nullptr;
		EvBuf* ShortEvents = nullptr;
		LEvBuf* LongEvents = nullptr;
		HANDLE PShortEvents = nullptr, PLongEvents = nullptr;

	public:
		ShakraPipe(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		bool LoadSynthModule() { return true; }
		bool UnloadSynthModule() { return true; }
		bool StartSynthModule();
		bool StopSynthModule();
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return false; }
		unsigned int GetSampleRate() { return 48000; }
		bool IsSynthInitialized() { return !PShortEvents && !PLongEvents; }
		int SynthID() { return 0x12345678; }
		std::wstring GenerateID();

		unsigned int PlayLongEvent(char* ev, unsigned int size);
		unsigned int UPlayLongEvent(char* ev, unsigned int size);

		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return NotSupported; }
	};
}

#endif
#endif