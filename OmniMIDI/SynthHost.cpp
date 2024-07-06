/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#include "SynthHost.hpp"

#ifdef _WIN32 
bool OmniMIDI::SynthHost::SpInit() {
	if (StreamPlayer != nullptr)
		SpFree();

	StreamPlayer = new OmniMIDI::CookedPlayer(Synth, DrvCallback);
	LOG(SHErr, "StreamPlayer allocated.");

	if (!StreamPlayer)
	{
		SpFree();
		return false;
	}

	LOG(SHErr, "StreamPlayer address: %x", StreamPlayer);
	return true;
}

bool OmniMIDI::SynthHost::SpFree() {
	if (StreamPlayer != nullptr) {
		if (StreamPlayer->EmptyQueue()) {
			StreamPlayer->Stop();
			delete StreamPlayer;
			StreamPlayer = nullptr;
		}
		else return false;
	}

	return true;
}
#endif

void OmniMIDI::SynthHost::HostHealthCheck() {
	wchar_t* confPath = SHSettings->GetConfigPath();

	if (confPath) {
		auto ftime = std::filesystem::last_write_time(confPath);
		auto chktime = ftime;

		while (!Synth->IsSynthInitialized());

		while (Synth->IsSynthInitialized() && Synth->SynthID() != EMPTYMODULE) {
			ftime = std::filesystem::last_write_time(confPath);

			if (chktime != ftime) {			
				if (Stop(true)) {
					Start();
					confPath = SHSettings->GetConfigPath();
					while (!Synth->IsSynthInitialized() && Synth->SynthID() != EMPTYMODULE);
					chktime = std::filesystem::last_write_time(confPath);
				}
			}
		}
	}
}

void OmniMIDI::SynthHost::RefreshSettings() {
	LOG(SHErr, "Refreshing synth host settings...");

	auto tSHSettings = new OmniMIDI::SHSettings;
	auto oSHSettings = SHSettings;

	SHSettings = tSHSettings;
	delete oSHSettings;
	oSHSettings = nullptr;

	LOG(SHErr, "Settings refreshed! (renderer %d, kdmapi %d, crender %s)", SHSettings->GetRenderer(), SHSettings->IsKDMAPIEnabled(), SHSettings->GetCustomRenderer());
}

bool OmniMIDI::SynthHost::Start(bool StreamPlayer) {
	RefreshSettings();

	auto tSynth = GetSynth();
	auto oSynth = Synth;

	if (tSynth) {
		if (tSynth->LoadSynthModule()) {
			tSynth->SetInstance(hwndMod);

			if (tSynth->StartSynthModule()) {
#ifdef _WIN32 
				if (StreamPlayer) {
					if (!SpInit())
						return false;
				}
#endif

				if (!_HealthThread.joinable()) 
					_HealthThread = std::jthread(&SynthHost::HostHealthCheck, this);

				if (_HealthThread.joinable()) {
					Synth = tSynth;
					delete oSynth;
					return true;
				}
				else NERROR(SHErr, "_HealthThread failed. (ID: %x)", true, _HealthThread.get_id());

				if (!tSynth->StopSynthModule())
					FNERROR(SHErr, "StopSynthModule() failed!!!");
			}
			else NERROR(SHErr, "StartSynthModule() failed! The driver will not output audio.", true);
		}
		else NERROR(SHErr, "LoadSynthModule() failed! The driver will not output audio.", true);
	}

	if (!tSynth->UnloadSynthModule())
		FNERROR(SHErr, "UnloadSynthModule() failed!!!");

	delete tSynth;

	return false;
}

bool OmniMIDI::SynthHost::Stop(bool restart) {
	auto tSynth = new SynthModule;
	auto oSynth = Synth;

	Synth = tSynth;

#ifdef _WIN32 
	SpFree();
#endif

	if (!oSynth->StopSynthModule()) 
		FNERROR(SHErr, "StopSynthModule() failed!!!");

	if (!oSynth->UnloadSynthModule())
		FNERROR(SHErr, "UnloadSynthModule() failed!!!");

	delete oSynth;

	if (!restart) {
		if (DrvCallback->IsCallbackReady()) {
			DrvCallback->CallbackFunction(MOM_CLOSE, 0, 0);
			DrvCallback->ClearCallbackFunction();
			LOG(SHErr, "Callback system has been freed.");
		}

		if (_HealthThread.joinable())
			_HealthThread.join();
	}

	return true;
}

OmniMIDI::SynthModule* OmniMIDI::SynthHost::GetSynth() {
	SynthModule* tSynth;

	char r = SHSettings->GetRenderer();
	switch (r) {
	case Synthesizers::External:
		extModule = loadLib(SHSettings->GetCustomRenderer());

		if (extModule) {
			auto iM = reinterpret_cast<rInitModule>(getAddr(extModule, "initModule"));

			if (iM) {
				tSynth = iM();

				if (tSynth) {
					LOG(SHErr, "R%d (EXTERNAL >> %s)",
						r,
						SHSettings->GetCustomRenderer());
					break;
				}
			}
		}

		tSynth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The requested external module (%s) could not be loaded.", SHSettings->GetCustomRenderer());
		break;

	case Synthesizers::TinySoundFont:
		tSynth = new OmniMIDI::TinySFSynth;
		LOG(SHErr, "R%d (TINYSF)", r);
		break;

	case Synthesizers::FluidSynth:
		tSynth = new OmniMIDI::FluidSynth;
		LOG(SHErr, "R%d (FLUIDSYNTH)", r);
		break;

#if !defined _M_ARM
	case Synthesizers::BASSMIDI:
		tSynth = new OmniMIDI::BASSSynth;
		LOG(SHErr, "R%d (BASSMIDI)", r);
		break;
#endif

#if defined _M_AMD64
	case Synthesizers::XSynth:
		tSynth = new OmniMIDI::XSynth;
		LOG(SHErr, "R%d (XSYNTH)", r);
		break;
#endif

#if !defined _M_ARM || !defined _M_ARM64
	case Synthesizers::ksynth:
		tSynth = new OmniMIDI::KSynthM;
		LOG(SHErr, "R%d (KSYNTH)", r);
		break;
#endif

#if defined WIN32 
	case Synthesizers::ShakraPipe:
		tSynth = new OmniMIDI::ShakraPipe;
		LOG(SHErr, "R%d (SHAKRA)", r);
		break;
#endif

	default:
		tSynth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The chosen synthesizer (Syn%d) is not available on this platform, or does not exist.", false, r);
		break;
	}

	return tSynth;
}

void OmniMIDI::SynthHost::PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) {
	Synth->PlayShortEvent(status, param1, param2);
}

OmniMIDI::SynthResult OmniMIDI::SynthHost::PlayLongEvent(char* ev, unsigned int size) {
	if (!Synth->IsSynthInitialized())
		return NotInitialized;

	if (size < 4)
		return InvalidBuffer;

	for (int i = 0, n = size - 1; i < n; ) {
		switch (ev[i] & 0xF0) {
		case SystemMessageStart:
			switch (ev[i + 1]) {
				// GM, GS and XG should go here
			case 0x7E:
			case 0x41:
			case 0x43: {
				return Synth->PlayLongEvent(ev, size);
			}

			default:
				break;
			}

			i += 5;
			continue;

		case SystemMessageEnd:
			break;

		default:
			Synth->PlayShortEvent(ev[i], ev[i + 1], ev[i + 2]);
			i += 3;
			continue;
		}
	}

	return Ok;
}