#ifdef _WIN32

#include "StreamPlayer.h"

OmniMIDI::StreamPlayer::StreamPlayer(OmniMIDI::SynthModule* sptr, WinDriver::DriverCallback* dptr) {
	synthModule = sptr;
	drvCallback = dptr;

	_CooThread = std::jthread(&StreamPlayer::PlayerThread, this);
	if (!_CooThread.joinable()) {
		StreamPlayer::~StreamPlayer();
	}
}

OmniMIDI::StreamPlayer::~StreamPlayer() {
	goToBed = true;

	if (_CooThread.joinable())
		_CooThread.join();
}

void OmniMIDI::StreamPlayer::PlayerThread() {
	bool noMoreDelta = false;

	LOG(StrmErr, "PlayerThread is ready.");
	NTFuncs.querySystemTime(&startTime);
	
	while (!goToBed) {
		while (paused || !mhdrQueue)
		{
			NTFuncs.uSleep(-1);
			if (goToBed) break;
		}

		if (goToBed) break;

		MIDIHDR* hdr = mhdrQueue;
		if (hdr->dwFlags & MHDR_DONE)
		{
			LOG(StrmErr, "Moving onto next packet... %x >>> %x", mhdrQueue, hdr->lpNext);
			mhdrQueue = hdr->lpNext;
			continue;
		}

		while (!paused) {
			if (hdr->dwOffset >= hdr->dwBytesRecorded)
			{
				hdr->dwFlags |= MHDR_DONE;
				hdr->dwFlags &= ~MHDR_INQUEUE;
				MIDIHDR* nexthdr = hdr->lpNext;

				mhdrQueue = nexthdr;
				drvCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)hdr, 0);

				hdr->dwOffset = 0;
				hdr = nexthdr;

				break;
			}

			MIDIEVENT* event = (MIDIEVENT*)(hdr->lpData + hdr->dwOffset);
			curSongPtr = event->dwEvent;

			if (event->dwEvent & MEVT_F_CALLBACK) {
				drvCallback->CallbackFunction(MOM_POSITIONCB, (DWORD_PTR)hdr, 0);
				LOG(StrmErr, "MEVT_F_CALLBACK called! (MOM_POSITIONCB, ready to process addr: %x)", hdr);
			}
			
			if (!smpte && !noMoreDelta && event->dwDeltaTime) {
				unsigned int deltaTicks = event->dwDeltaTime;
				unsigned long long deltaMicroseconds = (tempo * deltaTicks / ticksPerQN);

				tickAcc += deltaTicks;

				NTFuncs.uSleep(((signed long long)deltaMicroseconds) * -10); // * -10 to convert it to negative nanoseconds

				noMoreDelta = true;
				break;
			}
			else if (smpte) {
				frameAcc++;
				unsigned int smpteDelta = -1000000000 / smpteval;
				NTFuncs.uSleep(smpteDelta);
			}

			NTFuncs.querySystemTime(&curTime);
			timeAcc = curTime - startTime;
			noMoreDelta = false;

			switch (MEVT_EVENTTYPE(event->dwEvent)) {
			case MEVT_SHORTMSG:
				synthModule->PlayShortEvent(event->dwEvent);
				break;
			case MEVT_LONGMSG:
				synthModule->PlayLongEvent((char*)event->dwParms, event->dwEvent & 0xFFFFFF);
				break;
			case MEVT_TEMPO:
				SetTempo(event->dwEvent & 0xFFFFFF);
				break;
			default:
				break;
			}

			if (event->dwEvent & MEVT_F_LONG)
			{
				DWORD accum = ((event->dwEvent & 0xFFFFFF) + 3) & ~3;
				byteAcc += accum;
				hdr->dwOffset += accum;
			}

			byteAcc += 0xC;
			hdr->dwOffset += 0xC;
		}
	}

	// LOG(StrmErr, "timeAcc: %d - tickAcc: %d - byteAcc %x", timeAcc, tickAcc, byteAcc);
}

void OmniMIDI::StreamPlayer::SetTempo(unsigned int ntempo) {
	tempo = ntempo;
	bpm = 60000000 / tempo;
	LOG(StrmErr, "Received new tempo. (tempo: %d, ticksPerQN : %d, BPM: %d)", tempo, ticksPerQN, bpm);
}

void OmniMIDI::StreamPlayer::SetTicksPerQN(unsigned int nTicksPerQN) {
	bool tsmpte = (nTicksPerQN & 0x4000) >> 12;
	unsigned int tsmpteval = (nTicksPerQN & 0x7F00) >> 8;

	if (tsmpte) {
		smpte = true;
		smpteval = tsmpteval;
		LOG(StrmErr, "Received new SMPTE setting. (framerate: %dFPS)", smpteval);
		return;
	}

	smpte = false;
	smpteval = -30;
	ticksPerQN = nTicksPerQN & 0x7FFF;
	LOG(StrmErr, "Received new TPQ. (tempo: %d, ticksPerQN : %d, BPM: %d)", tempo, ticksPerQN, bpm);
}

bool OmniMIDI::StreamPlayer::AddToQueue(MIDIHDR* mhdr) {
	MIDIHDR* pmhdrQueue = mhdrQueue;

	if (!mhdrQueue) {
		mhdrQueue = mhdr;
	}
	else {
		if (pmhdrQueue == mhdr) {
			return false;
		}

		while (pmhdrQueue->lpNext != nullptr)
		{
			pmhdrQueue = pmhdrQueue->lpNext;
			if (pmhdrQueue == mhdr)
				return false;
		}

		pmhdrQueue->lpNext = mhdr;
	}

	return true;
}

bool OmniMIDI::StreamPlayer::EmptyQueue() {
	if (IsQueueEmpty())
		return false;

	paused = true;

	for (MIDIHDR* hdr = mhdrQueue; hdr; hdr = hdr->lpNext)
	{
		hdr->dwFlags &= ~MHDR_INQUEUE;
		hdr->dwFlags |= MHDR_DONE;
		drvCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)hdr, 0);
	}

	tickAcc = 0;
	timeAcc = 0;
	byteAcc = 0;
	frameAcc = 0;
	startTime = 0;
	curTime = 0;
	curSongPtr = 0;

	return true;
}

void OmniMIDI::StreamPlayer::GetPosition(MMTIME* mmtime) {
	switch (mmtime->wType) {
	case TIME_BYTES:
		mmtime->u.cb = byteAcc;

	case TIME_MS:
		mmtime->u.ms = timeAcc / 10000;

	case TIME_SMPTE:
	{
		int totalseconds = curTime - startTime / 1000000000;

		mmtime->u.smpte.fps = smpteval * -1;
		mmtime->u.smpte.frame = frameAcc % smpteval;
		mmtime->u.smpte.sec = totalseconds % 60;
		mmtime->u.smpte.min = (totalseconds / 60) % 60;
		mmtime->u.smpte.hour = totalseconds / 3600;
	}

	case TIME_MIDI:
		mmtime->u.midi.songptrpos = curSongPtr;

	case TIME_TICKS:
	default:
		mmtime->wType = TIME_TICKS;
		mmtime->u.ticks = tickAcc;
	}
}

#endif