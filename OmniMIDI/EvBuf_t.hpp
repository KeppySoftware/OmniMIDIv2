/*

	OmniMIDI v15+ (Rewrite)

	This file contains the required code to run the driver under any OS.

*/

#ifndef _EVBUF_T_H
#define _EVBUF_T_H

#pragma once

#define EvBuf				EvBuf_t

#ifdef _STATSDEV
#include <iostream>
#include <future>
#endif

namespace OmniMIDI {
	class EvBuf_t {
	private:
		size_t ReadHead = 0;
		size_t WriteHead = 0;

#ifdef _STATSDEV
		FILE* dummy;
		size_t EventsSent = 0;
		size_t EventsSkipped = 0;
#endif

		size_t Size = 0;
		unsigned int* Buffer;

	public:
		EvBuf_t(size_t ReqSize) {			
			Size = ReqSize;

			if (Size < 8)
				Size = 8;

			Buffer = new unsigned int[Size];
		}

		~EvBuf_t() {
#ifdef _STATSDEV
			GetStats();
			EventsSent = 0;
			EventsSkipped = 0;
#endif

			Size = 0;
			ReadHead = 0;
			WriteHead = 0;

			delete[] Buffer;
		}

		void Push(unsigned int ev) {
#ifndef EVBUF_OLD
			auto tWriteHead = WriteHead + 1;
			if (tWriteHead >= Size)
				tWriteHead = 0;

			if (tWriteHead != ReadHead)
				WriteHead = tWriteHead;

			Buffer[WriteHead] = ev;
#else
			auto tReadHead = ReadHead;
			auto tWriteHead = WriteHead + 1;

			if (tWriteHead >= Size)
				tWriteHead = 0;

#ifdef _STATSDEV
			EventsSent++;
#endif

			if (tWriteHead == tReadHead) {
#ifdef _STATSDEV
				EventsSkipped++;
#endif
				return;
			}

			Buffer[WriteHead] = ev;
			WriteHead = tWriteHead;
#endif
		}

		void Pop(unsigned int* ev) {
#ifndef EVBUF_OLD
			if (ReadHead != WriteHead)
			{
				if (++ReadHead >= Size) ReadHead = 0;
				*ev = Buffer[ReadHead];			
			}
#else
			auto tWriteHead = WriteHead;

			if (ReadHead == tWriteHead)
				return;

			size_t tNextHead = ReadHead + 1;
			if (tNextHead >= Size)
				tNextHead = 0;

			*ev = Buffer[ReadHead];
			ReadHead = tNextHead;
#endif
		}

		void Peek(unsigned int* ev) {
			auto tNextHead = ReadHead + 1;

			if (tNextHead >= Size)
				tNextHead = 0;

			*ev = Buffer[tNextHead];
		}

		size_t GetReadHeadPos() { return ReadHead; }
		size_t GetWriteHeadPos() { return WriteHead; }
	};
}

#endif