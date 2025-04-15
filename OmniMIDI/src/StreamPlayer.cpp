/*

	OmniMIDI v15+ (Rewrite) for Windows NT

	This file contains the required code to run the driver under Windows 7 SP1 and later.
	This file is useful only if you want to compile the driver under Windows, it's not needed for Linux/macOS porting.

*/

#ifdef _WIN32

#include "StreamPlayer.hpp"

OmniMIDI::CookedPlayer::CookedPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr, ErrorSystem::Logger* perr) : StreamPlayer(sptr, dptr, perr) {
	synthModule = sptr;
	drvCallback = dptr;
	ErrLog = perr;
	Utils = OMShared::Funcs(ErrLog); 
	
	_CooThread = std::jthread(&CookedPlayer::PlayerThread, this);
	if (!_CooThread.joinable()) {
		CookedPlayer::~CookedPlayer();
	}
}

OmniMIDI::CookedPlayer::~CookedPlayer() {
	goToBed = true;

	if (_CooThread.joinable())
		_CooThread.join();
}

void OmniMIDI::CookedPlayer::PlayerThread() {
	bool noMoreDelta = false;
	unsigned long deltaMicroseconds = 0;

	Message("PlayerThread is ready.");
	while (!goToBed) {
		while (paused || !mhdrQueue)
		{
			Utils.MicroSleep(SLEEPVAL(1));
			if (goToBed) break;
		}

		if (goToBed) break;

		longMsg* hdr = mhdrQueue;
		if (hdr->flags & LONGMSG_DONE)
		{
			Message("Moving onto next packet... %x >>> %x", mhdrQueue, hdr->nextLongMsg);
			mhdrQueue = hdr->nextLongMsg;
			continue;
		}

		while (!paused) {
			if (hdr->dataOffset >= hdr->bufLen)
			{
				hdr->flags |= LONGMSG_DONE;
				hdr->flags &= ~LONGMSG_QUEUE;
				auto nexthdr = hdr->nextLongMsg;

				mhdrQueue = nexthdr;

#ifdef _WIN32
				drvCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)hdr->lpFollow, 0);
#endif

				hdr->dataOffset = 0;
				hdr = nexthdr;
				
				break;
			}

			MIDIEVENT* event = (MIDIEVENT*)(hdr->midiData + hdr->dataOffset);

			unsigned int deltaTicks = event->dwDeltaTime;
			tickAcc += deltaTicks;

#ifdef _WIN32
			if (event->dwEvent & LONGMSG_F_CALLBACK) {
				drvCallback->CallbackFunction(MOM_POSITIONCB, (DWORD_PTR)hdr->lpFollow, 0);
				Message("LONGMSG_CALLBACK called! (MOM_POSITIONCB, ready to process addr: %x)", hdr);
			}
#endif

			if (!smpte && !noMoreDelta && event->dwDeltaTime) {
				deltaMicroseconds = (tempo * deltaTicks / ticksPerQN);
				timeAcc += deltaMicroseconds;

				Utils.MicroSleep(SLEEPVAL(((signed long)deltaMicroseconds) * 10));
				noMoreDelta = true;
				
				break;
			}
			else if (smpte) {
				unsigned long fpsToFrlen = (unsigned long)((1000000 / smpteFramerate) / smpteFrameTicks);
				timeAcc += fpsToFrlen;
				Utils.MicroSleep(SLEEPVAL(fpsToFrlen));
			}

			noMoreDelta = false;

			switch (COOK_EVENTTYPE(event->dwEvent)) {
			case COOK_SHORTMSG:
				synthModule->PlayShortEvent((event->dwEvent >> 16) & 0xFF, (event->dwEvent >> 8) & 0xFF, event->dwEvent & 0xFF);
				break;
			case COOK_LONGMSG:
				synthModule->PlayLongEvent((char*)event->dwParms, event->dwEvent & 0xFFFFFF);
				break;
			case COOK_TEMPO:
				if (!smpte)
					SetTempo(event->dwEvent & 0xFFFFFF);

				break;
			default:
				break;
			}

			if (event->dwEvent & LONGMSG_F_LONG)
			{
				auto accum = ((event->dwEvent & 0xFFFFFF) + 3) & ~3;
				byteAcc += accum;
				hdr->dataOffset += accum;
			}

			byteAcc += 0xC;
			hdr->dataOffset += 0xC;
		}
	}

	// LOG(StrmErr, "timeAcc: %d - tickAcc: %d - byteAcc %x", timeAcc, tickAcc, byteAcc);
}

