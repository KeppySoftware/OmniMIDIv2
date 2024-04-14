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

// Cooked player, let it cook...
#include "StreamPlayer.hpp"

// Synthesizers
#include "BASSSynth.hpp"
#include "FluidSynth.hpp"
#include "XSynthM.hpp"
#include "TSFSynth.hpp"
#include "KSynthM.hpp"

typedef OmniMIDI::SynthModule* (*rInitModule)();
typedef void(*rStopModule)();

namespace OmniMIDI {
	class SynthHost
	{
	private:
		// From driver lib
		SHSettings* SHSettings = nullptr;
		WinDriver::DriverCallback* DrvCallback = nullptr;

		// Ours
		ErrorSystem::Logger SHErr;
		SynthModule* Synth = nullptr;
		StreamPlayer* StreamPlayer = nullptr;
		void* extModule = nullptr;

	public:
		SynthHost(WinDriver::DriverCallback* dsrc) {
			DrvCallback = dsrc;
			SHSettings = new OmniMIDI::SHSettings;
			Synth = new OmniMIDI::SynthModule;
			StreamPlayer = new OmniMIDI::StreamPlayer(nullptr, DrvCallback);
		}

		~SynthHost() {
			Synth->StopSynthModule();
			Synth->UnloadSynthModule();

			if (StreamPlayer != nullptr)
				delete StreamPlayer;

			if (Synth != nullptr)
				delete Synth;

			if (SHSettings != nullptr)
				delete SHSettings;
		}

		void RefreshSettings();
		bool Start(bool StreamPlayer = false);
		bool Stop();
		void Get();
		void Free();
		bool IsKDMAPIAvailable() { return SHSettings->KDMAPIEnabled; }

#ifdef _WIN32 
		bool IsStreamPlayerAvailable() { return !StreamPlayer->IsDummy(); }
		bool IsBlacklistedProcess() { return SHSettings->IsBlacklistedProcess(); }

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

		// Event handling system
		void PlayShortEvent(unsigned int ev) { Synth->PlayShortEvent(ev); }
		void UPlayShortEvent(unsigned int ev) { Synth->UPlayShortEvent(ev); }
		SynthResult PlayLongEvent(char* ev, unsigned int size) { return Synth->PlayLongEvent(ev, size); }
		SynthResult UPlayLongEvent(char* ev, unsigned int size) { return Synth->UPlayLongEvent(ev, size); }
		SynthResult Reset() { return Synth->Reset(); }
		SynthResult TalkToSynthDirectly(unsigned int evt, unsigned int chan, unsigned int param) { return Synth->TalkToSynthDirectly(evt, chan, param); }
		bool SettingsManager(unsigned int setting, bool get, void* var, size_t size) { return Synth->SettingsManager(setting, get, var, size); }
	};
}

#endif