/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#ifndef _COOKEDPLAYER_H
#define _COOKEDPLAYER_H

#pragma once
#define MAX_WAIT 10000

#include <windows.h>
#include <mmeapi.h>
#include "WDMDrv.hpp"
#include "../ErrSys.hpp"
#include "../synth/SynthModule.hpp"

namespace OmniMIDI {
	class StreamPlayer {
	protected:
		ErrorSystem::Logger* ErrLog;
		WinDriver::DriverCallback* drvCallback;

	public:
		void Start() { return; }
		void Stop() { return; }

		bool AddToQueue(MIDIHDR*) { return true; }
		bool ResetQueue() { return true; }
		bool EmptyQueue() { return true; }
		bool IsQueueEmpty() { return true; }

		bool IsDummy() { return true; }
		void SetTempo(uint32_t ntempo) { return; }
		void SetTicksPerQN(uint16_t ntimeDiv) { return; }
		uint32_t GetTempo() { return 0; }
		uint16_t GetTicksPerQN() { return 0; }
		void GetPosition(MMTIME* mmtime) { return; }

		StreamPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr, ErrorSystem::Logger* PErr) { drvCallback = dptr; ErrLog = PErr; }
		~StreamPlayer() { drvCallback = nullptr; }
	};

	class CookedPlayer : public StreamPlayer {
	private:
		OMShared::Funcs Utils;
		OmniMIDI::SynthModule* synthModule;
		WinDriver::DriverCallback* drvCallback;

		MIDIHDR* mhdrQueue = 0;
		bool paused = true;
		bool goToBed = false;

		// tempo is microseconds per quarter note,
		// while timeDiv is the time division (lower 15 bits)
		bool smpte = false;
		uint8_t smpteFramerate = 30;
		uint8_t smpteFrameTicks = 4;
		double smpteFrameLen = 3.33333333333333364E+39;
		uint32_t timeAcc = 0;

		uint32_t tempo = 500000;
		uint16_t ticksPerQN = 0x18;
		uint32_t bpm = 60000000 / tempo;
		uint32_t byteAcc = 0;
		uint32_t tickAcc = 0;

		std::jthread _CooThread;
		void PlayerThread();

	public:
		void Start() { paused = false; }
		void Stop() { paused = true; }

		bool AddToQueue(MIDIHDR* hdr);
		bool ResetQueue();
		bool EmptyQueue();
		bool IsQueueEmpty() { return (!mhdrQueue); }

		bool IsDummy() { return false; }
		void SetTempo(uint32_t ntempo);
		void SetTicksPerQN(uint16_t nTicksPerQN);
		const uint32_t GetTempo() { return tempo; }
		const uint16_t GetTicksPerQN() { return ticksPerQN; }
		void GetPosition(MMTIME* mmtime);

		CookedPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr, ErrorSystem::Logger* PErr);
		~CookedPlayer();
	};
}

#endif
#endif