void OmniMIDI::CookedPlayer::SetTempo(unsigned int ntempo) {
	tempo = ntempo;
	bpm = 60000000 / tempo;
	Message("Received new tempo. (tempo: %d, ticksPerQN : %d, BPM: %d)", tempo, ticksPerQN, bpm);
}

void OmniMIDI::CookedPlayer::SetTicksPerQN(unsigned short nTicksPerQN) {
	char smptePortion = (nTicksPerQN >> 8) & 0xFF;

	bool qSmpte = smptePortion & 0x80;
	char qSmpteFramerate = smptePortion & 0x7F;
	char qSmpteFrameTicks = nTicksPerQN & 0xFF;

	if (qSmpte) {
		smpte = true;
		smpteFramerate = smptePortion;
		smpteFrameTicks = qSmpteFrameTicks;
		Message("Received new SMPTE setting. (framerate: %dFPS, %d ticks per frame)", smpteFramerate, qSmpteFrameTicks);
		return;
	}

	smpte = false;
	ticksPerQN = nTicksPerQN;
	Message("Received new TPQ. (tempo: %d, ticksPerQN : %d, BPM: %d)", tempo, ticksPerQN, bpm);
}

bool OmniMIDI::CookedPlayer::AddToQueue(longMsg* mhdr) {
	auto pmhdrQueue = mhdrQueue;

	if (!mhdrQueue) {
		mhdrQueue = mhdr;
	}
	else {
		if (pmhdrQueue->midiData == mhdr->midiData) {
			return false;
		}

		while (pmhdrQueue->nextLongMsg != nullptr)
		{
			pmhdrQueue = pmhdrQueue->nextLongMsg;

			if (pmhdrQueue->midiData == mhdr->midiData)
				return false;
		}

		pmhdrQueue->nextLongMsg = mhdr;

#ifdef _WIN32
		pmhdrQueue->lpFollow->lpNext = mhdr->lpFollow;
#endif
	}

	return true;
}

bool OmniMIDI::CookedPlayer::ResetQueue() {
	for (auto hdr = mhdrQueue; hdr; hdr = hdr->nextLongMsg)
	{
		hdr->flags &= ~LONGMSG_QUEUE;
		hdr->flags |= LONGMSG_DONE;

#ifdef _WIN32
		drvCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)hdr->lpFollow, 0);
#endif
	}

	return true;
}

bool OmniMIDI::CookedPlayer::EmptyQueue() {
	if (IsQueueEmpty())
		return false;

	paused = true;

	for (auto hdr = mhdrQueue; hdr; hdr = hdr->nextLongMsg)
	{
		hdr->flags &= ~LONGMSG_QUEUE;
		hdr->flags |= LONGMSG_DONE;
	}

	tickAcc = 0;
	timeAcc = 0;
	byteAcc = 0;
	smpteFramerate = 30;
	smpteFrameTicks = 4;
	tempo = 500000;
	ticksPerQN = 0x18;

	return true;
}

void OmniMIDI::CookedPlayer::GetPosition(MMTIME* mmtime) {
	signed long totalSeconds = timeAcc / 1000000;
	unsigned int playedSamples = (unsigned int)(timeAcc / (1000000 / synthModule->GetSampleRate()));

	mmtime->u.cb = byteAcc;

	mmtime->u.ms = timeAcc / 10000;

	mmtime->u.smpte.fps = smpteFramerate;
	mmtime->u.smpte.frame = (unsigned char)(totalSeconds / smpteFrameTicks) % smpteFramerate;
	mmtime->u.smpte.sec = totalSeconds % 60;
	mmtime->u.smpte.min = (totalSeconds / 60) % 60;
	mmtime->u.smpte.hour = (unsigned char)(totalSeconds / 3600);

	mmtime->u.sample = playedSamples;

	mmtime->u.midi.songptrpos = (tickAcc + ticksPerQN / 8) / (ticksPerQN / 4);

	mmtime->u.ticks = tickAcc;
}

#endif