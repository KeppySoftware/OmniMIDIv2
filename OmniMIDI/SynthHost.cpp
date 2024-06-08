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

		while (Synth->IsSynthInitialized()) {
			ftime = std::filesystem::last_write_time(confPath);

			if (chktime != ftime) {
				chktime = ftime;

				if (Stop(true))
				{
					Start();
					confPath = SHSettings->GetConfigPath();
					while (!Synth->IsSynthInitialized());
				}
			}
		}
	}
}

void OmniMIDI::SynthHost::RefreshSettings() {
	LOG(SHErr, "Refreshing synth host settings...");

	if (SHSettings != nullptr)
	{
		delete SHSettings;
		SHSettings = nullptr;
		LOG(SHErr, "SHSettings freed.");
	}

	SHSettings = new OmniMIDI::SHSettings;
	LOG(SHErr, "Settings refreshed! (renderer %d, kdmapi %d, crender %s)", SHSettings->GetRenderer(), SHSettings->IsKDMAPIEnabled(), SHSettings->GetCustomRenderer());
}

bool OmniMIDI::SynthHost::Start(bool StreamPlayer) {
	Get();

	if (Synth->LoadSynthModule()) {
#ifdef _WIN32
		Synth->SetInstance(hwndMod);
#endif

		if (Synth->StartSynthModule()) {
			LOG(SHErr, "Synth ID: %x", Synth->SynthID());

#ifdef _WIN32 
			if (StreamPlayer) {
				if (!SpInit())
					return false;
			}	
#endif

			if (!_HealthThread.joinable()) {
				_HealthThread = std::jthread(&SynthHost::HostHealthCheck, this);
				if (!_HealthThread.joinable()) {
					NERROR(SHErr, "_HealthThread failed. (ID: %x)", true, _HealthThread.get_id());
					return false;
				}
			}

			return true;
		}
		else NERROR(SHErr, "StartSynthModule() failed! The driver will not output audio.", true);

		if (!Synth->StopSynthModule())
			FNERROR(SHErr, "StopSynthModule() failed!!!");
	}

	if (!Synth->UnloadSynthModule())
		FNERROR(SHErr, "UnloadSynthModule() failed!!!");

	Free();
	return false;
}

bool OmniMIDI::SynthHost::Stop(bool restart) {
#ifdef _WIN32 
	SpFree();
#endif

	if (Synth->SynthID() != EMPTYMODULE)
	{
		if (Synth->StopSynthModule()) {
			LOG(SHErr, "StopSynthModule() called.");

			if (Synth->UnloadSynthModule())
				LOG(SHErr, "UnloadSynthModule() called.");
			else return false;
		}
		else return false;
	}

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

void OmniMIDI::SynthHost::Get() {
	Free();
	RefreshSettings();

	char r = SHSettings->GetRenderer();
	switch (r) {
	case Synthesizers::External:
		extModule = loadLib(SHSettings->GetCustomRenderer());

		if (extModule) {
			auto iM = reinterpret_cast<rInitModule>(getAddr(extModule, "initModule"));

			if (iM) {
				Synth = iM();

				if (Synth) {
					LOG(SHErr, "R%d (EXTERNAL >> %s)",
						r,
						SHSettings->GetCustomRenderer());
					break;
				}
			}
		}
		Free();

		Synth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The requested external module (%s) could not be loaded.", SHSettings->GetCustomRenderer());
		break;

	case Synthesizers::TinySoundFont:
		Synth = new OmniMIDI::TinySFSynth;
		LOG(SHErr, "R%d (TINYSF)", r);
		break;

	case Synthesizers::FluidSynth:
		Synth = new OmniMIDI::FluidSynth;
		LOG(SHErr, "R%d (FLUIDSYNTH)", r);
		break;

#if !defined _M_ARM
	case Synthesizers::BASSMIDI:
		Synth = new OmniMIDI::BASSSynth;
		LOG(SHErr, "R%d (BASSMIDI)", r);
		break;
#endif

#if defined _M_AMD64
	case Synthesizers::XSynth:
		Synth = new OmniMIDI::XSynth;
		LOG(SHErr, "R%d (XSYNTH)", r);
		break;
#endif

#if !defined _M_ARM || !defined _M_ARM64
	case Synthesizers::ksynth:
		Synth = new OmniMIDI::KSynthM;
		LOG(SHErr, "R%d (KSYNTH)", r);
		break;
#endif

#if defined WIN32 
	case Synthesizers::ShakraPipe:
		Synth = new OmniMIDI::ShakraPipe;
		LOG(SHErr, "R%d (SHAKRA)", r);
		break;
#endif

	default:
		Synth = new OmniMIDI::SynthModule;
		NERROR(SHErr, "The chosen synthesizer (Syn%d) is not available on this platform, or does not exist.", false, r);
		break;
	}
}

void OmniMIDI::SynthHost::Free() {
	if (extModule != nullptr) {
		if (Synth) {
			auto fM = (rStopModule)getAddr(extModule, "freeModule");
			fM();
			LOG(SHErr, "Called freeModule() from custom external synth module. ADDR:%x", extModule);
		}

		freeLib(extModule);
		extModule = nullptr;
		Synth = nullptr;

		LOG(SHErr, "Custom Synth freed.");
	}

	if (Synth != nullptr)
	{
		auto tSynth = new SynthModule;
		auto oSynth = Synth;

		Synth = tSynth;

		delete oSynth;

		LOG(SHErr, "Synth freed.");
	}

	Synth = new OmniMIDI::SynthModule;
}