#pragma once

#ifdef _WIN32

#ifndef _COOKEDPLAYER_H
#define _COOKEDPLAYER_H

#define MAX_WAIT 10000

#include "WDMEntry.hpp"
#include <mmeapi.h>

namespace OmniMIDI {
	class StreamPlayer {
	private:
		ErrorSystem::Logger StrmErr;
		OMShared::Funcs MiscFuncs;
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
		unsigned long long timeAcc = 0;

		unsigned int tempo = 500000;
		unsigned short ticksPerQN = 0x18;
		unsigned int bpm = 60000000 / tempo;
		unsigned long long byteAcc = 0;
		unsigned long long tickAcc = 0;

		std::jthread _CooThread;
		void PlayerThread();

	public:
		void Start() { paused = false; }
		void Stop() { paused = true; }

		bool AddToQueue(MIDIHDR* hdr);
		bool ResetQueue();
		bool EmptyQueue();
		bool IsQueueEmpty() { return (!mhdrQueue); }

		void SetTempo(unsigned int ntempo);
		void SetTicksPerQN(unsigned short ntimeDiv);
		unsigned int GetTempo() { return tempo; }
		unsigned short GetTicksPerQN() { return ticksPerQN; }
		void GetPosition(MMTIME* mmtime);

		StreamPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr);
		~StreamPlayer();
	};
}

#endif
#endif