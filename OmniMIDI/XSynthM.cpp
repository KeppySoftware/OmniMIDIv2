/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifndef _M_ARM

#include "XSynthM.hpp"

bool OmniMIDI::XSynth::LoadSynthModule() {
	auto ptr = (LibImport**)&xLibImp;

	if (!Settings)
		Settings = new XSynthSettings;

	if (!XLib)
		XLib = new Lib(L"xsynth", ptr, xLibImpLen);

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
	if (Running)
		return true;

	if (!Settings)
		return false;

	if (StartModule(Settings->BufSize))
	{
		std::vector<OmniMIDI::SoundFont>* SFv = SFSystem.LoadList();
		if (SFv != nullptr) {
			std::vector<SoundFont>& dSFv = *SFv;

			for (int i = 0; i < SFv->size(); i++) {
				LoadSoundFont(dSFv[i].path.c_str());
				break;
			}
		}

		Running = true;
		return true;
	}

	return false;
}

bool OmniMIDI::XSynth::StopSynthModule() {
	if (StopModule()) {
		// FIXME: Waiting for Arduano to fix a ghost thread
		Sleep(4000);
		return true;
	}

	return false;
}

void OmniMIDI::XSynth::PlayShortEvent(unsigned int ev) {
	if (!XLib->IsOnline())
		return;

	UPlayShortEvent(ev);
}

void OmniMIDI::XSynth::UPlayShortEvent(unsigned int ev) {
	SendData(ev);
}

OmniMIDI::SynthResult OmniMIDI::XSynth::PlayLongEvent(char* ev, unsigned int size) {
	unsigned char tLRS = 0;
	unsigned int tev = 0;

	for (unsigned int i = 0; i < size; i++) {
		if ((ev[i] - 0xC0) & 0xE0) 
			tLRS = ev[i] & 0xFF;
		
		tev = tLRS | (ev[i + 1] & 0xFF) << 8 | (ev[i + 2] & 0xFF) << 16;
		UPlayShortEvent(tev);
	}
	return Ok;
}

OmniMIDI::SynthResult OmniMIDI::XSynth::UPlayLongEvent(char* ev, unsigned int size) {
	return Ok;
}

#endif