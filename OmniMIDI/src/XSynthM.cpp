/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "XSynthM.hpp"

#ifdef _XSYNTHM_H

void OmniMIDI::XSynth::XSynthThread() {
	while (!IsSynthInitialized())
		Utils.MicroSleep(-1);

	while (IsSynthInitialized()) {
		auto data = XSynth_Realtime_GetStats(realtimeSynth);

		RenderingTime = data.render_time * 100.0f;
		ActiveVoices = data.voice_count;

		Utils.MicroSleep(-10000);
	}
}

bool OmniMIDI::XSynth::IsSynthInitialized() {
	if (!XLib)
		return false;

	return Running;
}

bool OmniMIDI::XSynth::LoadSynthModule() {
	auto ptr = (LibImport*)xLibImp;

	if (!Settings) {
		Settings = new XSynthSettings(ErrLog);
		Settings->LoadSynthConfig();
	}

	if (!XLib)
		XLib = new Lib("xsynth", nullptr, ErrLog, &ptr, xLibImpLen);

	if (XLib->IsOnline())
		return true;

	if (!XLib->LoadLib())
		return false;

	return true;
}

bool OmniMIDI::XSynth::UnloadSynthModule() {
	if (!XLib)
		return true;

	if (!XLib->UnloadLib())
		return false;

	return true;
}

bool OmniMIDI::XSynth::StartSynthModule() {
	if (IsSynthInitialized())
		return true;

	if (!Settings)
		return false;

	realtimeConf = XSynth_GenDefault_RealtimeConfig();

	realtimeConf.fade_out_killing = Settings->FadeOutKilling;
	realtimeConf.render_window_ms = Settings->RenderWindow;
	realtimeConf.multithreading = Settings->ThreadsCount;

	realtimeSynth = XSynth_Realtime_Create(realtimeConf);

	if (realtimeSynth.synth) {
		XSynth_Realtime_SendConfigEventAll(realtimeSynth, XSYNTH_CONFIG_SETLAYERS, Settings->LayerCount);

		LoadSoundFonts();
		SFSystem.RegisterCallback(this);

		_XSyThread = std::jthread(&XSynth::XSynthThread, this);
		if (!_XSyThread.joinable()) {
			Error("_XSyThread failed. (ID: %x)", true, _XSyThread.get_id());
			return false;
		}

		if (Settings->IsDebugMode()) {
			_LogThread = std::jthread(&SynthModule::LogFunc, this);
			if (!_LogThread.joinable()) {
				Error("_LogThread failed. (ID: %x)", true, _LogThread.get_id());
				return false;
			}
			else Message("_LogThread running! (ID: %x)", _LogThread.get_id());

			Settings->OpenConsole();
		}

		Running = true;
	}

	return Running;
}

bool OmniMIDI::XSynth::StopSynthModule() {
	SFSystem.RegisterCallback();

	if (IsSynthInitialized()) {
		Running = false;
		XSynth_Realtime_ClearSoundfonts(realtimeSynth);
		XSynth_Realtime_Drop(realtimeSynth);
	}

	if (_XSyThread.joinable())
		_XSyThread.join();

	if (_LogThread.joinable())
		_LogThread.join();

	if (Settings->IsDebugMode() && Settings->IsOwnConsole())
		Settings->CloseConsole();

	return true;
}

void OmniMIDI::XSynth::LoadSoundFonts() {
	if (SoundFonts.size() > 0)
		SoundFonts.clear();

	if (SFSystem.ClearList()) {
		SoundFontsVector = SFSystem.LoadList();

		if (SoundFontsVector != nullptr) {
			auto& dSFv = *SoundFontsVector;
			auto sf = XSynth_GenDefault_SoundfontOptions();
			auto realtimeParams = XSynth_Realtime_GetStreamParams(realtimeSynth);
			
			if (dSFv.size() > 0) {
				for (int i = 0; i < dSFv.size(); i++) {
					auto item = dSFv[i];
					auto envOptions = XSynth_EnvelopeOptions { item.linattmod, item.lindecvol, item.lindecvol };
					auto sfPath = item.path.c_str();

					if (!item.enabled)
						continue;

					sf.stream_params.audio_channels = realtimeParams.audio_channels;
					sf.stream_params.sample_rate = realtimeParams.sample_rate;
					sf.preset = item.spreset;
					sf.bank = item.sbank;
					sf.interpolator = XSYNTH_INTERPOLATION_LINEAR;						// << Always linear, to avoid audio glitches
					sf.vol_envelope_options = envOptions;
					sf.use_effects = item.minfx;

					auto sfHandle = XSynth_Soundfont_LoadNew(sfPath, sf);

					if (!sfHandle.soundfont) {
						Error("An error has occurred while loading the SoundFont \"%s\".", false, sfPath);
						continue;
					}

					SoundFonts.push_back(sfHandle);
				}
			}
			
			if (SoundFonts.size() > 0) {
				XSynth_Realtime_ClearSoundfonts(realtimeSynth);	
				XSynth_Realtime_SetSoundfonts(realtimeSynth, &SoundFonts[0], SoundFonts.size());
			}			
		}
	}

}

void OmniMIDI::XSynth::PlayShortEvent(unsigned int ev) {
	if (!XLib->IsOnline() || !IsSynthInitialized())
		return;

	UPlayShortEvent(ev);
}

void OmniMIDI::XSynth::UPlayShortEvent(unsigned int ev) {
	XSynth_Realtime_SendEventU32(realtimeSynth, ev);
}

#endif