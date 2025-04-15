/*

	OmniMIDI v15+ (Rewrite) for Win32/Linux

	This file contains the required code to run the driver under both Windows and Linux

*/

#ifndef _PLUGINSYNTH_H
#define _PLUGINSYNTH_H

#pragma once

#define OMV2_ENTRY		"OMv2_PluginEntryPoint"

#include "SynthModule.hpp"

struct PluginFuncs {
	bool (WINAPI* InitPlugin)() = nullptr;
	bool (WINAPI* StopPlugin)() = nullptr;
	void (WINAPI* ShortData)(unsigned int ev) = nullptr;
	unsigned int (WINAPI* LongData)(char* data, unsigned int len) = nullptr;
	void (WINAPI* Reset)() = nullptr;
	unsigned long long (WINAPI* ActiveVoices)() = nullptr;
	float (WINAPI* RenderingTime)() = nullptr;
};

typedef PluginFuncs* (*OMv2PEP)();

namespace OmniMIDI {
	class PluginSynth : public SynthModule {
	protected:
		std::jthread _EvtThread;

		bool Init = false;
		Lib* Plugin = nullptr;
		OMv2PEP _PluginEntryPoint = nullptr;
		PluginFuncs* _PluginFuncs = nullptr;

	public:
		PluginSynth(const char* pluginName, ErrorSystem::Logger* PErr);
		virtual bool LoadSynthModule();
		virtual bool UnloadSynthModule();
		virtual bool StartSynthModule();
		virtual bool StopSynthModule();
		virtual bool IsSynthInitialized() { return Init; }
		virtual unsigned int SynthID() { return 0x504C474E; }

		// Event handling system
		virtual void PlayShortEvent(unsigned int ev) {
			if (!_PluginFuncs)
				return;

			UPlayShortEvent(ev);
		}
		virtual void PlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) {
			if (!_PluginFuncs)
				return;

			UPlayShortEvent(status, param1, param2);
		}
		virtual void UPlayShortEvent(unsigned int ev) { _PluginFuncs->ShortData(ev); }
		virtual void UPlayShortEvent(unsigned char status, unsigned char param1, unsigned char param2) { _PluginFuncs->ShortData(status | (param1 << 8) | (param2 << 16)); }

		virtual unsigned int PlayLongEvent(char* ev, unsigned int size) {
			if (!_PluginFuncs)
				return 0; 

			return UPlayLongEvent(ev, size);
		}
		virtual unsigned int UPlayLongEvent(char* ev, unsigned int size) { return _PluginFuncs->LongData(ev, size); }

		virtual SynthResult Reset(char type = 0) { _PluginFuncs->ShortData(0xFF | (type << 8)); return Ok; }
	};
}

#endif