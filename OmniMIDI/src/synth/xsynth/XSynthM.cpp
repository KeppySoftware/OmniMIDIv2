/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#include "XSynthM.hpp"

#ifdef _XSYNTHM_H

void OmniMIDI::XSynth::XSynthThread() {
	while (!IsSynthInitialized())
		Utils.MicroSleep(SLEEPVAL(1));

	while (IsSynthInitialized()) {
		realtimeStats = XSynth_Realtime_GetStats(realtimeSynth);

		RenderingTime = realtimeStats.render_time * 100.0f;
		ActiveVoices = realtimeStats.voice_count;

		Utils.MicroSleep(SLEEPVAL(10000));
	}
}

void OmniMIDI::XSynth::UnloadSoundfonts() {
	for (auto sf : SoundFonts) {
		XSynth_Soundfont_Remove(sf);
	}
	XSynth_Realtime_ClearSoundfonts(realtimeSynth);
	SoundFonts.clear();
}

bool OmniMIDI::XSynth::IsSynthInitialized() {
	if (!XLib)
		return false;

	return Running;
}

bool OmniMIDI::XSynth::LoadSynthModule() {
	auto ptr = (LibImport*)xLibImp;
	_XSyConfig = LoadSynthConfig<XSynthSettings>();

	if (_XSyConfig == nullptr)
		return false;

	if (!XLib)
		XLib = new Lib("xsynth", nullptr, ErrLog, &ptr, xLibImpLen);

	if (XLib->IsOnline())
		return true;

	if (!XLib->LoadLib())
		return false;

	// Check if the loaded lib is compatible with the API
	uint32_t apiVer = XSYNTH_VERSION >> 8;
	uint32_t libVer = XSynth_GetVersion() >> 8;
	if (apiVer != libVer) {
		Error("Loaded incompatible XSynth version (%d.%d.X). Please use version %d.%d.X", true,
			libVer >> 8, libVer & 15, apiVer >> 8, apiVer & 15);
		UnloadSynthModule();
		return false;
	}

	return true;
}

bool OmniMIDI::XSynth::UnloadSynthModule() {
	FreeSynthConfig(_XSyConfig);	

	if (!XLib)
		return true;

	if (!XLib->UnloadLib())
		return false;

	return true;
}

bool OmniMIDI::XSynth::StartSynthModule() {
	if (IsSynthInitialized())
		return true;

	if (!_XSyConfig)
		return false;

	realtimeConf = XSynth_GenDefault_RealtimeConfig();

	realtimeConf.fade_out_killing = _XSyConfig->FadeOutKilling;
	realtimeConf.render_window_ms = _XSyConfig->RenderWindow;
	realtimeConf.multithreading = _XSyConfig->ThreadsCount;

	realtimeSynth = XSynth_Realtime_Create(realtimeConf);

	if (realtimeSynth.synth) {
		XSynth_Realtime_SendConfigEventAll(realtimeSynth, XSYNTH_CONFIG_SETLAYERS, _XSyConfig->LayerCount);

		LoadSoundFonts();
		_sfSystem.RegisterCallback(this);

		_XSyThread = std::jthread(&XSynth::XSynthThread, this);
		if (!_XSyThread.joinable()) {
			Error("_XSyThread failed. (ID: %x)", true, _XSyThread.get_id());
			return false;
		}

		StartDebugOutput();

		Running = true;
	}

	return Running;
}

bool OmniMIDI::XSynth::StopSynthModule() {
	_sfSystem.RegisterCallback();

	if (IsSynthInitialized()) {
		Running = false;
		UnloadSoundfonts();
		XSynth_Realtime_Drop(realtimeSynth);
	}

	if (_XSyThread.joinable())
		_XSyThread.join();

	StopDebugOutput();

	return true;
}

void OmniMIDI::XSynth::LoadSoundFonts() {
	UnloadSoundfonts();

	if (_sfSystem.ClearList()) {
		_sfVec = _sfSystem.LoadList();

		if (_sfVec == nullptr)
			return;

		auto& _sfVecIter = *_sfVec;
		auto sf = XSynth_GenDefault_SoundfontOptions();
		auto realtimeParams = XSynth_Realtime_GetStreamParams(realtimeSynth);
		
		if (_sfVecIter.size() < 1)
			return;

		for (size_t i = 0; i < _sfVecIter.size(); i++) {
			auto item = _sfVecIter[i];
			auto envOptions = XSynth_EnvelopeOptions { item.linattmod, item.lindecvol, item.lindecvol };
			auto sfPath = item.path.c_str();

			if (!item.enabled)
				continue;

			sf.stream_params.audio_channels = realtimeParams.audio_channels;
			sf.stream_params.sample_rate = realtimeParams.sample_rate;
			sf.preset = item.spreset;
			sf.bank = item.sbank;
			sf.interpolator = _XSyConfig->Interpolation;
			sf.vol_envelope_options = envOptions;
			sf.use_effects = item.minfx;

			auto sfHandle = XSynth_Soundfont_LoadNew(sfPath, sf);

			if (!sfHandle.soundfont) {
				Error("An error has occurred while loading the SoundFont \"%s\".", false, sfPath);
				continue;
			}

			SoundFonts.push_back(sfHandle);
		}
		
		if (SoundFonts.size() > 0) {
			XSynth_Realtime_SetSoundfonts(realtimeSynth, &SoundFonts[0], SoundFonts.size());
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