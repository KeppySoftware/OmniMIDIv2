/*

	OmniMIDI v15+ (Rewrite)

	This file contains the required code to run the driver under any OS.

*/

#ifndef _EVBUF_T_H
#define _EVBUF_T_H

#pragma once

#define BEvBuf				BaseEvBuf_t
#define EvBuf				EvBuf_t
#define LEvBuf				LEvBuf_t

#define DEF_EVBUF_SIZE		4096
#define MAX_EVBUF_SIZE		UINT_MAX / 2
#define MAX_LEVBUF_SIZE		64

#if !defined(_WIN64) && !defined(__x86_64__)
#define MAX_MIDIHDR_BUF		65536
#else
#define MAX_MIDIHDR_BUF		131072
#endif

#ifdef _STATSDEV
#include <iostream>
#include <future>
#endif

namespace OmniMIDI {
	typedef struct ShortEvent {
		unsigned char status = 0;
		unsigned char param1 = 0;
		unsigned char param2 = 0;
	} ShortEv, *PShortEv, SE, *PSE;

	typedef struct LongEvent {
		char ev[MAX_MIDIHDR_BUF] = { 0 };
		int len = 0;
	} LongEv, *PLongEv, LE, *PLE;

	class BaseEvBuf_t {
	protected:
		std::atomic<size_t> readHead = 0;
		std::atomic<size_t> writeHead = 0;
		size_t size = 0;

	private:
		unsigned int shortDummy = 0xFFFFFF;
		char longDummy = 'p';

	public:
		BaseEvBuf_t() { }
		BaseEvBuf_t(size_t size) { Allocate(size); }
		virtual ~BaseEvBuf_t() { Free(); }

		virtual bool Allocate(size_t ReqSize) { return true; }
		virtual bool Free() { return true; }

		// Short messages
		virtual void Write(unsigned int ev) { }
		virtual void Write(unsigned char status, unsigned char param1, unsigned char param2) { }
		virtual void PushSlow(unsigned char status, unsigned char param1, unsigned char param2) { }
		virtual unsigned int ReadDword() { return shortDummy; }
		virtual unsigned int PeekDword() { return shortDummy; }

		virtual unsigned int Read() { return 0; }
		virtual unsigned int Peek() { return 0; }

		// Long messages
		virtual void Write(char* ev, size_t len) { }
		virtual void ReadDword(char* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }
		virtual void PeekDword(char* ev, size_t* len) { *ev = longDummy; *len = sizeof(longDummy); }

		virtual bool NewEventsAvailable() { return false; }
		virtual size_t GetReadHeadPos() { return 0; }
		virtual size_t GetWriteHeadPos() { return 0; }
	};

	class LEvBuf_t : public BaseEvBuf_t {
	private:
		PLE buf = nullptr;

	public:
		LEvBuf_t() { }

		LEvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~LEvBuf_t() {
			Free();
		}

		bool Allocate(size_t ReqSize) {
			if (buf)
				return false;

			size = ReqSize;

			if (size < 2)
				size = 2;
			else if (size > MAX_LEVBUF_SIZE)
				size = MAX_LEVBUF_SIZE;

			buf = new LE[size];
			return true;
		}

		bool Free() {
			if (!buf)
				return false;

			size = 0;
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			return true;
		}

		void Write(char* ev, size_t len) {
			auto tWriteHead = writeHead + 1;
			if (tWriteHead >= size)
				tWriteHead = 0;

			if (tWriteHead != readHead)
				writeHead = tWriteHead;

			memcpy(buf[writeHead].ev, ev, len);
			buf[writeHead].len = len;
		}

		void ReadDword(char* ev, size_t* len) {
			if (readHead != writeHead)
			{
				if (++readHead >= size) readHead = 0;
				ev = buf[readHead].ev;
				*len = buf[readHead].len;
			}
		}

		void PeekDword(char* ev, size_t* len) {
			auto tNextHead = readHead + 1;

			if (tNextHead >= size)
				tNextHead = 0;

			ev = buf[readHead].ev;
			*len = buf[readHead].len;
		}

		bool NewEventsAvailable() {
			return (readHead != writeHead);
		}

		size_t GetReadHeadPos() { return readHead; }
		size_t GetWriteHeadPos() { return writeHead; }
	};

	class EvBuf_t : public BaseEvBuf_t {
	private:
		unsigned char midiRS = 0;

#ifdef _STATSDEV
		FILE* dummy;
		size_t evSent = 0;
		size_t evSkipped = 0;
#endif

		unsigned int* buf = nullptr;
		bool dontMiss = false;

		constexpr bool IsRunningStatus(unsigned int ev) { return ((ev & 0xFF) & 0x80) < 1; }
		constexpr unsigned int ApplyRunningStatus(unsigned int ev) {
			if (ev & 0x80) {
				midiRS = ev & 0xFF;
				return ev;
			}

			return (ev << 8) | midiRS;
		}
		constexpr unsigned char GetStatus(unsigned int ev) { return (ev & 0xFF) & 0x80; }
		constexpr unsigned char GetFirstParam(unsigned int ev) { return ((ev >> 8) & 0xFF); }
		constexpr unsigned char GetSecondParam(unsigned int ev) { return ((ev >> 16) & 0xFF); }

	public:
		EvBuf_t() { }

		EvBuf_t(size_t ReqSize) {
			Allocate(ReqSize);
		}

		~EvBuf_t() {
			Free();
		}

		void SetMissMode(bool val) {
			dontMiss = val;
		}

		bool Allocate(size_t ReqSize) {		
			if (buf)
				return false;
			
			size = ReqSize;

			if (size < 8)
				size = 8;

#if !defined(_WIN64) && !defined(__x86_64__)
			else if (size > MAX_EVBUF_SIZE)
				size = MAX_EVBUF_SIZE;
#endif

			buf = new unsigned int[size]();

			return true;
		}

		bool Free() {
			if (!buf)
				return false;

#ifdef _STATSDEV
			GetStats();
			evSent = 0;
			evSkipped = 0;
#endif
			readHead = 0;
			writeHead = 0;

			delete[] buf;
			buf = nullptr;

			size = 0;
			return true;
		}

		void Write(unsigned char status, unsigned char param1, unsigned char param2) {
			Write(status | (param1 << 8) | (param2 << 16));
		}

		void Write(unsigned int ev) {
			auto tReadHead = (size_t)readHead;
			auto tWriteHead = writeHead + 1;

			if (tWriteHead >= size)
				tWriteHead = 0;

#ifdef _STATSDEV
			evSent++;
#endif

			if (tWriteHead == tReadHead) {
#ifdef _STATSDEV
				evSkipped++;
#endif
				return;
			}

			buf[tWriteHead] = ev;
			writeHead = tWriteHead;
		}

		unsigned int Read() {
			auto tReadHead = (size_t)readHead;
			auto tWriteHead = (size_t)writeHead;

			if (tReadHead == tWriteHead)
				return 0;

			size_t tNextHead = tReadHead + 1;
			if (tNextHead >= size)
				tNextHead = 0;

			readHead = tNextHead;
			return buf[tNextHead];
		}

		unsigned int Peek() {
			auto tNextHead = (size_t)readHead + 1;

			if (tNextHead >= size)
				tNextHead = 0;

			return buf[tNextHead];
		}


		bool NewEventsAvailable() {
			return ((size_t)readHead != (size_t)writeHead);
		}

		size_t GetReadHeadPos() { return (size_t)readHead; }
		size_t GetWriteHeadPos() { return (size_t)writeHead; }
	};
}

#endif