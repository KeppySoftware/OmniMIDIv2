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
#include "ErrSys.hpp"
#include "SynthMain.hpp"

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
		void SetTempo(unsigned int ntempo) { return; }
		void SetTicksPerQN(unsigned short ntimeDiv) { return; }
		unsigned int GetTempo() { return 0; }
		unsigned short GetTicksPerQN() { return 0; }
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
		unsigned char smpteFramerate = 30;
		unsigned char smpteFrameTicks = 4;
		double smpteFrameLen = 3.33333333333333364E+39;
		unsigned long timeAcc = 0;

		unsigned int tempo = 500000;
		unsigned short ticksPerQN = 0x18;
		unsigned int bpm = 60000000 / tempo;
		unsigned long byteAcc = 0;
		unsigned long tickAcc = 0;

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
		void SetTempo(unsigned int ntempo);
		void SetTicksPerQN(unsigned short ntimeDiv);
		const unsigned int GetTempo() { return tempo; }
		const unsigned short GetTicksPerQN() { return ticksPerQN; }
		void GetPosition(MMTIME* mmtime);

		CookedPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr, ErrorSystem::Logger* PErr);
		~CookedPlayer();
	};
}

#endif
#endif