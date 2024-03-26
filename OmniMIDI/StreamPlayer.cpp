#ifdef _WIN32

#include "StreamPlayer.hpp"

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
	unsigned long long deltaMicroseconds = 0;

	LOG(StrmErr, "PlayerThread is ready.");
	while (!goToBed) {
		while (paused || !mhdrQueue)
		{
			MiscFuncs.uSleep(-1);
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

			unsigned int deltaTicks = event->dwDeltaTime;
			tickAcc += deltaTicks;

			if (event->dwEvent & MEVT_F_CALLBACK) {
				drvCallback->CallbackFunction(MOM_POSITIONCB, (DWORD_PTR)hdr, 0);
				LOG(StrmErr, "MEVT_F_CALLBACK called! (MOM_POSITIONCB, ready to process addr: %x)", hdr);
			}

			if (!smpte && !noMoreDelta && event->dwDeltaTime) {
				deltaMicroseconds = (tempo * deltaTicks / ticksPerQN);
				timeAcc += deltaMicroseconds;

				MiscFuncs.uSleep(((signed long long)deltaMicroseconds) * -10); // * -10 to convert it to negative nanoseconds
				noMoreDelta = true;
				
				break;
			}
			else if (smpte) {
				double fpsToFrlen = (1000000 / smpteFramerate) / smpteFrameTicks;
				timeAcc += fpsToFrlen;
				MiscFuncs.uSleep(fpsToFrlen * -1);
			}

			noMoreDelta = false;

			switch (MEVT_EVENTTYPE(event->dwEvent)) {
			case MEVT_SHORTMSG:
				synthModule->PlayShortEvent(event->dwEvent);
				break;
			case MEVT_LONGMSG:
				synthModule->PlayLongEvent((char*)event->dwParms, event->dwEvent & 0xFFFFFF);
				break;
			case MEVT_TEMPO:
				if (!smpte)
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

void OmniMIDI::StreamPlayer::SetTicksPerQN(unsigned short nTicksPerQN) {
	char smptePortion = (nTicksPerQN >> 8) & 0xFF;

	bool qSmpte = smptePortion & 0x80;
	char qSmpteFramerate = smptePortion & 0x7F;
	char qSmpteFrameTicks = nTicksPerQN & 0xFF;

	if (qSmpte) {
		smpte = true;
		smpteFramerate = smptePortion;
		smpteFrameTicks = qSmpteFrameTicks;
		LOG(StrmErr, "Received new SMPTE setting. (framerate: %dFPS, %d ticks per frame)", smpteFramerate, qSmpteFrameTicks);
		return;
	}

	smpte = false;
	ticksPerQN = nTicksPerQN;
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

bool OmniMIDI::StreamPlayer::ResetQueue() {
	for (MIDIHDR* hdr = mhdrQueue; hdr; hdr = hdr->lpNext)
	{
		hdr->dwFlags &= ~MHDR_INQUEUE;
		hdr->dwFlags |= MHDR_DONE;
		drvCallback->CallbackFunction(MOM_DONE, (DWORD_PTR)hdr, 0);
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

void OmniMIDI::StreamPlayer::GetPosition(MMTIME* mmtime) {
	signed long long totalSeconds = timeAcc / 1000000;
	unsigned int playedSamples = (unsigned int)(timeAcc / (1000000 / synthModule->GetSampleRate()));

	mmtime->u.cb = byteAcc;

	mmtime->u.ms = timeAcc / 10000;

	mmtime->u.smpte.fps = smpteFramerate;
	mmtime->u.smpte.frame = (unsigned char)(totalSeconds / smpteFrameTicks) % smpteFramerate;
	mmtime->u.smpte.sec = totalSeconds % 60;
	mmtime->u.smpte.min = (totalSeconds / 60) % 60;
	mmtime->u.smpte.hour = totalSeconds / 3600;

	mmtime->u.sample = playedSamples;

	mmtime->u.midi.songptrpos = (tickAcc + ticksPerQN / 8) / (ticksPerQN / 4);

	mmtime->u.ticks = tickAcc;
}

#endif