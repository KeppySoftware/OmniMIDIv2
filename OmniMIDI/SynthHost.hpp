/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _SYNTHHOST_H
#define _SYNTHHOST_H

#include "ErrSys.hpp"
#include "SynthMain.hpp"
#include "SHSettings.hpp"

#ifdef _WIN32
// Cooked player, let it cook...
#include "StreamPlayer.hpp"
#endif

// Synthesizers
#include "BASSSynth.hpp"
#include "FluidSynth.hpp"
#include "XSynthM.hpp"
#include "TSFSynth.hpp"
#include "ShakraPipe.hpp"

typedef OmniMIDI::SynthModule* (*rInitModule)();
typedef void(*rStopModule)();

namespace OmniMIDI {
	class SynthHost
	{
	private:
		// Ours
		ErrorSystem::Logger* ErrLog = nullptr;
		SynthModule* Synth = nullptr;
		SHSettings* _SHSettings = nullptr;
		std::jthread _HealthThread;
		void* extModule = nullptr;

#ifdef _WIN32
		// From driver lib
		HMODULE hwndMod = nullptr;
		StreamPlayer* StreamPlayer = nullptr;
		WinDriver::DriverCallback* DrvCallback = nullptr;
#endif

	public:
#ifdef _WIN32
		SynthHost(WinDriver::DriverCallback* dcasrc, HMODULE mod, ErrorSystem::Logger* PErr) {
			DrvCallback = dcasrc;
			hwndMod = mod;
			_SHSettings = new OmniMIDI::SHSettings(ErrLog);
			Synth = new OmniMIDI::SynthModule(ErrLog);
			StreamPlayer = new OmniMIDI::StreamPlayer(nullptr, DrvCallback, ErrLog);
			ErrLog = PErr;
		}
#else
		SynthHost(ErrorSystem::Logger* PErr) {
			_SHSettings = new OmniMIDI::SHSettings(ErrLog);
			Synth = new OmniMIDI::SynthModule(ErrLog);
			ErrLog = PErr;
		}
#endif

		~SynthHost() {
			Synth->StopSynthModule();
			Synth->UnloadSynthModule();

#ifdef _WIN32
			if (StreamPlayer != nullptr)
				delete StreamPlayer;
#endif

			if (Synth != nullptr)
				delete Synth;

			if (_SHSettings != nullptr)
				delete _SHSettings;
		}

		void RefreshSettings();
		bool Start(bool StreamPlayer = false);
		bool Stop(bool restart = false);
		OmniMIDI::SynthModule* GetSynth();
		bool IsKDMAPIAvailable() { return _SHSettings->IsKDMAPIEnabled(); }

#ifdef _WIN32 
		bool IsStreamPlayerAvailable() { return !StreamPlayer->IsDummy(); }
		bool IsBlacklistedProcess() { return _SHSettings->IsBlacklistedProcess(); }

		// Cooked player system
		bool SpInit();
		bool SpFree();
		void SpStart() { StreamPlayer->Start(); }
		void SpStop() { StreamPlayer->Stop(); }
		bool SpEmptyQueue() { return StreamPlayer->EmptyQueue(); }
		bool SpResetQueue() { return StreamPlayer->ResetQueue(); }
		bool SpAddToQueue(MIDIHDR* mhdr) { return StreamPlayer->AddToQueue(mhdr); }
		void SpSetTempo(unsigned int ntempo) { StreamPlayer->SetTempo(ntempo); }
		unsigned int SpGetTempo() { return StreamPlayer->GetTempo(); }
		void SpSetTicksPerQN(unsigned int nTimeDiv) { StreamPlayer->SetTicksPerQN(nTimeDiv); }
		unsigned int SpGetTicksPerQN() { return StreamPlayer->GetTicksPerQN(); }
		void SpGetPosition(MMTIME* mmtime) { StreamPlayer->GetPosition(mmtime); }
#endif

		void HostHealthCheck();

		// Event handling system
		void PlayShortEvent(unsigned int ev);
		void PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2);
		float GetRenderingTime();
		unsigned int GetActiveVoices();
		SynthResult PlayLongEvent(char* ev, unsigned int size);
		SynthResult Reset() { return Synth->Reset(); }
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Synth->TalkToSynthDirectly(evt, chan, param); }
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return Synth->SettingsManager(setting, get, var, size); }
	};
}

#endif