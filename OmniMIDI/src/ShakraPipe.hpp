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
	class WSPSettings : public SettingsModule {
	public:
		// Global settings
		unsigned int EvBufSize = 32768;

		WSPSettings(ErrorSystem::Logger* PErr) : SettingsModule(PErr) {}

		void RewriteSynthConfig() {
			nlohmann::json DefConfig = {
					ConfGetVal(EvBufSize)
			};

			if (AppendToConfig(DefConfig))
				WriteConfig();

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

		std::wstring GenerateID();

	public:
		ShakraPipe(ErrorSystem::Logger* PErr) : SynthModule(PErr) {}
		bool LoadSynthModule() override { return true; }
		bool UnloadSynthModule() override { return true; }
		bool StartSynthModule() override;
		bool StopSynthModule() override;
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) override { return false; }
		unsigned int GetSampleRate() override { return 48000; }
		bool IsSynthInitialized() override { return !PShortEvents && !PLongEvents; }
		unsigned int SynthID() override { return 0x12345678; }

		unsigned int PlayLongEvent(char* ev, unsigned int size) override;
		unsigned int UPlayLongEvent(char* ev, unsigned int size) override;

		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) override { return NotSupported; }
	};
}

#endif
#endif